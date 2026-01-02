#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: Wake Word Detection Service

// Wake word types
typedef enum {
    SX_WAKE_WORD_TYPE_ESP_SR = 0,  // ESP-SR wake word (e.g., "Hi, Lexin")
    SX_WAKE_WORD_TYPE_CUSTOM = 1,  // Custom wake word model
} sx_wake_word_type_t;

// Wake word configuration
typedef struct {
    sx_wake_word_type_t type;      // Wake word type
    const char *model_path;         // Path to wake word model (for custom)
    float threshold;                // Detection threshold (0.0-1.0)
    bool enable_opus_encoding;      // Enable Opus encoding for wake word packets
} sx_wake_word_config_t;

// Wake word detected callback
typedef void (*sx_wake_word_detected_cb_t)(void *user_data);

// Initialize wake word service
esp_err_t sx_wake_word_service_init(const sx_wake_word_config_t *config);

// Start wake word detection
esp_err_t sx_wake_word_start(sx_wake_word_detected_cb_t callback, void *user_data);

// Stop wake word detection
esp_err_t sx_wake_word_stop(void);

// Check if wake word detection is active
bool sx_wake_word_is_active(void);

// Feed audio data for wake word detection
esp_err_t sx_wake_word_feed_audio(const int16_t *pcm, size_t sample_count);

#ifdef __cplusplus
}
#endif




















