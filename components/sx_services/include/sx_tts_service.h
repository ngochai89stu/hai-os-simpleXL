#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// TTS (Text-to-Speech) Service
// Server-based TTS with priority queue support

// TTS priority levels
typedef enum {
    SX_TTS_PRIORITY_LOW = 0,
    SX_TTS_PRIORITY_NORMAL = 1,
    SX_TTS_PRIORITY_HIGH = 2,
    SX_TTS_PRIORITY_URGENT = 3,
} sx_tts_priority_t;

// TTS configuration
typedef struct {
    const char *endpoint_url;  // TTS API endpoint URL
    const char *api_key;       // API key (optional)
    uint32_t timeout_ms;       // Request timeout in milliseconds
    sx_tts_priority_t default_priority; // Default priority for requests
} sx_tts_config_t;

// TTS request callback
// Called when TTS audio data is received
// audio_data: PCM audio data
// audio_size: Size of audio data in bytes
// sample_rate: Sample rate in Hz
// user_data: User-provided data
typedef void (*sx_tts_audio_callback_t)(const int16_t *audio_data, size_t audio_size,
                                         uint32_t sample_rate, void *user_data);

// TTS error callback
// error: Error code
// error_msg: Error message
// user_data: User-provided data
typedef void (*sx_tts_error_callback_t)(esp_err_t error, const char *error_msg, void *user_data);

// Initialize TTS service
// config: TTS configuration (can be NULL for defaults)
// Returns: ESP_OK on success
esp_err_t sx_tts_service_init(const sx_tts_config_t *config);

// Deinitialize TTS service
void sx_tts_service_deinit(void);

// Request TTS synthesis
// text: Text to synthesize
// priority: Request priority
// audio_callback: Callback for audio data (can be NULL to use default playback)
// error_callback: Callback for errors (optional)
// user_data: User data passed to callbacks
// Returns: ESP_OK on success
esp_err_t sx_tts_speak(const char *text, sx_tts_priority_t priority,
                      sx_tts_audio_callback_t audio_callback,
                      sx_tts_error_callback_t error_callback,
                      void *user_data);

// Request TTS synthesis with default priority
// text: Text to synthesize
// Returns: ESP_OK on success
esp_err_t sx_tts_speak_simple(const char *text);

// Cancel current TTS request
// Returns: ESP_OK on success
esp_err_t sx_tts_cancel(void);

// Check if TTS is currently speaking
bool sx_tts_is_speaking(void);

// Set TTS configuration
// config: New configuration
// Returns: ESP_OK on success
esp_err_t sx_tts_set_config(const sx_tts_config_t *config);

// Get current TTS configuration
// config: Output configuration
// Returns: ESP_OK on success
esp_err_t sx_tts_get_config(sx_tts_config_t *config);

#ifdef __cplusplus
}
#endif



















