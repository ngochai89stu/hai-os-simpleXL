#include "sx_codec_flac.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>

// Note: FLAC decoder support in esp_audio_codec may vary
// If ESP_AUDIO_SIMPLE_DEC_TYPE_FLAC is not available, we'll use a fallback
// or indicate that FLAC support requires additional library

#ifdef ESP_AUDIO_SIMPLE_DEC_TYPE_FLAC
#include "esp_audio_simple_dec_default.h"
#endif

static const char *TAG = "sx_codec_flac";

// FLAC decoder state
static bool s_initialized = false;
#ifdef ESP_AUDIO_SIMPLE_DEC_TYPE_FLAC
static esp_audio_simple_dec_handle_t s_decoder = NULL;
static esp_audio_simple_dec_info_t s_decoder_info = {0};
static bool s_info_ready = false;
static uint8_t *s_out_buffer = NULL;
static size_t s_out_buffer_size = 0;
#define DEFAULT_OUT_BUFFER_SIZE 4096
#endif

esp_err_t sx_codec_flac_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
#ifdef ESP_AUDIO_SIMPLE_DEC_TYPE_FLAC
    ESP_LOGI(TAG, "Initializing FLAC Simple Decoder");
    
    // Register default decoders
    esp_audio_dec_register_default();
    esp_audio_simple_dec_register_default();
    
    // Configure FLAC decoder
    esp_audio_simple_dec_cfg_t flac_cfg = {};
    flac_cfg.dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_FLAC;
    flac_cfg.dec_cfg = NULL;   // Use default config
    flac_cfg.cfg_size = 0;
    
    esp_audio_err_t dec_ret = esp_audio_simple_dec_open(&flac_cfg, &s_decoder);
    if (dec_ret != ESP_AUDIO_ERR_OK || !s_decoder) {
        ESP_LOGE(TAG, "Failed to open FLAC simple decoder, ret=%d", dec_ret);
        esp_audio_simple_dec_unregister_default();
        esp_audio_dec_unregister_default();
        return ESP_FAIL;
    }
    
    // Allocate output buffer
    s_out_buffer_size = DEFAULT_OUT_BUFFER_SIZE;
    s_out_buffer = (uint8_t *)malloc(s_out_buffer_size);
    if (!s_out_buffer) {
        ESP_LOGE(TAG, "Failed to allocate output buffer");
        esp_audio_simple_dec_close(s_decoder);
        s_decoder = NULL;
        esp_audio_simple_dec_unregister_default();
        esp_audio_dec_unregister_default();
        return ESP_ERR_NO_MEM;
    }
    
    s_info_ready = false;
    s_initialized = true;
    
    ESP_LOGI(TAG, "FLAC Simple Decoder initialized successfully");
    return ESP_OK;
#else
    // FLAC decoder type not available in esp_audio_codec
    // This requires ESP-ADF or additional FLAC library
    ESP_LOGW(TAG, "FLAC decoder not available - ESP_AUDIO_SIMPLE_DEC_TYPE_FLAC not defined");
    ESP_LOGW(TAG, "FLAC support requires ESP-ADF or additional FLAC decoder library");
    s_initialized = true; // Mark as initialized to prevent repeated warnings
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t sx_codec_flac_decode(const uint8_t *flac_data, size_t flac_size,
                                int16_t *pcm, size_t pcm_capacity, size_t *pcm_samples) {
    if (!s_initialized || flac_data == NULL || pcm == NULL || pcm_samples == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (flac_size == 0 || pcm_capacity == 0) {
        *pcm_samples = 0;
        return ESP_ERR_INVALID_ARG;
    }
    
#ifdef ESP_AUDIO_SIMPLE_DEC_TYPE_FLAC
    if (!s_decoder) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Prepare input raw data
    esp_audio_simple_dec_raw_t raw = {};
    raw.buffer = (uint8_t *)flac_data;
    raw.len = flac_size;
    raw.eos = 0; // Not end of stream
    
    // Prepare output frame
    esp_audio_simple_dec_out_t out_frame = {};
    out_frame.buffer = s_out_buffer;
    out_frame.len = s_out_buffer_size;
    
    // Decode FLAC to PCM
    esp_audio_err_t ret = esp_audio_simple_dec_process(s_decoder, &raw, &out_frame);
    
    if (ret == ESP_AUDIO_ERR_EOF) {
        // End of stream
        *pcm_samples = 0;
        return ESP_OK;
    }
    
    if (ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "FLAC decode failed: %d", ret);
        *pcm_samples = 0;
        return ESP_FAIL;
    }
    
    // Check if decoder info is ready
    if (!s_info_ready) {
        esp_audio_err_t info_ret = esp_audio_simple_dec_get_info(s_decoder, &s_decoder_info);
        if (info_ret == ESP_AUDIO_ERR_OK) {
            s_info_ready = true;
            ESP_LOGI(TAG, "FLAC decoder info: %lu Hz, %d ch, %lu bps",
                     (unsigned long)s_decoder_info.sample_rate,
                     s_decoder_info.channel,
                     (unsigned long)s_decoder_info.bitrate);
        }
    }
    
    // Check output buffer size
    size_t output_samples = out_frame.len / sizeof(int16_t);
    if (output_samples > pcm_capacity) {
        ESP_LOGE(TAG, "Output buffer too small: need %zu samples, have %zu",
                 output_samples, pcm_capacity);
        *pcm_samples = 0;
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Copy decoded PCM to output buffer
    memcpy(pcm, out_frame.buffer, out_frame.len);
    *pcm_samples = output_samples;
    
    return ESP_OK;
#else
    // FLAC decoder not available
    ESP_LOGW(TAG, "FLAC decode - decoder not available");
    *pcm_samples = 0;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t sx_codec_flac_get_info(sx_flac_decoder_info_t *info) {
    if (!s_initialized || info == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
#ifdef ESP_AUDIO_SIMPLE_DEC_TYPE_FLAC
    if (!s_decoder) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!s_info_ready) {
        // Try to get info
        esp_audio_err_t ret = esp_audio_simple_dec_get_info(s_decoder, &s_decoder_info);
        if (ret == ESP_AUDIO_ERR_OK) {
            s_info_ready = true;
        } else {
            info->info_ready = false;
            return ESP_ERR_NOT_FINISHED;
        }
    }
    
    info->sample_rate_hz = s_decoder_info.sample_rate;
    info->channels = s_decoder_info.channel;
    info->bitrate_bps = s_decoder_info.bitrate;
    info->info_ready = true;
    
    return ESP_OK;
#else
    info->info_ready = false;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t sx_codec_flac_reset(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
#ifdef ESP_AUDIO_SIMPLE_DEC_TYPE_FLAC
    ESP_LOGD(TAG, "FLAC decoder reset");
    s_info_ready = false;
    memset(&s_decoder_info, 0, sizeof(s_decoder_info));
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t sx_codec_flac_flush(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
#ifdef ESP_AUDIO_SIMPLE_DEC_TYPE_FLAC
    if (!s_decoder) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGD(TAG, "FLAC decoder flush");
    
    // Flush decoder buffers by sending empty frame with EOS flag
    esp_audio_simple_dec_raw_t raw = {};
    raw.buffer = NULL;
    raw.len = 0;
    raw.eos = 1; // End of stream to flush
    
    esp_audio_simple_dec_out_t out_frame = {};
    out_frame.buffer = s_out_buffer;
    out_frame.len = s_out_buffer_size;
    
    // Call decode with EOS to flush
    esp_audio_err_t ret = esp_audio_simple_dec_process(s_decoder, &raw, &out_frame);
    if (ret != ESP_AUDIO_ERR_OK && ret != ESP_AUDIO_ERR_EOF) {
        ESP_LOGW(TAG, "Flush returned: %d (may be normal)", ret);
    }
    
    // Clear decoder state
    s_info_ready = false;
    memset(&s_decoder_info, 0, sizeof(s_decoder_info));
    
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

void sx_codec_flac_deinit(void) {
    if (!s_initialized) {
        return;
    }
    
#ifdef ESP_AUDIO_SIMPLE_DEC_TYPE_FLAC
    if (s_out_buffer) {
        free(s_out_buffer);
        s_out_buffer = NULL;
        s_out_buffer_size = 0;
    }
    
    if (s_decoder) {
        esp_audio_simple_dec_close(s_decoder);
        s_decoder = NULL;
    }
    
    esp_audio_simple_dec_unregister_default();
    esp_audio_dec_unregister_default();
    
    s_info_ready = false;
    memset(&s_decoder_info, 0, sizeof(s_decoder_info));
    s_initialized = false;
    
    ESP_LOGI(TAG, "FLAC decoder deinitialized");
#else
    s_initialized = false;
    ESP_LOGI(TAG, "FLAC decoder deinitialized (was not available)");
#endif
}











