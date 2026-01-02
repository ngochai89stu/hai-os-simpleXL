#include "sx_tts_service.h"
#include "sx_settings_service.h"
#include "sx_wifi_service.h"
#include "sx_audio_service.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_http_client.h"
#include "cJSON.h"

static const char *TAG = "sx_tts";

// TTS request structure
typedef struct {
    char text[512];
    sx_tts_priority_t priority;
    sx_tts_audio_callback_t audio_callback;
    sx_tts_error_callback_t error_callback;
    void *user_data;
} sx_tts_request_t;

// TTS service state
static bool s_initialized = false;
static sx_tts_config_t s_config = {0};
static char s_endpoint_url[256] = {0};
static char s_api_key[128] = {0};
static TaskHandle_t s_tts_task_handle = NULL;
static QueueHandle_t s_tts_queue = NULL;
static SemaphoreHandle_t s_tts_mutex = NULL;
static bool s_speaking = false;
static sx_tts_request_t s_current_request = {0};

// Forward declarations
static void sx_tts_task(void *arg);
static esp_err_t sx_tts_synthesize(const sx_tts_request_t *request);
static esp_err_t sx_tts_http_request(const char *text, int16_t **audio_data, size_t *audio_size, uint32_t *sample_rate);

esp_err_t sx_tts_service_init(const sx_tts_config_t *config) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    // Load configuration from settings
    if (sx_settings_get_string_default("tts_endpoint_url", s_endpoint_url,
                                       sizeof(s_endpoint_url), NULL) != ESP_OK) {
        s_endpoint_url[0] = '\0';
    }
    
    if (sx_settings_get_string_default("tts_api_key", s_api_key,
                                       sizeof(s_api_key), NULL) != ESP_OK) {
        s_api_key[0] = '\0';
    }
    
    // Apply provided config or use defaults
    if (config) {
        s_config = *config;
        if (config->endpoint_url) {
            strncpy(s_endpoint_url, config->endpoint_url, sizeof(s_endpoint_url) - 1);
            s_endpoint_url[sizeof(s_endpoint_url) - 1] = '\0';
        }
        if (config->api_key) {
            strncpy(s_api_key, config->api_key, sizeof(s_api_key) - 1);
            s_api_key[sizeof(s_api_key) - 1] = '\0';
        }
    } else {
        // Default config
        s_config.timeout_ms = 10000;
        s_config.default_priority = SX_TTS_PRIORITY_NORMAL;
    }
    
    // Create mutex
    s_tts_mutex = xSemaphoreCreateMutex();
    if (!s_tts_mutex) {
        return ESP_ERR_NO_MEM;
    }
    
    // Create request queue (priority queue - higher priority first)
    s_tts_queue = xQueueCreate(10, sizeof(sx_tts_request_t));
    if (!s_tts_queue) {
        vSemaphoreDelete(s_tts_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Create TTS task
    BaseType_t ret = xTaskCreatePinnedToCore(sx_tts_task, "sx_tts", 4096, NULL, 5,
                                             &s_tts_task_handle, 1);
    if (ret != pdPASS) {
        vQueueDelete(s_tts_queue);
        vSemaphoreDelete(s_tts_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "TTS service initialized (endpoint: %s)", s_endpoint_url);
    return ESP_OK;
}

void sx_tts_service_deinit(void) {
    if (!s_initialized) {
        return;
    }
    
    // Cancel current request
    sx_tts_cancel();
    
    // Delete task
    if (s_tts_task_handle) {
        vTaskDelete(s_tts_task_handle);
        s_tts_task_handle = NULL;
    }
    
    // Delete queue
    if (s_tts_queue) {
        vQueueDelete(s_tts_queue);
        s_tts_queue = NULL;
    }
    
    // Delete mutex
    if (s_tts_mutex) {
        vSemaphoreDelete(s_tts_mutex);
        s_tts_mutex = NULL;
    }
    
    s_initialized = false;
    ESP_LOGI(TAG, "TTS service deinitialized");
}

esp_err_t sx_tts_speak(const char *text, sx_tts_priority_t priority,
                       sx_tts_audio_callback_t audio_callback,
                       sx_tts_error_callback_t error_callback,
                       void *user_data) {
    if (!s_initialized || !text) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (strlen(text) == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    sx_tts_request_t request = {0};
    strncpy(request.text, text, sizeof(request.text) - 1);
    request.text[sizeof(request.text) - 1] = '\0';
    request.priority = priority;
    request.audio_callback = audio_callback;
    request.error_callback = error_callback;
    request.user_data = user_data;
    
    // Add to queue
    if (xQueueSend(s_tts_queue, &request, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to queue TTS request (queue full)");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Queued TTS request: \"%s\" (priority: %d)", text, priority);
    return ESP_OK;
}

esp_err_t sx_tts_speak_simple(const char *text) {
    return sx_tts_speak(text, s_config.default_priority, NULL, NULL, NULL);
}

esp_err_t sx_tts_cancel(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_tts_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    s_speaking = false;
    memset(&s_current_request, 0, sizeof(s_current_request));
    
    xSemaphoreGive(s_tts_mutex);
    
    // Clear queue
    sx_tts_request_t dummy;
    while (xQueueReceive(s_tts_queue, &dummy, 0) == pdTRUE) {
        // Discard queued requests
    }
    
    ESP_LOGI(TAG, "TTS cancelled");
    return ESP_OK;
}

bool sx_tts_is_speaking(void) {
    if (!s_initialized) {
        return false;
    }
    
    bool speaking = false;
    if (xSemaphoreTake(s_tts_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        speaking = s_speaking;
        xSemaphoreGive(s_tts_mutex);
    }
    
    return speaking;
}

esp_err_t sx_tts_set_config(const sx_tts_config_t *config) {
    if (!s_initialized || !config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_tts_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    s_config = *config;
    if (config->endpoint_url) {
        strncpy(s_endpoint_url, config->endpoint_url, sizeof(s_endpoint_url) - 1);
        s_endpoint_url[sizeof(s_endpoint_url) - 1] = '\0';
    }
    if (config->api_key) {
        strncpy(s_api_key, config->api_key, sizeof(s_api_key) - 1);
        s_api_key[sizeof(s_api_key) - 1] = '\0';
    }
    
    xSemaphoreGive(s_tts_mutex);
    
    return ESP_OK;
}

esp_err_t sx_tts_get_config(sx_tts_config_t *config) {
    if (!s_initialized || !config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_tts_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    *config = s_config;
    config->endpoint_url = s_endpoint_url;
    config->api_key = s_api_key;
    
    xSemaphoreGive(s_tts_mutex);
    
    return ESP_OK;
}

static void sx_tts_task(void *arg) {
    (void)arg;
    
    sx_tts_request_t request;
    
    while (1) {
        // Wait for request
        if (xQueueReceive(s_tts_queue, &request, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        
        // Mark as speaking
        if (xSemaphoreTake(s_tts_mutex, portMAX_DELAY) == pdTRUE) {
            s_speaking = true;
            s_current_request = request;
            xSemaphoreGive(s_tts_mutex);
        }
        
        // Synthesize
        esp_err_t ret = sx_tts_synthesize(&request);
        
        // Mark as not speaking
        if (xSemaphoreTake(s_tts_mutex, portMAX_DELAY) == pdTRUE) {
            s_speaking = false;
            memset(&s_current_request, 0, sizeof(s_current_request));
            xSemaphoreGive(s_tts_mutex);
        }
        
        if (ret != ESP_OK && request.error_callback) {
            request.error_callback(ret, esp_err_to_name(ret), request.user_data);
        }
    }
}

static esp_err_t sx_tts_synthesize(const sx_tts_request_t *request) {
    if (!sx_wifi_is_connected()) {
        ESP_LOGE(TAG, "WiFi not connected, cannot synthesize TTS");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_endpoint_url[0] == '\0') {
        ESP_LOGE(TAG, "TTS endpoint URL not configured");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Request audio from TTS server
    int16_t *audio_data = NULL;
    size_t audio_size = 0;
    uint32_t sample_rate = 16000;
    
    esp_err_t ret = sx_tts_http_request(request->text, &audio_data, &audio_size, &sample_rate);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TTS HTTP request failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Call audio callback or use default playback
    if (request->audio_callback) {
        request->audio_callback(audio_data, audio_size, sample_rate, request->user_data);
    } else {
        // Default: play through audio service
        // Note: This is a simplified implementation - in practice, you'd want to
        // feed audio in chunks to avoid blocking
        extern esp_err_t sx_audio_service_feed_pcm(const int16_t *pcm, size_t samples, uint32_t sample_rate);
        size_t samples = audio_size / sizeof(int16_t);
        sx_audio_service_feed_pcm(audio_data, samples, sample_rate);
    }
    
    // Free audio data
    if (audio_data) {
        free(audio_data);
    }
    
    return ESP_OK;
}

static esp_err_t sx_tts_http_request(const char *text, int16_t **audio_data, size_t *audio_size, uint32_t *sample_rate) {
    // Build request URL
    char url[512];
    snprintf(url, sizeof(url), "%s?text=%s", s_endpoint_url, text);
    // Note: In production, URL encoding would be needed
    
    // Create HTTP client
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = s_config.timeout_ms,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return ESP_ERR_NO_MEM;
    }
    
    // Add API key header if available
    if (s_api_key[0] != '\0') {
        esp_http_client_set_header(client, "X-API-Key", s_api_key);
    }
    
    // Set content type
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    
    // Perform request
    esp_err_t ret = esp_http_client_perform(client);
    if (ret != ESP_OK) {
        esp_http_client_cleanup(client);
        return ret;
    }
    
    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        ESP_LOGE(TAG, "TTS API returned status code: %d", status_code);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    
    // Read response
    int content_length = esp_http_client_get_content_length(client);
    if (content_length <= 0) {
        content_length = 8192; // Default buffer size
    }
    
    uint8_t *response_buffer = (uint8_t *)malloc(content_length);
    if (!response_buffer) {
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }
    
    int data_read = esp_http_client_read_response(client, (char *)response_buffer, content_length);
    esp_http_client_cleanup(client);
    
    if (data_read <= 0) {
        free(response_buffer);
        return ESP_FAIL;
    }
    
    // Parse response (assuming WAV or raw PCM format)
    // This is a simplified implementation - actual TTS API may return different formats
    // For now, assume raw PCM 16-bit mono
    
    *audio_size = data_read;
    *audio_data = (int16_t *)malloc(*audio_size);
    if (!*audio_data) {
        free(response_buffer);
        return ESP_ERR_NO_MEM;
    }
    
    memcpy(*audio_data, response_buffer, *audio_size);
    *sample_rate = 16000; // Default, should be detected from response
    
    free(response_buffer);
    
    return ESP_OK;
}



















