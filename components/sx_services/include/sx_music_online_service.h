#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_http_client.h"

#ifdef __cplusplus
extern "C" {
#endif

// Display modes for music online
typedef enum {
    SX_MUSIC_ONLINE_DISPLAY_LYRICS = 0,  // Show lyrics
    SX_MUSIC_ONLINE_DISPLAY_INFO = 1,     // Show track info
} sx_music_online_display_mode_t;

// Music online track info
typedef struct {
    char title[128];
    char artist[128];
    char album[128];
    char url[256];
    uint32_t duration_ms;
    bool has_lyrics;
} sx_music_online_track_t;

// Lyrics structure
typedef struct {
    char *text;           // Lyrics text (allocated)
    size_t text_len;      // Length of lyrics text
    bool synced;          // Whether lyrics are time-synced
    uint32_t *timestamps; // Timestamps for synced lyrics (if synced)
    size_t line_count;    // Number of lines
} sx_music_online_lyrics_t;

// Initialize music online service
esp_err_t sx_music_online_service_init(void);

// Set display mode
esp_err_t sx_music_online_set_display_mode(sx_music_online_display_mode_t mode);

// Get current display mode
sx_music_online_display_mode_t sx_music_online_get_display_mode(void);

// Download lyrics for a track
esp_err_t sx_music_online_download_lyrics(const char *track_title, const char *artist, 
                                          sx_music_online_lyrics_t *lyrics);

// Free lyrics memory
void sx_music_online_free_lyrics(sx_music_online_lyrics_t *lyrics);

// Get current playing track info
const sx_music_online_track_t* sx_music_online_get_current_track(void);

// Set current track
esp_err_t sx_music_online_set_current_track(const sx_music_online_track_t *track);

// Authentication configuration
typedef struct {
    bool enable_auth;        // Enable authentication headers
    const char *auth_key;    // SHA256 key for authentication (from settings)
} sx_music_online_auth_config_t;

// Set authentication configuration
esp_err_t sx_music_online_set_auth_config(const sx_music_online_auth_config_t *config);

// Add authentication headers to HTTP client
// This adds: X-MAC, X-Chip-ID, X-Timestamp, X-Signature headers
esp_err_t sx_music_online_add_auth_headers(esp_http_client_handle_t client);

// Play online music song
// song_name: Name of the song
// artist_name: Name of the artist (optional)
// Returns: ESP_OK on success
esp_err_t sx_music_online_play_song(const char *song_name, const char *artist_name);

#ifdef __cplusplus
}
#endif
