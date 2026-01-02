#include "event_handlers.h"

#include <esp_log.h>
#include <string.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sx_ota_full_service.h"
#include "sx_audio_service.h"
#include "sx_protocol_ws.h"
#include "sx_protocol_mqtt.h"
#include "sx_settings_service.h"
#include "sx_chatbot_service.h"

static const char *TAG = "sx_evt_ota";

// Helper: Play digit sound (0-9)
// SimpleXL: assume OGG files are in assets partition or SD card
// Path format: "/sdcard/sounds/digit_X.ogg" or "/spiffs/sounds/digit_X.ogg"
// Wait until audio is idle (or timeout) to avoid overlapping sounds.
static void wait_audio_idle_ms(uint32_t timeout_ms) {
    const uint32_t step_ms = 20;
    uint32_t waited = 0;
    while (sx_audio_is_playing() && waited < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(step_ms));
        waited += step_ms;
    }
}

static esp_err_t play_digit_sound(char digit) {
    if (!isdigit((unsigned char)digit)) return ESP_ERR_INVALID_ARG;

    char path[64];
    // Try SD card first, then SPIFFS/assets
    snprintf(path, sizeof(path), "/sdcard/sounds/digit_%c.ogg", digit);
    esp_err_t ret = sx_audio_play_file(path);
    if (ret != ESP_OK) {
        // Fallback to SPIFFS/assets
        snprintf(path, sizeof(path), "/spiffs/sounds/digit_%c.ogg", digit);
        ret = sx_audio_play_file(path);
    }
    return ret;
}

static esp_err_t play_activation_sound(void) {
    esp_err_t ret = sx_audio_play_file("/sdcard/sounds/activation.ogg");
    if (ret != ESP_OK) {
        ret = sx_audio_play_file("/spiffs/sounds/activation.ogg");
    }
    return ret;
}

bool sx_event_handler_wifi_connected(const sx_event_t *evt, sx_state_t *state) {
    (void)evt;
    (void)state;
    ESP_LOGI(TAG, "WiFi connected -> trigger OTA check");
    sx_ota_full_check_version();
    return false; // state not updated here
}

static void activation_sound_task(void *arg) {
    const char *code = (const char *)arg;
    if (!code) {
        vTaskDelete(NULL);
        return;
    }

    // Wait if something is currently playing
    wait_audio_idle_ms(3000);

    // Play activation prompt
    if (play_activation_sound() == ESP_OK) {
        wait_audio_idle_ms(8000);
    }

    // Digits
    for (const char *p = code; *p != '\0'; p++) {
        if (!isdigit((unsigned char)*p)) continue;
        if (play_digit_sound(*p) == ESP_OK) {
            wait_audio_idle_ms(3000);
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

    free((void*)code);
    vTaskDelete(NULL);
}

bool sx_event_handler_activation_required(const sx_event_t *evt, sx_state_t *state) {
    const char *code = (const char *)evt->ptr;
    if (!code || !state) return false;

    // Update UI state via event-bus state
    state->ui.has_alert = true;
    strncpy(state->ui.alert_status, "Activation", sizeof(state->ui.alert_status) - 1);
    snprintf(state->ui.alert_message, sizeof(state->ui.alert_message), "Code: %s", code);
    strncpy(state->ui.alert_emotion, "link", sizeof(state->ui.alert_emotion) - 1);

    // Offload audio playback to a task to avoid blocking the event handler loop.
    char *code_copy = (char *)malloc(strlen(code) + 1);
    if (code_copy) {
        strcpy(code_copy, code);
        xTaskCreate(activation_sound_task, "sx_act_snd", 4096, code_copy, tskIDLE_PRIORITY + 1, NULL);
    }

    return true;
}

// Helper: Start protocol (WebSocket or MQTT) based on settings from OTA response
static void start_protocol_from_settings(void) {
    char buf[256];
    
    // Check for WebSocket config first
    if (sx_settings_get_string_default("websocket_url", buf, sizeof(buf), NULL) == ESP_OK && buf[0] != '\0') {
        sx_protocol_ws_config_t ws_cfg = {0};
        ws_cfg.url = buf;
        ws_cfg.reconnect_ms = 5000;
        ws_cfg.protocol_version = 2;
        
        // Get token if available
        char token_buf[128];
        if (sx_settings_get_string_default("websocket_token", token_buf, sizeof(token_buf), NULL) == ESP_OK && token_buf[0] != '\0') {
            ws_cfg.auth_token = token_buf;
        }
        
        esp_err_t ret = sx_protocol_ws_start(&ws_cfg);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "WebSocket protocol started after activation");
            sx_chatbot_set_protocol_ws_available(true);
            return; // Success, don't try MQTT
        } else {
            ESP_LOGW(TAG, "WebSocket start failed: %s", esp_err_to_name(ret));
        }
    }
    
    // Fallback to MQTT if WebSocket not available
    if (sx_settings_get_string_default("mqtt_endpoint", buf, sizeof(buf), NULL) == ESP_OK && buf[0] != '\0') {
        char mqtt_client_id[128] = {0};
        char mqtt_username[128] = {0};
        char mqtt_password[256] = {0};
        char mqtt_subscribe_topic[128] = {0};
        char mqtt_publish_topic[128] = {0};
        
        sx_settings_get_string_default("mqtt_client_id", mqtt_client_id, sizeof(mqtt_client_id), NULL);
        sx_settings_get_string_default("mqtt_username", mqtt_username, sizeof(mqtt_username), NULL);
        sx_settings_get_string_default("mqtt_password", mqtt_password, sizeof(mqtt_password), NULL);
        sx_settings_get_string_default("mqtt_subscribe_topic", mqtt_subscribe_topic, sizeof(mqtt_subscribe_topic), NULL);
        sx_settings_get_string_default("mqtt_publish_topic", mqtt_publish_topic, sizeof(mqtt_publish_topic), NULL);
        
        sx_protocol_mqtt_config_t mqtt_cfg = {
            .broker_uri = buf,
            .client_id = (mqtt_client_id[0] != '\0') ? mqtt_client_id : NULL,
            .username = (mqtt_username[0] != '\0') ? mqtt_username : NULL,
            .password = (mqtt_password[0] != '\0') ? mqtt_password : NULL,
            .topic_prefix = NULL,
            .keepalive_sec = 60,
            .clean_session = true,
        };
        
        esp_err_t init_ret = sx_protocol_mqtt_init(&mqtt_cfg);
        if (init_ret == ESP_OK) {
            esp_err_t ret = sx_protocol_mqtt_start();
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "MQTT protocol started after activation");
                if (mqtt_subscribe_topic[0] != '\0') {
                    sx_protocol_mqtt_subscribe(mqtt_subscribe_topic, 1);
                }
                sx_chatbot_set_protocol_mqtt_available(true);
            } else {
                ESP_LOGW(TAG, "MQTT start failed: %s", esp_err_to_name(ret));
            }
        } else {
            ESP_LOGW(TAG, "MQTT init failed: %s", esp_err_to_name(init_ret));
        }
    }
}

bool sx_event_handler_activation_pending(const sx_event_t *evt, sx_state_t *state) {
    if (!state || !evt) return false;

    // Show "pending" state while polling
    state->ui.has_alert = true;
    strncpy(state->ui.alert_status, "Activation", sizeof(state->ui.alert_status) - 1);
    snprintf(state->ui.alert_message, sizeof(state->ui.alert_message),
             "Dang cho xac nhan... (lan %u, cho %ums)",
             (unsigned)evt->arg0, (unsigned)evt->arg1);
    strncpy(state->ui.alert_emotion, "hourglass", sizeof(state->ui.alert_emotion) - 1);
    return true;
}

bool sx_event_handler_activation_timeout(const sx_event_t *evt, sx_state_t *state) {
    (void)evt;
    if (!state) return false;

    state->ui.has_alert = true;
    strncpy(state->ui.alert_status, "Activation", sizeof(state->ui.alert_status) - 1);
    strncpy(state->ui.alert_message, "Activation timeout", sizeof(state->ui.alert_message) - 1);
    strncpy(state->ui.alert_emotion, "circle_xmark", sizeof(state->ui.alert_emotion) - 1);
    return true;
}

bool sx_event_handler_activation_done(const sx_event_t *evt, sx_state_t *state) {
    (void)evt;
    if (!state) return false;

    state->ui.has_alert = true;
    strncpy(state->ui.alert_status, "Activation", sizeof(state->ui.alert_status) - 1);
    strncpy(state->ui.alert_message, "Activation successful", sizeof(state->ui.alert_message) - 1);
    strncpy(state->ui.alert_emotion, "check", sizeof(state->ui.alert_emotion) - 1);

    // Start protocol after activation (like reference repo)
    start_protocol_from_settings();

    return true;
}

bool sx_event_handler_ota_finished(const sx_event_t *evt, sx_state_t *state) {
    (void)evt;
    (void)state;
    
    ESP_LOGI(TAG, "OTA check finished (no activation required)");
    
    // If no activation was required, start protocol immediately
    // (activation_done handler will start protocol if activation was required)
    if (!sx_ota_full_has_activation()) {
        start_protocol_from_settings();
    }
    
    return false; // state not updated
}

bool sx_event_handler_ota_error(const sx_event_t *evt, sx_state_t *state) {
    const char *msg = (const char *)evt->ptr;
    if (!msg || !state) return false;

    state->ui.has_alert = true;
    strncpy(state->ui.alert_status, "OTA Error", sizeof(state->ui.alert_status) - 1);
    strncpy(state->ui.alert_message, msg, sizeof(state->ui.alert_message) - 1);
    strncpy(state->ui.alert_emotion, "circle_xmark", sizeof(state->ui.alert_emotion) - 1);
    return true;
}
