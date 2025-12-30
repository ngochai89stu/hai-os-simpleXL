#include "sx_codec_aac.h"
#include "sx_codec_common.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "esp_audio_simple_dec_default.h"

static const char *TAG = "sx_codec_aac";

// AAC decoder state
static bool s_initialized = false;
static esp_audio_simple_dec_handle_t s_decoder = NULL;
static esp_audio_simple_dec_info_t s_decoder_info = {0};
static bool s_info_ready = false;
static uint8_t *s_out_buffer = NULL;
static size_t s_out_buffer_size = 0;

#define DEFAULT_OUT_BUFFER_SIZE 4096

esp_err_t sx_codec_aac_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing AAC Simple Decoder");
    
    // Register default decoders
    esp_audio_dec_register_default();
    esp_audio_simple_dec_register_default();
    
    // Configure AAC decoder
    esp_audio_simple_dec_cfg_t aac_cfg = {};
    aac_cfg.dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_AAC;
    aac_cfg.dec_cfg = NULL;   // Use default config
    aac_cfg.cfg_size = 0;
    
    esp_audio_err_t dec_ret = esp_audio_simple_dec_open(&aac_cfg, &s_decoder);
    if (dec_ret != ESP_AUDIO_ERR_OK || !s_decoder) {
        ESP_LOGE(TAG, "Failed to open AAC simple decoder, ret=%d", dec_ret);
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
    
    ESP_LOGI(TAG, "AAC Simple Decoder initialized successfully");
    return ESP_OK;
}

esp_err_t sx_codec_aac_reset(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGD(TAG, "AAC decoder reset");
    s_info_ready = false;
    memset(&s_decoder_info, 0, sizeof(s_decoder_info));
    
    // Note: We don't close/reopen decoder on reset, just clear state
    // This matches the reference implementation behavior
    return ESP_OK;
}

esp_err_t sx_codec_aac_flush(void) {
    if (!s_initialized || !s_decoder) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGD(TAG, "AAC decoder flush");
    
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
    
    ESP_LOGD(TAG, "AAC decoder flushed");
    return ESP_OK;
}

esp_err_t sx_codec_aac_seek(uint32_t position_ms) {
    if (!s_initialized || !s_decoder) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Note: Seek is typically not supported for streaming decoders
    // This is mainly for file-based playback
    // For HTTP streams, seek would require reconnecting to the stream
    // with a Range header, which is not commonly supported by radio streams
    
    ESP_LOGW(TAG, "AAC seek to %lu ms - not supported for streaming decoders", (unsigned long)position_ms);
    
    // For file-based streams, we would need:
    // 1. File position tracking
    // 2. Frame boundary detection
    // 3. Decoder reset and re-sync
    // This is complex and requires file I/O integration
    
    // For now, just flush and reset decoder
    // The caller should handle file seeking separately
    esp_err_t ret = sx_codec_aac_flush();
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = sx_codec_aac_reset();
    return ret;
}

bool sx_codec_aac_supports_heaac_v2(void) {
    // The esp_audio_simple_dec with AAC type should support HE-AAC v2
    // if the underlying decoder library (typically libfdk-aac) supports it
    // We can't directly query this, but we assume support if decoder is initialized
    // In practice, HE-AAC v2 streams will decode successfully if supported
    
    if (!s_initialized || !s_decoder) {
        return false;
    }
    
    // esp_audio_simple_dec with ESP_AUDIO_SIMPLE_DEC_TYPE_AAC typically uses
    // libfdk-aac or similar, which supports:
    // - AAC-LC
    // - HE-AAC (AAC+)
    // - HE-AAC v2 (with Parametric Stereo)
    // 
    // If the decoder successfully processes HE-AAC v2 streams, it's supported
    // We return true if decoder is initialized, as the library should support it
    
    return true; // Assume support if decoder is initialized
}

esp_err_t sx_codec_aac_decode(const uint8_t *in_data, size_t in_len,
                              int16_t *out_pcm, size_t out_cap_samples,
                              size_t *out_written_samples,
                              sx_codec_aac_info_t *out_info) {
    if (!s_initialized || !in_data || !out_pcm || !out_written_samples) {
        return ESP_ERR_INVALID_ARG;
    }
    if (in_len == 0 || out_cap_samples == 0) {
        *out_written_samples = 0;
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_decoder) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Prepare input frame
    esp_audio_simple_dec_raw_t raw = {};
    raw.buffer = (uint8_t *)in_data;
    raw.len = in_len;
    raw.eos = 0; // Not end of stream
    
    // Prepare output frame
    esp_audio_simple_dec_out_t out_frame = {};
    out_frame.buffer = s_out_buffer;
    out_frame.len = s_out_buffer_size;
    
    // Single decode attempt - esp_audio_simple_dec_process handles frame boundaries
    esp_audio_err_t dec_ret = esp_audio_simple_dec_process(s_decoder, &raw, &out_frame);
    
    if (dec_ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
        // Output buffer not enough, expand and retry
        size_t new_size = out_frame.needed_size;
        if (new_size > s_out_buffer_size) {
            uint8_t *new_buf = (uint8_t *)realloc(s_out_buffer, new_size);
            if (!new_buf) {
                ESP_LOGE(TAG, "Failed to expand output buffer to %zu", new_size);
                *out_written_samples = 0;
                return ESP_ERR_NO_MEM;
            }
            s_out_buffer = new_buf;
            s_out_buffer_size = new_size;
            out_frame.buffer = s_out_buffer;
            out_frame.len = new_size;
            // Retry decode
            dec_ret = esp_audio_simple_dec_process(s_decoder, &raw, &out_frame);
        }
    }
    
    if (dec_ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGD(TAG, "AAC decode error: %d (input: %zu bytes)", dec_ret, in_len);
        *out_written_samples = 0;
        return ESP_ERR_NOT_FINISHED; // Need more input or different error
    }
    
    if (out_frame.decoded_size == 0) {
        *out_written_samples = 0;
        return ESP_ERR_NOT_FINISHED; // No output, might need more input
    }
    
    // Get stream info on first successful decode
    if (!s_info_ready) {
        esp_audio_simple_dec_get_info(s_decoder, &s_decoder_info);
        s_info_ready = true;
        ESP_LOGI(TAG, "AAC stream info: %d Hz, %d bits, %d ch",
                 s_decoder_info.sample_rate,
                 s_decoder_info.bits_per_sample,
                 s_decoder_info.channel);
    }
    
    // Convert decoded bytes to samples
    int bits_per_sample = (s_decoder_info.bits_per_sample > 0) ? s_decoder_info.bits_per_sample : 16;
    int bytes_per_sample = bits_per_sample / 8;
    int channels = (s_decoder_info.channel > 0) ? s_decoder_info.channel : 2;
    
    size_t decoded_bytes = out_frame.decoded_size;
    size_t decoded_samples = decoded_bytes / bytes_per_sample;
    
    // Limit to output capacity
    size_t samples_to_copy = (decoded_samples <= out_cap_samples) ? decoded_samples : out_cap_samples;
    size_t bytes_to_copy = samples_to_copy * bytes_per_sample;
    
    // Copy PCM data to output buffer (interleaved stereo)
    memcpy(out_pcm, s_out_buffer, bytes_to_copy);
    
    // Update output info if requested
    if (out_info && s_info_ready) {
        out_info->sample_rate_hz = s_decoder_info.sample_rate;
        out_info->channels = channels;
        out_info->initialized = true;
    }
    
    *out_written_samples = samples_to_copy;
    
    if (samples_to_copy > 0) {
        return ESP_OK;
    } else {
        // No samples decoded, but not an error (might need more input)
        return ESP_ERR_NOT_FINISHED;
    }
}

void sx_codec_aac_deinit(void) {
    if (!s_initialized) {
        return;
    }
    
    if (s_decoder) {
        esp_audio_simple_dec_close(s_decoder);
        s_decoder = NULL;
    }
    
    esp_audio_simple_dec_unregister_default();
    esp_audio_dec_unregister_default();
    
    if (s_out_buffer) {
        free(s_out_buffer);
        s_out_buffer = NULL;
        s_out_buffer_size = 0;
    }
    
    s_info_ready = false;
    memset(&s_decoder_info, 0, sizeof(s_decoder_info));
    s_initialized = false;
    
    ESP_LOGI(TAG, "AAC decoder deinitialized");
}

