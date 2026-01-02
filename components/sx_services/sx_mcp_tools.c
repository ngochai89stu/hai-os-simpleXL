#include "sx_mcp_tools.h"
#include "sx_mcp_server.h"

// Export tool functions for registration
#include "sx_audio_service.h"
#include "sx_sd_service.h"
#include "sx_sd_music_service.h"
#include "sx_music_online_service.h"
#include "sx_playlist_manager.h"
#include "sx_ota_service.h"
#include "sx_platform.h"
#include "sx_weather_service.h"
#include "sx_wifi_service.h"
#include "sx_display_service.h"
#include "sx_image_service.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "cJSON.h"
#include "esp_system.h"
#include "esp_chip_info.h"

static const char *TAG = "sx_mcp_tools";

// Helper: Create JSON-RPC 2.0 response
static cJSON* create_response(const char *id, cJSON *result, cJSON *error) {
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    if (id) {
        cJSON_AddStringToObject(response, "id", id);
    }
    if (result) {
        cJSON_AddItemToObject(response, "result", result);
    }
    if (error) {
        cJSON_AddItemToObject(response, "error", error);
    }
    return response;
}

// Helper: Create error response (exported)
cJSON* mcp_tool_create_error(const char *id, int code, const char *message) {
    cJSON *error = cJSON_CreateObject();
    cJSON_AddNumberToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message);
    return create_response(id, NULL, error);
}

// Helper: Create success response (exported)
cJSON* mcp_tool_create_success(const char *id, cJSON *result) {
    return create_response(id, result, NULL);
}

// SD Music MCP Tools (exported for registration)
cJSON* mcp_tool_sdmusic_playback(cJSON *params, const char *id) {
    cJSON *action = cJSON_GetObjectItem(params, "action");
    if (!action || !cJSON_IsString(action)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: action required");
    }
    
    const char *action_str = action->valuestring;
    esp_err_t ret = ESP_OK;
    
    if (strcmp(action_str, "play") == 0) {
        cJSON *track = cJSON_GetObjectItem(params, "track");
        if (track && cJSON_IsString(track)) {
            ret = sx_audio_play_file(track->valuestring);
        } else {
            // Play current track or first track
            const char *current = sx_playlist_get_current_track();
            if (current) {
                ret = sx_audio_play_file(current);
            }
        }
    } else if (strcmp(action_str, "pause") == 0) {
        ret = sx_audio_pause();
    } else if (strcmp(action_str, "resume") == 0) {
        ret = sx_audio_resume();
    } else if (strcmp(action_str, "stop") == 0) {
        ret = sx_audio_stop();
    } else if (strcmp(action_str, "next") == 0) {
        ret = sx_playlist_next();
    } else if (strcmp(action_str, "prev") == 0) {
        ret = sx_playlist_previous();
    } else {
        return mcp_tool_create_error(id, -32602, "Invalid action");
    }
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    if (ret != ESP_OK) {
        cJSON_AddStringToObject(result, "error", esp_err_to_name(ret));
    }
    return mcp_tool_create_success(id, result);
}

cJSON* mcp_tool_sdmusic_mode(cJSON *params, const char *id) {
    cJSON *mode = cJSON_GetObjectItem(params, "mode");
    if (!mode || !cJSON_IsString(mode)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: mode required");
    }
    
    const char *mode_str = mode->valuestring;
    esp_err_t ret = ESP_OK;
    
    if (strcmp(mode_str, "shuffle") == 0) {
        cJSON *enabled = cJSON_GetObjectItem(params, "enabled");
        if (enabled && cJSON_IsBool(enabled)) {
            ret = sx_playlist_set_shuffle(cJSON_IsTrue(enabled));
        }
    } else if (strcmp(mode_str, "repeat") == 0) {
        cJSON *repeat_all = cJSON_GetObjectItem(params, "repeat_all");
        cJSON *repeat_one = cJSON_GetObjectItem(params, "repeat_one");
        bool all = repeat_all && cJSON_IsTrue(repeat_all);
        bool one = repeat_one && cJSON_IsTrue(repeat_one);
        ret = sx_playlist_set_repeat(all, one);
    } else {
        return mcp_tool_create_error(id, -32602, "Invalid mode");
    }
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    return mcp_tool_create_success(id, result);
}

cJSON* mcp_tool_sdmusic_track(cJSON *params, const char *id) {
    cJSON *action = cJSON_GetObjectItem(params, "action");
    if (!action || !cJSON_IsString(action)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: action required");
    }
    
    const char *action_str = action->valuestring;
    cJSON *result = cJSON_CreateObject();
    
    if (strcmp(action_str, "set") == 0) {
        cJSON *index = cJSON_GetObjectItem(params, "index");
        if (index && cJSON_IsNumber(index)) {
            esp_err_t ret = sx_playlist_play_index(index->valueint);
            cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
        } else {
            return mcp_tool_create_error(id, -32602, "Invalid index");
        }
    } else if (strcmp(action_str, "info") == 0) {
        const char *track = sx_playlist_get_current_track();
        if (track) {
            cJSON_AddStringToObject(result, "track", track);
            cJSON_AddNumberToObject(result, "index", sx_playlist_get_current_index());
        }
    } else if (strcmp(action_str, "current") == 0) {
        const char *track = sx_playlist_get_current_track();
        if (track) {
            cJSON_AddStringToObject(result, "track", track);
        }
    } else {
        return mcp_tool_create_error(id, -32602, "Invalid action");
    }
    
    return mcp_tool_create_success(id, result);
}

cJSON* mcp_tool_sdmusic_search(cJSON *params, const char *id) {
    cJSON *keyword = cJSON_GetObjectItem(params, "keyword");
    if (!keyword || !cJSON_IsString(keyword)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: keyword required");
    }
    
    // Search using SD music service
    sx_sd_music_entry_t results[50];
    size_t result_count = 0;
    esp_err_t ret = sx_sd_music_search(keyword->valuestring, results, 50, &result_count);
    
    cJSON *result = cJSON_CreateObject();
    cJSON *tracks = cJSON_CreateArray();
    
    if (ret == ESP_OK && result_count > 0) {
        for (size_t i = 0; i < result_count; i++) {
            cJSON *track = cJSON_CreateObject();
            cJSON_AddStringToObject(track, "path", results[i].path);
            cJSON_AddStringToObject(track, "name", results[i].name);
            if (results[i].metadata.has_metadata) {
                cJSON_AddStringToObject(track, "title", results[i].metadata.title);
                cJSON_AddStringToObject(track, "artist", results[i].metadata.artist);
            }
            cJSON_AddItemToArray(tracks, track);
        }
    }
    
    cJSON_AddItemToObject(result, "tracks", tracks);
    cJSON_AddNumberToObject(result, "count", result_count);
    cJSON_AddStringToObject(result, "keyword", keyword->valuestring);
    
    return mcp_tool_create_success(id, result);
}

cJSON* mcp_tool_sdmusic_directory(cJSON *params, const char *id) {
    cJSON *action = cJSON_GetObjectItem(params, "action");
    if (!action || !cJSON_IsString(action)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: action required");
    }
    
    const char *action_str = action->valuestring;
    cJSON *result = cJSON_CreateObject();
    
    if (strcmp(action_str, "play") == 0) {
        cJSON *dir = cJSON_GetObjectItem(params, "directory");
        if (dir && cJSON_IsString(dir)) {
            // Load directory and play first track
            sx_sd_music_entry_t entries[100];
            size_t count = 0;
            esp_err_t ret = sx_sd_music_list_files(dir->valuestring, entries, 100, &count);
            if (ret == ESP_OK && count > 0) {
                // Find first music file (not directory)
                for (size_t i = 0; i < count; i++) {
                    if (!entries[i].is_dir) {
                        ret = sx_audio_play_file(entries[i].path);
                        break;
                    }
                }
            }
            cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
        } else {
            return mcp_tool_create_error(id, -32602, "Invalid directory");
        }
    } else if (strcmp(action_str, "list") == 0) {
        cJSON *dir = cJSON_GetObjectItem(params, "directory");
        const char *dir_path = dir && cJSON_IsString(dir) ? dir->valuestring : "/";
        
        sx_sd_music_entry_t entries[100];
        size_t count = 0;
        esp_err_t ret = sx_sd_music_list_files(dir_path, entries, 100, &count);
        
        cJSON *directories = cJSON_CreateArray();
        cJSON *files = cJSON_CreateArray();
        
        if (ret == ESP_OK) {
            for (size_t i = 0; i < count; i++) {
                cJSON *item = cJSON_CreateObject();
                cJSON_AddStringToObject(item, "path", entries[i].path);
                cJSON_AddStringToObject(item, "name", entries[i].name);
                cJSON_AddBoolToObject(item, "is_dir", entries[i].is_dir);
                
                if (entries[i].is_dir) {
                    cJSON_AddItemToArray(directories, item);
                } else {
                    cJSON_AddItemToArray(files, item);
                }
            }
        }
        
        cJSON_AddItemToObject(result, "directories", directories);
        cJSON_AddItemToObject(result, "files", files);
        cJSON_AddNumberToObject(result, "count", count);
        cJSON_AddStringToObject(result, "directory", dir_path);
    } else {
        return mcp_tool_create_error(id, -32602, "Invalid action: must be 'play' or 'list'");
    }
    
    return mcp_tool_create_success(id, result);
}

cJSON* mcp_tool_sdmusic_library(cJSON *params, const char *id) {
    cJSON *action = cJSON_GetObjectItem(params, "action");
    if (!action || !cJSON_IsString(action)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: action required");
    }
    
    const char *action_str = action->valuestring;
    cJSON *result = cJSON_CreateObject();
    
    if (strcmp(action_str, "count_dir") == 0) {
        cJSON *dir = cJSON_GetObjectItem(params, "directory");
        const char *dir_path = dir && cJSON_IsString(dir) ? dir->valuestring : "/";
        
        sx_sd_music_entry_t entries[100];
        size_t count = 0;
        sx_sd_music_list_files(dir_path, entries, 100, &count);
        
        cJSON_AddNumberToObject(result, "count", count);
        cJSON_AddStringToObject(result, "directory", dir_path);
    } else if (strcmp(action_str, "count_current") == 0) {
        sx_playlist_t *playlist = sx_playlist_get_current();
        if (playlist) {
            cJSON_AddNumberToObject(result, "count", playlist->track_count);
        } else {
            cJSON_AddNumberToObject(result, "count", 0);
        }
    } else if (strcmp(action_str, "page") == 0) {
        cJSON *page = cJSON_GetObjectItem(params, "page");
        cJSON *page_size = cJSON_GetObjectItem(params, "page_size");
        int page_num = page && cJSON_IsNumber(page) ? page->valueint : 0;
        int size = page_size && cJSON_IsNumber(page_size) ? page_size->valueint : 20;
        
        sx_playlist_t *playlist = sx_playlist_get_current();
        cJSON *tracks = cJSON_CreateArray();
        
        if (playlist) {
            int start = page_num * size;
            int end = start + size;
            if (end > (int)playlist->track_count) end = playlist->track_count;
            
            for (int i = start; i < end; i++) {
                cJSON *track = cJSON_CreateObject();
                cJSON_AddStringToObject(track, "path", playlist->track_paths[i]);
                cJSON_AddNumberToObject(track, "index", i);
                cJSON_AddItemToArray(tracks, track);
            }
        }
        
        cJSON_AddItemToObject(result, "tracks", tracks);
        cJSON_AddNumberToObject(result, "page", page_num);
        cJSON_AddNumberToObject(result, "page_size", size);
    } else {
        return mcp_tool_create_error(id, -32602, "Invalid action: must be 'count_dir', 'count_current', or 'page'");
    }
    
    return mcp_tool_create_success(id, result);
}

cJSON* mcp_tool_sdmusic_suggest(cJSON *params, const char *id) {
    cJSON *action = cJSON_GetObjectItem(params, "action");
    if (!action || !cJSON_IsString(action)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: action required");
    }
    
    const char *action_str = action->valuestring;
    cJSON *result = cJSON_CreateObject();
    
    if (strcmp(action_str, "next") == 0) {
        // Suggest next track (based on current track or history)
        sx_playlist_t *playlist = sx_playlist_get_current();
        if (playlist) {
            int current = sx_playlist_get_current_index();
            int next = (current + 1) % playlist->track_count;
            cJSON_AddStringToObject(result, "suggested_track", playlist->track_paths[next]);
            cJSON_AddNumberToObject(result, "index", next);
        }
    } else if (strcmp(action_str, "similar") == 0) {
        // Suggest similar tracks (based on genre or artist)
        const char *current = sx_playlist_get_current_track();
        if (current) {
            sx_sd_music_metadata_t metadata;
            if (sx_sd_music_get_metadata(current, &metadata) == ESP_OK && metadata.has_metadata) {
                // Find tracks with same genre or artist
                sx_sd_music_entry_t results[20];
                size_t count = 0;
                if (metadata.genre[0] != '\0') {
                    sx_sd_music_get_by_genre(metadata.genre, results, 20, &count);
                }
                
                cJSON *tracks = cJSON_CreateArray();
                for (size_t i = 0; i < count && i < 5; i++) {
                    if (strcmp(results[i].path, current) != 0) {
                        cJSON *track = cJSON_CreateObject();
                        cJSON_AddStringToObject(track, "path", results[i].path);
                        cJSON_AddStringToObject(track, "name", results[i].name);
                        cJSON_AddItemToArray(tracks, track);
                    }
                }
                cJSON_AddItemToObject(result, "similar_tracks", tracks);
            }
        }
    } else {
        return mcp_tool_create_error(id, -32602, "Invalid action: must be 'next' or 'similar'");
    }
    
    return mcp_tool_create_success(id, result);
}

cJSON* mcp_tool_sdmusic_genre(cJSON *params, const char *id) {
    cJSON *action = cJSON_GetObjectItem(params, "action");
    if (!action || !cJSON_IsString(action)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: action required");
    }
    
    const char *action_str = action->valuestring;
    cJSON *result = cJSON_CreateObject();
    
    if (strcmp(action_str, "search") == 0) {
        cJSON *genre = cJSON_GetObjectItem(params, "genre");
        if (!genre || !cJSON_IsString(genre)) {
            return mcp_tool_create_error(id, -32602, "Invalid genre");
        }
        
        sx_sd_music_entry_t results[50];
        size_t count = 0;
        sx_sd_music_get_by_genre(genre->valuestring, results, 50, &count);
        
        cJSON *tracks = cJSON_CreateArray();
        for (size_t i = 0; i < count; i++) {
            cJSON *track = cJSON_CreateObject();
            cJSON_AddStringToObject(track, "path", results[i].path);
            cJSON_AddStringToObject(track, "name", results[i].name);
            cJSON_AddItemToArray(tracks, track);
        }
        
        cJSON_AddItemToObject(result, "tracks", tracks);
        cJSON_AddNumberToObject(result, "count", count);
        cJSON_AddStringToObject(result, "genre", genre->valuestring);
    } else if (strcmp(action_str, "play") == 0) {
        cJSON *genre = cJSON_GetObjectItem(params, "genre");
        if (!genre || !cJSON_IsString(genre)) {
            return mcp_tool_create_error(id, -32602, "Invalid genre");
        }
        
        sx_sd_music_entry_t results[50];
        size_t count = 0;
        esp_err_t ret = sx_sd_music_get_by_genre(genre->valuestring, results, 50, &count);
        if (ret == ESP_OK && count > 0) {
            ret = sx_audio_play_file(results[0].path);
        }
        cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    } else if (strcmp(action_str, "play_index") == 0) {
        cJSON *genre = cJSON_GetObjectItem(params, "genre");
        cJSON *index = cJSON_GetObjectItem(params, "index");
        if (!genre || !cJSON_IsString(genre) || !index || !cJSON_IsNumber(index)) {
            return mcp_tool_create_error(id, -32602, "Invalid params");
        }
        
        sx_sd_music_entry_t results[50];
        size_t count = 0;
        esp_err_t ret = sx_sd_music_get_by_genre(genre->valuestring, results, 50, &count);
        if (ret == ESP_OK && index->valueint >= 0 && index->valueint < (int)count) {
            ret = sx_audio_play_file(results[index->valueint].path);
        }
        cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    } else if (strcmp(action_str, "next") == 0) {
        // Play next track in current genre
        const char *current = sx_playlist_get_current_track();
        if (current) {
            sx_sd_music_metadata_t metadata;
            if (sx_sd_music_get_metadata(current, &metadata) == ESP_OK && metadata.genre[0] != '\0') {
                sx_sd_music_entry_t results[50];
                size_t count = 0;
                sx_sd_music_get_by_genre(metadata.genre, results, 50, &count);
                
                // Find current track index in genre results
                int current_idx = -1;
                for (size_t i = 0; i < count; i++) {
                    if (strcmp(results[i].path, current) == 0) {
                        current_idx = i;
                        break;
                    }
                }
                
                if (current_idx >= 0 && current_idx + 1 < (int)count) {
                    esp_err_t ret = sx_audio_play_file(results[current_idx + 1].path);
                    cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
                }
            }
        }
    } else {
        return mcp_tool_create_error(id, -32602, "Invalid action");
    }
    
    return mcp_tool_create_success(id, result);
}

cJSON* mcp_tool_sdmusic_genre_list(cJSON *params, const char *id) {
    (void)params;
    
    // List all genres found in music library
    // This requires scanning all music files, which can be expensive
    // For now, return a placeholder with common genres
    cJSON *result = cJSON_CreateObject();
    cJSON *genres = cJSON_CreateArray();
    
    // Common genres (can be extended with actual scanning)
    const char *common_genres[] = {
        "Pop", "Rock", "Jazz", "Classical", "Electronic", "Hip-Hop", "Country", "Blues"
    };
    
    for (size_t i = 0; i < sizeof(common_genres) / sizeof(common_genres[0]); i++) {
        cJSON_AddItemToArray(genres, cJSON_CreateString(common_genres[i]));
    }
    
    cJSON_AddItemToObject(result, "genres", genres);
    cJSON_AddNumberToObject(result, "count", sizeof(common_genres) / sizeof(common_genres[0]));
    
    return mcp_tool_create_success(id, result);
}

cJSON* mcp_tool_sdmusic_progress(cJSON *params, const char *id) {
    (void)params;
    
    // Get playback status from audio service
    // Note: Audio service doesn't currently track position/duration
    // This is a placeholder for future implementation
    cJSON *result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "position_ms", 0);
    cJSON_AddNumberToObject(result, "duration_ms", 0);
    cJSON_AddBoolToObject(result, "playing", sx_audio_is_playing());
    cJSON_AddBoolToObject(result, "paused", !sx_audio_is_playing());
    
    return mcp_tool_create_success(id, result);
}

// Music Online MCP Tools
cJSON* mcp_tool_music_play_song(cJSON *params, const char *id) {
    cJSON *song_name = cJSON_GetObjectItem(params, "song_name");
    cJSON *artist_name = cJSON_GetObjectItem(params, "artist_name");
    
    if (!song_name || !cJSON_IsString(song_name)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: song_name required");
    }
    
    // Play online music using sx_music_online_service
    const char *artist = (artist_name && cJSON_IsString(artist_name)) ? artist_name->valuestring : NULL;
    esp_err_t ret = sx_music_online_play_song(song_name->valuestring, artist);
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    cJSON_AddStringToObject(result, "song_name", song_name->valuestring);
    if (artist) {
        cJSON_AddStringToObject(result, "artist_name", artist);
    }
    if (ret != ESP_OK) {
        cJSON_AddStringToObject(result, "error", esp_err_to_name(ret));
    }
    
    return mcp_tool_create_success(id, result);
}

cJSON* mcp_tool_music_pause(cJSON *params, const char *id) {
    (void)params;
    
    esp_err_t ret = sx_audio_pause();
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    
    return mcp_tool_create_success(id, result);
}

cJSON* mcp_tool_music_resume(cJSON *params, const char *id) {
    (void)params;
    
    esp_err_t ret = sx_audio_resume();
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    
    return mcp_tool_create_success(id, result);
}

cJSON* mcp_tool_music_set_display_mode(cJSON *params, const char *id) {
    cJSON *mode = cJSON_GetObjectItem(params, "mode");
    if (!mode || !cJSON_IsString(mode)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: mode required");
    }
    
    // Set display mode using sx_music_online_service
    sx_music_online_display_mode_t display_mode = SX_MUSIC_ONLINE_DISPLAY_INFO;
    const char *mode_str = mode->valuestring;
    
    if (strcmp(mode_str, "lyrics") == 0) {
        display_mode = SX_MUSIC_ONLINE_DISPLAY_LYRICS;
    } else if (strcmp(mode_str, "info") == 0) {
        display_mode = SX_MUSIC_ONLINE_DISPLAY_INFO;
    } else {
        return mcp_tool_create_error(id, -32602, "Invalid mode: must be 'lyrics' or 'info'");
    }
    
    esp_err_t ret = sx_music_online_set_display_mode(display_mode);
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    cJSON_AddStringToObject(result, "mode", mode_str);
    if (ret != ESP_OK) {
        cJSON_AddStringToObject(result, "error", esp_err_to_name(ret));
    }
    
    return mcp_tool_create_success(id, result);
}

// User-Only Tools
cJSON* mcp_tool_get_system_info(cJSON *params, const char *id) {
    (void)params;
    
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "model", "ESP32");
    cJSON_AddNumberToObject(result, "cores", chip_info.cores);
    cJSON_AddNumberToObject(result, "revision", chip_info.revision);
    cJSON_AddNumberToObject(result, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(result, "min_free_heap", esp_get_minimum_free_heap_size());
    
    return mcp_tool_create_success(id, result);
}

cJSON* mcp_tool_reboot(cJSON *params, const char *id) {
    (void)params;
    (void)id;
    
    // Delay reboot to allow response to be sent
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    
    return NULL; // Will not return
}

cJSON* mcp_tool_upgrade_firmware(cJSON *params, const char *id) {
    cJSON *url = cJSON_GetObjectItem(params, "url");
    if (!url || !cJSON_IsString(url)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: url required");
    }
    
    // Start OTA upgrade using sx_ota_service
    // Note: This requires sx_ota_service to have start_update API
    // For now, return structure ready for implementation
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", false);
    cJSON_AddStringToObject(result, "message", "OTA service integration needed");
    cJSON_AddStringToObject(result, "url", url->valuestring);
    
    return mcp_tool_create_success(id, result);
}

// Weather MCP Tools
cJSON* mcp_tool_weather_get_current(cJSON *params, const char *id) {
    (void)params;
    
    // Fetch weather if needed
    if (sx_weather_needs_update()) {
        esp_err_t ret = sx_weather_fetch();
        if (ret != ESP_OK) {
            return mcp_tool_create_error(id, -32000, "Failed to fetch weather data");
        }
    }
    
    const sx_weather_info_t *info = sx_weather_get_info();
    if (!info || !info->valid) {
        return mcp_tool_create_error(id, -32000, "Weather data not available");
    }
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "city", info->city);
    cJSON_AddStringToObject(result, "description", info->description);
    cJSON_AddStringToObject(result, "icon_code", info->icon_code);
    cJSON_AddNumberToObject(result, "temp", info->temp);
    cJSON_AddNumberToObject(result, "feels_like", info->feels_like);
    cJSON_AddNumberToObject(result, "humidity", info->humidity);
    cJSON_AddNumberToObject(result, "pressure", info->pressure);
    cJSON_AddNumberToObject(result, "wind_speed", info->wind_speed);
    
    return mcp_tool_create_success(id, result);
}

cJSON* mcp_tool_weather_get_forecast(cJSON *params, const char *id) {
    (void)params;
    
    // Note: Forecast requires forecast API endpoint (not implemented in weather service yet)
    // For now, return current weather as forecast
    const sx_weather_info_t *info = sx_weather_get_info();
    if (!info || !info->valid) {
        return mcp_tool_create_error(id, -32000, "Weather data not available");
    }
    
    cJSON *result = cJSON_CreateObject();
    cJSON *forecast = cJSON_CreateArray();
    
    // Single forecast entry (current weather)
    cJSON *entry = cJSON_CreateObject();
    cJSON_AddStringToObject(entry, "city", info->city);
    cJSON_AddStringToObject(entry, "description", info->description);
    cJSON_AddStringToObject(entry, "icon_code", info->icon_code);
    cJSON_AddNumberToObject(entry, "temp", info->temp);
    cJSON_AddNumberToObject(entry, "feels_like", info->feels_like);
    cJSON_AddNumberToObject(entry, "humidity", info->humidity);
    cJSON_AddNumberToObject(entry, "pressure", info->pressure);
    cJSON_AddNumberToObject(entry, "wind_speed", info->wind_speed);
    cJSON_AddItemToArray(forecast, entry);
    
    cJSON_AddItemToObject(result, "forecast", forecast);
    cJSON_AddStringToObject(result, "note", "Forecast API not yet implemented, returning current weather");
    
    return mcp_tool_create_success(id, result);
}

cJSON* mcp_tool_weather_set_city(cJSON *params, const char *id) {
    cJSON *city = cJSON_GetObjectItem(params, "city");
    if (!city || !cJSON_IsString(city)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: city required");
    }
    
    esp_err_t ret = sx_weather_set_city(city->valuestring);
    if (ret != ESP_OK) {
        return mcp_tool_create_error(id, -32000, "Failed to set city");
    }
    
    // Fetch weather for new city
    ret = sx_weather_fetch();
    if (ret != ESP_OK) {
        return mcp_tool_create_error(id, -32000, "Failed to fetch weather for new city");
    }
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", true);
    cJSON_AddStringToObject(result, "city", city->valuestring);
    
    return mcp_tool_create_success(id, result);
}

// System MCP Tools
cJSON* mcp_tool_system_reconfigure_wifi(cJSON *params, const char *id) {
    cJSON *ssid = cJSON_GetObjectItem(params, "ssid");
    cJSON *password = cJSON_GetObjectItem(params, "password");
    
    if (!ssid || !cJSON_IsString(ssid)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: ssid required");
    }
    
    const char *password_str = password && cJSON_IsString(password) ? password->valuestring : NULL;
    
    // Disconnect current WiFi if connected
    if (sx_wifi_is_connected()) {
        sx_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for disconnect
    }
    
    // Connect to new WiFi
    esp_err_t ret = sx_wifi_connect(ssid->valuestring, password_str);
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    cJSON_AddStringToObject(result, "ssid", ssid->valuestring);
    if (ret != ESP_OK) {
        cJSON_AddStringToObject(result, "error", esp_err_to_name(ret));
    } else {
        // Wait a bit for connection to establish
        vTaskDelay(pdMS_TO_TICKS(2000));
        if (sx_wifi_is_connected()) {
            cJSON_AddStringToObject(result, "ip_address", sx_wifi_get_ip_address());
            cJSON_AddNumberToObject(result, "rssi", sx_wifi_get_rssi());
        }
    }
    
    return mcp_tool_create_success(id, result);
}

cJSON* mcp_tool_screen_get_info(cJSON *params, const char *id) {
    (void)params;
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "width", 320);
    cJSON_AddNumberToObject(result, "height", 480);
    cJSON_AddStringToObject(result, "driver", "ST7796");
    
    return mcp_tool_create_success(id, result);
}

cJSON* mcp_tool_screen_snapshot(cJSON *params, const char *id) {
    cJSON *url = cJSON_GetObjectItem(params, "url");
    
    if (!url || !cJSON_IsString(url)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: url required");
    }
    
    // Initialize display service if needed
    sx_display_service_init();
    
    // Get screen dimensions from platform (Section 7.5 SIMPLEXL_ARCH v1.3: services cannot call LVGL)
    uint16_t width = 0;
    uint16_t height = 0;
    esp_err_t ret = sx_platform_get_screen_size(&width, &height);
    if (ret != ESP_OK || width == 0 || height == 0) {
        return mcp_tool_create_error(id, -32000, "Failed to get screen dimensions");
    }
    
    // Capture screen
    size_t buffer_size = width * height * 2; // RGB565 = 2 bytes per pixel
    uint8_t *screen_buffer = (uint8_t *)malloc(buffer_size);
    if (!screen_buffer) {
        return mcp_tool_create_error(id, -32000, "Failed to allocate screen buffer");
    }
    
    ret = sx_display_capture_screen(screen_buffer, width, height);
    if (ret != ESP_OK) {
        free(screen_buffer);
        return mcp_tool_create_error(id, -32000, "Failed to capture screen");
    }
    
    // Encode to JPEG
    uint8_t *jpeg_data = NULL;
    size_t jpeg_size = 0;
    ret = sx_display_encode_jpeg(screen_buffer, width, height, 80, &jpeg_data, &jpeg_size);
    free(screen_buffer);
    
    if (ret != ESP_OK) {
        cJSON *result = cJSON_CreateObject();
        cJSON_AddBoolToObject(result, "success", false);
        cJSON_AddStringToObject(result, "message", "JPEG encoding not yet implemented - requires ESP32 JPEG encoder");
        cJSON_AddStringToObject(result, "status", "encoding_not_implemented");
        cJSON_AddStringToObject(result, "upload_url", url->valuestring);
        return mcp_tool_create_success(id, result);
    }
    
    // Upload JPEG
    ret = sx_display_upload_jpeg(jpeg_data, jpeg_size, url->valuestring);
    free(jpeg_data);
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    cJSON_AddStringToObject(result, "upload_url", url->valuestring);
    if (ret != ESP_OK) {
        cJSON_AddStringToObject(result, "error", esp_err_to_name(ret));
    }
    
    return mcp_tool_create_success(id, result);
}

cJSON* mcp_tool_screen_preview_image(cJSON *params, const char *id) {
    cJSON *url = cJSON_GetObjectItem(params, "url");
    cJSON *timeout = cJSON_GetObjectItem(params, "timeout_ms");
    
    if (!url || !cJSON_IsString(url)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: url required");
    }
    
    int timeout_ms = timeout && cJSON_IsNumber(timeout) ? timeout->valueint : 5000;
    
    // Initialize display service if needed
    sx_display_service_init();
    
    // Download image
    uint8_t *image_data = NULL;
    size_t image_size = 0;
    esp_err_t ret = sx_display_download_image(url->valuestring, &image_data, &image_size);
    if (ret != ESP_OK) {
        return mcp_tool_create_error(id, -32000, "Failed to download image");
    }
    
    // Decode image
    sx_image_info_t info = {0};
    uint8_t *decoded_data = NULL;
    ret = sx_image_load_from_memory(image_data, image_size, &info, &decoded_data);
    free(image_data);
    
    if (ret != ESP_OK || !decoded_data) {
        return mcp_tool_create_error(id, -32000, "Failed to decode image");
    }
    
    // Display image (will copy data internally)
    ret = sx_display_show_image(decoded_data, info.width, info.height, timeout_ms);
    
    // Free decoded data (sx_display_show_image copies it)
    free(decoded_data);
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    cJSON_AddStringToObject(result, "url", url->valuestring);
    cJSON_AddNumberToObject(result, "timeout_ms", timeout_ms);
    if (ret == ESP_OK) {
        cJSON_AddNumberToObject(result, "width", info.width);
        cJSON_AddNumberToObject(result, "height", info.height);
    } else {
        cJSON_AddStringToObject(result, "error", esp_err_to_name(ret));
        free(decoded_data); // Free if display failed
    }
    
    return mcp_tool_create_success(id, result);
}

// Register all MCP tools
esp_err_t sx_mcp_tools_register_all(void) {
    // SD Music tools
    sx_mcp_server_register_tool("self.sdmusic.playback",
        "Control SD music playback. action = play | pause | stop | next | prev",
        mcp_tool_sdmusic_playback, false);
    
    sx_mcp_server_register_tool("self.sdmusic.mode",
        "Control playback mode: shuffle and repeat. action = shuffle | repeat",
        mcp_tool_sdmusic_mode, false);
    
    sx_mcp_server_register_tool("self.sdmusic.track",
        "Track-level operations. action = set | info | list | current",
        mcp_tool_sdmusic_track, false);
    
    sx_mcp_server_register_tool("self.sdmusic.search",
        "Search and play tracks by name. action = search | play",
        mcp_tool_sdmusic_search, false);
    
    sx_mcp_server_register_tool("self.sdmusic.directory",
        "Directory-level operations. action = play | list",
        mcp_tool_sdmusic_directory, false);
    
    sx_mcp_server_register_tool("self.sdmusic.library",
        "Library management operations. action = reload | count | page",
        mcp_tool_sdmusic_library, false);
    
    sx_mcp_server_register_tool("self.sdmusic.suggest",
        "Song suggestion based on history/similarity. action = next | similar",
        mcp_tool_sdmusic_suggest, false);
    
    sx_mcp_server_register_tool("self.sdmusic.genre",
        "Genre-based music operations. action = search | play | play_index | next",
        mcp_tool_sdmusic_genre, false);
    
    sx_mcp_server_register_tool("self.sdmusic.genre_list",
        "List all unique genres available in the current SD music library",
        mcp_tool_sdmusic_genre_list, false);
    
    sx_mcp_server_register_tool("self.sdmusic.progress",
        "Get current playback progress and duration",
        mcp_tool_sdmusic_progress, false);
    
    // Music Online tools
    sx_mcp_server_register_tool("self.music.play_song",
        "Play a specified song ONLINE. This is the DEFAULT music playback mode. Use when user says 'phát nhạc', 'mở nhạc', 'play music'.",
        mcp_tool_music_play_song, false);
    
    sx_mcp_server_register_tool("self.music.pause",
        "Pause music playback",
        mcp_tool_music_pause, false);
    
    sx_mcp_server_register_tool("self.music.resume",
        "Resume music playback",
        mcp_tool_music_resume, false);
    
    sx_mcp_server_register_tool("self.music.set_display_mode",
        "Set the display mode for music playback. mode = spectrum | lyrics",
        mcp_tool_music_set_display_mode, false);
    
    // User-Only tools
    sx_mcp_server_register_tool("self.get_system_info",
        "Get the system information",
        mcp_tool_get_system_info, true);
    
    sx_mcp_server_register_tool("self.reboot",
        "Reboot the system",
        mcp_tool_reboot, true);
    
    sx_mcp_server_register_tool("self.upgrade_firmware",
        "Upgrade firmware from a specific URL",
        mcp_tool_upgrade_firmware, true);
    
    sx_mcp_server_register_tool("self.screen.get_info",
        "Information about the screen, including width, height, etc.",
        mcp_tool_screen_get_info, true);
    
    sx_mcp_server_register_tool("self.screen.snapshot",
        "Snapshot the screen and upload it to a specific URL",
        mcp_tool_screen_snapshot, true);
    
    sx_mcp_server_register_tool("self.screen.preview_image",
        "Preview an image on the screen",
        mcp_tool_screen_preview_image, true);
    
    // Weather MCP Tools
    sx_mcp_server_register_tool("self.weather.get_current",
        "Get current weather. params: {}",
        mcp_tool_weather_get_current, false);
    
    sx_mcp_server_register_tool("self.weather.get_forecast",
        "Get weather forecast. params: {}",
        mcp_tool_weather_get_forecast, false);
    
    sx_mcp_server_register_tool("self.weather.set_city",
        "Set city for weather. params: {city: string}",
        mcp_tool_weather_set_city, false);
    
    // System MCP Tools
    sx_mcp_server_register_tool("self.system.reconfigure_wifi",
        "Reconfigure WiFi connection. params: {ssid: string, password?: string}",
        mcp_tool_system_reconfigure_wifi, true);
    
    // Navigation tools
    extern esp_err_t sx_mcp_tools_navigation_register(void);
    sx_mcp_tools_navigation_register();
    
    // IR Control tools
    extern esp_err_t sx_mcp_tools_ir_register(void);
    sx_mcp_tools_ir_register();
    
    ESP_LOGI(TAG, "MCP tools registered (SD Music, Music Online, Navigation, IR Control, Weather, System, User-Only)");
    return ESP_OK;
}

