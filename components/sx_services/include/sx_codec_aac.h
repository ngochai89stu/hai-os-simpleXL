#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// AAC decoder info structure
typedef struct {
    uint32_t sample_rate_hz;
    uint8_t channels;
    bool initialized;
} sx_codec_aac_info_t;

// Initialize AAC decoder
esp_err_t sx_codec_aac_init(void);

// Reset AAC decoder
esp_err_t sx_codec_aac_reset(void);

// Flush AAC decoder
esp_err_t sx_codec_aac_flush(void);

// Seek in AAC stream
esp_err_t sx_codec_aac_seek(uint32_t position_ms);

// Check if HE-AAC v2 is supported
bool sx_codec_aac_supports_heaac_v2(void);

// Decode AAC data
esp_err_t sx_codec_aac_decode(const uint8_t *in_data, size_t in_len,
                              int16_t *out_pcm, size_t out_cap_samples,
                              size_t *out_written_samples,
                              sx_codec_aac_info_t *out_info);

// Deinitialize AAC decoder
void sx_codec_aac_deinit(void);

#ifdef __cplusplus
}
#endif

