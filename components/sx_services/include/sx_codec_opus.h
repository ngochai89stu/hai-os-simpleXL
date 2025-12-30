#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: Opus Codec Service - Encoder/Decoder

// Opus encoder configuration
typedef struct {
    uint32_t sample_rate_hz;   // Sample rate (8000, 12000, 16000, 24000, 48000)
    uint8_t channels;          // Number of channels (1 = mono, 2 = stereo)
    uint8_t application;       // Application type (0=VOIP, 1=AUDIO, 2=LOWDELAY)
    int32_t bitrate_bps;       // Bitrate in bits per second (0 = auto)
} sx_opus_encoder_config_t;

// Opus decoder configuration
typedef struct {
    uint32_t sample_rate_hz;   // Sample rate
    uint8_t channels;          // Number of channels
} sx_opus_decoder_config_t;

// Initialize Opus encoder
esp_err_t sx_codec_opus_encoder_init(const sx_opus_encoder_config_t *config);

// Encode PCM to Opus
// pcm: Input PCM samples (int16_t, interleaved if stereo)
// pcm_samples: Number of PCM samples
// opus_data: Output buffer for Opus data
// opus_capacity: Capacity of output buffer in bytes
// opus_size: Output - actual size of encoded data
esp_err_t sx_codec_opus_encode(const int16_t *pcm, size_t pcm_samples,
                               uint8_t *opus_data, size_t opus_capacity, size_t *opus_size);

// Initialize Opus decoder
esp_err_t sx_codec_opus_decoder_init(const sx_opus_decoder_config_t *config);

// Decode Opus to PCM
// opus_data: Input Opus data
// opus_size: Size of Opus data in bytes
// pcm: Output buffer for PCM samples
// pcm_capacity: Capacity of output buffer in samples
// pcm_samples: Output - actual number of decoded samples
esp_err_t sx_codec_opus_decode(const uint8_t *opus_data, size_t opus_size,
                                int16_t *pcm, size_t pcm_capacity, size_t *pcm_samples);

// Reset encoder/decoder state
esp_err_t sx_codec_opus_encoder_reset(void);
esp_err_t sx_codec_opus_decoder_reset(void);

// Deinitialize
void sx_codec_opus_encoder_deinit(void);
void sx_codec_opus_decoder_deinit(void);

#ifdef __cplusplus
}
#endif












