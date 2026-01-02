#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// MP3 Decoder Service
// Uses esp_audio_codec library for MP3 decoding

// Initialize MP3 decoder
esp_err_t sx_codec_mp3_init(void);

// Decode MP3 data to PCM
// mp3_data: Input MP3 data
// mp3_size: Size of MP3 data in bytes
// pcm: Output buffer for PCM samples (int16_t, interleaved if stereo)
// pcm_capacity: Capacity of output buffer in samples
// pcm_samples: Output - actual number of decoded samples
// Returns: ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized,
//          ESP_ERR_INVALID_SIZE if output buffer too small
esp_err_t sx_codec_mp3_decode(const uint8_t *mp3_data, size_t mp3_size,
                               int16_t *pcm, size_t pcm_capacity, size_t *pcm_samples);

// Get decoder info (sample rate, channels, bitrate)
// Call after first decode to get stream info
typedef struct {
    uint32_t sample_rate_hz;
    uint8_t channels;
    uint32_t bitrate_bps;
    bool info_ready;
} sx_mp3_decoder_info_t;

esp_err_t sx_codec_mp3_get_info(sx_mp3_decoder_info_t *info);

// Reset decoder state (clear internal buffers)
esp_err_t sx_codec_mp3_reset(void);

// Flush decoder buffers
esp_err_t sx_codec_mp3_flush(void);

// Deinitialize decoder
void sx_codec_mp3_deinit(void);

#ifdef __cplusplus
}
#endif



















