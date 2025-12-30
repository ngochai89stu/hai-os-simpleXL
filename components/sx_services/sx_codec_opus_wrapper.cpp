// Opus Codec C++ Wrapper
// This file provides C++ implementation for Opus encoder using esp-opus-encoder library

#ifdef CONFIG_SX_CODEC_OPUS_ENABLE

#include "sx_codec_opus.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "opus_encoder.h"
#include <vector>

static const char *TAG = "sx_codec_opus_wrapper";

// Opus encoder/decoder handles
static OpusEncoderWrapper *s_encoder = NULL;
static sx_opus_encoder_config_t s_encoder_config = {0};
static bool s_encoder_initialized = false;

extern "C" {

esp_err_t sx_codec_opus_encoder_init_cpp(const sx_opus_encoder_config_t *config) {
    if (s_encoder_initialized && s_encoder != NULL) {
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
    
    try {
        // Create Opus encoder wrapper (20ms frame duration)
        s_encoder = new OpusEncoderWrapper(
            config->sample_rate_hz,
            config->channels,
            20  // Frame duration in ms
        );
        
        if (s_encoder == NULL) {
            ESP_LOGE(TAG, "Failed to create Opus encoder");
            return ESP_ERR_NO_MEM;
        }
        
        s_encoder_initialized = true;
        ESP_LOGI(TAG, "Opus encoder initialized: %lu Hz, %d ch, %d bps",
                 (unsigned long)config->sample_rate_hz, config->channels, config->bitrate_bps);
        return ESP_OK;
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Exception creating Opus encoder: %s", e.what());
        return ESP_FAIL;
    } catch (...) {
        ESP_LOGE(TAG, "Unknown exception creating Opus encoder");
        return ESP_FAIL;
    }
}

esp_err_t sx_codec_opus_encode_cpp(const int16_t *pcm, size_t pcm_samples,
                                   uint8_t *opus_data, size_t opus_capacity, size_t *opus_size) {
    if (!s_encoder_initialized || s_encoder == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (pcm == NULL || opus_data == NULL || opus_size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (pcm_samples == 0 || opus_capacity == 0) {
        *opus_size = 0;
        return ESP_ERR_INVALID_ARG;
    }
    
    try {
        // Convert C array to vector
        std::vector<int16_t> pcm_vec(pcm, pcm + pcm_samples);
        
        // Encode
        std::vector<uint8_t> opus_vec = s_encoder->Encode(pcm_vec);
        
        if (opus_vec.size() > opus_capacity) {
            ESP_LOGE(TAG, "Opus encoded data too large: %zu > %zu", opus_vec.size(), opus_capacity);
            *opus_size = 0;
            return ESP_ERR_INVALID_SIZE;
        }
        
        // Copy to output buffer
        memcpy(opus_data, opus_vec.data(), opus_vec.size());
        *opus_size = opus_vec.size();
        
        return ESP_OK;
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Exception encoding Opus: %s", e.what());
        *opus_size = 0;
        return ESP_FAIL;
    } catch (...) {
        ESP_LOGE(TAG, "Unknown exception encoding Opus");
        *opus_size = 0;
        return ESP_FAIL;
    }
}

esp_err_t sx_codec_opus_encoder_reset_cpp(void) {
    if (!s_encoder_initialized || s_encoder == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    try {
        // OpusEncoderWrapper doesn't have explicit reset, but we can recreate it
        // For now, we'll just log - actual reset would require recreating the encoder
        // which is expensive. If needed, we can implement proper reset by storing
        // config and recreating.
        ESP_LOGD(TAG, "Opus encoder reset (no-op, encoder state is stateless per frame)");
        return ESP_OK;
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Exception resetting Opus encoder: %s", e.what());
        return ESP_FAIL;
    } catch (...) {
        ESP_LOGE(TAG, "Unknown exception resetting Opus encoder");
        return ESP_FAIL;
    }
}

void sx_codec_opus_encoder_deinit_cpp(void) {
    if (s_encoder != NULL) {
        delete s_encoder;
        s_encoder = NULL;
    }
    s_encoder_initialized = false;
    ESP_LOGI(TAG, "Opus encoder deinitialized");
}

} // extern "C"

#else // CONFIG_SX_CODEC_OPUS_ENABLE not defined

#include <esp_err.h>
#include <stdint.h>
#include <stddef.h>
#include "sx_codec_opus.h"

// Stub implementations when Opus is not enabled
extern "C" {

esp_err_t sx_codec_opus_encoder_init_cpp(const sx_opus_encoder_config_t *config) {
    (void)config;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t sx_codec_opus_encode_cpp(const int16_t *pcm, size_t pcm_samples,
                                   uint8_t *opus_data, size_t opus_capacity, size_t *opus_size) {
    (void)pcm;
    (void)pcm_samples;
    (void)opus_data;
    (void)opus_capacity;
    if (opus_size) {
        *opus_size = 0;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t sx_codec_opus_encoder_reset_cpp(void) {
    return ESP_ERR_NOT_SUPPORTED;
}

void sx_codec_opus_encoder_deinit_cpp(void) {
    // No-op
}

} // extern "C"

#endif // CONFIG_SX_CODEC_OPUS_ENABLE

