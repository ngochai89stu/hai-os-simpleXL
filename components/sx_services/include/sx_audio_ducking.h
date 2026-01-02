#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: Audio Ducking Manager - Lower volume when Assistant speaks

// Configuration for audio ducking
typedef struct {
    float duck_level;      // Volume level when ducked (0.0 = mute, 1.0 = no ducking), default 0.3
    uint32_t fade_duration_ms; // Fade duration in milliseconds, default 200ms
} sx_audio_ducking_config_t;

// Initialize audio ducking manager
esp_err_t sx_audio_ducking_init(const sx_audio_ducking_config_t *config);

// Duck audio (lower volume when Assistant is speaking)
// Returns ESP_OK on success
esp_err_t sx_audio_duck(void);

// Restore audio (restore volume after Assistant finishes speaking)
// Returns ESP_OK on success
esp_err_t sx_audio_restore(void);

// Check if audio is currently ducked
bool sx_audio_is_ducked(void);

// Set duck level (0.0 = mute, 1.0 = no ducking)
esp_err_t sx_audio_ducking_set_level(float duck_level);

// Get current duck level
float sx_audio_ducking_get_level(void);

#ifdef __cplusplus
}
#endif




















