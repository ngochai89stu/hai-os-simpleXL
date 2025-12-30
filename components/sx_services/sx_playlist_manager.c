#include "sx_playlist_manager.h"
#include "sx_audio_service.h"
#include "sx_dispatcher.h"
#include "sx_event.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "sx_playlist";

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
            strcpy(playlist->track_paths[i], track_paths[i]);
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
    
    // Play the track
    esp_err_t ret = sx_audio_play_file(track_path);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Playing next track: %s", track_path);
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
            strcpy(s_preloaded_track_path, s_current_playlist->track_paths[next_index]);
            s_preloaded_index = next_index;
            s_next_preloaded = true;
            ESP_LOGI(TAG, "Preloaded next track: %s (index %zu)", s_preloaded_track_path, next_index);
            
            // TODO: Actually preload audio data into buffer for gapless playback
            // This would require audio service integration to load and decode the next track
            // For now, we just store the track path
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

