// ESP-SR AFE Integration (C++ wrapper)
// This file provides C++ implementation for ESP-SR AFE integration
// Architecture: This is a service implementation, follows SimpleXL architecture:
// - No UI dependencies
// - Communication via callbacks (not events, as this is internal service)
// - C API wrapper for C code integration

// Common includes for both branches
#include <stdint.h>
#include <stddef.h>
#include <esp_err.h>
#include "sx_audio_afe.h"

#if defined(CONFIG_SX_AUDIO_AFE_ESP_SR_ENABLE) || defined(CONFIG_SX_AUDIO_AFE_ESP_SR)

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ESP-SR includes
#include "esp_afe_sr_models.h"
#include "esp_afe_sr_iface.h"

static const char *TAG = "sx_audio_afe_sr";

// AFE handle
static esp_afe_sr_data_t *s_afe_handle = NULL;
static const esp_afe_sr_iface_t *s_afe_iface = NULL;
static srmodel_list_t *s_models = NULL;
static bool s_afe_initialized = false;
static sx_audio_afe_config_t s_afe_config = {
    .enable_aec = false,
    .enable_vad = false,
    .enable_ns = false,
    .enable_agc = false,
    .sample_rate_hz = 16000
};
static sx_afe_vad_cb_t s_vad_callback = NULL;
static void *s_vad_user_data = NULL;

extern "C" {

esp_err_t sx_audio_afe_init_esp_sr(const sx_audio_afe_config_t *config) {
    if (s_afe_initialized) {
        return ESP_OK;
    }
    
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    s_afe_config = *config;
    
    // Initialize ESP-SR AFE
    // Note: This requires ESP-SR models to be available
    // Model path should be configured in settings or passed via config
    // Default path: "model" (relative to partition or filesystem)
    
    // Get model list (default path: "model")
    // Try partition label first, then fallback to "model" path
    const char *model_path = "model";
#ifdef CONFIG_MODEL_IN_FLASH
    // If model is in flash partition, use partition label
    model_path = "model";
#endif
    s_models = esp_srmodel_init(model_path);
    if (s_models == NULL || s_models->num == -1) {
        ESP_LOGE(TAG, "Failed to initialize ESP-SR models (check model files in 'model' directory or partition)");
        ESP_LOGE(TAG, "Model partition may not exist in partition table. Add 'model' partition or disable ESP-SR AFE.");
        return ESP_FAIL;
    }
    
    // Build input format string (M = microphone, R = reference for AEC)
    const char *input_format = config->enable_aec ? "MR" : "M";
    
    // Create AFE config
    afe_config_t *afe_config = afe_config_init(input_format, s_models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    if (afe_config == NULL) {
        ESP_LOGE(TAG, "Failed to create AFE config");
        esp_srmodel_deinit(s_models);
        s_models = NULL;
        return ESP_FAIL;
    }
    
    afe_config->aec_init = config->enable_aec;
    afe_config->aec_mode = AEC_MODE_SR_HIGH_PERF;
    afe_config->se_init = config->enable_vad;  // VAD (Speech Enhancement)
    afe_config->ns_init = config->enable_ns;
    afe_config->agc_init = config->enable_agc;
    afe_config->afe_perferred_core = 1;
    afe_config->afe_perferred_priority = 1;
    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    
    // Create AFE interface
    const esp_afe_sr_iface_t *afe_iface = esp_afe_handle_from_config(afe_config);
    if (afe_iface == NULL) {
        ESP_LOGE(TAG, "Failed to create AFE interface");
        afe_config_free(afe_config);
        esp_srmodel_deinit(s_models);
        s_models = NULL;
        return ESP_FAIL;
    }
    
    // Create AFE data handle
    s_afe_handle = afe_iface->create_from_config(afe_config);
    if (s_afe_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create AFE data handle");
        afe_config_free(afe_config);
        esp_srmodel_deinit(s_models);
        s_models = NULL;
        return ESP_FAIL;
    }
    
    // Store interface pointer (needed for feed/fetch)
    s_afe_iface = afe_iface;
    
    s_afe_initialized = true;
    ESP_LOGI(TAG, "ESP-SR AFE initialized: AEC=%d, VAD=%d, NS=%d, AGC=%d, %lu Hz",
             config->enable_aec, config->enable_vad,
             config->enable_ns, config->enable_agc,
             (unsigned long)config->sample_rate_hz);
    
    return ESP_OK;
}

bool sx_audio_afe_process_esp_sr(const int16_t *input, int16_t *output, size_t sample_count) {
    if (!s_afe_initialized || s_afe_handle == NULL || input == NULL || output == NULL) {
        return false;
    }
    
    // Feed audio to AFE
    // Note: ESP-SR AFE expects audio in specific format and frame size
    // Frame size should match get_feed_chunksize()
    if (s_afe_iface == NULL || s_afe_handle == NULL) {
        return false;
    }
    
    // Feed audio data to AFE
    s_afe_iface->feed(s_afe_handle, (int16_t *)input);
    
    // Fetch processed audio
    afe_fetch_result_t *afe_fetch = s_afe_iface->fetch(s_afe_handle);
    
    if (afe_fetch != NULL) {
        // Check if voice activity detected
        // ret_value: -1 means channel verified (WAKENET_CHANNEL_VERIFIED)
        // vad_state indicates voice activity
        // Fixed: Cast enum to int to avoid enum comparison warning
        bool voice_active = ((int)afe_fetch->vad_state == (int)AFE_VAD_SPEECH);
        
        // Copy processed audio if available
        if (afe_fetch->data != NULL && afe_fetch->data_size > 0) {
            size_t copy_size = (afe_fetch->data_size < sample_count * sizeof(int16_t)) ?
                              afe_fetch->data_size : sample_count * sizeof(int16_t);
            memcpy(output, afe_fetch->data, copy_size);
            
            // Fill remaining with zeros if needed
            if (copy_size < sample_count * sizeof(int16_t)) {
                memset((uint8_t *)output + copy_size, 0, 
                       sample_count * sizeof(int16_t) - copy_size);
            }
        } else {
            // No processed data, copy input
            memcpy(output, input, sample_count * sizeof(int16_t));
        }
        
        // Call VAD callback
        if (s_vad_callback != NULL) {
            s_vad_callback(voice_active, s_vad_user_data);
        }
        
        return voice_active;
    }
    
    // No fetch result, fallback
    memcpy(output, input, sample_count * sizeof(int16_t));
    if (s_vad_callback != NULL) {
        s_vad_callback(false, s_vad_user_data);
    }
    return false;
}

esp_err_t sx_audio_afe_reset_esp_sr(void) {
    if (!s_afe_initialized || s_afe_handle == NULL || s_afe_iface == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Reset VAD state using ESP-SR API
    int ret = s_afe_iface->reset_vad(s_afe_handle);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to reset ESP-SR AFE VAD");
        return ESP_FAIL;
    }
    
    // Reset buffer
    ret = s_afe_iface->reset_buffer(s_afe_handle);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to reset ESP-SR AFE buffer");
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "ESP-SR AFE reset");
    return ESP_OK;
}

esp_err_t sx_audio_afe_deinit_esp_sr(void) {
    if (!s_afe_initialized) {
        return ESP_OK;
    }
    
    if (s_afe_handle != NULL && s_afe_iface != NULL) {
        s_afe_iface->destroy(s_afe_handle);
        s_afe_handle = NULL;
        s_afe_iface = NULL;
    }
    
    if (s_models != NULL) {
        esp_srmodel_deinit(s_models);
        s_models = NULL;
    }
    
    s_afe_initialized = false;
    ESP_LOGI(TAG, "ESP-SR AFE deinitialized");
    return ESP_OK;
}

} // extern "C"

#else // AFE ESP-SR not enabled

// Stub implementations when ESP-SR is not enabled
// Ensure headers are available for stub functions
#include <stdint.h>
#include <stddef.h>
#include <esp_err.h>
#include "sx_audio_afe.h"

extern "C" {

esp_err_t sx_audio_afe_init_esp_sr(const sx_audio_afe_config_t *config) {
    (void)config;
    return ESP_ERR_NOT_SUPPORTED;
}

bool sx_audio_afe_process_esp_sr(const int16_t *input, int16_t *output, size_t sample_count) {
    (void)input;
    (void)output;
    (void)sample_count;
    return false;
}

esp_err_t sx_audio_afe_reset_esp_sr(void) {
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t sx_audio_afe_deinit_esp_sr(void) {
    return ESP_ERR_NOT_SUPPORTED;
}

} // extern "C"

#endif // AFE ESP-SR enable

