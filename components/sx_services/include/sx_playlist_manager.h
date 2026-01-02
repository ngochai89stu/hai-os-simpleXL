#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: Playlist Manager - Track navigation and auto-play

// Playlist structure
typedef struct {
    char **track_paths;      // Array of track file paths
    size_t track_count;      // Number of tracks
    size_t current_index;    // Current playing track index
    bool shuffle;            // Shuffle mode
    bool repeat_all;         // Repeat all tracks
    bool repeat_one;         // Repeat current track
} sx_playlist_t;

// Initialize playlist manager
esp_err_t sx_playlist_manager_init(void);

// Create a new playlist from track paths
esp_err_t sx_playlist_create(const char **track_paths, size_t track_count, sx_playlist_t **out_playlist);

// Free playlist memory
void sx_playlist_free(sx_playlist_t *playlist);

// Set current playlist
esp_err_t sx_playlist_set_current(sx_playlist_t *playlist);

// Get current playlist
sx_playlist_t* sx_playlist_get_current(void);

// Play next track
esp_err_t sx_playlist_next(void);

// Play previous track
esp_err_t sx_playlist_previous(void);

// Play track at index
esp_err_t sx_playlist_play_index(size_t index);

// Get current track index
int sx_playlist_get_current_index(void);

// Get current track path
const char* sx_playlist_get_current_track(void);

// Set shuffle mode
esp_err_t sx_playlist_set_shuffle(bool enabled);

// Set repeat mode (repeat_all takes precedence over repeat_one)
esp_err_t sx_playlist_set_repeat(bool repeat_all, bool repeat_one);

// Check if should auto-play next track (called when current track ends)
bool sx_playlist_should_auto_play_next(void);

// Phase 5: Gapless Playback
// Preload next track for gapless playback
esp_err_t sx_playlist_preload_next(void);

// Check if next track is preloaded
bool sx_playlist_is_next_preloaded(void);

// Get preloaded track data (for gapless transition)
const char* sx_playlist_get_preloaded_track(void);

// Track Info (Phase 1: Hybrid Music Screen)
const char* sx_playlist_get_title(size_t track_index);
const char* sx_playlist_get_artist(size_t track_index);
const char* sx_playlist_get_genre(size_t track_index);
uint32_t sx_playlist_get_duration(size_t track_index);  // seconds
size_t sx_playlist_get_count(void);

// Album Art (Phase 1: Hybrid Music Screen)
esp_err_t sx_playlist_get_cover_path(size_t track_index, char *path, size_t path_len);

#ifdef __cplusplus
}
#endif

