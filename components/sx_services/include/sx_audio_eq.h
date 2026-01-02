#pragma once

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: Audio Equalizer - 10-band EQ with presets

// EQ preset IDs
typedef enum {
    SX_AUDIO_EQ_PRESET_FLAT = 0,
    SX_AUDIO_EQ_PRESET_POP,
    SX_AUDIO_EQ_PRESET_ROCK,
    SX_AUDIO_EQ_PRESET_JAZZ,
    SX_AUDIO_EQ_PRESET_CLASSICAL,
    SX_AUDIO_EQ_PRESET_CUSTOM,
    SX_AUDIO_EQ_PRESET_MAX
} sx_audio_eq_preset_t;

// Number of EQ bands
#define SX_AUDIO_EQ_NUM_BANDS 10

// EQ band frequencies (Hz)
#define SX_AUDIO_EQ_BAND_31HZ    0
#define SX_AUDIO_EQ_BAND_62HZ    1
#define SX_AUDIO_EQ_BAND_125HZ   2
#define SX_AUDIO_EQ_BAND_250HZ   3
#define SX_AUDIO_EQ_BAND_500HZ   4
#define SX_AUDIO_EQ_BAND_1KHZ    5
#define SX_AUDIO_EQ_BAND_2KHZ    6
#define SX_AUDIO_EQ_BAND_4KHZ    7
#define SX_AUDIO_EQ_BAND_8KHZ    8
#define SX_AUDIO_EQ_BAND_16KHZ   9

// EQ gain range: -12dB to +12dB (in 0.1dB steps, so -120 to +120)
#define SX_AUDIO_EQ_GAIN_MIN -120  // -12.0 dB
#define SX_AUDIO_EQ_GAIN_MAX  120  // +12.0 dB

// Initialize EQ service
esp_err_t sx_audio_eq_init(uint32_t sample_rate_hz);

// Deinitialize EQ service
void sx_audio_eq_deinit(void);

// Set gain for a specific band (gain_db in 0.1dB units, range: -120 to +120)
esp_err_t sx_audio_eq_set_band(int band, int16_t gain_db);

// Get gain for a specific band
int16_t sx_audio_eq_get_band(int band);

// Set all bands at once (gains in 0.1dB units)
esp_err_t sx_audio_eq_set_bands(const int16_t gains[SX_AUDIO_EQ_NUM_BANDS]);

// Get all bands at once
esp_err_t sx_audio_eq_get_bands(int16_t gains[SX_AUDIO_EQ_NUM_BANDS]);

// Set EQ preset
esp_err_t sx_audio_eq_set_preset(sx_audio_eq_preset_t preset);

// Get current preset
sx_audio_eq_preset_t sx_audio_eq_get_preset(void);

// Enable/disable EQ
esp_err_t sx_audio_eq_enable(bool enable);

// Check if EQ is enabled
bool sx_audio_eq_is_enabled(void);

// Process PCM samples through EQ (in-place processing)
// samples: interleaved stereo samples (int16_t)
// sample_count: number of samples (not pairs, so stereo = 2 * frame_count)
esp_err_t sx_audio_eq_process(int16_t *samples, size_t sample_count);

// Update sample rate (if audio sample rate changes)
esp_err_t sx_audio_eq_set_sample_rate(uint32_t sample_rate_hz);

#ifdef __cplusplus
}
#endif




















