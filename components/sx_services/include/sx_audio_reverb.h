#pragma once

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: Audio Reverb Service
// Simple delay-based reverb effect

// Initialize reverb service
// sample_rate_hz: Audio sample rate (default: 16000)
esp_err_t sx_audio_reverb_init(uint32_t sample_rate_hz);

// Deinitialize reverb service
void sx_audio_reverb_deinit(void);

// Set reverb level (0-100, where 0 = no reverb, 100 = maximum reverb)
esp_err_t sx_audio_reverb_set_level(uint8_t level);

// Get current reverb level (0-100)
uint8_t sx_audio_reverb_get_level(void);

// Enable/disable reverb
esp_err_t sx_audio_reverb_enable(bool enable);

// Check if reverb is enabled
bool sx_audio_reverb_is_enabled(void);

// Process PCM samples through reverb (in-place processing)
// samples: interleaved stereo samples (int16_t)
// sample_count: number of samples (not pairs, so stereo = 2 * frame_count)
esp_err_t sx_audio_reverb_process(int16_t *samples, size_t sample_count);

// Update sample rate (if audio sample rate changes)
esp_err_t sx_audio_reverb_set_sample_rate(uint32_t sample_rate_hz);

#ifdef __cplusplus
}
#endif










