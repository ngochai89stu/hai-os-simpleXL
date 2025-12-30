#pragma once

#include <esp_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 4: Radio service (online streaming via HTTP)

// Display modes for radio
typedef enum {
    SX_RADIO_DISPLAY_INFO = 0,      // Show station info (title, bitrate, etc.)
    SX_RADIO_DISPLAY_METADATA = 1,  // Show metadata only
} sx_radio_display_mode_t;

typedef struct {
    bool auto_reconnect;        // Enable auto-reconnect on connection loss
    uint32_t max_reconnect_attempts; // Max reconnect attempts (0 = unlimited)
    size_t buffer_size;         // Buffer size for streaming (default: 4096)
    size_t min_buffer_before_play; // Minimum buffer before starting playback (default: 2048)
} sx_radio_config_t;

// Radio metadata structure
typedef struct {
    char stream_title[256];
    char stream_url[512];
    char content_type[64];
    int bitrate;
    int sample_rate;
    int channels;
} sx_radio_metadata_t;

// Per-station volume boost (0-100, where 100 = no boost, >100 = amplification)
typedef struct {
    char station_key[32];  // Station identifier (e.g., "VOV3")
    uint8_t volume_boost;   // Volume boost percentage (100 = normal, 150 = 50% boost)
} sx_radio_station_volume_t;

esp_err_t sx_radio_service_init(const sx_radio_config_t *cfg);
esp_err_t sx_radio_service_start(void);
esp_err_t sx_radio_service_stop(void);

// Play radio stream from URL or station ID
// station_id can be:
// - Station key (e.g. "VOV3") - will lookup from station table
// - HTTP/HTTPS URL: "http://radio.example.com/stream"
esp_err_t sx_radio_play_station(const char *station_id);

// Play radio stream directly from URL
esp_err_t sx_radio_play_url(const char *url);

esp_err_t sx_radio_stop_playback(void);
esp_err_t sx_radio_pause(void);
esp_err_t sx_radio_resume(void);

bool sx_radio_is_playing(void);
bool sx_radio_is_paused(void);

// Get current stream URL
const char *sx_radio_get_current_url(void);

// Get current metadata (returns NULL if not available)
const sx_radio_metadata_t *sx_radio_get_metadata(void);

// Set display mode
esp_err_t sx_radio_set_display_mode(sx_radio_display_mode_t mode);

// Get current display mode
sx_radio_display_mode_t sx_radio_get_display_mode(void);

// Get buffer status (bytes buffered, buffer capacity)
esp_err_t sx_radio_get_buffer_status(size_t *buffered_bytes, size_t *buffer_capacity);

// Per-station volume boost management
// Set volume boost for a station (100 = normal, 150 = 50% boost, 200 = 100% boost)
esp_err_t sx_radio_set_station_volume_boost(const char *station_key, uint8_t volume_boost);

// Get volume boost for a station (returns 100 if not configured)
uint8_t sx_radio_get_station_volume_boost(const char *station_key);

#ifdef __cplusplus
}
#endif


