#include "sx_codec_opus.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>

// Forward declarations for C++ wrapper functions
#ifdef CONFIG_SX_CODEC_OPUS_ENABLE
extern esp_err_t sx_codec_opus_encoder_init_cpp(const sx_opus_encoder_config_t *config);
extern esp_err_t sx_codec_opus_encode_cpp(const int16_t *pcm, size_t pcm_samples,
                                           uint8_t *opus_data, size_t opus_capacity, size_t *opus_size);
extern esp_err_t sx_codec_opus_encoder_reset_cpp(void);
extern void sx_codec_opus_encoder_deinit_cpp(void);

extern esp_err_t sx_codec_opus_decoder_init_cpp(const sx_opus_decoder_config_t *config);
extern esp_err_t sx_codec_opus_decode_cpp(const uint8_t *opus_data, size_t opus_size,
                                           int16_t *pcm, size_t pcm_capacity, size_t *pcm_samples);
extern esp_err_t sx_codec_opus_decoder_reset_cpp(void);
extern void sx_codec_opus_decoder_deinit_cpp(void);
extern uint32_t sx_codec_opus_decoder_get_sample_rate_cpp(void);
extern uint32_t sx_codec_opus_decoder_get_frame_duration_cpp(void);
#endif

static const char *TAG = "sx_codec_opus";

// Opus codec state
static bool s_encoder_initialized = false;
static bool s_decoder_initialized = false;
static sx_opus_encoder_config_t s_encoder_config = {0};
static sx_opus_decoder_config_t s_decoder_config = {0};

// Note: Opus codec requires libopus library
// For ESP32, we can use:
// - ESP-ADF opus component (if available)
// - libopus port for ESP32
// - External opus library integration

esp_err_t sx_codec_opus_encoder_init(const sx_opus_encoder_config_t *config) {
    if (s_encoder_initialized) {
        return ESP_OK;
    }
    
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate sample rate
    if (config->sample_rate_hz != 8000 && config->sample_rate_hz != 12000 &&
        config->sample_rate_hz != 16000 && config->sample_rate_hz != 24000 &&
        config->sample_rate_hz != 48000) {
        ESP_LOGE(TAG, "Unsupported sample rate: %lu Hz", (unsigned long)config->sample_rate_hz);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Copy configuration
    s_encoder_config = *config;
    
#ifdef CONFIG_SX_CODEC_OPUS_ENABLE
    // Initialize Opus encoder using C++ wrapper
    esp_err_t ret = sx_codec_opus_encoder_init_cpp(config);
    if (ret != ESP_OK) {
        return ret;
    }
    s_encoder_initialized = true;
    return ESP_OK;
#else
    ESP_LOGW(TAG, "Opus encoder init - library not enabled (CONFIG_SX_CODEC_OPUS_ENABLE)");
    ESP_LOGI(TAG, "Opus encoder config: %lu Hz, %d ch, %d bps",
             (unsigned long)config->sample_rate_hz, config->channels, config->bitrate_bps);
    s_encoder_initialized = true;
    return ESP_OK;
#endif
}

esp_err_t sx_codec_opus_encode(const int16_t *pcm, size_t pcm_samples,
                               uint8_t *opus_data, size_t opus_capacity, size_t *opus_size) {
    if (!s_encoder_initialized || pcm == NULL || opus_data == NULL || opus_size == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (pcm_samples == 0 || opus_capacity == 0) {
        *opus_size = 0;
        return ESP_ERR_INVALID_ARG;
    }
    
#ifdef CONFIG_SX_CODEC_OPUS_ENABLE
    return sx_codec_opus_encode_cpp(pcm, pcm_samples, opus_data, opus_capacity, opus_size);
#else
    ESP_LOGW(TAG, "Opus encode - library not enabled");
    *opus_size = 0;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t sx_codec_opus_decoder_init(const sx_opus_decoder_config_t *config) {
    if (s_decoder_initialized) {
        // Reinitialize if sample rate changed
        if (s_decoder_config.sample_rate_hz != config->sample_rate_hz ||
            s_decoder_config.channels != config->channels) {
            sx_codec_opus_decoder_deinit();
            s_decoder_initialized = false;
        } else {
            return ESP_OK;
        }
    }
    
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate sample rate
    if (config->sample_rate_hz != 8000 && config->sample_rate_hz != 12000 &&
        config->sample_rate_hz != 16000 && config->sample_rate_hz != 24000 &&
        config->sample_rate_hz != 48000) {
        ESP_LOGE(TAG, "Unsupported sample rate: %lu Hz", (unsigned long)config->sample_rate_hz);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Copy configuration
    s_decoder_config = *config;
    
#ifdef CONFIG_SX_CODEC_OPUS_ENABLE
    // Initialize Opus decoder using C++ wrapper
    esp_err_t ret = sx_codec_opus_decoder_init_cpp(config);
    if (ret != ESP_OK) {
        return ret;
    }
    s_decoder_initialized = true;
    return ESP_OK;
#else
    ESP_LOGW(TAG, "Opus decoder init - library not enabled");
    ESP_LOGI(TAG, "Opus decoder config: %lu Hz, %d ch",
             (unsigned long)config->sample_rate_hz, config->channels);
    s_decoder_initialized = true;
    return ESP_OK;
#endif
}

esp_err_t sx_codec_opus_decode(const uint8_t *opus_data, size_t opus_size,
                                int16_t *pcm, size_t pcm_capacity, size_t *pcm_samples) {
    if (!s_decoder_initialized || opus_data == NULL || pcm == NULL || pcm_samples == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (opus_size == 0 || pcm_capacity == 0) {
        *pcm_samples = 0;
        return ESP_ERR_INVALID_ARG;
    }
    
#ifdef CONFIG_SX_CODEC_OPUS_ENABLE
    return sx_codec_opus_decode_cpp(opus_data, opus_size, pcm, pcm_capacity, pcm_samples);
#else
    ESP_LOGW(TAG, "Opus decode - library not enabled");
    *pcm_samples = 0;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t sx_codec_opus_encoder_reset(void) {
    if (!s_encoder_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
#ifdef CONFIG_SX_CODEC_OPUS_ENABLE
    // Call C++ wrapper reset function
    extern esp_err_t sx_codec_opus_encoder_reset_cpp(void);
    esp_err_t ret = sx_codec_opus_encoder_reset_cpp();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset Opus encoder");
        return ret;
    }
#else
    ESP_LOGW(TAG, "Opus encoder reset - library not enabled");
#endif
    
    ESP_LOGD(TAG, "Opus encoder reset");
    return ESP_OK;
}

esp_err_t sx_codec_opus_decoder_reset(void) {
    if (!s_decoder_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
#ifdef CONFIG_SX_CODEC_OPUS_ENABLE
    return sx_codec_opus_decoder_reset_cpp();
#else
    ESP_LOGW(TAG, "Opus decoder reset - library not enabled");
    ESP_LOGD(TAG, "Opus decoder reset (no-op)");
    return ESP_OK;
#endif
}

void sx_codec_opus_encoder_deinit(void) {
    if (!s_encoder_initialized) {
        return;
    }
    
#ifdef CONFIG_SX_CODEC_OPUS_ENABLE
    sx_codec_opus_encoder_deinit_cpp();
#endif
    
    s_encoder_initialized = false;
    ESP_LOGI(TAG, "Opus encoder deinitialized");
}

void sx_codec_opus_decoder_deinit(void) {
    if (!s_decoder_initialized) {
        return;
    }
    
#ifdef CONFIG_SX_CODEC_OPUS_ENABLE
    sx_codec_opus_decoder_deinit_cpp();
#endif
    
    s_decoder_initialized = false;
    ESP_LOGI(TAG, "Opus decoder deinitialized");
}


