// ESP-SR Wake Word Integration (C++ wrapper)
// This file provides C++ implementation for ESP-SR wake word detection

// Common includes for both branches
#include <stdint.h>
#include <stddef.h>
#include <esp_err.h>
#include "sx_wake_word_service.h"

#if defined(CONFIG_SX_WAKE_WORD_ESP_SR_ENABLE) || defined(CONFIG_SX_WAKE_WORD_ESP_SR)

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ESP-SR includes
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_models.h"

static const char *TAG = "sx_wake_word_sr";

// Wake word handle
static const esp_wn_iface_t *s_wakenet = NULL;
static model_iface_data_t *s_model_data = NULL;
static bool s_wakenet_initialized = false;
static sx_wake_word_config_t s_wake_config = {
    .type = SX_WAKE_WORD_TYPE_ESP_SR,
    .model_path = NULL,
    .threshold = 0.5f,
    .enable_opus_encoding = false
};
static sx_wake_word_detected_cb_t s_detected_callback = NULL;
static void *s_user_data = NULL;

extern "C" {

esp_err_t sx_wake_word_init_esp_sr(const sx_wake_word_config_t *config) {
    if (s_wakenet_initialized) {
        return ESP_OK;
    }
    
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    s_wake_config = *config;
    
    // Initialize ESP-SR wake word model
    // Default model path: "model" (relative to partition or filesystem)
    const char *model_path = config->model_path;
    if (model_path == NULL || model_path[0] == '\0') {
        model_path = "model"; // Default path
    }
    
    // Get wake word model interface using ESP-SR API
    s_wakenet = esp_wn_handle_from_name(model_path);
    if (s_wakenet == NULL) {
        ESP_LOGE(TAG, "Failed to get wake word model interface for: %s", model_path);
        return ESP_FAIL;
    }
    
    // Create model data with DET_MODE_90 (normal mode)
    s_model_data = s_wakenet->create(model_path, DET_MODE_90);
    if (s_model_data == NULL) {
        ESP_LOGE(TAG, "Failed to create wake word model data");
        return ESP_FAIL;
    }
    
    s_wakenet_initialized = true;
    ESP_LOGI(TAG, "ESP-SR wake word initialized: model=%s, threshold=%.2f",
             model_path, config->threshold);
    
    return ESP_OK;
}

esp_err_t sx_wake_word_feed_audio_esp_sr(const int16_t *pcm, size_t sample_count) {
    if (!s_wakenet_initialized || s_wakenet == NULL || s_model_data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (pcm == NULL || sample_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Feed audio to wake word model
    // ESP-SR wake word expects audio in specific format (16kHz, 16-bit, mono)
    int ret = s_wakenet->detect(s_model_data, (int16_t *)pcm);
    
    if (ret > 0) {
        // Wake word detected!
        float confidence = (float)ret / 100.0f; // Convert to 0.0-1.0 range
        
        // Check threshold
        if (confidence >= s_wake_config.threshold) {
            ESP_LOGI(TAG, "Wake word detected! Confidence: %.2f (threshold: %.2f)",
                     confidence, s_wake_config.threshold);
            
            // Call callback
            if (s_detected_callback != NULL) {
                s_detected_callback(s_user_data);
            }
        } else {
            ESP_LOGD(TAG, "Wake word detected but below threshold: %.2f < %.2f",
                     confidence, s_wake_config.threshold);
        }
    }
    
    return ESP_OK;
}

esp_err_t sx_wake_word_register_callback_esp_sr(sx_wake_word_detected_cb_t callback, void *user_data) {
    if (!s_wakenet_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    s_detected_callback = callback;
    s_user_data = user_data;
    ESP_LOGI(TAG, "ESP-SR wake word callback %s", callback ? "registered" : "unregistered");
    return ESP_OK;
}

esp_err_t sx_wake_word_reset_esp_sr(void) {
    if (!s_wakenet_initialized || s_model_data == NULL || s_wakenet == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Clean wake word model state (ESP-SR uses clean instead of reset)
    s_wakenet->clean(s_model_data);
    ESP_LOGD(TAG, "ESP-SR wake word reset");
    return ESP_OK;
}

esp_err_t sx_wake_word_deinit_esp_sr(void) {
    if (!s_wakenet_initialized) {
        return ESP_OK;
    }
    
    if (s_model_data != NULL && s_wakenet != NULL) {
        s_wakenet->destroy(s_model_data);
        s_model_data = NULL;
    }
    
    s_wakenet = NULL;
    s_wakenet_initialized = false;
    s_detected_callback = NULL;
    s_user_data = NULL;
    
    ESP_LOGI(TAG, "ESP-SR wake word deinitialized");
    return ESP_OK;
}

} // extern "C"

#else // Wake word ESP-SR not enabled

// Stub implementations when ESP-SR is not enabled
// Ensure headers are available for stub functions
#include <stdint.h>
#include <stddef.h>
#include <esp_err.h>
#include "sx_wake_word_service.h"

extern "C" {

esp_err_t sx_wake_word_init_esp_sr(const sx_wake_word_config_t *config) {
    (void)config;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t sx_wake_word_feed_audio_esp_sr(const int16_t *pcm, size_t sample_count) {
    (void)pcm;
    (void)sample_count;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t sx_wake_word_register_callback_esp_sr(sx_wake_word_detected_cb_t callback, void *user_data) {
    (void)callback;
    (void)user_data;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t sx_wake_word_reset_esp_sr(void) {
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t sx_wake_word_deinit_esp_sr(void) {
    return ESP_ERR_NOT_SUPPORTED;
}

} // extern "C"

#endif // Wake word ESP-SR enable


