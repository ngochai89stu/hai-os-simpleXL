#include "sx_codec_mp3.h"
#include "sx_codec_common.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "esp_audio_simple_dec_default.h"

static const char *TAG = "sx_codec_mp3";

// MP3 decoder state
static bool s_initialized = false;
static esp_audio_simple_dec_handle_t s_decoder = NULL;
static esp_audio_simple_dec_info_t s_decoder_info = {0};
static bool s_info_ready = false;
static uint8_t *s_out_buffer = NULL;
static size_t s_out_buffer_size = 0;

#define DEFAULT_OUT_BUFFER_SIZE 4096

esp_err_t sx_codec_mp3_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing MP3 Simple Decoder");
    
    // Register default decoders
    esp_audio_dec_register_default();
    esp_audio_simple_dec_register_default();
    
    // Configure MP3 decoder
    esp_audio_simple_dec_cfg_t mp3_cfg = {};
    mp3_cfg.dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3;
    mp3_cfg.dec_cfg = NULL;   // Use default config
    mp3_cfg.cfg_size = 0;
    
    esp_audio_err_t dec_ret = esp_audio_simple_dec_open(&mp3_cfg, &s_decoder);
    if (dec_ret != ESP_AUDIO_ERR_OK || !s_decoder) {
        ESP_LOGE(TAG, "Failed to open MP3 simple decoder, ret=%d", dec_ret);
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
    
    ESP_LOGI(TAG, "MP3 Simple Decoder initialized successfully");
    return ESP_OK;
}

esp_err_t sx_codec_mp3_decode(const uint8_t *mp3_data, size_t mp3_size,
                              int16_t *pcm, size_t pcm_capacity, size_t *pcm_samples) {
    if (!s_initialized || !s_decoder || mp3_data == NULL || pcm == NULL || pcm_samples == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (mp3_size == 0 || pcm_capacity == 0) {
        *pcm_samples = 0;
        return ESP_ERR_INVALID_ARG;
    }
    
    // Prepare input raw data
    esp_audio_simple_dec_raw_t raw = {};
    raw.buffer = (uint8_t *)mp3_data;
    raw.len = mp3_size;
    raw.eos = 0; // Not end of stream
    
    // Prepare output frame
    esp_audio_simple_dec_out_t out_frame = {};
    out_frame.buffer = s_out_buffer;
    out_frame.len = s_out_buffer_size;
    
    // Decode MP3 to PCM
    esp_audio_err_t ret = esp_audio_simple_dec_process(s_decoder, &raw, &out_frame);
    
    if (ret != ESP_AUDIO_ERR_OK) {
        // Check if it's end of stream (no data or data lack)
        if (ret == ESP_AUDIO_ERR_DATA_LACK || out_frame.len == 0) {
            // End of stream
            *pcm_samples = 0;
            return ESP_OK;
        }
        ESP_LOGE(TAG, "MP3 decode failed: %d", ret);
        *pcm_samples = 0;
        return ESP_FAIL;
    }
    
    // Check if decoder info is ready
    if (!s_info_ready) {
        esp_audio_err_t info_ret = esp_audio_simple_dec_get_info(s_decoder, &s_decoder_info);
        if (info_ret == ESP_AUDIO_ERR_OK) {
            s_info_ready = true;
            ESP_LOGI(TAG, "MP3 decoder info: %lu Hz, %d ch, %lu bps",
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
}

esp_err_t sx_codec_mp3_get_info(sx_mp3_decoder_info_t *info) {
    if (!s_initialized || info == NULL) {
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
}

esp_err_t sx_codec_mp3_reset(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGD(TAG, "MP3 decoder reset");
    s_info_ready = false;
    memset(&s_decoder_info, 0, sizeof(s_decoder_info));
    
    // Note: We don't close/reopen decoder on reset, just clear state
    return ESP_OK;
}

esp_err_t sx_codec_mp3_flush(void) {
    if (!s_initialized || !s_decoder) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGD(TAG, "MP3 decoder flush");
    
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
    if (ret != ESP_AUDIO_ERR_OK && ret != ESP_AUDIO_ERR_DATA_LACK) {
        ESP_LOGW(TAG, "Flush returned: %d (may be normal)", ret);
    }
    
    // Clear decoder state
    s_info_ready = false;
    memset(&s_decoder_info, 0, sizeof(s_decoder_info));
    
    return ESP_OK;
}

void sx_codec_mp3_deinit(void) {
    if (!s_initialized) {
        return;
    }
    
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
    
    ESP_LOGI(TAG, "MP3 decoder deinitialized");
}


