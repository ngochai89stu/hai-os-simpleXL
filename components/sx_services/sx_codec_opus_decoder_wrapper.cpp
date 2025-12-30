// Opus Decoder C++ Wrapper
// This file provides C++ implementation for Opus decoder using esp-opus-encoder library

#ifdef CONFIG_SX_CODEC_OPUS_ENABLE

#include "sx_codec_opus.h"
#include <opus_decoder.h>
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <mutex>

static const char *TAG = "sx_codec_opus_decoder_wrapper";

extern "C" {

// Opus decoder handle
static OpusDecoderWrapper *s_decoder = NULL;
static sx_opus_decoder_config_t s_decoder_config = {0};
static std::mutex s_decoder_mutex;

esp_err_t sx_codec_opus_decoder_init_cpp(const sx_opus_decoder_config_t *config) {
    std::lock_guard<std::mutex> lock(s_decoder_mutex);
    
    if (s_decoder != NULL) {
        delete s_decoder;
        s_decoder = NULL;
    }
    
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    try {
        // Create Opus decoder wrapper (60ms frame duration by default, như repo mẫu)
        s_decoder = new OpusDecoderWrapper(
            config->sample_rate_hz,
            config->channels,
            60  // Frame duration in ms (default, có thể điều chỉnh)
        );
        
        if (s_decoder == NULL) {
            ESP_LOGE(TAG, "Failed to create Opus decoder");
            return ESP_ERR_NO_MEM;
        }
        
        s_decoder_config = *config;
        
        ESP_LOGI(TAG, "Opus decoder initialized: %lu Hz, %d ch, 60ms frames",
                 s_decoder_config.sample_rate_hz, s_decoder_config.channels);
        
        return ESP_OK;
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Exception creating Opus decoder: %s", e.what());
        return ESP_FAIL;
    } catch (...) {
        ESP_LOGE(TAG, "Unknown exception creating Opus decoder");
        return ESP_FAIL;
    }
}

esp_err_t sx_codec_opus_decode_cpp(const uint8_t *opus_data, size_t opus_size,
                                    int16_t *pcm, size_t pcm_capacity, size_t *pcm_samples) {
    std::lock_guard<std::mutex> lock(s_decoder_mutex);
    
    if (s_decoder == NULL || opus_data == NULL || pcm == NULL || pcm_samples == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (opus_size == 0 || pcm_capacity == 0) {
        *pcm_samples = 0;
        return ESP_ERR_INVALID_ARG;
    }
    
    try {
        // Convert to vector
        std::vector<uint8_t> opus_vec(opus_data, opus_data + opus_size);
        std::vector<int16_t> pcm_vec;
        
        // Decode
        bool success = s_decoder->Decode(std::move(opus_vec), pcm_vec);
        
        if (!success) {
            ESP_LOGE(TAG, "Opus decode failed");
            *pcm_samples = 0;
            return ESP_FAIL;
        }
        
        // Check capacity
        if (pcm_vec.size() > pcm_capacity) {
            ESP_LOGE(TAG, "Decoded PCM data too large: %zu > %zu", pcm_vec.size(), pcm_capacity);
            *pcm_samples = 0;
            return ESP_ERR_NO_MEM;
        }
        
        // Copy to output buffer
        memcpy(pcm, pcm_vec.data(), pcm_vec.size() * sizeof(int16_t));
        *pcm_samples = pcm_vec.size();
        
        return ESP_OK;
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Exception decoding Opus: %s", e.what());
        *pcm_samples = 0;
        return ESP_FAIL;
    } catch (...) {
        ESP_LOGE(TAG, "Unknown exception decoding Opus");
        *pcm_samples = 0;
        return ESP_FAIL;
    }
}

esp_err_t sx_codec_opus_decoder_reset_cpp(void) {
    std::lock_guard<std::mutex> lock(s_decoder_mutex);
    
    if (s_decoder == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    try {
        s_decoder->ResetState();
        ESP_LOGD(TAG, "Opus decoder reset");
        return ESP_OK;
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Exception resetting Opus decoder: %s", e.what());
        return ESP_FAIL;
    } catch (...) {
        ESP_LOGE(TAG, "Unknown exception resetting Opus decoder");
        return ESP_FAIL;
    }
}

void sx_codec_opus_decoder_deinit_cpp(void) {
    std::lock_guard<std::mutex> lock(s_decoder_mutex);
    
    if (s_decoder != NULL) {
        delete s_decoder;
        s_decoder = NULL;
        ESP_LOGI(TAG, "Opus decoder deinitialized");
    }
}

uint32_t sx_codec_opus_decoder_get_sample_rate_cpp(void) {
    std::lock_guard<std::mutex> lock(s_decoder_mutex);
    
    if (s_decoder == NULL) {
        return 0;
    }
    
    return s_decoder->sample_rate();
}

uint32_t sx_codec_opus_decoder_get_frame_duration_cpp(void) {
    std::lock_guard<std::mutex> lock(s_decoder_mutex);
    
    if (s_decoder == NULL) {
        return 0;
    }
    
    return s_decoder->duration_ms();
}

} // extern "C"

#else // CONFIG_SX_CODEC_OPUS_ENABLE not defined

#include <esp_err.h>
#include <stdint.h>
#include <stddef.h>
#include "sx_codec_opus.h"

// Stub implementations when Opus is not enabled
extern "C" {

esp_err_t sx_codec_opus_decoder_init_cpp(const sx_opus_decoder_config_t *config) {
    (void)config;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t sx_codec_opus_decode_cpp(const uint8_t *opus_data, size_t opus_size,
                                    int16_t *pcm, size_t pcm_capacity, size_t *pcm_samples) {
    (void)opus_data;
    (void)opus_size;
    (void)pcm;
    (void)pcm_capacity;
    if (pcm_samples) {
        *pcm_samples = 0;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t sx_codec_opus_decoder_reset_cpp(void) {
    return ESP_ERR_NOT_SUPPORTED;
}

void sx_codec_opus_decoder_deinit_cpp(void) {
    // No-op
}

uint32_t sx_codec_opus_decoder_get_duration_ms_cpp(void) {
    return 0;
}

} // extern "C"

#endif // CONFIG_SX_CODEC_OPUS_ENABLE

