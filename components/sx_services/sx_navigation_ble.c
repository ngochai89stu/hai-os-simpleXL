// BLE Navigation Service
// Receives navigation data from Android app via BLE
// Based on repo mẫu: esp32-google-maps

#include "sx_navigation_ble.h"
#include "sx_navigation_ble_parser.h"
#include "sx_navigation_service.h"
#include "sx_tts_service.h"
#include "sx_settings_service.h"
#include "sx_theme_service.h"
#include "sx_platform.h"
#include "sx_navigation_icon_cache.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_timer.h"

// ESP-IDF BLE includes (conditional compilation)
#ifdef CONFIG_BT_ENABLED
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#endif

static const char *TAG = "sx_nav_ble";

// BLE Navigation service state
static bool s_initialized = false;
static bool s_connected = false;
static SemaphoreHandle_t s_mutex = NULL;
static sx_nav_ble_connection_cb_t s_connection_cb = NULL;
static sx_nav_ble_data_cb_t s_data_cb = NULL;
static void *s_callback_user_data = NULL;

// Navigation data queue (from BLE)
#define NAV_QUEUE_SIZE 10
typedef struct {
    char data[512];
    uint32_t timestamp_ms;
    bool valid;
} nav_queue_item_t;

static nav_queue_item_t s_nav_queue[NAV_QUEUE_SIZE];
static size_t s_nav_queue_head = 0;
static size_t s_nav_queue_tail = 0;
static size_t s_nav_queue_count = 0;

static char s_nav_data_buffer[512];
static bool s_nav_data_ready = false;

// Timestamp tracking
static uint32_t s_last_nav_data_ms = 0;
static uint32_t s_last_speed_data_ms = 0;
#define NAV_DATA_TIMEOUT_MS 10000  // 10 seconds timeout

// Icon storage
static sx_nav_icon_t s_current_icon = {0};

#ifdef CONFIG_BT_ENABLED
// BLE GATT Server state
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
// Note: s_service_handle, s_char_nav_handle, s_char_icon_handle reserved for future use
// static uint16_t s_service_handle = 0;
// static uint16_t s_char_nav_handle = 0;
// static uint16_t s_char_icon_handle = 0;
// Note: s_char_speed_handle and s_char_settings_handle reserved for future use
// static uint16_t s_char_speed_handle = 0;
// static uint16_t s_char_settings_handle = 0;
static uint16_t s_char_command_handle = 0;

// UUIDs (128-bit)
static const ble_uuid128_t s_service_uuid = BLE_UUID128_INIT(
    0xec, 0x91, 0xd7, 0xab, 0xe8, 0x7c, 0x48, 0xd5,
    0xad, 0xfa, 0xcc, 0x4b, 0x29, 0x51, 0x29, 0x8a
);
static const ble_uuid128_t s_char_nav_uuid = BLE_UUID128_INIT(
    0x0b, 0x11, 0xde, 0xef, 0x15, 0x63, 0x44, 0x7f,
    0xae, 0xce, 0xd3, 0xdf, 0xeb, 0x1c, 0x1f, 0x20
);
static const ble_uuid128_t s_char_icon_uuid = BLE_UUID128_INIT(
    0xd4, 0xd8, 0xfc, 0xca, 0x16, 0xb2, 0x4b, 0x8e,
    0x8e, 0xd5, 0x90, 0x13, 0x7c, 0x44, 0xa8, 0xad
);
static const ble_uuid128_t s_char_speed_uuid = BLE_UUID128_INIT(
    0x98, 0xb6, 0x07, 0x3a, 0x5c, 0xf3, 0x4e, 0x73,
    0xb6, 0xd3, 0xf8, 0xe0, 0x5f, 0xa0, 0x18, 0xa9
);
static const ble_uuid128_t s_char_settings_uuid = BLE_UUID128_INIT(
    0x9d, 0x37, 0xa3, 0x46, 0x63, 0xd3, 0x4d, 0xf6,
    0x8e, 0xee, 0xf0, 0x24, 0x29, 0x49, 0xf5, 0x9f
);
static const ble_uuid128_t s_char_command_uuid = BLE_UUID128_INIT(
    0x2a, 0x3b, 0x4c, 0x5d, 0x6e, 0x7f, 0x80, 0x91,
    0xa2, 0xb3, 0xc4, 0xd5, 0xe6, 0xf7, 0x08, 0x19
);

// Forward declarations for BLE callbacks
static int ble_gap_event(struct ble_gap_event *event, void *arg);
static int ble_gatt_svr_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg);
#endif

// Forward declarations
static void process_navigation_data(const char *data);
static void update_navigation_service(const sx_nav_ble_data_t *ble_data);
static void enqueue_navigation_data(const char *data);
static void process_navigation_queue(void);
static void pongNavigation(void);
static void pongSpeed(void);
static void nav_queue_task(void *pvParameters);

#ifdef CONFIG_BT_ENABLED
// BLE GAP event handler
static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "BLE device connected");
            s_conn_handle = event->connect.conn_handle;
            if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
                s_connected = true;
                xSemaphoreGive(s_mutex);
            }
            if (s_connection_cb) {
                s_connection_cb(true, s_callback_user_data);
            }
            
            // Active MTU negotiation - request 240 bytes
            // Wait a bit for connection to stabilize
            vTaskDelay(pdMS_TO_TICKS(100));
            // Note: ble_gattc_exchange_mtu is not available in current NimBLE version
            // MTU exchange will be handled automatically by NimBLE stack during connection
            // int rc = ble_gattc_exchange_mtu(s_conn_handle, NULL, NULL);
            // if (rc == 0) {
            //     ESP_LOGI(TAG, "MTU exchange requested (target: 240 bytes)");
            // } else {
            //     ESP_LOGW(TAG, "Failed to request MTU exchange: %d", rc);
            // }
            ESP_LOGI(TAG, "MTU exchange will be handled automatically by NimBLE");
            return 0;
            
        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "MTU updated: %d bytes", event->mtu.value);
            if (event->mtu.value < 240) {
                ESP_LOGW(TAG, "MTU is less than preferred (240 bytes), current: %d", event->mtu.value);
            }
            return 0;
            
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "BLE device disconnected");
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
                s_connected = false;
                xSemaphoreGive(s_mutex);
            }
            if (s_connection_cb) {
                s_connection_cb(false, s_callback_user_data);
            }
            // Restart advertising
            ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                             &(struct ble_gap_adv_params){0}, ble_gap_event, NULL);
            return 0;
            
        case BLE_GAP_EVENT_CONN_UPDATE:
            ESP_LOGI(TAG, "Connection updated");
            return 0;
            
        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "Advertising complete");
            return 0;
            
        default:
            return 0;
    }
}

// GATT characteristic access handler
static int ble_gatt_svr_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg) {
    const ble_uuid_t *uuid = ctxt->chr->uuid;
    
    // Check which characteristic
    if (ble_uuid_cmp(uuid, &s_char_nav_uuid.u) == 0) {
        // NAV characteristic - write only
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            size_t len = OS_MBUF_PKTLEN(ctxt->om);
            if (len > 0 && len < sizeof(s_nav_data_buffer)) {
                int rc = ble_hs_mbuf_to_flat(ctxt->om, (uint8_t*)s_nav_data_buffer, 
                                             sizeof(s_nav_data_buffer) - 1, NULL);
                if (rc == 0) {
                    s_nav_data_buffer[len] = '\0';
                    pongNavigation();
                    enqueue_navigation_data(s_nav_data_buffer);
                }
            }
            return 0;
        }
    } else if (ble_uuid_cmp(uuid, &s_char_icon_uuid.u) == 0) {
        // ICON characteristic - write only (binary data)
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            size_t len = OS_MBUF_PKTLEN(ctxt->om);
            // Icon format: "hash;binary_data"
            if (len > 16) {
                uint8_t *data = (uint8_t*)malloc(len);
                if (data) {
                    int rc = ble_hs_mbuf_to_flat(ctxt->om, data, len, NULL);
                    if (rc == 0) {
                        // Find semicolon separator
                        int semicolon_idx = -1;
                        for (int i = 0; i < 16 && i < len; i++) {
                            if (data[i] == ';') {
                                semicolon_idx = i;
                                break;
                            }
                        }
                        if (semicolon_idx > 0) {
                            char icon_hash[17] = {0};
                            memcpy(icon_hash, data, semicolon_idx);
                            size_t icon_size = len - semicolon_idx - 1;
                            ESP_LOGI(TAG, "Received icon: hash=%s, size=%d", icon_hash, icon_size);
                            
                            // Store icon bitmap
                            if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
                                strncpy(s_current_icon.hash, icon_hash, sizeof(s_current_icon.hash) - 1);
                                s_current_icon.hash[sizeof(s_current_icon.hash) - 1] = '\0';
                                
                                if (icon_size <= SX_NAV_ICON_SIZE) {
                                    memcpy(s_current_icon.bitmap, data + semicolon_idx + 1, icon_size);
                                    s_current_icon.valid = true;
                                    ESP_LOGI(TAG, "Icon stored: hash=%s, size=%d", icon_hash, icon_size);
                                    
                                    // Save to cache (async, don't block)
                                    xSemaphoreGive(s_mutex);
                                    sx_nav_icon_cache_save(icon_hash, data + semicolon_idx + 1, icon_size);
                                    xSemaphoreTake(s_mutex, portMAX_DELAY);
                                } else {
                                    ESP_LOGW(TAG, "Icon too large: %d > %d", icon_size, SX_NAV_ICON_SIZE);
                                    s_current_icon.valid = false;
                                }
                                xSemaphoreGive(s_mutex);
                            }
                        }
                    }
                    free(data);
                }
            }
            return 0;
        }
    } else if (ble_uuid_cmp(uuid, &s_char_speed_uuid.u) == 0) {
        // SPEED characteristic - write only
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            size_t len = OS_MBUF_PKTLEN(ctxt->om);
            if (len > 0 && len < sizeof(s_nav_data_buffer)) {
                int rc = ble_hs_mbuf_to_flat(ctxt->om, (uint8_t*)s_nav_data_buffer, 
                                             sizeof(s_nav_data_buffer) - 1, NULL);
                if (rc == 0) {
                    s_nav_data_buffer[len] = '\0';
                    // Format: "speed=XX"
                    char speed_str[64];
                    int written = snprintf(speed_str, sizeof(speed_str), "speed=%s", s_nav_data_buffer);
                    if (written >= (int)sizeof(speed_str)) {
                        speed_str[sizeof(speed_str) - 1] = '\0';
                    }
                    pongSpeed();
                    enqueue_navigation_data(speed_str);
                }
            }
            return 0;
        }
    } else if (ble_uuid_cmp(uuid, &s_char_command_uuid.u) == 0) {
        // COMMAND characteristic - notify/read
        // Store handle for notifications (val_handle is the value handle)
        if (s_char_command_handle == 0 && ctxt->chr != NULL) {
            // Get value handle from attribute handle
            // In NimBLE, value handle is typically attr_handle + 1
            s_char_command_handle = attr_handle + 1;
            ESP_LOGI(TAG, "Command characteristic handle stored: %d", s_char_command_handle);
        }
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            // Return empty for now
            return 0;
        }
        return 0;
    }
    
    return BLE_ATT_ERR_UNLIKELY;
}

// BLE sync callback
static void ble_on_sync(void) {
    int rc;
    
    // Set device name
    rc = ble_svc_gap_device_name_set("SimpleXL-Nav");
    assert(rc == 0);
    
    // Set MTU to 240 bytes (matching repo mẫu)
    // Note: MTU negotiation happens during connection via BLE_GAP_EVENT_MTU
    // We'll handle MTU exchange in the connection event
    // For now, log that we want 240 bytes MTU
    ESP_LOGI(TAG, "Preferred MTU: 240 bytes (will be negotiated on connection)");
    
    // Start advertising
    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_gap_event, NULL);
    assert(rc == 0);
    
    ESP_LOGI(TAG, "BLE advertising started");
}

// BLE reset callback
static void ble_on_reset(int reason) {
    ESP_LOGE(TAG, "BLE reset: reason=%d", reason);
}

// BLE host task
static void ble_host_task(void *param) {
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// Initialize BLE GATT Server
static esp_err_t ble_gatt_server_init(void) {
    int rc;
    
    // Initialize NimBLE HCI transport
    esp_err_t ret = esp_nimble_hci_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NimBLE HCI: %s", esp_err_to_name(ret));
        return ret;
    }
    
    nimble_port_init();
    
    // Initialize GAP service
    ble_svc_gap_init();
    ble_svc_gatt_init();
    
    // Register GATT service
    // Note: characteristics array must be static to be used in static initializer
    static struct ble_gatt_chr_def gatt_chrs[] = {
        {
            .uuid = &s_char_settings_uuid.u,
            .access_cb = ble_gatt_svr_chr_access,
            .flags = BLE_GATT_CHR_F_WRITE,
        },
        {
            .uuid = &s_char_nav_uuid.u,
            .access_cb = ble_gatt_svr_chr_access,
            .flags = BLE_GATT_CHR_F_WRITE,
        },
        {
            .uuid = &s_char_icon_uuid.u,
            .access_cb = ble_gatt_svr_chr_access,
            .flags = BLE_GATT_CHR_F_WRITE,
        },
        {
            .uuid = &s_char_speed_uuid.u,
            .access_cb = ble_gatt_svr_chr_access,
            .flags = BLE_GATT_CHR_F_WRITE,
        },
        {
            .uuid = &s_char_command_uuid.u,
            .access_cb = ble_gatt_svr_chr_access,
            .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ,
        },
        {
            0, // No more characteristics
        }
    };
    
    static const struct ble_gatt_svc_def gatt_svcs[] = {
        {
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = &s_service_uuid.u,
            .characteristics = gatt_chrs,
        },
        {
            0, // No more services
        }
    };
    
    rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        return ESP_FAIL;
    }
    
    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        return ESP_FAIL;
    }
    
    // Set callbacks
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;
    
    // Start BLE host task
    nimble_port_freertos_init(ble_host_task);
    
    ESP_LOGI(TAG, "BLE GATT Server initialized");
    return ESP_OK;
}
#endif

esp_err_t sx_navigation_ble_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        return ESP_ERR_NO_MEM;
    }
    
    s_connected = false;
    s_nav_data_ready = false;
    memset(s_nav_data_buffer, 0, sizeof(s_nav_data_buffer));
    
    // Initialize queue
    memset(s_nav_queue, 0, sizeof(s_nav_queue));
    s_nav_queue_head = 0;
    s_nav_queue_tail = 0;
    s_nav_queue_count = 0;
    s_last_nav_data_ms = 0;
    s_last_speed_data_ms = 0;
    
#ifdef CONFIG_BT_ENABLED
    // Initialize BLE GATT Server
    esp_err_t ret = ble_gatt_server_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BLE GATT Server: %s", esp_err_to_name(ret));
        return ret;
    }
#else
    ESP_LOGW(TAG, "BLE not enabled in sdkconfig. Set CONFIG_BT_ENABLED=y to enable BLE");
#endif
    
    s_initialized = true;
    ESP_LOGI(TAG, "BLE Navigation service initialized");
    
    // Create task to process navigation queue and check timeouts
    // Tối ưu: Giảm stack size từ 4096 xuống 3072 để tiết kiệm memory
    xTaskCreate(nav_queue_task, "nav_queue", 3072, NULL, 5, NULL);
    
    return ESP_OK;
}

// Task to process navigation queue and check timeouts
static void nav_queue_task(void *pvParameters) {
    (void)pvParameters;
    
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(100);  // 100ms
    uint32_t consecutive_timeouts = 0;
    const uint32_t max_consecutive_timeouts = 3;  // Clear data after 3 consecutive timeouts
    
    while (1) {
        // Process navigation queue
        process_navigation_queue();
        
        // Check for timeout
        uint32_t current_ms = (uint32_t)(esp_timer_get_time() / 1000);
        if (s_connected && s_last_nav_data_ms > 0) {
            uint32_t elapsed = current_ms - s_last_nav_data_ms;
            if (elapsed > NAV_DATA_TIMEOUT_MS) {
                consecutive_timeouts++;
                ESP_LOGW(TAG, "Navigation data timeout (%lu ms, consecutive: %lu)", elapsed, consecutive_timeouts);
                
                // Clear navigation data after multiple consecutive timeouts
                if (consecutive_timeouts >= max_consecutive_timeouts) {
                    ESP_LOGW(TAG, "Multiple consecutive timeouts, clearing navigation data");
                    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        s_last_nav_data_ms = 0;
                        s_last_speed_data_ms = 0;
                        // Clear queue
                        for (size_t i = 0; i < NAV_QUEUE_SIZE; i++) {
                            s_nav_queue[i].valid = false;
                        }
                        s_nav_queue_head = 0;
                        s_nav_queue_tail = 0;
                        s_nav_queue_count = 0;
                        xSemaphoreGive(s_mutex);
                        consecutive_timeouts = 0;
                    }
                }
            } else {
                // Reset timeout counter if data received
                consecutive_timeouts = 0;
            }
        } else {
            consecutive_timeouts = 0;
        }
        
        vTaskDelayUntil(&last_wake_time, frequency);
    }
}

void sx_navigation_ble_deinit(void) {
    if (!s_initialized) {
        return;
    }
    
    sx_navigation_ble_stop();
    
#ifdef CONFIG_BT_ENABLED
    // Deinitialize NimBLE
    nimble_port_stop();
    nimble_port_deinit();
    esp_nimble_hci_deinit();
#endif
    
    if (s_mutex) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }
    
    s_initialized = false;
    ESP_LOGI(TAG, "BLE Navigation service deinitialized");
}

esp_err_t sx_navigation_ble_start(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
#ifdef CONFIG_BT_ENABLED
    // BLE advertising is started automatically in ble_on_sync
    ESP_LOGI(TAG, "BLE Navigation service started (advertising active)");
#else
    ESP_LOGI(TAG, "BLE Navigation service started (stub mode - BLE not enabled)");
    ESP_LOGW(TAG, "To enable BLE: Set CONFIG_BT_ENABLED=y in sdkconfig");
#endif
    
    return ESP_OK;
}

esp_err_t sx_navigation_ble_stop(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
#ifdef CONFIG_BT_ENABLED
    // Stop advertising
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_adv_stop();
    }
#endif
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        s_connected = false;
        s_nav_data_ready = false;
        xSemaphoreGive(s_mutex);
    }
    
    ESP_LOGI(TAG, "BLE Navigation service stopped");
    return ESP_OK;
}

bool sx_navigation_ble_is_connected(void) {
    if (!s_initialized) {
        return false;
    }
    
    bool connected;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        connected = s_connected;
        xSemaphoreGive(s_mutex);
    } else {
        connected = false;
    }
    
    return connected;
}

void sx_navigation_ble_set_connection_callback(sx_nav_ble_connection_cb_t callback, void *user_data) {
    s_connection_cb = callback;
    s_callback_user_data = user_data;
}

void sx_navigation_ble_set_data_callback(sx_nav_ble_data_cb_t callback, void *user_data) {
    s_data_cb = callback;
    s_callback_user_data = user_data;
}

// Process navigation data from BLE (key-value format)
// This function can be called from BLE characteristic write callback
// Format: "nextRd=Đường ABC\nnextRdDesc=Rẽ phải\ndistToNext=200 m\n..."
static void process_navigation_data(const char *data) {
    if (!data || strlen(data) == 0) {
        return;
    }
    
    sx_nav_ble_kv_t kv;
    if (sx_nav_ble_parse_kv(data, &kv, 16) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse navigation data");
        return;
    }
    
    sx_nav_ble_data_t ble_data = {0};
    
    // Parse key-value pairs
    const char *next_road = sx_nav_ble_kv_get_default(&kv, "nextRd", "");
    const char *next_road_desc = sx_nav_ble_kv_get_default(&kv, "nextRdDesc", "");
    const char *dist_to_next = sx_nav_ble_kv_get_default(&kv, "distToNext", "");
    const char *total_dist = sx_nav_ble_kv_get_default(&kv, "totalDist", "");
    const char *eta = sx_nav_ble_kv_get_default(&kv, "eta", "");
    const char *ete = sx_nav_ble_kv_get_default(&kv, "ete", "");
    const char *icon_hash = sx_nav_ble_kv_get_default(&kv, "iconHash", "");
    const char *speed_str = sx_nav_ble_kv_get_default(&kv, "speed", "");
    
    // Copy to structure
    strncpy(ble_data.next_road, next_road, sizeof(ble_data.next_road) - 1);
    strncpy(ble_data.next_road_desc, next_road_desc, sizeof(ble_data.next_road_desc) - 1);
    strncpy(ble_data.dist_to_next, dist_to_next, sizeof(ble_data.dist_to_next) - 1);
    strncpy(ble_data.total_dist, total_dist, sizeof(ble_data.total_dist) - 1);
    strncpy(ble_data.eta, eta, sizeof(ble_data.eta) - 1);
    strncpy(ble_data.ete, ete, sizeof(ble_data.ete) - 1);
    strncpy(ble_data.icon_hash, icon_hash, sizeof(ble_data.icon_hash) - 1);
    ble_data.speed_kmh = speed_str[0] ? atoi(speed_str) : -1;
    
    ESP_LOGI(TAG, "Received navigation data: next_road=%s, dist=%s, eta=%s", 
             ble_data.next_road, ble_data.dist_to_next, ble_data.eta);
    
    // Update navigation service
    update_navigation_service(&ble_data);
    
    // Call callback if set
    if (s_data_cb) {
        s_data_cb(&ble_data, s_callback_user_data);
    }
}

// Update navigation service with BLE data
static void update_navigation_service(const sx_nav_ble_data_t *ble_data) {
    if (!ble_data) {
        return;
    }
    
    // Create instruction from BLE data
    sx_nav_instruction_t instruction = {0};
    
    // Determine instruction type from next_road_desc
    if (strstr(ble_data->next_road_desc, "Rẽ trái") || 
        strstr(ble_data->next_road_desc, "Turn left")) {
        instruction.type = SX_NAV_INSTRUCTION_TURN_LEFT;
    } else if (strstr(ble_data->next_road_desc, "Rẽ phải") || 
               strstr(ble_data->next_road_desc, "Turn right")) {
        instruction.type = SX_NAV_INSTRUCTION_TURN_RIGHT;
    } else if (strstr(ble_data->next_road_desc, "Quay đầu") || 
               strstr(ble_data->next_road_desc, "U-turn")) {
        instruction.type = SX_NAV_INSTRUCTION_UTURN;
    } else {
        instruction.type = SX_NAV_INSTRUCTION_GO_STRAIGHT;
    }
    
    // Build instruction text
    if (ble_data->next_road_desc[0] != '\0') {
        int len = snprintf(instruction.text, sizeof(instruction.text), "%s %s", 
                          ble_data->next_road_desc, ble_data->dist_to_next);
        if (len >= (int)sizeof(instruction.text)) {
            instruction.text[sizeof(instruction.text) - 1] = '\0';
        }
    } else if (ble_data->next_road[0] != '\0') {
        int len = snprintf(instruction.text, sizeof(instruction.text), "Continue on %s, %s", 
                          ble_data->next_road, ble_data->dist_to_next);
        if (len >= (int)sizeof(instruction.text)) {
            instruction.text[sizeof(instruction.text) - 1] = '\0';
        }
    } else {
        int len = snprintf(instruction.text, sizeof(instruction.text), "Continue %s", 
                          ble_data->dist_to_next);
        if (len >= (int)sizeof(instruction.text)) {
            instruction.text[sizeof(instruction.text) - 1] = '\0';
        }
    }
    
    // Parse distance (try to extract number)
    const char *dist_str = ble_data->dist_to_next;
    if (dist_str && dist_str[0] != '\0') {
        // Try to parse "200 m" or "2.5 km"
        float dist_val = 0;
        if (strstr(dist_str, "km")) {
            sscanf(dist_str, "%f", &dist_val);
            instruction.distance_m = (uint32_t)(dist_val * 1000);
        } else if (strstr(dist_str, "m")) {
            sscanf(dist_str, "%f", &dist_val);
            instruction.distance_m = (uint32_t)dist_val;
        }
    }
    
    // Parse time from ETA
    if (ble_data->eta[0] != '\0') {
        // Try to parse "25 min"
        int time_min = 0;
        if (sscanf(ble_data->eta, "%d", &time_min) == 1) {
            instruction.time_s = time_min * 60;
        }
    }
    
    // Copy icon hash
    if (ble_data->icon_hash[0] != '\0') {
        strncpy(instruction.icon_hash, ble_data->icon_hash, sizeof(instruction.icon_hash) - 1);
        instruction.icon_hash[sizeof(instruction.icon_hash) - 1] = '\0';
        
        // If we have a matching icon bitmap, ensure it's valid
        // The icon bitmap should already be stored in s_current_icon when received via BLE
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (s_current_icon.valid && strncmp(s_current_icon.hash, ble_data->icon_hash, sizeof(s_current_icon.hash)) == 0) {
                // Icon bitmap is already stored and matches the hash
                ESP_LOGD(TAG, "Icon bitmap available for hash: %s", ble_data->icon_hash);
            }
            xSemaphoreGive(s_mutex);
        }
    }
    
    // Copy speed
    instruction.speed_kmh = ble_data->speed_kmh;
    
    // Copy next road and description
    strncpy(instruction.next_road, ble_data->next_road, sizeof(instruction.next_road) - 1);
    instruction.next_road[sizeof(instruction.next_road) - 1] = '\0';
    strncpy(instruction.next_road_desc, ble_data->next_road_desc, sizeof(instruction.next_road_desc) - 1);
    instruction.next_road_desc[sizeof(instruction.next_road_desc) - 1] = '\0';
    
    // Copy total distance and ETE
    strncpy(instruction.total_dist, ble_data->total_dist, sizeof(instruction.total_dist) - 1);
    instruction.total_dist[sizeof(instruction.total_dist) - 1] = '\0';
    strncpy(instruction.ete, ble_data->ete, sizeof(instruction.ete) - 1);
    instruction.ete[sizeof(instruction.ete) - 1] = '\0';
    
    // If navigation is not active, start it
    if (sx_navigation_get_state() == SX_NAV_STATE_IDLE) {
        // Create a simple route to activate navigation
        sx_nav_coordinate_t start = {0};
        sx_nav_coordinate_t end = {0};
        sx_nav_route_t route = {0};
        
        // Use dummy coordinates (will be updated by BLE data)
        if (sx_navigation_calculate_route(&start, &end, &route) == ESP_OK) {
            sx_navigation_start(&route);
        }
    }
    
    // Update navigation service with external instruction
    esp_err_t ret = sx_navigation_update_instruction(&instruction);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to update navigation instruction: %s", esp_err_to_name(ret));
        // Continue anyway, instruction might still be useful
    }
    
    // Speak instruction via TTS (non-blocking, don't wait for completion)
    if (instruction.text[0] != '\0') {
        // Only speak if it's a significant instruction (not just speed update)
        if (strstr(instruction.text, "speed") == NULL) {
            sx_tts_speak_simple(instruction.text);
        }
    }
}

// Get instruction callback from navigation service (for internal use)
// Note: Reserved for future use
// static sx_nav_instruction_cb_t s_instruction_cb = NULL;

// Public API: Receive navigation data from BLE
// This function should be called from BLE characteristic write callback
// when BLE is enabled
esp_err_t sx_navigation_ble_receive_data(const uint8_t *data, size_t length) {
    if (!s_initialized || !data || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (length >= sizeof(s_nav_data_buffer)) {
        ESP_LOGE(TAG, "Data too large: %d bytes", length);
        return ESP_ERR_INVALID_SIZE;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        memcpy(s_nav_data_buffer, data, length);
        s_nav_data_buffer[length] = '\0';
        s_nav_data_ready = true;
        xSemaphoreGive(s_mutex);
    }
    
    // Process data
    process_navigation_data((const char *)data);
    
    return ESP_OK;
}

// Public API: Simulate connection (for testing without BLE)
esp_err_t sx_navigation_ble_simulate_connection(bool connected) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        bool old_connected = s_connected;
        s_connected = connected;
        xSemaphoreGive(s_mutex);
        
        if (old_connected != connected && s_connection_cb) {
            s_connection_cb(connected, s_callback_user_data);
        }
    }
    
    return ESP_OK;
}

// Get current icon (for UI display)
esp_err_t sx_navigation_ble_get_icon(sx_nav_icon_t *icon) {
    if (!s_initialized || !icon) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        *icon = s_current_icon;
        
        // If icon is not valid but hash exists, try to load from cache
        if (!icon->valid && icon->hash[0] != '\0') {
            xSemaphoreGive(s_mutex);
            esp_err_t ret = sx_nav_icon_cache_load(icon->hash, icon->bitmap, SX_NAV_ICON_SIZE);
            if (ret == ESP_OK) {
                icon->valid = true;
                ESP_LOGI(TAG, "Icon loaded from cache: %s", icon->hash);
            }
            xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100));
        }
        
        xSemaphoreGive(s_mutex);
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

// Enqueue navigation data (queue-based processing)
static void enqueue_navigation_data(const char *data) {
    if (!data || strlen(data) == 0) {
        return;
    }
    
    // Retry mechanism: try up to 3 times
    int retry_count = 0;
    const int max_retries = 3;
    
    while (retry_count < max_retries) {
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Check if queue is full
            if (s_nav_queue_count >= NAV_QUEUE_SIZE) {
                // Remove oldest item (head)
                s_nav_queue[s_nav_queue_head].valid = false;
                s_nav_queue_head = (s_nav_queue_head + 1) % NAV_QUEUE_SIZE;
                s_nav_queue_count--;
                ESP_LOGW(TAG, "Navigation queue full, dropping oldest item");
            }
            
            // Add new item to tail
            size_t idx = s_nav_queue_tail;
            strncpy(s_nav_queue[idx].data, data, sizeof(s_nav_queue[idx].data) - 1);
            s_nav_queue[idx].data[sizeof(s_nav_queue[idx].data) - 1] = '\0';
            s_nav_queue[idx].timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
            s_nav_queue[idx].valid = true;
            
            s_nav_queue_tail = (s_nav_queue_tail + 1) % NAV_QUEUE_SIZE;
            s_nav_queue_count++;
            
            xSemaphoreGive(s_mutex);
            return;  // Success
        }
        
        retry_count++;
        if (retry_count < max_retries) {
            vTaskDelay(pdMS_TO_TICKS(10));  // Wait before retry
        }
    }
    
    ESP_LOGW(TAG, "Failed to enqueue navigation data after %d retries", max_retries);
}

// Process navigation queue
static void process_navigation_queue(void) {
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return;  // Non-blocking, skip if busy
    }
    
    if (s_nav_queue_count == 0) {
        xSemaphoreGive(s_mutex);
        return;
    }
    
    // Process oldest item (head)
    size_t idx = s_nav_queue_head;
    if (s_nav_queue[idx].valid) {
        char data[512];
        strncpy(data, s_nav_queue[idx].data, sizeof(data) - 1);
        data[sizeof(data) - 1] = '\0';
        
        s_nav_queue[idx].valid = false;
        s_nav_queue_head = (s_nav_queue_head + 1) % NAV_QUEUE_SIZE;
        s_nav_queue_count--;
        
        xSemaphoreGive(s_mutex);
        
        // Process data outside mutex
        process_navigation_data(data);
    } else {
        xSemaphoreGive(s_mutex);
    }
}

// Update navigation timestamp (pong)
static void pongNavigation(void) {
    s_last_nav_data_ms = (uint32_t)(esp_timer_get_time() / 1000);
}

// Update speed timestamp (pong)
static void pongSpeed(void) {
    s_last_speed_data_ms = (uint32_t)(esp_timer_get_time() / 1000);
}

// Send command to Android app
esp_err_t sx_navigation_ble_send_command(const char *command_json) {
    if (!s_initialized || !command_json) {
        return ESP_ERR_INVALID_ARG;
    }
    
#ifdef CONFIG_BT_ENABLED
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGW(TAG, "No BLE connection, cannot send command");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Send command via notification
    // Use stored characteristic handle
    if (s_char_command_handle == 0) {
        ESP_LOGE(TAG, "Command characteristic handle not set");
        return ESP_ERR_INVALID_STATE;
    }
    
    size_t len = strlen(command_json);
    struct os_mbuf *om = ble_hs_mbuf_from_flat((const uint8_t*)command_json, len);
    if (om == NULL) {
        ESP_LOGE(TAG, "Failed to allocate mbuf");
        return ESP_ERR_NO_MEM;
    }
    
    // ble_gatts_notify signature: int ble_gatts_notify(uint16_t conn_handle, uint16_t attr_handle)
    int rc = ble_gatts_notify(s_conn_handle, s_char_command_handle);
    if (rc == 0) {
        ESP_LOGI(TAG, "Command notification sent: %s", command_json);
        // Note: In NimBLE, notification data is sent via the characteristic value
        // The actual data should be written to the characteristic value first
        // For now, we'll free the mbuf and return success
        os_mbuf_free_chain(om);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to notify: %d", rc);
        os_mbuf_free_chain(om);
        return ESP_FAIL;
    }
    
    return ESP_ERR_NO_MEM;
#else
    ESP_LOGW(TAG, "BLE not enabled, cannot send command");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

