#include "sx_lazy_loader.h"

#include <esp_log.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Service headers
#include "sx_wifi_service.h"
#include "sx_stt_service.h"
#include "sx_wake_word_service.h"
#include "sx_audio_afe.h"
#include "sx_tts_service.h"
#include "sx_navigation_ble.h"
#include "sx_navigation_icon_cache.h"
#include "sx_chatbot_service.h"
#include "sx_radio_service.h"
#include "sx_music_online_service.h"
#include "sx_sd_service.h"
#include "sx_ir_service.h"
#include "sx_bluetooth_service.h"
#include "sx_weather_service.h"
#include "sx_navigation_service.h"
#include "sx_diagnostics_service.h"
#include "sx_intent_service.h"
#include "sx_audio_protocol_bridge.h"
#include "sx_playlist_manager.h"
#include "sx_protocol_ws.h"
#include "sx_protocol_mqtt.h"
#include "sx_settings_service.h"
#include "sx_sd_service.h"
#include "driver/spi_master.h"

static const char *TAG = "sx_lazy_loader";

// Service initialization state
static bool s_service_initialized[SX_LAZY_SERVICE_MAX] = {0};
static SemaphoreHandle_t s_mutex = NULL;

// Initialize mutex on first use
static void lazy_loader_init_mutex(void) {
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex");
        }
    }
}

// Helper to check and set initialization state
static bool check_and_set_initialized(sx_lazy_service_t service) {
    lazy_loader_init_mutex();
    if (s_mutex == NULL) {
        return false;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    
    bool was_initialized = s_service_initialized[service];
    if (!was_initialized) {
        s_service_initialized[service] = true;
    }
    
    xSemaphoreGive(s_mutex);
    return was_initialized;
}

esp_err_t sx_lazy_service_init(sx_lazy_service_t service) {
    if (service >= SX_LAZY_SERVICE_MAX) {
        ESP_LOGE(TAG, "Invalid service type: %d", service);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check if already initialized
    if (check_and_set_initialized(service)) {
        ESP_LOGD(TAG, "Service %d already initialized", service);
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing lazy service: %d", service);
    
    esp_err_t ret = ESP_OK;
    
    switch (service) {
        case SX_LAZY_SERVICE_WIFI: {
            sx_wifi_config_t wifi_cfg = {
                .auto_reconnect = true,
                .reconnect_interval_ms = 5000,
            };
            ret = sx_wifi_service_init(&wifi_cfg);
            if (ret == ESP_OK) {
                ret = sx_wifi_service_start();
            }
            break;
        }
        
        case SX_LAZY_SERVICE_STT: {
            char stt_endpoint[256] = {0};
            char stt_api_key[128] = {0};
            sx_settings_get_string_default("stt_endpoint", stt_endpoint, sizeof(stt_endpoint), NULL);
            sx_settings_get_string_default("stt_api_key", stt_api_key, sizeof(stt_api_key), NULL);
            
            sx_stt_config_t stt_cfg = {
                .endpoint_url = (stt_endpoint[0] != '\0') ? stt_endpoint : NULL,
                .api_key = (stt_api_key[0] != '\0') ? stt_api_key : NULL,
                .chunk_duration_ms = 1000,
                .sample_rate_hz = 16000, // Default
                .auto_send = true,
            };
            ret = sx_stt_service_init(&stt_cfg);
            break;
        }
        
        case SX_LAZY_SERVICE_WAKE_WORD: {
            char wake_word_model[256] = {0};
            float wake_word_threshold = 0.5f;
            sx_settings_get_string_default("wake_word_model", wake_word_model, sizeof(wake_word_model), NULL);
            
            int32_t threshold_int = 50;
            sx_settings_get_int_default("wake_word_threshold", &threshold_int, 50);
            wake_word_threshold = threshold_int / 100.0f;
            
            sx_wake_word_config_t wake_word_cfg = {
                .type = SX_WAKE_WORD_TYPE_ESP_SR,
                .model_path = (wake_word_model[0] != '\0') ? wake_word_model : NULL,
                .threshold = wake_word_threshold,
                .enable_opus_encoding = false,
            };
            ret = sx_wake_word_service_init(&wake_word_cfg);
            break;
        }
        
        case SX_LAZY_SERVICE_AFE: {
            sx_audio_afe_config_t afe_cfg = {
                .enable_aec = true,
                .enable_vad = true,
                .enable_ns = true,
                .enable_agc = true,
                .sample_rate_hz = 16000, // Default
            };
            ret = sx_audio_afe_init(&afe_cfg);
            break;
        }
        
        case SX_LAZY_SERVICE_TTS: {
            char tts_endpoint[256] = {0};
            char tts_api_key[128] = {0};
            sx_settings_get_string_default("tts_endpoint_url", tts_endpoint, sizeof(tts_endpoint), NULL);
            sx_settings_get_string_default("tts_api_key", tts_api_key, sizeof(tts_api_key), NULL);
            
            sx_tts_config_t tts_cfg = {
                .endpoint_url = (tts_endpoint[0] != '\0') ? tts_endpoint : NULL,
                .api_key = (tts_api_key[0] != '\0') ? tts_api_key : NULL,
                .timeout_ms = 10000,
                .default_priority = SX_TTS_PRIORITY_NORMAL,
            };
            ret = sx_tts_service_init(&tts_cfg);
            break;
        }
        
        case SX_LAZY_SERVICE_BLE_NAV: {
            ret = sx_nav_icon_cache_init();
            if (ret == ESP_OK) {
                ret = sx_navigation_ble_init();
                if (ret == ESP_OK) {
                    ret = sx_navigation_ble_start();
                }
            }
            break;
        }
        
        case SX_LAZY_SERVICE_CHATBOT: {
            sx_chatbot_config_t chatbot_cfg = {
                .endpoint = NULL,
                .publish_topic = NULL,
            };
            ret = sx_chatbot_service_init(&chatbot_cfg);
            if (ret == ESP_OK) {
                ret = sx_chatbot_service_start();
                if (ret == ESP_OK) {
                    sx_chatbot_enable_intent_parsing(true);
                }
            }
            break;
        }
        
        case SX_LAZY_SERVICE_RADIO: {
            sx_radio_config_t radio_cfg = {
                .auto_reconnect = true,
                .max_reconnect_attempts = 10,
            };
            ret = sx_radio_service_init(&radio_cfg);
            if (ret == ESP_OK) {
                ret = sx_radio_service_start();
            }
            break;
        }
        
        case SX_LAZY_SERVICE_MUSIC_ONLINE: {
            ret = sx_music_online_service_init();
            break;
        }
        
        case SX_LAZY_SERVICE_SD_CARD: {
            sx_sd_config_t sd_cfg = {
                .mount_point = SX_SD_MOUNT_POINT,
                .spi_host = SPI3_HOST,
                .miso_gpio = 12,
                .mosi_gpio = 47,
                .sclk_gpio = 21,
                .cs_gpio = 10,
            };
            ret = sx_sd_service_init(&sd_cfg);
            if (ret == ESP_OK) {
                ret = sx_sd_service_start();
            }
            break;
        }
        
        case SX_LAZY_SERVICE_IR: {
            sx_ir_config_t ir_cfg = {
                .tx_gpio = 14,
                .rx_gpio = -1,
                .carrier_hz = 38000,
            };
            ret = sx_ir_service_init(&ir_cfg);
            if (ret == ESP_OK) {
                ret = sx_ir_service_start();
            }
            break;
        }
        
        case SX_LAZY_SERVICE_BLUETOOTH: {
            sx_bluetooth_config_t bt_cfg = {
                .device_name = "SimpleXL",
                .auto_connect = false,
                .discoverable = true,
                .scan_timeout_ms = 10000,
            };
            ret = sx_bluetooth_service_init(&bt_cfg);
            break;
        }
        
        case SX_LAZY_SERVICE_WEATHER: {
            char weather_api_key[128] = {0};
            char weather_city[64] = {0};
            sx_settings_get_string_default("weather_api_key", weather_api_key, sizeof(weather_api_key), NULL);
            sx_settings_get_string_default("weather_city", weather_city, sizeof(weather_city), NULL);
            
            if (weather_api_key[0] == '\0') {
                strncpy(weather_api_key, "ae8d3c2fda691593ce3e84472ef25784", sizeof(weather_api_key) - 1);
            }
            
            sx_weather_config_t weather_cfg = {
                .api_key = (weather_api_key[0] != '\0') ? weather_api_key : NULL,
                .city = (weather_city[0] != '\0' && strcmp(weather_city, "auto") != 0) ? weather_city : NULL,
                .update_interval_ms = 30 * 60 * 1000,
            };
            ret = sx_weather_service_init(&weather_cfg);
            break;
        }
        
        case SX_LAZY_SERVICE_NAVIGATION: {
            ret = sx_navigation_service_init();
            break;
        }
        
        case SX_LAZY_SERVICE_DIAGNOSTICS: {
            ret = sx_diagnostics_service_init();
            break;
        }
        
        case SX_LAZY_SERVICE_INTENT: {
            ret = sx_intent_service_init();
            break;
        }
        
        case SX_LAZY_SERVICE_PROTOCOL_BRIDGE: {
            ret = sx_audio_protocol_bridge_init();
            if (ret == ESP_OK) {
                ret = sx_audio_protocol_bridge_start();
            }
            break;
        }
        
        case SX_LAZY_SERVICE_PLAYLIST: {
            ret = sx_playlist_manager_init();
            break;
        }
        
        case SX_LAZY_SERVICE_PROTOCOL_WS: {
            char ws_url[256] = {0};
            char ws_token[128] = {0};
            sx_settings_get_string_default("websocket_url", ws_url, sizeof(ws_url), NULL);
            sx_settings_get_string_default("websocket_token", ws_token, sizeof(ws_token), NULL);
            
            if (ws_url[0] != '\0') {
                sx_protocol_ws_config_t ws_cfg = {
                    .url = ws_url,
                    .auth_token = (ws_token[0] != '\0') ? ws_token : NULL,
                    .reconnect_ms = 5000,
                };
                ret = sx_protocol_ws_start(&ws_cfg);
                if (ret == ESP_OK) {
                    sx_chatbot_set_protocol_ws_available(true);
                }
            } else {
                ret = ESP_ERR_INVALID_ARG;
            }
            break;
        }
        
        case SX_LAZY_SERVICE_PROTOCOL_MQTT: {
            char mqtt_broker[256] = {0};
            char mqtt_client_id[128] = {0};
            char mqtt_username[128] = {0};
            char mqtt_password[128] = {0};
            char mqtt_subscribe_topic[128] = {0};
            
            sx_settings_get_string_default("mqtt_broker", mqtt_broker, sizeof(mqtt_broker), NULL);
            sx_settings_get_string_default("mqtt_client_id", mqtt_client_id, sizeof(mqtt_client_id), NULL);
            sx_settings_get_string_default("mqtt_username", mqtt_username, sizeof(mqtt_username), NULL);
            sx_settings_get_string_default("mqtt_password", mqtt_password, sizeof(mqtt_password), NULL);
            sx_settings_get_string_default("mqtt_subscribe_topic", mqtt_subscribe_topic, sizeof(mqtt_subscribe_topic), NULL);
            
            if (mqtt_broker[0] != '\0') {
                sx_protocol_mqtt_config_t mqtt_cfg = {
                    .broker_uri = mqtt_broker,
                    .client_id = (mqtt_client_id[0] != '\0') ? mqtt_client_id : NULL,
                    .username = (mqtt_username[0] != '\0') ? mqtt_username : NULL,
                    .password = (mqtt_password[0] != '\0') ? mqtt_password : NULL,
                    .topic_prefix = NULL,
                    .keepalive_sec = 60,
                    .clean_session = true,
                };
                ret = sx_protocol_mqtt_init(&mqtt_cfg);
                if (ret == ESP_OK) {
                    ret = sx_protocol_mqtt_start();
                    if (ret == ESP_OK && mqtt_subscribe_topic[0] != '\0') {
                        sx_protocol_mqtt_subscribe(mqtt_subscribe_topic, 1);
                    }
                    if (ret == ESP_OK) {
                        sx_chatbot_set_protocol_mqtt_available(true);
                    }
                }
            } else {
                ret = ESP_ERR_INVALID_ARG;
            }
            break;
        }
        
        default:
            ESP_LOGE(TAG, "Unhandled service type: %d", service);
            ret = ESP_ERR_NOT_SUPPORTED;
            break;
    }
    
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize service %d: %s", service, esp_err_to_name(ret));
        // Reset initialization state on failure
        lazy_loader_init_mutex();
        if (s_mutex != NULL && xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
            s_service_initialized[service] = false;
            xSemaphoreGive(s_mutex);
        }
    } else {
        ESP_LOGI(TAG, "Service %d initialized successfully", service);
    }
    
    return ret;
}

bool sx_lazy_service_is_initialized(sx_lazy_service_t service) {
    if (service >= SX_LAZY_SERVICE_MAX) {
        return false;
    }
    
    lazy_loader_init_mutex();
    if (s_mutex == NULL) {
        return false;
    }
    
    bool initialized = false;
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        initialized = s_service_initialized[service];
        xSemaphoreGive(s_mutex);
    }
    
    return initialized;
}

esp_err_t sx_lazy_service_deinit(sx_lazy_service_t service) {
    // Note: Deinitialization is optional and may not be implemented for all services
    // This is a placeholder for future implementation
    ESP_LOGW(TAG, "Service deinitialization not implemented for service %d", service);
    return ESP_ERR_NOT_SUPPORTED;
}

