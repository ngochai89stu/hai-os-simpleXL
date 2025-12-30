#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: Audio Front-End (AFE) Service - AEC, VAD

// AFE configuration
typedef struct {
    bool enable_aec;        // Enable Acoustic Echo Cancellation
    bool enable_vad;        // Enable Voice Activity Detection
    bool enable_ns;         // Enable Noise Suppression
    bool enable_agc;        // Enable Automatic Gain Control
    uint32_t sample_rate_hz; // Sample rate (default: 16000)
} sx_audio_afe_config_t;

// VAD result callback
typedef void (*sx_afe_vad_cb_t)(bool voice_active, void *user_data);

// Initialize AFE service
esp_err_t sx_audio_afe_init(const sx_audio_afe_config_t *config);

// Process audio frame (AEC, VAD, NS, AGC)
// input: Input PCM samples (int16_t, mono)
// output: Output PCM samples (processed)
// sample_count: Number of samples
// Returns: true if voice activity detected (if VAD enabled)
bool sx_audio_afe_process(const int16_t *input, int16_t *output, size_t sample_count);

// Register VAD callback
esp_err_t sx_audio_afe_register_vad_callback(sx_afe_vad_cb_t callback, void *user_data);

// Get AEC status
bool sx_audio_afe_is_aec_enabled(void);

// Get VAD status
bool sx_audio_afe_is_vad_enabled(void);

// Reset AFE state
esp_err_t sx_audio_afe_reset(void);

#ifdef __cplusplus
}
#endif












