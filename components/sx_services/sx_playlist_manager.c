#include "sx_playlist_manager.h"
#include "sx_audio_service.h"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_media_metadata.h"
#include "sx_sd_service.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"  // Phase 2: xTaskGetTickCount for LRU
#include "freertos/semphr.h"
#include "cJSON.h"

static const char *TAG = "sx_playlist";

// Phase 5: Metadata cache structure with LRU support
typedef struct {
    char file_path[512];
    sx_track_meta_t meta;
    bool valid;
    uint32_t last_access_time;  // Phase 2: For LRU eviction (tick count)
} sx_track_meta_cache_t;

#define METADATA_CACHE_SIZE 32
static sx_track_meta_cache_t s_metadata_cache[METADATA_CACHE_SIZE];
static size_t s_cache_next_index = 0;

// Phase 5: Get metadata from cache or parse new
static sx_track_meta_t* get_track_metadata(const char *file_path) {
    if (file_path == NULL) {
        return NULL;
    }
    
    // Check cache first
    uint32_t current_time = xTaskGetTickCount();
    for (size_t i = 0; i < METADATA_CACHE_SIZE; i++) {
        if (s_metadata_cache[i].valid && strcmp(s_metadata_cache[i].file_path, file_path) == 0) {
            // Cache hit: update access time and return
            s_metadata_cache[i].last_access_time = current_time;
            return &s_metadata_cache[i].meta;
        }
    }
    
    // Not in cache, find LRU entry for eviction
    size_t lru_idx = 0;
    uint32_t lru_time = s_metadata_cache[0].valid ? s_metadata_cache[0].last_access_time : 0;
    bool found_invalid = false;
    
    // First, try to find an invalid entry
    for (size_t i = 0; i < METADATA_CACHE_SIZE; i++) {
        if (!s_metadata_cache[i].valid) {
            lru_idx = i;
            found_invalid = true;
            break;
        }
    }
    
    // If no invalid entry, find LRU (oldest access time)
    if (!found_invalid) {
        for (size_t i = 1; i < METADATA_CACHE_SIZE; i++) {
            if (s_metadata_cache[i].last_access_time < lru_time) {
                lru_time = s_metadata_cache[i].last_access_time;
                lru_idx = i;
            }
        }
    }
    
    sx_track_meta_cache_t *cache_entry = &s_metadata_cache[lru_idx];
    
    // Clear old entry
    memset(cache_entry, 0, sizeof(sx_track_meta_cache_t));
    
    // Parse metadata
    sx_track_meta_t meta;
    esp_err_t ret = sx_meta_parse_file(file_path, &meta);
    
    if (ret == ESP_OK && meta.has_metadata) {
        // Copy to cache
        strncpy(cache_entry->file_path, file_path, sizeof(cache_entry->file_path) - 1);
        cache_entry->meta = meta;
        cache_entry->valid = true;
        cache_entry->last_access_time = current_time;  // Phase 2: Set access time
        return &cache_entry->meta;
    } else {
        // Parse failed, try duration estimate
        meta.duration_ms = sx_meta_estimate_duration(file_path);
        if (meta.duration_ms > 0) {
            // At least we have duration estimate
            strncpy(cache_entry->file_path, file_path, sizeof(cache_entry->file_path) - 1);
            cache_entry->meta = meta;
            cache_entry->valid = true;
            cache_entry->last_access_time = current_time;  // Phase 2: Set access time
            return &cache_entry->meta;
        }
    }
    
    return NULL;
}

// Playlist manager state
static bool s_initialized = false;
static sx_playlist_t *s_current_playlist = NULL;
static SemaphoreHandle_t s_playlist_mutex = NULL;

// Gapless playback state
static bool s_next_preloaded = false;
static size_t s_preloaded_index = 0;
static char *s_preloaded_track_path = NULL;

esp_err_t sx_playlist_manager_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    s_playlist_mutex = xSemaphoreCreateMutex();
    if (s_playlist_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create playlist mutex");
        return ESP_ERR_NO_MEM;
    }
    
    s_initialized = true;
    s_current_playlist = NULL;
    
    ESP_LOGI(TAG, "Playlist manager initialized");
    return ESP_OK;
}

esp_err_t sx_playlist_create(const char **track_paths, size_t track_count, sx_playlist_t **out_playlist) {
    if (!s_initialized || track_paths == NULL || track_count == 0 || out_playlist == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    sx_playlist_t *playlist = (sx_playlist_t *)malloc(sizeof(sx_playlist_t));
    if (playlist == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    memset(playlist, 0, sizeof(sx_playlist_t));
    playlist->track_count = track_count;
    playlist->current_index = 0;
    playlist->shuffle = false;
    playlist->repeat_all = false;
    playlist->repeat_one = false;
    
    // Allocate array for track paths
    playlist->track_paths = (char **)malloc(track_count * sizeof(char *));
    if (playlist->track_paths == NULL) {
        free(playlist);
        return ESP_ERR_NO_MEM;
    }
    
    // Copy track paths
    for (size_t i = 0; i < track_count; i++) {
        if (track_paths[i] != NULL) {
            size_t path_len = strlen(track_paths[i]) + 1;
            playlist->track_paths[i] = (char *)malloc(path_len);
            if (playlist->track_paths[i] == NULL) {
                // Free already allocated paths
                for (size_t j = 0; j < i; j++) {
                    free(playlist->track_paths[j]);
                }
                free(playlist->track_paths);
                free(playlist);
                return ESP_ERR_NO_MEM;
            }
            strncpy(playlist->track_paths[i], track_paths[i], path_len - 1);
            playlist->track_paths[i][path_len - 1] = '\0';
        } else {
            playlist->track_paths[i] = NULL;
        }
    }
    
    *out_playlist = playlist;
    ESP_LOGI(TAG, "Playlist created with %zu tracks", track_count);
    return ESP_OK;
}

void sx_playlist_free(sx_playlist_t *playlist) {
    if (playlist == NULL) {
        return;
    }
    
    if (playlist->track_paths != NULL) {
        for (size_t i = 0; i < playlist->track_count; i++) {
            if (playlist->track_paths[i] != NULL) {
                free(playlist->track_paths[i]);
            }
        }
        free(playlist->track_paths);
    }
    
    free(playlist);
}

esp_err_t sx_playlist_set_current(sx_playlist_t *playlist) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_playlist_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Free old playlist if exists
    if (s_current_playlist != NULL) {
        sx_playlist_free(s_current_playlist);
    }
    
    s_current_playlist = playlist;
    
    xSemaphoreGive(s_playlist_mutex);
    
    ESP_LOGI(TAG, "Current playlist set (%zu tracks)", playlist ? playlist->track_count : 0);
    return ESP_OK;
}

sx_playlist_t* sx_playlist_get_current(void) {
    return s_current_playlist;
}

static size_t get_next_index(sx_playlist_t *playlist) {
    if (playlist == NULL || playlist->track_count == 0) {
        return 0;
    }
    
    if (playlist->repeat_one) {
        return playlist->current_index; // Repeat current track
    }
    
    if (playlist->shuffle) {
        // Simple shuffle: random index
        return (size_t)(rand() % playlist->track_count);
    }
    
    // Normal: next index
    size_t next = playlist->current_index + 1;
    if (next >= playlist->track_count) {
        if (playlist->repeat_all) {
            next = 0; // Loop to beginning
        } else {
            next = playlist->track_count; // End of playlist
        }
    }
    
    return next;
}

static size_t get_previous_index(sx_playlist_t *playlist) {
    if (playlist == NULL || playlist->track_count == 0) {
        return 0;
    }
    
    if (playlist->repeat_one) {
        return playlist->current_index; // Repeat current track
    }
    
    if (playlist->shuffle) {
        // Simple shuffle: random index
        return (size_t)(rand() % playlist->track_count);
    }
    
    // Normal: previous index
    if (playlist->current_index == 0) {
        if (playlist->repeat_all) {
            return playlist->track_count - 1; // Loop to end
        } else {
            return 0; // Stay at beginning
        }
    }
    
    return playlist->current_index - 1;
}

esp_err_t sx_playlist_next(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_playlist_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (s_current_playlist == NULL || s_current_playlist->track_count == 0) {
        xSemaphoreGive(s_playlist_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    size_t next_index = get_next_index(s_current_playlist);
    if (next_index >= s_current_playlist->track_count) {
        // End of playlist
        xSemaphoreGive(s_playlist_mutex);
        ESP_LOGI(TAG, "End of playlist reached");
        return ESP_ERR_NOT_FOUND;
    }
    
    s_current_playlist->current_index = next_index;
    const char *track_path = s_current_playlist->track_paths[next_index];
    
    xSemaphoreGive(s_playlist_mutex);
    
    // Phase 2: Gapless playback - check if this is the preloaded track
    bool is_preloaded = (s_next_preloaded && s_preloaded_track_path != NULL && 
                         strcmp(track_path, s_preloaded_track_path) == 0);
    
    // Play the track
    esp_err_t ret = sx_audio_play_file(track_path);
    if (ret == ESP_OK) {
        if (is_preloaded) {
            ESP_LOGI(TAG, "Gapless: Playing preloaded track: %s", track_path);
            // Clear preload state since we're now playing it
            if (xSemaphoreTake(s_playlist_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                s_next_preloaded = false;
                xSemaphoreGive(s_playlist_mutex);
            }
        } else {
            ESP_LOGI(TAG, "Playing next track: %s", track_path);
        }
    } else {
        ESP_LOGE(TAG, "Failed to play track: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t sx_playlist_previous(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_playlist_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (s_current_playlist == NULL || s_current_playlist->track_count == 0) {
        xSemaphoreGive(s_playlist_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    size_t prev_index = get_previous_index(s_current_playlist);
    s_current_playlist->current_index = prev_index;
    const char *track_path = s_current_playlist->track_paths[prev_index];
    
    xSemaphoreGive(s_playlist_mutex);
    
    // Play the track
    esp_err_t ret = sx_audio_play_file(track_path);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Playing previous track: %s", track_path);
    } else {
        ESP_LOGE(TAG, "Failed to play track: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t sx_playlist_play_index(size_t index) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_playlist_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (s_current_playlist == NULL || index >= s_current_playlist->track_count) {
        xSemaphoreGive(s_playlist_mutex);
        return ESP_ERR_INVALID_ARG;
    }
    
    s_current_playlist->current_index = index;
    const char *track_path = s_current_playlist->track_paths[index];
    
    xSemaphoreGive(s_playlist_mutex);
    
    // Play the track
    esp_err_t ret = sx_audio_play_file(track_path);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Playing track at index %zu: %s", index, track_path);
    } else {
        ESP_LOGE(TAG, "Failed to play track: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

int sx_playlist_get_current_index(void) {
    if (!s_initialized || s_current_playlist == NULL) {
        return -1;
    }
    
    return (int)s_current_playlist->current_index;
}

const char* sx_playlist_get_current_track(void) {
    if (!s_initialized || s_current_playlist == NULL) {
        return NULL;
    }
    
    if (s_current_playlist->current_index >= s_current_playlist->track_count) {
        return NULL;
    }
    
    return s_current_playlist->track_paths[s_current_playlist->current_index];
}

esp_err_t sx_playlist_set_shuffle(bool enabled) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_playlist_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (s_current_playlist != NULL) {
        s_current_playlist->shuffle = enabled;
    }
    
    xSemaphoreGive(s_playlist_mutex);
    
    ESP_LOGI(TAG, "Shuffle mode %s", enabled ? "enabled" : "disabled");
    return ESP_OK;
}

esp_err_t sx_playlist_set_repeat(bool repeat_all, bool repeat_one) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_playlist_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (s_current_playlist != NULL) {
        s_current_playlist->repeat_all = repeat_all;
        s_current_playlist->repeat_one = repeat_one;
    }
    
    xSemaphoreGive(s_playlist_mutex);
    
    ESP_LOGI(TAG, "Repeat mode: all=%d, one=%d", repeat_all, repeat_one);
    return ESP_OK;
}

bool sx_playlist_should_auto_play_next(void) {
    if (!s_initialized || s_current_playlist == NULL) {
        return false;
    }
    
    // Auto-play if repeat is enabled or not at end of playlist
    if (s_current_playlist->repeat_one || s_current_playlist->repeat_all) {
        return true;
    }
    
    size_t next_index = get_next_index(s_current_playlist);
    return (next_index < s_current_playlist->track_count);
}

// Phase 5: Gapless Playback
// Preload next track for gapless playback
esp_err_t sx_playlist_preload_next(void) {
    if (!s_initialized || s_current_playlist == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_playlist_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Clear previous preload
    if (s_preloaded_track_path != NULL) {
        free(s_preloaded_track_path);
        s_preloaded_track_path = NULL;
    }
    s_next_preloaded = false;
    
    // Calculate next track index
    size_t next_index = s_current_playlist->current_index;
    
    if (s_current_playlist->repeat_one) {
        // Repeat current track
        next_index = s_current_playlist->current_index;
    } else if (s_current_playlist->shuffle) {
        // Shuffle mode - pick random next track
        if (s_current_playlist->track_count > 1) {
            do {
                next_index = rand() % s_current_playlist->track_count;
            } while (next_index == s_current_playlist->current_index && s_current_playlist->track_count > 1);
        }
    } else {
        // Normal mode - next track
        next_index = s_current_playlist->current_index + 1;
        if (next_index >= s_current_playlist->track_count) {
            if (s_current_playlist->repeat_all) {
                next_index = 0;  // Loop to start
            } else {
                // No next track
                xSemaphoreGive(s_playlist_mutex);
                return ESP_OK;  // No error, just no next track
            }
        }
    }
    
    // Get next track path
    if (next_index < s_current_playlist->track_count && 
        s_current_playlist->track_paths[next_index] != NULL) {
        size_t path_len = strlen(s_current_playlist->track_paths[next_index]) + 1;
        s_preloaded_track_path = (char *)malloc(path_len);
        if (s_preloaded_track_path != NULL) {
            strncpy(s_preloaded_track_path, s_current_playlist->track_paths[next_index], path_len - 1);
            s_preloaded_track_path[path_len - 1] = '\0';
            s_preloaded_index = next_index;
            s_next_preloaded = true;
            ESP_LOGI(TAG, "Preloaded next track: %s (index %zu)", s_preloaded_track_path, next_index);
            
            // Future: Preload audio data into buffer for gapless playback
            // This would require:
            // - Audio service integration to load and decode the next track
            // - Buffer management for pre-decoded audio data
            // - Coordination between playlist and audio service
            // For now, we just store the track path to minimize delay
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory for preloaded track path");
            xSemaphoreGive(s_playlist_mutex);
            return ESP_ERR_NO_MEM;
        }
    }
    
    xSemaphoreGive(s_playlist_mutex);
    return ESP_OK;
}

// Check if next track is preloaded
bool sx_playlist_is_next_preloaded(void) {
    if (!s_initialized || s_current_playlist == NULL) {
        return false;
    }
    
    if (xSemaphoreTake(s_playlist_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    
    bool preloaded = s_next_preloaded && (s_preloaded_track_path != NULL);
    xSemaphoreGive(s_playlist_mutex);
    
    return preloaded;
}

// Get preloaded track data (for gapless transition)
const char* sx_playlist_get_preloaded_track(void) {
    if (!s_initialized || !s_next_preloaded) {
        return NULL;
    }
    
    if (xSemaphoreTake(s_playlist_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return NULL;
    }
    
    const char *track_path = s_preloaded_track_path;
    xSemaphoreGive(s_playlist_mutex);
    
    return track_path;
}

// Track Info (Phase 1: Hybrid Music Screen)
// Helper functions removed - not currently used
// May be needed in future for track title extraction

size_t sx_playlist_get_count(void) {
    if (!s_initialized || s_current_playlist == NULL) {
        return 0;
    }
    
    if (xSemaphoreTake(s_playlist_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return 0;
    }
    
    size_t count = s_current_playlist->track_count;
    xSemaphoreGive(s_playlist_mutex);
    
    return count;
}

const char* sx_playlist_get_title(size_t track_index) {
    if (!s_initialized || s_current_playlist == NULL) {
        return "Unknown Title";
    }
    
    if (xSemaphoreTake(s_playlist_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return "Unknown Title";
    }
    
    if (track_index >= s_current_playlist->track_count || 
        s_current_playlist->track_paths[track_index] == NULL) {
        xSemaphoreGive(s_playlist_mutex);
        return "Unknown Title";
    }
    
    // Phase 5: Get metadata from cache or parse
    sx_track_meta_t *meta = get_track_metadata(s_current_playlist->track_paths[track_index]);
    
    static char title_buf[256];
    if (meta != NULL && meta->has_metadata && strlen(meta->title) > 0) {
        strncpy(title_buf, meta->title, sizeof(title_buf) - 1);
        title_buf[sizeof(title_buf) - 1] = '\0';
    } else {
        // Fallback: extract from filename
        const char *filename = strrchr(s_current_playlist->track_paths[track_index], '/');
        if (filename == NULL) {
            filename = strrchr(s_current_playlist->track_paths[track_index], '\\');
        }
        if (filename == NULL) {
            filename = s_current_playlist->track_paths[track_index];
        } else {
            filename++; // Skip slash
        }
        
        // Remove extension
        strncpy(title_buf, filename, sizeof(title_buf) - 1);
        char *dot = strrchr(title_buf, '.');
        if (dot != NULL) {
            *dot = '\0';
        }
    }
    
    xSemaphoreGive(s_playlist_mutex);
    return title_buf;
}

const char* sx_playlist_get_artist(size_t track_index) {
    if (!s_initialized || s_current_playlist == NULL) {
        return "Unknown Artist";
    }
    
    if (xSemaphoreTake(s_playlist_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return "Unknown Artist";
    }
    
    if (track_index >= s_current_playlist->track_count || 
        s_current_playlist->track_paths[track_index] == NULL) {
        xSemaphoreGive(s_playlist_mutex);
        return "Unknown Artist";
    }
    
    // Phase 5: Get metadata from cache or parse
    sx_track_meta_t *meta = get_track_metadata(s_current_playlist->track_paths[track_index]);
    
    static char artist_buf[256];
    if (meta != NULL && meta->has_metadata && strlen(meta->artist) > 0) {
        strncpy(artist_buf, meta->artist, sizeof(artist_buf) - 1);
        artist_buf[sizeof(artist_buf) - 1] = '\0';
    } else {
        strncpy(artist_buf, "Unknown Artist", sizeof(artist_buf) - 1);
    }
    
    xSemaphoreGive(s_playlist_mutex);
    return artist_buf;
}

const char* sx_playlist_get_genre(size_t track_index) {
    if (!s_initialized || s_current_playlist == NULL) {
        return "Unknown Genre";
    }
    
    if (xSemaphoreTake(s_playlist_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return "Unknown Genre";
    }
    
    if (track_index >= s_current_playlist->track_count || 
        s_current_playlist->track_paths[track_index] == NULL) {
        xSemaphoreGive(s_playlist_mutex);
        return "Unknown Genre";
    }
    
    // Phase 5: Get metadata from cache or parse
    sx_track_meta_t *meta = get_track_metadata(s_current_playlist->track_paths[track_index]);
    
    static char genre_buf[64];
    if (meta != NULL && meta->has_metadata && strlen(meta->genre) > 0) {
        strncpy(genre_buf, meta->genre, sizeof(genre_buf) - 1);
        genre_buf[sizeof(genre_buf) - 1] = '\0';
    } else {
        strncpy(genre_buf, "Unknown Genre", sizeof(genre_buf) - 1);
    }
    
    xSemaphoreGive(s_playlist_mutex);
    return genre_buf;
}

uint32_t sx_playlist_get_duration(size_t track_index) {
    if (!s_initialized || s_current_playlist == NULL) {
        return 0;
    }
    
    if (xSemaphoreTake(s_playlist_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return 0;
    }
    
    if (track_index >= s_current_playlist->track_count || 
        s_current_playlist->track_paths[track_index] == NULL) {
        xSemaphoreGive(s_playlist_mutex);
        return 0;
    }
    
    // Phase 5: Get metadata from cache or parse
    sx_track_meta_t *meta = get_track_metadata(s_current_playlist->track_paths[track_index]);
    
    uint32_t duration_sec = 0;
    if (meta != NULL && meta->duration_ms > 0) {
        duration_sec = meta->duration_ms / 1000; // Convert ms to seconds
    }
    
    xSemaphoreGive(s_playlist_mutex);
    return duration_sec;
}

esp_err_t sx_playlist_get_cover_path(size_t track_index, char *path, size_t path_len) {
    if (!s_initialized || s_current_playlist == NULL || path == NULL || path_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_playlist_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (track_index >= s_current_playlist->track_count || 
        s_current_playlist->track_paths[track_index] == NULL) {
        xSemaphoreGive(s_playlist_mutex);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Phase 5: Get metadata from cache first (may have cover_hint)
    sx_track_meta_t *meta = get_track_metadata(s_current_playlist->track_paths[track_index]);
    if (meta != NULL && meta->has_metadata && strlen(meta->cover_hint) > 0) {
        strncpy(path, meta->cover_hint, path_len - 1);
        path[path_len - 1] = '\0';
        xSemaphoreGive(s_playlist_mutex);
        return ESP_OK;
    }
    
    // Phase 5: Search for cover image in directory
    esp_err_t ret = sx_meta_find_cover(s_current_playlist->track_paths[track_index], path, path_len);
    
    xSemaphoreGive(s_playlist_mutex);
    return ret;
}

// Phase 1: Save playlist to JSON file on SD card
esp_err_t sx_playlist_save_to_file(const char *file_path) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (file_path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_playlist_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (s_current_playlist == NULL) {
        xSemaphoreGive(s_playlist_mutex);
        ESP_LOGW(TAG, "No playlist to save");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Create JSON object
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        xSemaphoreGive(s_playlist_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Create tracks array
    cJSON *tracks_array = cJSON_CreateArray();
    if (tracks_array == NULL) {
        cJSON_Delete(json);
        xSemaphoreGive(s_playlist_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    for (size_t i = 0; i < s_current_playlist->track_count; i++) {
        if (s_current_playlist->track_paths[i] != NULL) {
            cJSON *track_item = cJSON_CreateString(s_current_playlist->track_paths[i]);
            if (track_item != NULL) {
                cJSON_AddItemToArray(tracks_array, track_item);
            }
        }
    }
    
    cJSON_AddItemToObject(json, "tracks", tracks_array);
    cJSON_AddNumberToObject(json, "current_index", s_current_playlist->current_index);
    cJSON_AddBoolToObject(json, "shuffle", s_current_playlist->shuffle);
    cJSON_AddBoolToObject(json, "repeat_all", s_current_playlist->repeat_all);
    cJSON_AddBoolToObject(json, "repeat_one", s_current_playlist->repeat_one);
    
    // Convert to string
    char *json_string = cJSON_Print(json);
    if (json_string == NULL) {
        cJSON_Delete(json);
        xSemaphoreGive(s_playlist_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    size_t json_len = strlen(json_string);
    size_t written = 0;
    esp_err_t ret = sx_sd_write_file(file_path, json_string, json_len, &written);
    
    free(json_string);
    cJSON_Delete(json);
    xSemaphoreGive(s_playlist_mutex);
    
    if (ret == ESP_OK && written == json_len) {
        ESP_LOGI(TAG, "Playlist saved to %s (%zu bytes, %zu tracks)", file_path, written, s_current_playlist->track_count);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to save playlist: %s (written %zu/%zu)", esp_err_to_name(ret), written, json_len);
        return ESP_FAIL;
    }
}

// Phase 1: Load playlist from JSON file on SD card
esp_err_t sx_playlist_load_from_file(const char *file_path) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (file_path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Read file from SD card
    size_t file_size = 0;
    esp_err_t ret = sx_sd_get_file_size(file_path, &file_size);
    if (ret != ESP_OK || file_size == 0) {
        ESP_LOGE(TAG, "Failed to get file size for %s", file_path);
        return ESP_FAIL;
    }
    
    // Allocate buffer for JSON
    char *json_buffer = (char *)malloc(file_size + 1);
    if (json_buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    size_t read = 0;
    ret = sx_sd_read_file(file_path, json_buffer, file_size, &read);
    if (ret != ESP_OK || read != file_size) {
        free(json_buffer);
        ESP_LOGE(TAG, "Failed to read playlist file: %s (read %zu/%zu)", esp_err_to_name(ret), read, file_size);
        return ESP_FAIL;
    }
    
    json_buffer[file_size] = '\0';
    
    // Parse JSON
    cJSON *json = cJSON_Parse(json_buffer);
    free(json_buffer);
    
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON from %s", file_path);
        return ESP_FAIL;
    }
    
    // Extract tracks array
    cJSON *tracks_array = cJSON_GetObjectItem(json, "tracks");
    if (tracks_array == NULL || !cJSON_IsArray(tracks_array)) {
        cJSON_Delete(json);
        ESP_LOGE(TAG, "Invalid JSON: missing or invalid tracks array");
        return ESP_ERR_INVALID_ARG;
    }
    
    size_t track_count = cJSON_GetArraySize(tracks_array);
    if (track_count == 0) {
        cJSON_Delete(json);
        ESP_LOGW(TAG, "Playlist file is empty");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Allocate track paths array
    const char **track_paths = (const char **)malloc(track_count * sizeof(const char *));
    if (track_paths == NULL) {
        cJSON_Delete(json);
        return ESP_ERR_NO_MEM;
    }
    
    // Extract track paths
    for (size_t i = 0; i < track_count; i++) {
        cJSON *track_item = cJSON_GetArrayItem(tracks_array, i);
        if (track_item != NULL && cJSON_IsString(track_item)) {
            track_paths[i] = track_item->valuestring;
        } else {
            track_paths[i] = NULL;
        }
    }
    
    // Extract other properties
    cJSON *current_index_item = cJSON_GetObjectItem(json, "current_index");
    size_t current_index = (current_index_item != NULL && cJSON_IsNumber(current_index_item)) 
                          ? (size_t)cJSON_GetNumberValue(current_index_item) : 0;
    
    cJSON *shuffle_item = cJSON_GetObjectItem(json, "shuffle");
    bool shuffle = (shuffle_item != NULL && cJSON_IsBool(shuffle_item)) && cJSON_IsTrue(shuffle_item);
    
    cJSON *repeat_all_item = cJSON_GetObjectItem(json, "repeat_all");
    bool repeat_all = (repeat_all_item != NULL && cJSON_IsBool(repeat_all_item)) && cJSON_IsTrue(repeat_all_item);
    
    cJSON *repeat_one_item = cJSON_GetObjectItem(json, "repeat_one");
    bool repeat_one = (repeat_one_item != NULL && cJSON_IsBool(repeat_one_item)) && cJSON_IsTrue(repeat_one_item);
    
    cJSON_Delete(json);
    
    // Create playlist
    sx_playlist_t *playlist = NULL;
    ret = sx_playlist_create(track_paths, track_count, &playlist);
    free(track_paths);
    
    if (ret != ESP_OK || playlist == NULL) {
        ESP_LOGE(TAG, "Failed to create playlist from loaded data");
        return ret;
    }
    
    // Set properties
    if (xSemaphoreTake(s_playlist_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        sx_playlist_free(playlist);
        return ESP_ERR_TIMEOUT;
    }
    
    playlist->current_index = (current_index < track_count) ? current_index : 0;
    playlist->shuffle = shuffle;
    playlist->repeat_all = repeat_all;
    playlist->repeat_one = repeat_one;
    
    // Set as current playlist
    if (s_current_playlist != NULL) {
        sx_playlist_free(s_current_playlist);
    }
    s_current_playlist = playlist;
    
    xSemaphoreGive(s_playlist_mutex);
    
    ESP_LOGI(TAG, "Playlist loaded from %s (%zu tracks, index %zu)", file_path, track_count, current_index);
    return ESP_OK;
}

