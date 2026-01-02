#include "sx_audio_afe.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for ESP-SR wrapper (if enabled)
#if defined(CONFIG_SX_AUDIO_AFE_ESP_SR_ENABLE) || defined(CONFIG_SX_AUDIO_AFE_ESP_SR)
extern esp_err_t sx_audio_afe_init_esp_sr(const sx_audio_afe_config_t *config);
extern bool sx_audio_afe_process_esp_sr(const int16_t *input, int16_t *output, size_t sample_count);
extern esp_err_t sx_audio_afe_reset_esp_sr(void);
extern esp_err_t sx_audio_afe_deinit_esp_sr(void);
#endif

static const char *TAG = "sx_audio_afe";

// AFE service state
static bool s_initialized = false;
static sx_audio_afe_config_t s_config = {0};
static sx_afe_vad_cb_t s_vad_callback = NULL;
static void *s_vad_user_data = NULL;

// Note: AFE requires ESP-SR (ESP Speech Recognition) library
// For ESP32, we can use:
// - ESP-SR AFE component (AEC, VAD, NS, AGC)
// - External AFE library integration

esp_err_t sx_audio_afe_init(const sx_audio_afe_config_t *config) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Copy configuration
    s_config = *config;
    
    // Validate sample rate
    if (s_config.sample_rate_hz == 0) {
        s_config.sample_rate_hz = 16000; // Default
    }
    
#if defined(CONFIG_SX_AUDIO_AFE_ESP_SR_ENABLE) || defined(CONFIG_SX_AUDIO_AFE_ESP_SR)
    // Initialize ESP-SR AFE using C++ wrapper
    esp_err_t ret = sx_audio_afe_init_esp_sr(config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP-SR AFE init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "ESP-SR AFE initialized: AEC=%d, VAD=%d, NS=%d, AGC=%d, %lu Hz",
             s_config.enable_aec, s_config.enable_vad,
             s_config.enable_ns, s_config.enable_agc,
             (unsigned long)s_config.sample_rate_hz);
#else
    ESP_LOGW(TAG, "AFE init - ESP-SR library not enabled (CONFIG_SX_AUDIO_AFE_ESP_SR_ENABLE)");
    ESP_LOGI(TAG, "AFE config: AEC=%d, VAD=%d, NS=%d, AGC=%d, %lu Hz (fallback mode)",
             s_config.enable_aec, s_config.enable_vad,
             s_config.enable_ns, s_config.enable_agc,
             (unsigned long)s_config.sample_rate_hz);
#endif
    
    s_initialized = true;
    return ESP_OK;
}

bool sx_audio_afe_process(const int16_t *input, int16_t *output, size_t sample_count) {
    if (!s_initialized || input == NULL || output == NULL || sample_count == 0) {
        return false;
    }
    
#if defined(CONFIG_SX_AUDIO_AFE_ESP_SR_ENABLE) || defined(CONFIG_SX_AUDIO_AFE_ESP_SR)
    // Process audio through ESP-SR AFE
    return sx_audio_afe_process_esp_sr(input, output, sample_count);
#else
    // Fallback: just copy input to output (no processing)
    memcpy(output, input, sample_count * sizeof(int16_t));
    return false; // No voice activity detected
#endif
}

esp_err_t sx_audio_afe_register_vad_callback(sx_afe_vad_cb_t callback, void *user_data) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    s_vad_callback = callback;
    s_vad_user_data = user_data;
    ESP_LOGI(TAG, "VAD callback %s", callback ? "registered" : "unregistered");
    return ESP_OK;
}

bool sx_audio_afe_is_aec_enabled(void) {
    return s_initialized && s_config.enable_aec;
}

bool sx_audio_afe_is_vad_enabled(void) {
    return s_initialized && s_config.enable_vad;
}

esp_err_t sx_audio_afe_reset(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
#if defined(CONFIG_SX_AUDIO_AFE_ESP_SR_ENABLE) || defined(CONFIG_SX_AUDIO_AFE_ESP_SR)
    // Call ESP-SR reset function
    extern esp_err_t sx_audio_afe_reset_esp_sr(void);
    esp_err_t ret = sx_audio_afe_reset_esp_sr();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset ESP-SR AFE");
        return ret;
    }
#else
    ESP_LOGW(TAG, "AFE reset - ESP-SR not enabled");
#endif
    
    ESP_LOGD(TAG, "AFE reset");
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif


