#include "sx_wifi_service.h"

#include <esp_log.h>
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_network_optimizer.h"

static const char *TAG = "sx_wifi";

// WiFi event group bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// WiFi state
static bool s_initialized = false;
static bool s_started = false;
static sx_wifi_config_t s_cfg = {0};
static esp_netif_t *s_netif = NULL;
static EventGroupHandle_t s_wifi_event_group = NULL;
static int s_retry_num = 0;
static const int MAX_RETRY = 5;

// Connection info
static char s_current_ssid[33] = {0};
static char s_current_password[65] = {0};
static char s_ip_address[16] = {0};
static int8_t s_rssi = 0;
static uint8_t s_channel = 0;
static bool s_connected = false;

// Forward declarations
static void sx_wifi_event_handler(void* arg, esp_event_base_t event_base,
                                  int32_t event_id, void* event_data);
// Reserved for future WiFi task implementation
// static void sx_wifi_task(void *arg);

esp_err_t sx_wifi_service_init(const sx_wifi_config_t *cfg) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    if (cfg != NULL) {
        s_cfg = *cfg;
    } else {
        // Default config
        s_cfg.auto_reconnect = true;
        s_cfg.reconnect_interval_ms = 5000;
    }
    
    // Initialize network optimizer
    sx_network_optimizer_init();
    
    // Initialize network interface (required before creating WiFi netif)
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Create default event loop (required for WiFi events)
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        // ESP_ERR_INVALID_STATE means event loop already exists, which is OK
        ESP_LOGE(TAG, "Failed to create default event loop: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize network interface
    s_netif = esp_netif_create_default_wifi_sta();
    if (s_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create default WiFi STA netif");
        return ESP_FAIL;
    }
    
    // Initialize WiFi with default config
    wifi_init_config_t cfg_init = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg_init);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register event handlers
    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                              ESP_EVENT_ANY_ID,
                                              &sx_wifi_event_handler,
                                              NULL,
                                              NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_event_handler_instance_register(IP_EVENT,
                                              IP_EVENT_STA_GOT_IP,
                                              &sx_wifi_event_handler,
                                              NULL,
                                              NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Create event group
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create WiFi event group");
        return ESP_ERR_NO_MEM;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "WiFi service initialized");
    return ESP_OK;
}

esp_err_t sx_wifi_service_start(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_started) {
        return ESP_OK;
    }
    
    // Set WiFi mode to station
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Start WiFi
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    s_started = true;
    ESP_LOGI(TAG, "WiFi service started");
    return ESP_OK;
}

esp_err_t sx_wifi_service_stop(void) {
    if (!s_started) {
        return ESP_OK;
    }
    
    esp_err_t ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    s_started = false;
    s_connected = false;
    memset(s_current_ssid, 0, sizeof(s_current_ssid));
    memset(s_ip_address, 0, sizeof(s_ip_address));
    s_rssi = 0;
    s_channel = 0;
    
    ESP_LOGI(TAG, "WiFi service stopped");
    return ESP_OK;
}

int sx_wifi_scan(sx_wifi_network_info_t *networks, uint8_t max_networks) {
    if (!s_started || networks == NULL || max_networks == 0) {
        return -1;
    }
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 100,
                .max = 300
            }
        }
    };
    
    esp_err_t ret = esp_wifi_scan_start(&scan_config, true); // Blocking scan
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi scan: %s", esp_err_to_name(ret));
        return -1;
    }
    
    uint16_t ap_count = 0;
    ret = esp_wifi_scan_get_ap_num(&ap_count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get scan count: %s", esp_err_to_name(ret));
        return -1;
    }
    
    if (ap_count == 0) {
        ESP_LOGI(TAG, "No WiFi networks found");
        return 0;
    }
    
    // Limit to max_networks
    if (ap_count > max_networks) {
        ap_count = max_networks;
    }
    
    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (ap_records == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for scan results");
        return -1;
    }
    
    ret = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get scan records: %s", esp_err_to_name(ret));
        free(ap_records);
        return -1;
    }
    
    // Convert to sx_wifi_network_info_t
    for (uint16_t i = 0; i < ap_count; i++) {
        strncpy(networks[i].ssid, (char *)ap_records[i].ssid, sizeof(networks[i].ssid) - 1);
        networks[i].ssid[sizeof(networks[i].ssid) - 1] = '\0';
        networks[i].rssi = ap_records[i].rssi;
        networks[i].channel = ap_records[i].primary;
        networks[i].is_encrypted = (ap_records[i].authmode != WIFI_AUTH_OPEN);
        networks[i].auth_mode = ap_records[i].authmode;
    }
    
    free(ap_records);
    ESP_LOGI(TAG, "WiFi scan completed: %d networks found", ap_count);
    return (int)ap_count;
}

esp_err_t sx_wifi_connect(const char *ssid, const char *password) {
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (ssid == NULL || strlen(ssid) == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Disconnect if already connected
    if (s_connected) {
        sx_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // Store credentials
    strncpy(s_current_ssid, ssid, sizeof(s_current_ssid) - 1);
    s_current_ssid[sizeof(s_current_ssid) - 1] = '\0';
    
    if (password != NULL) {
        strncpy(s_current_password, password, sizeof(s_current_password) - 1);
        s_current_password[sizeof(s_current_password) - 1] = '\0';
    } else {
        s_current_password[0] = '\0';
    }
    
    // Configure WiFi
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password != NULL && strlen(password) > 0) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Clear event bits
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    s_retry_num = 0;
    
    // Connect
    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi connection: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);
    return ESP_OK;
}

esp_err_t sx_wifi_disconnect(void) {
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    s_connected = false;
    memset(s_current_ssid, 0, sizeof(s_current_ssid));
    memset(s_ip_address, 0, sizeof(s_ip_address));
    s_rssi = 0;
    s_channel = 0;
    
    ESP_LOGI(TAG, "WiFi disconnected");
    return ESP_OK;
}

bool sx_wifi_is_connected(void) {
    return s_connected;
}

const char *sx_wifi_get_ssid(void) {
    return s_connected ? s_current_ssid : NULL;
}

int8_t sx_wifi_get_rssi(void) {
    return s_connected ? s_rssi : 0;
}

uint8_t sx_wifi_get_channel(void) {
    return s_connected ? s_channel : 0;
}

const char *sx_wifi_get_ip_address(void) {
    return s_connected ? s_ip_address : NULL;
}

static void sx_wifi_event_handler(void* arg, esp_event_base_t event_base,
                                  int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "WiFi station started");
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            if (s_connected) {
                ESP_LOGI(TAG, "WiFi disconnected");
                s_connected = false;
                memset(s_ip_address, 0, sizeof(s_ip_address));
                s_rssi = 0;
                s_channel = 0;
                
                // Publish disconnected event
                sx_event_t evt = {
                    .type = SX_EVT_WIFI_DISCONNECTED,
                    .arg0 = 0,
                    .arg1 = 0,
                    .ptr = NULL
                };
                sx_dispatcher_post_event(&evt);
            }
            
            if (s_cfg.auto_reconnect && s_retry_num < MAX_RETRY) {
                s_retry_num++;
                
                // Use network optimizer for retry delay
                sx_retry_config_t retry_config = {
                    .initial_delay_ms = 1000,
                    .max_delay_ms = 30000,
                    .max_attempts = MAX_RETRY,
                    .exponential_backoff = true,
                    .backoff_multiplier = 1.5f,
                };
                uint32_t delay_ms = sx_network_optimizer_get_retry_delay(s_retry_num, &retry_config);
                
                ESP_LOGI(TAG, "Retrying WiFi connection (%d/%d) after %lu ms...", s_retry_num, MAX_RETRY, (unsigned long)delay_ms);
                sx_network_optimizer_record_reconnect();
                vTaskDelay(pdMS_TO_TICKS(delay_ms));
                esp_wifi_connect();
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            } else {
                ESP_LOGE(TAG, "WiFi connection failed after %d retries", MAX_RETRY);
                sx_network_optimizer_record_connection(false);
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
        } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
            wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
            ESP_LOGI(TAG, "WiFi connected to: %s (channel: %d)", event->ssid, event->channel);
            s_channel = event->channel;
            s_retry_num = 0;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            snprintf(s_ip_address, sizeof(s_ip_address), IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(TAG, "Got IP address: %s", s_ip_address);
            
            // Get RSSI
            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                s_rssi = ap_info.rssi;
            }
            
            s_connected = true;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            
            // Record successful connection
            sx_network_optimizer_record_connection(true);
            
            // Publish connected event
            sx_event_t evt = {
                .type = SX_EVT_WIFI_CONNECTED,
                .arg0 = 0,
                .arg1 = 0,
                .ptr = (void *)s_current_ssid
            };
            sx_dispatcher_post_event(&evt);
        }
    }
}

// Reserved for future WiFi task implementation
// static void sx_wifi_task(void *arg) {
//     (void)arg;
//     // Task for WiFi management (if needed)
//     while (1) {
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }

