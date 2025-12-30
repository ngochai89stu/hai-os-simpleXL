#include "sx_bootstrap.h"

#include <esp_log.h>
#include <nvs_flash.h>

#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_orchestrator.h"

#include "sx_platform.h"
#include "sx_spi_bus_manager.h"
#include "sx_ui.h"
#include "sx_assets.h"
#include "sx_audio_service.h"
#include "sx_sd_service.h"
#include "sx_radio_service.h"
#include "sx_ir_service.h"
#include "sx_chatbot_service.h"
#include "sx_wifi_service.h"
#include "sx_settings_service.h"
#include "sx_intent_service.h"
#include "sx_ota_service.h"
#include "sx_audio_ducking.h"
#include "sx_playlist_manager.h"
#include "sx_theme_service.h"
#include "sx_stt_service.h"
#include "sx_audio_afe.h"
#include "sx_wake_word_service.h"
#include "sx_led_service.h"
#include "sx_power_service.h"
#include "sx_state_manager.h"
#include "sx_audio_power.h"
#include "sx_diagnostics_service.h"
#include "sx_audio_router.h"
#include "sx_tts_service.h"
#include "sx_music_online_service.h"
#include "sx_navigation_service.h"
#include "sx_navigation_ble.h"
#include "sx_navigation_icon_cache.h"
#include "sx_telegram_service.h"
#include "sx_bluetooth_service.h"
#include "sx_weather_service.h"
#include "sx_protocol_ws.h"
#include "sx_protocol_mqtt.h"
#include "sx_audio_protocol_bridge.h"
#include "sx_lazy_loader.h"

static const char *TAG = "sx_bootstrap";

esp_err_t sx_bootstrap_start(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "BOOTSTRAP START - BEGIN");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "bootstrap start");

    // 1) Init NVS (required for WiFi, settings, etc.)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs erase (no free pages / new version)");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Init settings service (Phase 5)
    ret = sx_settings_service_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Settings service init failed (non-critical): %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Settings service initialized");
    }
    
    // Init theme service (Phase 5)
    ret = sx_theme_service_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Theme service init failed (non-critical): %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Theme service initialized");
    }
    
    // Init OTA service (Phase 5)
    ret = sx_ota_service_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "OTA service init failed (non-critical): %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "OTA service initialized");
    }

    // 2) Init dispatcher
    ESP_LOGI(TAG, "Initializing dispatcher...");
    if (!sx_dispatcher_init()) {
        ESP_LOGE(TAG, "dispatcher init failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Dispatcher initialized");

    // 3) Start orchestrator (single writer for state)
    ESP_LOGI(TAG, "Starting orchestrator...");
    sx_orchestrator_start();
    ESP_LOGI(TAG, "Orchestrator started");

    // 4) Init platform hardware (display)
    ESP_LOGI(TAG, "Initializing display platform...");
    sx_display_handles_t display_handles = sx_platform_display_init();
    ESP_LOGI(TAG, "Display platform init completed");
    if (display_handles.panel_handle == NULL) {
        ESP_LOGE(TAG, "Display platform init failed");
        return ESP_FAIL;
    }

    // 5) Phase 3: Init touch driver
    ESP_LOGE(TAG, "========================================");
    ESP_LOGE(TAG, "=== TOUCH INIT START (BOOTSTRAP) ===");
    ESP_LOGE(TAG, "========================================");
    ESP_LOGI(TAG, "=== Starting touch driver initialization ===");
    ESP_LOGI(TAG, "About to call sx_platform_touch_init...");
    sx_touch_handles_t touch_handles = {0};
    touch_handles.touch_handle = NULL;  // Explicit zero-init
    ESP_LOGI(TAG, "touch_handles initialized (zeroed), calling sx_platform_touch_init...");
    ESP_LOGI(TAG, "touch_handles address: %p", (void*)&touch_handles);
    
    esp_err_t touch_ret = sx_platform_touch_init(&touch_handles);
    
    ESP_LOGI(TAG, "Returned from sx_platform_touch_init");
    ESP_LOGI(TAG, "Touch init returned: %s (error code: 0x%x)", esp_err_to_name(touch_ret), touch_ret);
    ESP_LOGI(TAG, "Touch handle after init: %p", (void*)touch_handles.touch_handle);
    
    if (touch_ret != ESP_OK) {
        ESP_LOGW(TAG, "Touch init failed (non-critical): %s (error code: 0x%x)", esp_err_to_name(touch_ret), touch_ret);
        ESP_LOGW(TAG, "Touch handle is NULL - UI will skip touch registration");
        // Continue without touch - not critical for boot
    } else {
        if (touch_handles.touch_handle != NULL) {
            ESP_LOGI(TAG, "✓ Touch driver initialized successfully");
            ESP_LOGI(TAG, "Touch handle: %p", (void*)touch_handles.touch_handle);
        } else {
            ESP_LOGW(TAG, "⚠ Touch init returned OK but touch_handle is NULL!");
        }
    }
    ESP_LOGI(TAG, "=== Touch driver initialization complete ===");
    ESP_LOGE(TAG, "========================================");
    ESP_LOGE(TAG, "=== TOUCH INIT END (BOOTSTRAP) ===");
    ESP_LOGE(TAG, "========================================");

    // 5.5) Init SPI bus manager (required for SD service)
    ret = sx_spi_bus_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SPI bus manager init failed (non-critical): %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPI bus manager initialized");
    }

    // 6) Phase 4: Init SD service (mount SD card)
    sx_sd_config_t sd_cfg = {
        .mount_point = SX_SD_MOUNT_POINT,
        .spi_host = SPI3_HOST,
        .miso_gpio = 12,  // SD MISO (SDO)
        .mosi_gpio = 47,  // SD MOSI
        .sclk_gpio = 21,  // SD SCLK
        .cs_gpio = 10,    // SD CS
    };
    esp_err_t sd_ret = sx_sd_service_init(&sd_cfg);
    if (sd_ret != ESP_OK) {
        ESP_LOGW(TAG, "SD service init failed (non-critical): %s", esp_err_to_name(sd_ret));
    } else {
        sd_ret = sx_sd_service_start();
        if (sd_ret != ESP_OK) {
            ESP_LOGW(TAG, "SD mount failed (non-critical): %s", esp_err_to_name(sd_ret));
        } else {
            ESP_LOGI(TAG, "SD mounted");
            sx_assets_set_sd_ready(true);
        }
    }

    // 7) Phase 3/4: Init asset loader (does not mount, only prepares)
    esp_err_t assets_ret = sx_assets_init();
    if (assets_ret != ESP_OK) {
        ESP_LOGW(TAG, "Assets init failed (non-critical): %s", esp_err_to_name(assets_ret));
    } else {
        ESP_LOGI(TAG, "Asset loader initialized");
    }

    // 8) Start UI (owner task) with the display handle and optional touch
    ESP_ERROR_CHECK(sx_ui_start(&display_handles, &touch_handles));
    
    // Load and apply saved brightness setting
    int32_t saved_brightness = CONFIG_SX_DISPLAY_BRIGHTNESS_DEFAULT;
    if (sx_settings_get_int_default("display_brightness", &saved_brightness, CONFIG_SX_DISPLAY_BRIGHTNESS_DEFAULT) == ESP_OK) {
        if (saved_brightness < 0) saved_brightness = 0;
        if (saved_brightness > 100) saved_brightness = 100;
        esp_err_t brightness_ret = sx_platform_set_brightness((uint8_t)saved_brightness);
        if (brightness_ret == ESP_OK) {
            ESP_LOGI(TAG, "Restored brightness to %d%%", saved_brightness);
        } else {
            ESP_LOGW(TAG, "Failed to restore brightness: %s", esp_err_to_name(brightness_ret));
        }
    }

    // 9) Phase 4 & 5: Initialize and start services
    // NOTE: Most services are now lazy-loaded (see sx_lazy_loader.h)
    // Only essential services are initialized here
    
    // Phase 5: STT Service - MOVED TO LAZY LOADING
    // Will be initialized when user starts voice input
    /*
    char stt_endpoint[256] = {0};
    char stt_api_key[128] = {0};
    // Load from Kconfig first, then fallback to Settings
#ifdef CONFIG_SX_STT_ENDPOINT_URL
    if (strlen(CONFIG_SX_STT_ENDPOINT_URL) > 0) {
        strncpy(stt_endpoint, CONFIG_SX_STT_ENDPOINT_URL, sizeof(stt_endpoint) - 1);
    }
#endif
#ifdef CONFIG_SX_STT_API_KEY
    if (strlen(CONFIG_SX_STT_API_KEY) > 0) {
        strncpy(stt_api_key, CONFIG_SX_STT_API_KEY, sizeof(stt_api_key) - 1);
    }
#endif
    // Fallback to Settings if Kconfig values are empty
    if (stt_endpoint[0] == '\0') {
        sx_settings_get_string_default("stt_endpoint", stt_endpoint, sizeof(stt_endpoint), NULL);
    }
    if (stt_api_key[0] == '\0') {
        sx_settings_get_string_default("stt_api_key", stt_api_key, sizeof(stt_api_key), NULL);
    }
    
    sx_stt_config_t stt_cfg = {
        .endpoint_url = (stt_endpoint[0] != '\0') ? stt_endpoint : NULL,
        .api_key = (stt_api_key[0] != '\0') ? stt_api_key : NULL,
        .chunk_duration_ms = 1000,
        .sample_rate_hz = CONFIG_SX_AUDIO_SAMPLE_RATE,
        .auto_send = true,
    };
    esp_err_t stt_ret = sx_stt_service_init(&stt_cfg);
    if (stt_ret != ESP_OK) {
        ESP_LOGW(TAG, "STT service init failed (non-critical): %s", esp_err_to_name(stt_ret));
    } else {
        ESP_LOGI(TAG, "STT service initialized");
    }
    */
    
    // Phase 5: Audio Front-End (AFE) Service - MOVED TO LAZY LOADING
    // Will be initialized when STT or Wake Word is needed
    /*
    sx_audio_afe_config_t afe_cfg = {
        .enable_aec = true,
        .enable_vad = true,
        .enable_ns = true,
        .enable_agc = true,
        .sample_rate_hz = CONFIG_SX_AUDIO_SAMPLE_RATE,
    };
    esp_err_t afe_ret = sx_audio_afe_init(&afe_cfg);
    if (afe_ret != ESP_OK) {
        ESP_LOGW(TAG, "AFE service init failed (non-critical): %s", esp_err_to_name(afe_ret));
    } else {
        ESP_LOGI(TAG, "AFE service initialized");
    }
    */
    
    // Phase 5: Wake Word Service - MOVED TO LAZY LOADING
    // Will be initialized when user enables wake word
    /*
    char wake_word_model[256] = {0};
    float wake_word_threshold = 0.5f;
    
    // Load from Kconfig first
#ifdef CONFIG_SX_WAKE_WORD_MODEL_PATH
    if (strlen(CONFIG_SX_WAKE_WORD_MODEL_PATH) > 0) {
        strncpy(wake_word_model, CONFIG_SX_WAKE_WORD_MODEL_PATH, sizeof(wake_word_model) - 1);
    }
#endif
    // Fallback to Settings
    if (wake_word_model[0] == '\0') {
        sx_settings_get_string_default("wake_word_model", wake_word_model, sizeof(wake_word_model), NULL);
    }
    
    // Load threshold from Kconfig
#ifdef CONFIG_SX_WAKE_WORD_THRESHOLD
    int32_t threshold_int = CONFIG_SX_WAKE_WORD_THRESHOLD;
#else
    int32_t threshold_int = 50; // Default 0.5 * 100
#endif
    // Fallback to Settings
    if (sx_settings_get_int_default("wake_word_threshold", &threshold_int, threshold_int) == ESP_OK) {
        // Settings override Kconfig
    }
    wake_word_threshold = threshold_int / 100.0f;
    
    // Determine wake word type from Kconfig
    sx_wake_word_type_t wake_word_type = SX_WAKE_WORD_TYPE_ESP_SR; // Default
#ifdef CONFIG_SX_WAKE_WORD_DISABLED
    wake_word_type = SX_WAKE_WORD_TYPE_DISABLED;
#elif defined(CONFIG_SX_WAKE_WORD_ESP_SR)
    wake_word_type = SX_WAKE_WORD_TYPE_ESP_SR;
#elif defined(CONFIG_SX_WAKE_WORD_CUSTOM)
    wake_word_type = SX_WAKE_WORD_TYPE_CUSTOM;
#endif
    
    sx_wake_word_config_t wake_word_cfg = {
        .type = wake_word_type,
        .model_path = (wake_word_model[0] != '\0') ? wake_word_model : NULL,
        .threshold = wake_word_threshold,
        .enable_opus_encoding = false,
    };
    esp_err_t wake_word_ret = sx_wake_word_service_init(&wake_word_cfg);
    if (wake_word_ret != ESP_OK) {
        ESP_LOGW(TAG, "Wake word service init failed (non-critical): %s", esp_err_to_name(wake_word_ret));
    } else {
        ESP_LOGI(TAG, "Wake word service initialized");
    }
    */
    
    // Playlist Manager - MOVED TO LAZY LOADING
    // Will be initialized when user accesses playlist
    /*
    esp_err_t playlist_ret = sx_playlist_manager_init();
    if (playlist_ret != ESP_OK) {
        ESP_LOGW(TAG, "Playlist manager init failed (non-critical): %s", esp_err_to_name(playlist_ret));
    } else {
        ESP_LOGI(TAG, "Playlist manager initialized");
    }
    */
    
    // Audio Ducking, Power, Router - KEEP IN BOOTSTRAP (needed for audio service)
    // Audio Ducking (must be initialized before audio service)
    sx_audio_ducking_config_t ducking_cfg = {
        .duck_level = 0.3f,        // 30% volume when ducked
        .fade_duration_ms = 200,    // 200ms fade duration
    };
    esp_err_t ducking_ret = sx_audio_ducking_init(&ducking_cfg);
    if (ducking_ret != ESP_OK) {
        ESP_LOGW(TAG, "Audio ducking init failed (non-critical): %s", esp_err_to_name(ducking_ret));
    } else {
        ESP_LOGI(TAG, "Audio ducking manager initialized");
    }
    
    // Audio Power Management (must be before audio service)
    sx_audio_power_config_t power_cfg = {
        .timeout_ms = CONFIG_SX_AUDIO_POWER_SAVE_TIMEOUT_MS,
        .check_interval_ms = 1000,
        .enable_auto_power_save = CONFIG_SX_AUDIO_POWER_SAVE_ENABLE,
    };
    esp_err_t audio_power_ret = sx_audio_power_init(&power_cfg);
    if (audio_power_ret != ESP_OK) {
        ESP_LOGW(TAG, "Audio power management init failed (non-critical): %s", esp_err_to_name(audio_power_ret));
    } else {
        ESP_LOGI(TAG, "Audio power management initialized");
    }
    
    // Audio Router
    esp_err_t audio_router_ret = sx_audio_router_init();
    if (audio_router_ret != ESP_OK) {
        ESP_LOGW(TAG, "Audio router init failed (non-critical): %s", esp_err_to_name(audio_router_ret));
    } else {
        ESP_LOGI(TAG, "Audio router initialized");
    }
    
    // Audio
    esp_err_t audio_ret = sx_audio_service_init();
    if (audio_ret != ESP_OK) {
        ESP_LOGW(TAG, "Audio service init failed (non-critical): %s", esp_err_to_name(audio_ret));
    } else {
        audio_ret = sx_audio_service_start();
        if (audio_ret != ESP_OK) {
            ESP_LOGW(TAG, "Audio service start failed (non-critical): %s", esp_err_to_name(audio_ret));
        } else {
            ESP_LOGI(TAG, "Audio service started");
        }
    }

    // Radio (online streaming) - MOVED TO LAZY LOADING
    // Will be initialized when user accesses radio screen
    /*
    sx_radio_config_t radio_cfg = {
        .auto_reconnect = true,
        .max_reconnect_attempts = 10,
    };
    esp_err_t radio_ret = sx_radio_service_init(&radio_cfg);
    if (radio_ret != ESP_OK) {
        ESP_LOGW(TAG, "Radio service init failed (non-critical): %s", esp_err_to_name(radio_ret));
    } else {
        radio_ret = sx_radio_service_start();
        if (radio_ret != ESP_OK) {
            ESP_LOGW(TAG, "Radio service start failed (non-critical): %s", esp_err_to_name(radio_ret));
        } else {
            ESP_LOGI(TAG, "Radio service started");
        }
    }
    */

    // IR (stub) - MOVED TO LAZY LOADING
    // Will be initialized when user accesses IR control screen
    /*
    sx_ir_config_t ir_cfg = {
        .tx_gpio = 14,  // IR TX pin (confirmed)
        .rx_gpio = -1,  // IR RX không sử dụng - đã tắt GPIO 15
        .carrier_hz = 38000,
    };
    esp_err_t ir_ret = sx_ir_service_init(&ir_cfg);
    if (ir_ret != ESP_OK) {
        ESP_LOGW(TAG, "IR service init failed (non-critical): %s", esp_err_to_name(ir_ret));
    } else {
        ir_ret = sx_ir_service_start();
        if (ir_ret != ESP_OK) {
            ESP_LOGW(TAG, "IR service start failed (non-critical): %s", esp_err_to_name(ir_ret));
        } else {
            ESP_LOGI(TAG, "IR service started");
        }
    }

    // Intent Service (Phase 5)
    esp_err_t intent_ret = sx_intent_service_init();
    if (intent_ret != ESP_OK) {
        ESP_LOGW(TAG, "Intent service init failed (non-critical): %s", esp_err_to_name(intent_ret));
    } else {
        ESP_LOGI(TAG, "Intent service initialized");
    }
    */
    
    // Chatbot/MCP (Phase 5 - with protocol integration) - MOVED TO LAZY LOADING
    // Will be initialized when user accesses chat screen
    /*
    // Load protocol config from Settings
    char ws_url[256] = {0};
    char ws_token[128] = {0};
    char mqtt_broker[256] = {0};
    char mqtt_client_id[128] = {0};
    char mqtt_username[128] = {0};
    char mqtt_password[128] = {0};
    char mqtt_publish_topic[128] = {0};
    char mqtt_subscribe_topic[128] = {0};
    
    // Load WebSocket config
    sx_settings_get_string_default("websocket_url", ws_url, sizeof(ws_url), NULL);
    sx_settings_get_string_default("websocket_token", ws_token, sizeof(ws_token), NULL);
    
    // Load MQTT config
    sx_settings_get_string_default("mqtt_broker", mqtt_broker, sizeof(mqtt_broker), NULL);
    sx_settings_get_string_default("mqtt_client_id", mqtt_client_id, sizeof(mqtt_client_id), NULL);
    sx_settings_get_string_default("mqtt_username", mqtt_username, sizeof(mqtt_username), NULL);
    sx_settings_get_string_default("mqtt_password", mqtt_password, sizeof(mqtt_password), NULL);
    sx_settings_get_string_default("mqtt_publish_topic", mqtt_publish_topic, sizeof(mqtt_publish_topic), NULL);
    sx_settings_get_string_default("mqtt_subscribe_topic", mqtt_subscribe_topic, sizeof(mqtt_subscribe_topic), NULL);
    
    // Initialize protocol (WebSocket or MQTT)
    bool protocol_initialized = false;
    
    if (ws_url[0] != '\0') {
        // Initialize WebSocket protocol
        sx_protocol_ws_config_t ws_cfg = {
            .url = ws_url,
            .auth_token = (ws_token[0] != '\0') ? ws_token : NULL,
            .reconnect_ms = 5000,
        };
        esp_err_t ws_ret = sx_protocol_ws_start(&ws_cfg);
        if (ws_ret == ESP_OK) {
            ESP_LOGI(TAG, "WebSocket protocol started: %s", ws_url);
            sx_chatbot_set_protocol_ws_available(true);
            protocol_initialized = true;
        } else {
            ESP_LOGW(TAG, "WebSocket protocol start failed: %s", esp_err_to_name(ws_ret));
        }
    } else if (mqtt_broker[0] != '\0') {
        // Initialize MQTT protocol
        sx_protocol_mqtt_config_t mqtt_cfg = {
            .broker_uri = mqtt_broker,
            .client_id = (mqtt_client_id[0] != '\0') ? mqtt_client_id : NULL,
            .username = (mqtt_username[0] != '\0') ? mqtt_username : NULL,
            .password = (mqtt_password[0] != '\0') ? mqtt_password : NULL,
            .topic_prefix = NULL,
            .keepalive_sec = 60,
            .clean_session = true,
        };
        esp_err_t mqtt_init_ret = sx_protocol_mqtt_init(&mqtt_cfg);
        if (mqtt_init_ret == ESP_OK) {
            esp_err_t mqtt_start_ret = sx_protocol_mqtt_start();
            if (mqtt_start_ret == ESP_OK) {
                ESP_LOGI(TAG, "MQTT protocol started: %s", mqtt_broker);
                // Subscribe to topic if specified
                if (mqtt_subscribe_topic[0] != '\0') {
                    sx_protocol_mqtt_subscribe(mqtt_subscribe_topic, 1);
                }
                sx_chatbot_set_protocol_mqtt_available(true);
                protocol_initialized = true;
            } else {
                ESP_LOGW(TAG, "MQTT protocol start failed: %s", esp_err_to_name(mqtt_start_ret));
            }
        } else {
            ESP_LOGW(TAG, "MQTT protocol init failed: %s", esp_err_to_name(mqtt_init_ret));
        }
    }
    
    // Initialize Chatbot service
    sx_chatbot_config_t chatbot_cfg = {
        .endpoint = NULL,
        .publish_topic = (mqtt_publish_topic[0] != '\0') ? mqtt_publish_topic : NULL,
    };
    esp_err_t cb_ret = sx_chatbot_service_init(&chatbot_cfg);
    if (cb_ret != ESP_OK) {
        ESP_LOGW(TAG, "Chatbot init failed (non-critical): %s", esp_err_to_name(cb_ret));
    } else {
        cb_ret = sx_chatbot_service_start();
        if (cb_ret != ESP_OK) {
            ESP_LOGW(TAG, "Chatbot start failed (non-critical): %s", esp_err_to_name(cb_ret));
        } else {
            ESP_LOGI(TAG, "Chatbot service started (protocol: %s)", 
                     protocol_initialized ? (ws_url[0] != '\0' ? "WebSocket" : "MQTT") : "none");
            // Enable intent parsing in chatbot
            sx_chatbot_enable_intent_parsing(true);
        }
    }
    */
    
    // Initialize Audio Protocol Bridge (for audio streaming) - MOVED TO LAZY LOADING
    // Will be initialized when protocol is initialized
    /*
    esp_err_t bridge_ret = sx_audio_protocol_bridge_init();
    if (bridge_ret != ESP_OK) {
        ESP_LOGW(TAG, "Audio protocol bridge init failed (non-critical): %s", esp_err_to_name(bridge_ret));
    } else {
        bridge_ret = sx_audio_protocol_bridge_start();
        if (bridge_ret != ESP_OK) {
            ESP_LOGW(TAG, "Audio protocol bridge start failed (non-critical): %s", esp_err_to_name(bridge_ret));
        } else {
            ESP_LOGI(TAG, "Audio protocol bridge started");
            // Note: Audio send/receive will be enabled when protocol connects
            // This is handled in orchestrator
        }
    }
    */

    // WiFi (Phase 5) - MOVED TO LAZY LOADING
    // Will be initialized when user accesses WiFi settings or needs network
    /*
    sx_wifi_config_t wifi_cfg = {
        .auto_reconnect = CONFIG_SX_NETWORK_AUTO_RECONNECT,
        .reconnect_interval_ms = CONFIG_SX_NETWORK_RECONNECT_INTERVAL_MS,
    };
    esp_err_t wifi_ret = sx_wifi_service_init(&wifi_cfg);
    if (wifi_ret != ESP_OK) {
        ESP_LOGW(TAG, "WiFi service init failed (non-critical): %s", esp_err_to_name(wifi_ret));
    } else {
        wifi_ret = sx_wifi_service_start();
        if (wifi_ret != ESP_OK) {
            ESP_LOGW(TAG, "WiFi service start failed (non-critical): %s", esp_err_to_name(wifi_ret));
        } else {
            ESP_LOGI(TAG, "WiFi service started");
        }
    }
    */
    
    // Music Online Service - MOVED TO LAZY LOADING
    // Will be initialized when user accesses music screen
    /*
    esp_err_t music_online_ret = sx_music_online_service_init();
    if (music_online_ret != ESP_OK) {
        ESP_LOGW(TAG, "Music online service init failed (non-critical): %s", esp_err_to_name(music_online_ret));
    } else {
        ESP_LOGI(TAG, "Music online service initialized");
        // Configure authentication from Kconfig
        char music_auth_mac[64] = {0};
        char music_auth_secret[128] = {0};
#ifdef CONFIG_SX_MUSIC_ONLINE_AUTH_MAC
        if (strlen(CONFIG_SX_MUSIC_ONLINE_AUTH_MAC) > 0) {
            strncpy(music_auth_mac, CONFIG_SX_MUSIC_ONLINE_AUTH_MAC, sizeof(music_auth_mac) - 1);
        }
#endif
#ifdef CONFIG_SX_MUSIC_ONLINE_AUTH_SECRET
        if (strlen(CONFIG_SX_MUSIC_ONLINE_AUTH_SECRET) > 0) {
            strncpy(music_auth_secret, CONFIG_SX_MUSIC_ONLINE_AUTH_SECRET, sizeof(music_auth_secret) - 1);
        }
#endif
        // Fallback to Settings
        if (music_auth_mac[0] == '\0') {
            sx_settings_get_string_default("music_online_auth_mac", music_auth_mac, sizeof(music_auth_mac), NULL);
        }
        if (music_auth_secret[0] == '\0') {
            sx_settings_get_string_default("music_online_auth_secret", music_auth_secret, sizeof(music_auth_secret), NULL);
        }
        
        if (music_auth_mac[0] != '\0' || music_auth_secret[0] != '\0') {
            // Note: sx_music_online_set_auth_config uses auth_key, not mac/secret
            // For now, use secret as auth_key if available
            if (music_auth_secret[0] != '\0') {
                sx_music_online_auth_config_t auth_cfg = {
                    .enable_auth = true,
                    .auth_key = music_auth_secret,
                };
                sx_music_online_set_auth_config(&auth_cfg);
            }
        }
    }
    */
    
    // TTS Service - MOVED TO LAZY LOADING
    // Will be initialized on first TTS call
    /*
    char tts_endpoint[256] = {0};
    char tts_api_key[128] = {0};
    // Load from Kconfig first
#ifdef CONFIG_SX_TTS_ENDPOINT_URL
    if (strlen(CONFIG_SX_TTS_ENDPOINT_URL) > 0) {
        strncpy(tts_endpoint, CONFIG_SX_TTS_ENDPOINT_URL, sizeof(tts_endpoint) - 1);
    }
#endif
#ifdef CONFIG_SX_TTS_API_KEY
    if (strlen(CONFIG_SX_TTS_API_KEY) > 0) {
        strncpy(tts_api_key, CONFIG_SX_TTS_API_KEY, sizeof(tts_api_key) - 1);
    }
#endif
    // Fallback to Settings
    if (tts_endpoint[0] == '\0') {
        sx_settings_get_string_default("tts_endpoint_url", tts_endpoint, sizeof(tts_endpoint), NULL);
    }
    if (tts_api_key[0] == '\0') {
        sx_settings_get_string_default("tts_api_key", tts_api_key, sizeof(tts_api_key), NULL);
    }
    
    sx_tts_config_t tts_cfg = {
        .endpoint_url = (tts_endpoint[0] != '\0') ? tts_endpoint : NULL,
        .api_key = (tts_api_key[0] != '\0') ? tts_api_key : NULL,
        .timeout_ms = 10000,
        .default_priority = SX_TTS_PRIORITY_NORMAL,
    };
    esp_err_t tts_ret = sx_tts_service_init(&tts_cfg);
    if (tts_ret != ESP_OK) {
        ESP_LOGW(TAG, "TTS service init failed (non-critical): %s", esp_err_to_name(tts_ret));
    } else {
        ESP_LOGI(TAG, "TTS service initialized");
    }
    */
    
    // Navigation Service - MOVED TO LAZY LOADING
    // Will be initialized when user accesses navigation screen
    /*
    esp_err_t nav_ret = sx_navigation_service_init();
    if (nav_ret != ESP_OK) {
        ESP_LOGW(TAG, "Navigation service init failed (non-critical): %s", esp_err_to_name(nav_ret));
    } else {
        ESP_LOGI(TAG, "Navigation service initialized");
    }
    */
    
    // BLE Navigation Service - MOVED TO LAZY LOADING
    // Will be initialized when user accesses navigation screen
    /*
    esp_err_t icon_cache_ret = sx_nav_icon_cache_init();
    if (icon_cache_ret != ESP_OK) {
        ESP_LOGW(TAG, "Icon cache init failed (non-critical): %s", esp_err_to_name(icon_cache_ret));
    }
    esp_err_t nav_ble_ret = sx_navigation_ble_init();
    if (nav_ble_ret != ESP_OK) {
        ESP_LOGW(TAG, "BLE Navigation service init failed (non-critical): %s", esp_err_to_name(nav_ble_ret));
    } else {
        nav_ble_ret = sx_navigation_ble_start();
        if (nav_ble_ret != ESP_OK) {
            ESP_LOGW(TAG, "BLE Navigation service start failed (non-critical): %s", esp_err_to_name(nav_ble_ret));
        } else {
            ESP_LOGI(TAG, "BLE Navigation service started");
        }
    }
    */
    
    // Telegram Service - DISABLED
    // Commented out to save memory and reduce boot time
    /*
    char telegram_bot_token[256] = {0};
    char telegram_chat_id[128] = {0};
    // Load from Kconfig first
#ifdef CONFIG_SX_TELEGRAM_BOT_TOKEN
    if (strlen(CONFIG_SX_TELEGRAM_BOT_TOKEN) > 0) {
        strncpy(telegram_bot_token, CONFIG_SX_TELEGRAM_BOT_TOKEN, sizeof(telegram_bot_token) - 1);
    }
#endif
#ifdef CONFIG_SX_TELEGRAM_CHAT_ID
    if (strlen(CONFIG_SX_TELEGRAM_CHAT_ID) > 0) {
        strncpy(telegram_chat_id, CONFIG_SX_TELEGRAM_CHAT_ID, sizeof(telegram_chat_id) - 1);
    }
#endif
    // Fallback to Settings
    if (telegram_bot_token[0] == '\0') {
        sx_settings_get_string_default("telegram_bot_token", telegram_bot_token, sizeof(telegram_bot_token), NULL);
    }
    if (telegram_chat_id[0] == '\0') {
        sx_settings_get_string_default("telegram_chat_id", telegram_chat_id, sizeof(telegram_chat_id), NULL);
    }
    
    sx_telegram_config_t telegram_cfg = {
        .bot_token = (telegram_bot_token[0] != '\0') ? telegram_bot_token : NULL,
        .chat_id = (telegram_chat_id[0] != '\0') ? telegram_chat_id : NULL,
        .timeout_ms = 10000,
    };
    esp_err_t telegram_ret = sx_telegram_service_init(&telegram_cfg);
    if (telegram_ret != ESP_OK) {
        ESP_LOGW(TAG, "Telegram service init failed (non-critical): %s", esp_err_to_name(telegram_ret));
    } else {
        ESP_LOGI(TAG, "Telegram service initialized");
    }
    */
    ESP_LOGI(TAG, "Telegram service disabled");
    
    // Bluetooth Service - MOVED TO LAZY LOADING
    // Will be initialized when user accesses Bluetooth settings
    /*
    sx_bluetooth_config_t bt_cfg = {
        .device_name = "SimpleXL",
        .auto_connect = false,
        .discoverable = true,
        .scan_timeout_ms = 10000,
    };
    esp_err_t bt_ret = sx_bluetooth_service_init(&bt_cfg);
    if (bt_ret != ESP_OK) {
        ESP_LOGW(TAG, "Bluetooth service init failed (non-critical): %s", esp_err_to_name(bt_ret));
    } else {
        ESP_LOGI(TAG, "Bluetooth service initialized");
    }
    */
    
    // Diagnostics Service - MOVED TO LAZY LOADING
    // Will be initialized when user accesses diagnostics screen
    /*
    esp_err_t diag_ret = sx_diagnostics_service_init();
    if (diag_ret != ESP_OK) {
        ESP_LOGW(TAG, "Diagnostics service init failed (non-critical): %s", esp_err_to_name(diag_ret));
    } else {
        ESP_LOGI(TAG, "Diagnostics service initialized");
    }
    */
    
    // Weather Service (Phase 5) - MOVED TO LAZY LOADING
    // Will be initialized when user accesses weather screen
    /*
    // Load configuration from Settings
    char weather_api_key[128] = {0};
    char weather_city[64] = {0};
    
    // Load from Settings
    sx_settings_get_string_default("weather_api_key", weather_api_key, sizeof(weather_api_key), NULL);
    sx_settings_get_string_default("weather_city", weather_city, sizeof(weather_city), NULL);
    
    // Use default API key if not set
    if (weather_api_key[0] == '\0') {
        // Default demo key (can be replaced)
        strncpy(weather_api_key, "ae8d3c2fda691593ce3e84472ef25784", sizeof(weather_api_key) - 1);
    }
    
    sx_weather_config_t weather_cfg = {
        .api_key = (weather_api_key[0] != '\0') ? weather_api_key : NULL,
        .city = (weather_city[0] != '\0' && strcmp(weather_city, "auto") != 0) ? weather_city : NULL,
        .update_interval_ms = 30 * 60 * 1000,  // 30 minutes
    };
    esp_err_t weather_ret = sx_weather_service_init(&weather_cfg);
    if (weather_ret != ESP_OK) {
        ESP_LOGW(TAG, "Weather service init failed (non-critical): %s", esp_err_to_name(weather_ret));
    } else {
        ESP_LOGI(TAG, "Weather service initialized");
        // Trigger initial fetch if WiFi is connected
        if (sx_wifi_is_connected()) {
            sx_weather_fetch();  // Async fetch
        }
    }
    */

    ESP_LOGI(TAG, "bootstrap done - core services enabled");
    return ESP_OK;
}
