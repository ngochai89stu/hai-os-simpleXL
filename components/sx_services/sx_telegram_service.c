#include "sx_telegram_service.h"
#include "sx_settings_service.h"
#include "sx_wifi_service.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "esp_http_client.h"
#include "cJSON.h"
#include "mbedtls/base64.h"

static const char *TAG = "sx_telegram";

// Telegram API base URL
#define TELEGRAM_API_BASE_URL "https://api.telegram.org/bot"

// Telegram service state
static bool s_initialized = false;
static sx_telegram_config_t s_config = {0};
static char s_bot_token[128] = {0};
static char s_chat_id[64] = {0};
static sx_telegram_message_cb_t s_message_cb = NULL;
static void *s_message_cb_user_data = NULL;

// Forward declarations
static esp_err_t sx_telegram_http_request(const char *method, const char *params_json,
                                         const uint8_t *file_data, size_t file_size,
                                         const char *file_field, const char *file_name);

esp_err_t sx_telegram_service_init(const sx_telegram_config_t *config) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    // Load configuration from settings
    if (sx_settings_get_string_default("telegram_bot_token", s_bot_token,
                                       sizeof(s_bot_token), NULL) != ESP_OK) {
        s_bot_token[0] = '\0';
    }
    
    if (sx_settings_get_string_default("telegram_chat_id", s_chat_id,
                                       sizeof(s_chat_id), NULL) != ESP_OK) {
        s_chat_id[0] = '\0';
    }
    
    // Apply provided config
    if (config) {
        s_config = *config;
        if (config->bot_token) {
            strncpy(s_bot_token, config->bot_token, sizeof(s_bot_token) - 1);
            s_bot_token[sizeof(s_bot_token) - 1] = '\0';
        }
        if (config->chat_id) {
            strncpy(s_chat_id, config->chat_id, sizeof(s_chat_id) - 1);
            s_chat_id[sizeof(s_chat_id) - 1] = '\0';
        }
    } else {
        // Default config
        s_config.bot_token = s_bot_token;
        s_config.chat_id = s_chat_id;
        s_config.timeout_ms = 10000;
    }
    
    if (s_bot_token[0] == '\0') {
        ESP_LOGW(TAG, "Telegram bot token not configured");
    }
    
    if (s_chat_id[0] == '\0') {
        ESP_LOGW(TAG, "Telegram chat ID not configured");
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "Telegram service initialized");
    return ESP_OK;
}

void sx_telegram_service_deinit(void) {
    if (!s_initialized) {
        return;
    }
    
    s_message_cb = NULL;
    s_message_cb_user_data = NULL;
    s_initialized = false;
    
    ESP_LOGI(TAG, "Telegram service deinitialized");
}

esp_err_t sx_telegram_send_message(const char *message) {
    if (!s_initialized || !message) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_bot_token[0] == '\0' || s_chat_id[0] == '\0') {
        ESP_LOGE(TAG, "Telegram bot token or chat ID not configured");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!sx_wifi_is_connected()) {
        ESP_LOGE(TAG, "WiFi not connected, cannot send Telegram message");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Build JSON parameters
    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "chat_id", s_chat_id);
    cJSON_AddStringToObject(params, "text", message);
    
    char *params_json = cJSON_Print(params);
    cJSON_Delete(params);
    
    if (!params_json) {
        return ESP_ERR_NO_MEM;
    }
    
    // Send HTTP request
    esp_err_t ret = sx_telegram_http_request("sendMessage", params_json, NULL, 0, NULL, NULL);
    
    free(params_json);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Telegram message sent: %s", message);
    } else {
        ESP_LOGE(TAG, "Failed to send Telegram message: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t sx_telegram_send_photo(const uint8_t *photo_data, size_t photo_size, const char *caption) {
    if (!s_initialized || !photo_data || photo_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (s_bot_token[0] == '\0' || s_chat_id[0] == '\0') {
        ESP_LOGE(TAG, "Telegram bot token or chat ID not configured");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!sx_wifi_is_connected()) {
        ESP_LOGE(TAG, "WiFi not connected, cannot send Telegram photo");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Build JSON parameters
    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "chat_id", s_chat_id);
    if (caption) {
        cJSON_AddStringToObject(params, "caption", caption);
    }
    
    char *params_json = cJSON_Print(params);
    cJSON_Delete(params);
    
    if (!params_json) {
        return ESP_ERR_NO_MEM;
    }
    
    // Send HTTP request with photo
    esp_err_t ret = sx_telegram_http_request("sendPhoto", params_json, photo_data, photo_size, "photo", "photo.jpg");
    
    free(params_json);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Telegram photo sent (%zu bytes)", photo_size);
    } else {
        ESP_LOGE(TAG, "Failed to send Telegram photo: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t sx_telegram_send_audio(const uint8_t *audio_data, size_t audio_size, const char *caption) {
    if (!s_initialized || !audio_data || audio_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (s_bot_token[0] == '\0' || s_chat_id[0] == '\0') {
        ESP_LOGE(TAG, "Telegram bot token or chat ID not configured");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!sx_wifi_is_connected()) {
        ESP_LOGE(TAG, "WiFi not connected, cannot send Telegram audio");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Build JSON parameters
    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "chat_id", s_chat_id);
    if (caption) {
        cJSON_AddStringToObject(params, "caption", caption);
    }
    
    char *params_json = cJSON_Print(params);
    cJSON_Delete(params);
    
    if (!params_json) {
        return ESP_ERR_NO_MEM;
    }
    
    // Send HTTP request with audio
    esp_err_t ret = sx_telegram_http_request("sendAudio", params_json, audio_data, audio_size, "audio", "audio.ogg");
    
    free(params_json);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Telegram audio sent (%zu bytes)", audio_size);
    } else {
        ESP_LOGE(TAG, "Failed to send Telegram audio: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

void sx_telegram_set_message_callback(sx_telegram_message_cb_t callback, void *user_data) {
    s_message_cb = callback;
    s_message_cb_user_data = user_data;
}

bool sx_telegram_is_initialized(void) {
    return s_initialized;
}

static esp_err_t sx_telegram_http_request(const char *method, const char *params_json,
                                         const uint8_t *file_data, size_t file_size,
                                         const char *file_field, const char *file_name) {
    // Build URL
    char url[512];
    snprintf(url, sizeof(url), "%s%s/%s", TELEGRAM_API_BASE_URL, s_bot_token, method);
    
    // Create HTTP client
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = s_config.timeout_ms,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return ESP_ERR_NO_MEM;
    }
    
    // Set method
    if (file_data && file_size > 0) {
        // Multipart form data for file upload
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_header(client, "Content-Type", "multipart/form-data");
        
        // Build multipart form data
        // Note: This is simplified - full implementation would need proper multipart encoding
        ESP_LOGW(TAG, "File upload not fully implemented - requires multipart form encoding");
        esp_http_client_cleanup(client);
        return ESP_ERR_NOT_SUPPORTED;
    } else {
        // JSON POST request
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_header(client, "Content-Type", "application/json");
        
        // Set POST data
        esp_http_client_set_post_field(client, params_json, strlen(params_json));
    }
    
    // Perform request
    esp_err_t ret = esp_http_client_perform(client);
    if (ret != ESP_OK) {
        esp_http_client_cleanup(client);
        return ret;
    }
    
    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        ESP_LOGE(TAG, "Telegram API returned status code: %d", status_code);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    
    // Read response (optional - for error checking)
    char response_buffer[256];
    int data_read = esp_http_client_read_response(client, response_buffer, sizeof(response_buffer) - 1);
    if (data_read > 0) {
        response_buffer[data_read] = '\0';
        ESP_LOGD(TAG, "Telegram API response: %s", response_buffer);
    }
    
    esp_http_client_cleanup(client);
    
    return ESP_OK;
}











