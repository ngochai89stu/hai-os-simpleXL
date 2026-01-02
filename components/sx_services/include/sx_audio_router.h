#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Audio Router Service
// Route audio between sources and external audio bridge

// Audio source types
typedef enum {
    SX_AUDIO_SOURCE_NONE = 0,
    SX_AUDIO_SOURCE_SD_MUSIC,
    SX_AUDIO_SOURCE_RADIO,
    SX_AUDIO_SOURCE_ONLINE_MUSIC,
    SX_AUDIO_SOURCE_TTS,
    SX_AUDIO_SOURCE_EXTERNAL,
    SX_AUDIO_SOURCE_COUNT,
} sx_audio_source_t;

// Audio sink types
typedef enum {
    SX_AUDIO_SINK_NONE = 0,
    SX_AUDIO_SINK_I2S,
    SX_AUDIO_SINK_EXTERNAL,
    SX_AUDIO_SINK_BOTH,
    SX_AUDIO_SINK_COUNT,
} sx_audio_sink_t;

// Routing configuration
typedef struct {
    sx_audio_source_t source;
    sx_audio_sink_t sink;
    bool enabled;
} sx_audio_route_t;

// Initialize audio router
esp_err_t sx_audio_router_init(void);

// Deinitialize audio router
void sx_audio_router_deinit(void);

// Set routing
// source: Audio source
// sink: Audio sink
// Returns: ESP_OK on success
esp_err_t sx_audio_router_set_route(sx_audio_source_t source, sx_audio_sink_t sink);

// Get current routing
// source: Audio source
// sink: Output sink
// Returns: ESP_OK on success
esp_err_t sx_audio_router_get_route(sx_audio_source_t source, sx_audio_sink_t *sink);

// Enable/disable route
// source: Audio source
// enabled: Enable or disable
// Returns: ESP_OK on success
esp_err_t sx_audio_router_enable_route(sx_audio_source_t source, bool enabled);

// Check if route is enabled
// source: Audio source
// Returns: true if enabled
bool sx_audio_router_is_route_enabled(sx_audio_source_t source);

// Route audio data
// source: Audio source
// pcm: PCM audio data
// samples: Number of samples
// sample_rate: Sample rate in Hz
// Returns: ESP_OK on success
esp_err_t sx_audio_router_route_audio(sx_audio_source_t source, const int16_t *pcm,
                                      size_t samples, uint32_t sample_rate);

// Get source name string
const char *sx_audio_router_source_name(sx_audio_source_t source);

// Get sink name string
const char *sx_audio_router_sink_name(sx_audio_sink_t sink);

#ifdef __cplusplus
}
#endif



















