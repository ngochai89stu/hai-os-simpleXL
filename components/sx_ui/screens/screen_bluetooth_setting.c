#include "screen_bluetooth_setting.h"

#include <esp_log.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "sx_state.h"
#include "sx_bluetooth_service.h"

static const char *TAG = "screen_bluetooth_setting";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_device_list = NULL;
static lv_obj_t *s_scan_btn = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_container = NULL;
static sx_bluetooth_device_t s_devices[20];
static size_t s_device_count = 0;
static bool s_scanning = false;

// Forward declarations
static void update_status(void);
static void scan_btn_cb(lv_event_t *e);
static void device_item_click_cb(lv_event_t *e);
static void refresh_device_list(void);
static void bluetooth_connection_cb(sx_bluetooth_state_t state, const char *device_address);

static void on_create(void) {
    ESP_LOGI(TAG, "Bluetooth Settings screen onCreate");
    
    if (!lvgl_port_lock(0)) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock");
        return;
    }
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        lvgl_port_unlock();
        return;
    }
    
    s_container = container;
    
    // Set background
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "Bluetooth Settings");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Status label
    s_status_label = lv_label_create(s_content);
    lv_label_set_text(s_status_label, "");
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x888888), 0);
    lv_obj_set_width(s_status_label, LV_PCT(100));
    
    // Scan button (matching web demo style)
    s_scan_btn = lv_btn_create(s_content);
    lv_obj_set_size(s_scan_btn, LV_PCT(100), 50);
    lv_obj_set_style_bg_color(s_scan_btn, lv_color_hex(0x5b7fff), LV_PART_MAIN);
    lv_obj_set_style_radius(s_scan_btn, 5, LV_PART_MAIN);
    lv_obj_t *scan_label = lv_label_create(s_scan_btn);
    lv_label_set_text(scan_label, "Scan Devices");
    lv_obj_set_style_text_font(scan_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(scan_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(scan_label);
    
    // Device list (scrollable) - matching web demo
    s_device_list = lv_obj_create(s_content);
    lv_obj_set_size(s_device_list, LV_PCT(100), LV_PCT(100) - 120);
    lv_obj_set_style_bg_opa(s_device_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_device_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_device_list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_device_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_device_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Add event handler for scan button
    lv_obj_add_event_cb(s_scan_btn, scan_btn_cb, LV_EVENT_CLICKED, NULL);
    
    // Register Bluetooth connection callback
    sx_bluetooth_set_connection_callback(bluetooth_connection_cb);
    
    lvgl_port_unlock();
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_BLUETOOTH_SETTING, "Bluetooth Settings", container, s_device_list);
    #endif
    
    // Initial status update (need to lock again)
    if (lvgl_port_lock(0)) {
        update_status();
        lvgl_port_unlock();
    }
}

static void update_status(void) {
    if (s_status_label == NULL) {
        return;
    }
    
    sx_bluetooth_state_t state = sx_bluetooth_get_state();
    sx_bluetooth_device_t connected_device = {0};
    
    if (state == SX_BT_STATE_CONNECTED && sx_bluetooth_get_connected_device(&connected_device) == ESP_OK) {
        char status[128];
        snprintf(status, sizeof(status), "Connected: %s", connected_device.name[0] ? connected_device.name : connected_device.address);
        lv_label_set_text(s_status_label, status);
    } else if (state == SX_BT_STATE_DISCOVERING || s_scanning) {
        lv_label_set_text(s_status_label, "Scanning...");
    } else if (state == SX_BT_STATE_CONNECTING) {
        lv_label_set_text(s_status_label, "Connecting...");
    } else if (state == SX_BT_STATE_DISCONNECTING) {
        lv_label_set_text(s_status_label, "Disconnecting...");
    } else if (state == SX_BT_STATE_ERROR) {
        lv_label_set_text(s_status_label, "Error");
    } else {
        lv_label_set_text(s_status_label, s_device_count > 0 ? "Ready" : "Tap Scan to find devices");
    }
}

static void refresh_device_list(void) {
    if (s_device_list == NULL) {
        return;
    }
    
    // Clear existing items
    lv_obj_clean(s_device_list);
    
    // Get discovered devices
    esp_err_t ret = sx_bluetooth_get_discovered_devices(s_devices, 20, &s_device_count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get discovered devices");
        s_device_count = 0;
    }
    
    // Add devices to list
    for (size_t i = 0; i < s_device_count; i++) {
        lv_obj_t *device_item = lv_obj_create(s_device_list);
        lv_obj_set_size(device_item, LV_PCT(100), 60);
        lv_obj_set_style_bg_color(device_item, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        lv_obj_set_style_bg_color(device_item, lv_color_hex(0x3a3a3a), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_border_width(device_item, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(device_item, 10, LV_PART_MAIN);
        lv_obj_set_style_radius(device_item, 5, LV_PART_MAIN);
        
        // Store device index in user data
        lv_obj_set_user_data(device_item, (void *)(intptr_t)i);
        
        // Device name label
        lv_obj_t *label = lv_label_create(device_item);
        const char *name = s_devices[i].name[0] ? s_devices[i].name : s_devices[i].address;
        lv_label_set_text(label, name);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
        
        // Status indicator (paired/connected)
        char status_str[32] = "";
        if (s_devices[i].connected) {
            strcpy(status_str, "🔵 Connected");
        } else if (s_devices[i].paired) {
            strcpy(status_str, "✓ Paired");
        } else {
            strcpy(status_str, "○ Available");
        }
        
        lv_obj_t *status = lv_label_create(device_item);
        lv_label_set_text(status, status_str);
        lv_obj_set_style_text_font(status, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(status, lv_color_hex(0x888888), 0);
        lv_obj_align(status, LV_ALIGN_RIGHT_MID, -10, 0);
        
        // Add click event
        lv_obj_add_event_cb(device_item, device_item_click_cb, LV_EVENT_CLICKED, NULL);
    }
    
    update_status();
}

static void scan_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Scan button clicked");
        
        if (s_scanning) {
            sx_bluetooth_stop_discovery();
            s_scanning = false;
            update_status();
            return;
        }
        
        if (s_status_label != NULL) {
            lv_label_set_text(s_status_label, "Scanning...");
        }
        
        s_scanning = true;
        esp_err_t ret = sx_bluetooth_start_discovery();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start Bluetooth discovery");
            s_scanning = false;
            if (s_status_label != NULL) {
                lv_label_set_text(s_status_label, "Scan failed");
            }
            return;
        }
        
        // Wait a bit for devices to be discovered, then refresh list
        // In real implementation, this should be done via callback
        vTaskDelay(pdMS_TO_TICKS(3000)); // Wait 3 seconds
        s_scanning = false;
        sx_bluetooth_stop_discovery();
        
        refresh_device_list();
    }
}

static void device_item_click_cb(lv_event_t *e) {
    lv_obj_t *item = lv_event_get_target(e);
    intptr_t device_idx = (intptr_t)lv_obj_get_user_data(item);
    
    if (device_idx >= 0 && device_idx < (intptr_t)s_device_count) {
        const sx_bluetooth_device_t *device = &s_devices[device_idx];
        ESP_LOGI(TAG, "Device clicked: %s (%s)", device->name, device->address);
        
        if (device->connected) {
            // Disconnect
            ESP_LOGI(TAG, "Disconnecting from device");
            esp_err_t ret = sx_bluetooth_disconnect();
            if (ret == ESP_OK) {
                if (s_status_label != NULL) {
                    lv_label_set_text(s_status_label, "Disconnecting...");
                }
            } else {
                ESP_LOGE(TAG, "Failed to disconnect");
            }
        } else {
            // Connect
            ESP_LOGI(TAG, "Connecting to device");
            esp_err_t ret = sx_bluetooth_connect(device->address);
            if (ret == ESP_OK) {
                if (s_status_label != NULL) {
                    lv_label_set_text(s_status_label, "Connecting...");
                }
            } else {
                ESP_LOGE(TAG, "Failed to connect to device");
                if (s_status_label != NULL) {
                    lv_label_set_text(s_status_label, "Connection failed");
                }
            }
        }
        
        // Refresh list after a delay to show updated status
        vTaskDelay(pdMS_TO_TICKS(1000));
        refresh_device_list();
    }
}

static void bluetooth_connection_cb(sx_bluetooth_state_t state, const char *device_address) {
    ESP_LOGI(TAG, "Bluetooth connection state changed: %d, device: %s", state, device_address ? device_address : "NULL");
    
    // Update UI in LVGL task context
    if (lvgl_port_lock(0)) {
        update_status();
        refresh_device_list();
        lvgl_port_unlock();
    }
}

static void on_show(void) {
    ESP_LOGI(TAG, "Bluetooth Settings screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_BLUETOOTH_SETTING);
    #endif
    
    // Refresh device list when showing screen
    if (lvgl_port_lock(0)) {
        refresh_device_list();
        lvgl_port_unlock();
    }
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Bluetooth Settings screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_BLUETOOTH_SETTING);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Bluetooth Settings screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_BLUETOOTH_SETTING);
    #endif
    
    if (lvgl_port_lock(0)) {
        if (s_top_bar != NULL) {
            lv_obj_del(s_top_bar);
            s_top_bar = NULL;
        }
        if (s_content != NULL) {
            lv_obj_del(s_content);
            s_content = NULL;
        }
        lvgl_port_unlock();
    }
}

static void on_update(const sx_state_t *state) {
    if (!lvgl_port_lock(0)) {
        return;
    }
    
    // Update connection status
    if (state != NULL && s_status_label != NULL) {
        update_status();
        refresh_device_list();
    }
    
    lvgl_port_unlock();
}

// Register this screen
void screen_bluetooth_setting_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
        .on_update = on_update,
    };
    ui_screen_registry_register(SCREEN_ID_BLUETOOTH_SETTING, &callbacks);
}
