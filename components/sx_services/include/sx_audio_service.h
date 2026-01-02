#pragma once

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>
#include "sx_event.h"

#ifdef __cplusplus
extern "C" {
#endif

// Phase 4: Audio service - I2S playback and recording
// Clean API, event-driven communication

// Audio service initialization
esp_err_t sx_audio_service_init(void);

// Start audio service task
esp_err_t sx_audio_service_start(void);

// Audio playback control
esp_err_t sx_audio_play_file(const char *file_path);
esp_err_t sx_audio_stop(void);
esp_err_t sx_audio_pause(void);
esp_err_t sx_audio_resume(void);
bool sx_audio_is_playing(void);

// Audio recording control
esp_err_t sx_audio_start_recording(void);
esp_err_t sx_audio_stop_recording(void);
bool sx_audio_is_recording(void);

// Phase 5: Recording with STT callback support
// Start recording with STT callback (automatically sends to STT service)
esp_err_t sx_audio_start_recording_with_stt(void);

// Phase 5: Recording callback for audio streaming
// Callback function type: (pcm_data, sample_count, sample_rate)
typedef void (*sx_audio_recording_callback_t)(const int16_t *pcm, size_t sample_count, uint32_t sample_rate);
// Set recording callback (called when PCM data is available during recording)
esp_err_t sx_audio_set_recording_callback(sx_audio_recording_callback_t callback);

// Volume control
esp_err_t sx_audio_set_volume(uint8_t volume); // 0-100
uint8_t sx_audio_get_volume(void);

// Position and Duration (Phase 1: Hybrid Music Screen)
uint32_t sx_audio_get_position(void);      // Current position (seconds)
uint32_t sx_audio_get_duration(void);      // Total duration (seconds)
esp_err_t sx_audio_seek(uint32_t position); // Seek to position (seconds)

// Phase 5: Audio capabilities
typedef struct {
    bool seek_supported;       // True if seek is supported
} sx_audio_caps_t;
sx_audio_caps_t sx_audio_get_caps(void);  // Get audio service capabilities

// Spectrum/FFT Data (Phase 1: Hybrid Music Screen)
esp_err_t sx_audio_get_spectrum(uint16_t *bands, size_t band_count);
// Returns: bands[0] = Bass, bands[1] = Mid-low, bands[2] = Mid-high, bands[3] = High
// Range: 0-255 for each band

// Feed raw PCM samples (signed 16-bit stereo interleaved) into I2S pipeline.
// sample_rate_hz must match current I2S clock or the service will ignore.
// Return ESP_OK on success, ESP_ERR_INVALID_STATE if I2S not ready.
esp_err_t sx_audio_service_feed_pcm(const int16_t *pcm, size_t sample_count, uint32_t sample_rate_hz);

// Events emitted by audio service:
// - SX_EVT_AUDIO_PLAYBACK_STARTED
// - SX_EVT_AUDIO_PLAYBACK_STOPPED
// - SX_EVT_AUDIO_PLAYBACK_PAUSED
// - SX_EVT_AUDIO_PLAYBACK_RESUMED
// - SX_EVT_AUDIO_RECORDING_STARTED
// - SX_EVT_AUDIO_RECORDING_STOPPED
// - SX_EVT_AUDIO_ERROR

#ifdef __cplusplus
}
#endif




