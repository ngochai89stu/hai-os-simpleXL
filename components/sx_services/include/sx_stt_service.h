#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: STT (Speech-to-Text) Service

// STT configuration
typedef struct {
    const char *endpoint_url;      // STT endpoint URL (e.g., "https://api.example.com/stt")
    const char *api_key;           // API key (NULL if not required)
    uint32_t chunk_duration_ms;    // Audio chunk duration in milliseconds (default: 1000ms)
    uint32_t sample_rate_hz;       // Audio sample rate (default: 16000)
    bool auto_send;                // Auto-send chunks when ready (default: true)
} sx_stt_config_t;

// STT result callback
typedef void (*sx_stt_result_cb_t)(const char *transcript, bool is_final, void *user_data);

// Initialize STT service
esp_err_t sx_stt_service_init(const sx_stt_config_t *config);

// Start STT session
esp_err_t sx_stt_start_session(sx_stt_result_cb_t callback, void *user_data);

// Stop STT session
esp_err_t sx_stt_stop_session(void);

// Send audio chunk to STT endpoint
esp_err_t sx_stt_send_audio_chunk(const int16_t *pcm, size_t sample_count);

// Check if STT is active
bool sx_stt_is_active(void);

// Get last error message
const char* sx_stt_get_last_error(void);

#ifdef __cplusplus
}
#endif












