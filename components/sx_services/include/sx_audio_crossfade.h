#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: Crossfade Engine - Smooth fade between audio tracks

// Crossfade configuration
typedef struct {
    uint32_t fade_duration_ms; // Crossfade duration in milliseconds, default 500ms
    bool enabled;              // Enable/disable crossfade, default true
} sx_audio_crossfade_config_t;

// Crossfade state
typedef enum {
    SX_CROSSFADE_IDLE,      // No crossfade in progress
    SX_CROSSFADE_FADING_OUT, // Fading out old track
    SX_CROSSFADE_FADING_IN,  // Fading in new track
    SX_CROSSFADE_COMPLETE,   // Crossfade complete
} sx_audio_crossfade_state_t;

// Initialize crossfade engine
esp_err_t sx_audio_crossfade_init(const sx_audio_crossfade_config_t *config);

// Start crossfade between tracks
// old_pcm: Current playing PCM buffer (will be faded out)
// new_pcm: New PCM buffer to fade in
// sample_count: Number of samples in each buffer
// Returns ESP_OK on success
esp_err_t sx_audio_crossfade_start(const int16_t *old_pcm, const int16_t *new_pcm, size_t sample_count);

// Process crossfade on PCM buffer
// This should be called for each audio chunk during crossfade
// pcm: PCM buffer to process (will be modified in-place)
// sample_count: Number of samples
// Returns true if crossfade is still in progress, false if complete
bool sx_audio_crossfade_process(int16_t *pcm, size_t sample_count);

// Check if crossfade is in progress
bool sx_audio_crossfade_is_active(void);

// Get current crossfade state
sx_audio_crossfade_state_t sx_audio_crossfade_get_state(void);

// Stop crossfade (abort current crossfade)
esp_err_t sx_audio_crossfade_stop(void);

// Enable/disable crossfade
esp_err_t sx_audio_crossfade_set_enabled(bool enabled);

// Get crossfade enabled state
bool sx_audio_crossfade_is_enabled(void);

// Set sample rate (called when audio sample rate changes)
void sx_audio_crossfade_set_sample_rate(uint32_t sample_rate);

#ifdef __cplusplus
}
#endif



