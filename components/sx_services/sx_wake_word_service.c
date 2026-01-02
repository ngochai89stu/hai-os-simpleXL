#include "sx_wake_word_service.h"
#include "sx_audio_afe.h"
#include "sx_settings_service.h"
#include "sx_audio_service.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for ESP-SR wrapper (if enabled)
// Lưu ý: Kconfig hiện dùng CONFIG_SX_WAKE_WORD_ESP_SR_ENABLE (theo sdkconfig)
#if defined(CONFIG_SX_WAKE_WORD_ESP_SR_ENABLE) || defined(CONFIG_SX_WAKE_WORD_ESP_SR)
extern esp_err_t sx_wake_word_init_esp_sr(const sx_wake_word_config_t *config);
extern esp_err_t sx_wake_word_feed_audio_esp_sr(const int16_t *pcm, size_t sample_count);
extern esp_err_t sx_wake_word_register_callback_esp_sr(sx_wake_word_detected_cb_t callback, void *user_data);
extern esp_err_t sx_wake_word_reset_esp_sr(void);
extern esp_err_t sx_wake_word_deinit_esp_sr(void);
#endif

static const char *TAG = "sx_wake_word";

// Wake word service state
static bool s_initialized = false;
static bool s_active = false;
static sx_wake_word_config_t s_config = {0};
static sx_wake_word_detected_cb_t s_detected_callback = NULL;
static void *s_user_data = NULL;
static TaskHandle_t s_wake_word_task_handle = NULL;
static QueueHandle_t s_audio_queue = NULL;

// Static buffers for settings-loaded configuration
static char s_settings_model_path[256] = {0};

// Note: Wake word detection requires ESP-SR library
// For ESP32, we can use:
// - ESP-SR wake word engine
// - Custom wake word models

static void sx_wake_word_task(void *arg) {
    (void)arg;
    
    ESP_LOGI(TAG, "Wake word detection task started");
    
    const size_t AUDIO_BUFFER_SAMPLES = 320; // 20ms at 16kHz
    int16_t *audio_buffer = (int16_t *)malloc(AUDIO_BUFFER_SAMPLES * sizeof(int16_t));
    if (audio_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        s_active = false;
        vTaskDelete(NULL);
        return;
    }
    
    while (s_active) {
        // Get audio from queue (fed by recording service or AFE)
        if (s_audio_queue != NULL) {
            if (xQueueReceive(s_audio_queue, audio_buffer, pdMS_TO_TICKS(100)) == pdTRUE) {
                // Process audio through wake word engine
#if defined(CONFIG_SX_WAKE_WORD_ESP_SR_ENABLE) || defined(CONFIG_SX_WAKE_WORD_ESP_SR)
                sx_wake_word_feed_audio_esp_sr(audio_buffer, AUDIO_BUFFER_SAMPLES);
#else
                ESP_LOGW(TAG, "Wake word processing - ESP-SR not enabled");
#endif
            }
        } else {
            // No queue, wait a bit
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    
    free(audio_buffer);
    ESP_LOGI(TAG, "Wake word detection task ended");
    vTaskDelete(NULL);
}

esp_err_t sx_wake_word_service_init(const sx_wake_word_config_t *config) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    // Initialize with default values
    memset(&s_config, 0, sizeof(s_config));
    memset(s_settings_model_path, 0, sizeof(s_settings_model_path));
    
    if (config != NULL) {
        // Use provided configuration
        s_config = *config;
    } else {
        // Load from settings
        s_config.type = SX_WAKE_WORD_TYPE_ESP_SR;
        
        // Load model path
        if (sx_settings_get_string_default("wake_word_model", s_settings_model_path,
                                           sizeof(s_settings_model_path), NULL) == ESP_OK &&
            s_settings_model_path[0] != '\0') {
            s_config.model_path = s_settings_model_path;
        }
        
        // Load threshold (stored as int 0-100, convert to float 0.0-1.0)
        int32_t threshold_int = 50; // Default 0.5
        if (sx_settings_get_int_default("wake_word_threshold", &threshold_int, 50) == ESP_OK) {
            s_config.threshold = threshold_int / 100.0f;
        } else {
            s_config.threshold = 0.5f;
        }
        
        s_config.enable_opus_encoding = false;
    }
    
    // Validate threshold
    if (s_config.threshold < 0.0f) s_config.threshold = 0.0f;
    if (s_config.threshold > 1.0f) s_config.threshold = 1.0f;
    
    // Initialize wake word engine
#if defined(CONFIG_SX_WAKE_WORD_ESP_SR_ENABLE) || defined(CONFIG_SX_WAKE_WORD_ESP_SR)
    if (s_config.type == SX_WAKE_WORD_TYPE_ESP_SR) {
        esp_err_t ret = sx_wake_word_init_esp_sr(&s_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize ESP-SR wake word: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "ESP-SR wake word initialized");
    } else {
        ESP_LOGW(TAG, "Wake word type %d not yet supported", s_config.type);
    }
#else
    ESP_LOGW(TAG, "Wake word service init - ESP-SR library not enabled (CONFIG_SX_WAKE_WORD_ESP_SR_ENABLE)");
#endif
    
    // Create audio queue for wake word detection
    const size_t AUDIO_BUFFER_SAMPLES = 320; // 20ms at 16kHz
    s_audio_queue = xQueueCreate(10, AUDIO_BUFFER_SAMPLES * sizeof(int16_t));
    if (s_audio_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create audio queue");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Wake word config: type=%d, threshold=%.2f, model=%s",
             s_config.type, s_config.threshold,
             s_config.model_path ? s_config.model_path : "(none)");
    
    s_initialized = true;
    return ESP_OK;
}

esp_err_t sx_wake_word_start(sx_wake_word_detected_cb_t callback, void *user_data) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_active) {
        return ESP_ERR_INVALID_STATE; // Already active
    }
    
    s_detected_callback = callback;
    s_user_data = user_data;
    
#if defined(CONFIG_SX_WAKE_WORD_ESP_SR_ENABLE) || defined(CONFIG_SX_WAKE_WORD_ESP_SR)
    // Register callback with ESP-SR wrapper
    esp_err_t ret = sx_wake_word_register_callback_esp_sr(callback, user_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register wake word callback: %s", esp_err_to_name(ret));
        return ret;
    }
#endif
    
    s_active = true;
    
    // Create wake word detection task
    BaseType_t task_ret = xTaskCreate(sx_wake_word_task, "sx_wake_word", 4096, NULL, 5, &s_wake_word_task_handle);
    if (task_ret != pdPASS) {
        s_active = false;
        s_detected_callback = NULL;
        s_user_data = NULL;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Wake word detection started");
    return ESP_OK;
}

esp_err_t sx_wake_word_stop(void) {
    if (!s_initialized || !s_active) {
        return ESP_ERR_INVALID_STATE;
    }
    
    s_active = false;
    
    // Wait for task to finish
    if (s_wake_word_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (s_wake_word_task_handle != NULL) {
            vTaskDelete(s_wake_word_task_handle);
            s_wake_word_task_handle = NULL;
        }
    }
    
    s_detected_callback = NULL;
    s_user_data = NULL;
    
    ESP_LOGI(TAG, "Wake word detection stopped");
    return ESP_OK;
}

bool sx_wake_word_is_active(void) {
    return s_active;
}

esp_err_t sx_wake_word_feed_audio(const int16_t *pcm, size_t sample_count) {
    if (!s_initialized || !s_active || pcm == NULL || sample_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Feed audio to queue (wake word task will process it)
    if (s_audio_queue != NULL) {
        // Send audio buffer to queue (non-blocking)
        if (xQueueSend(s_audio_queue, pcm, 0) != pdTRUE) {
            // Queue full, drop oldest and add new
            int16_t dummy[320];
            xQueueReceive(s_audio_queue, dummy, 0);
            xQueueSend(s_audio_queue, pcm, 0);
        }
    }
    
#if defined(CONFIG_SX_WAKE_WORD_ESP_SR_ENABLE) || defined(CONFIG_SX_WAKE_WORD_ESP_SR)
    // Also feed directly to ESP-SR (if queue is not used)
    // This allows immediate processing without queue delay
    sx_wake_word_feed_audio_esp_sr(pcm, sample_count);
#endif
    
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif

