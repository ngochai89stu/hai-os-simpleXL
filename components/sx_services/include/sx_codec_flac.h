#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// FLAC Decoder Service
// Uses esp_audio_codec library for FLAC decoding (if supported)
// Note: FLAC support may require additional library or ESP-ADF

// Initialize FLAC decoder
esp_err_t sx_codec_flac_init(void);

// Decode FLAC data to PCM
// flac_data: Input FLAC data
// flac_size: Size of FLAC data in bytes
// pcm: Output buffer for PCM samples (int16_t, interleaved if stereo)
// pcm_capacity: Capacity of output buffer in samples
// pcm_samples: Output - actual number of decoded samples
// Returns: ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized,
//          ESP_ERR_INVALID_SIZE if output buffer too small
esp_err_t sx_codec_flac_decode(const uint8_t *flac_data, size_t flac_size,
                               int16_t *pcm, size_t pcm_capacity, size_t *pcm_samples);

// Get decoder info (sample rate, channels, bitrate)
// Call after first decode to get stream info
typedef struct {
    uint32_t sample_rate_hz;
    uint8_t channels;
    uint32_t bitrate_bps;
    bool info_ready;
} sx_flac_decoder_info_t;

esp_err_t sx_codec_flac_get_info(sx_flac_decoder_info_t *info);

// Reset decoder state (clear internal buffers)
esp_err_t sx_codec_flac_reset(void);

// Flush decoder buffers
esp_err_t sx_codec_flac_flush(void);

// Deinitialize decoder
void sx_codec_flac_deinit(void);

#ifdef __cplusplus
}
#endif




