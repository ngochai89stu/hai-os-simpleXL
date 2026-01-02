#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Music file metadata (ID3v1 + ID3v2)
typedef struct {
    char title[64];
    char artist[64];
    char album[64];
    char genre[32];
    uint16_t year;
    uint8_t track;
    bool has_metadata;
} sx_sd_music_metadata_t;

// Music file entry (with metadata)
typedef struct {
    char path[256];
    char name[128];
    bool is_dir;
    sx_sd_music_metadata_t metadata;
} sx_sd_music_entry_t;

// Playlist entry
typedef struct {
    char name[64];
    char genre[32];
    size_t track_count;
    char **track_paths;  // Array of track paths
} sx_sd_playlist_t;

// Initialize SD music service
esp_err_t sx_sd_music_service_init(void);

// List music files in directory (with metadata parsing)
esp_err_t sx_sd_music_list_files(const char *dir_path, sx_sd_music_entry_t *entries, 
                                  size_t max_count, size_t *out_count);

// Get metadata for a specific music file
esp_err_t sx_sd_music_get_metadata(const char *file_path, sx_sd_music_metadata_t *metadata);

// Search music files (case-insensitive, UTF-8 support)
esp_err_t sx_sd_music_search(const char *query, sx_sd_music_entry_t *results, 
                              size_t max_results, size_t *out_count);

// Get tracks by genre
esp_err_t sx_sd_music_get_by_genre(const char *genre, sx_sd_music_entry_t *results, 
                                    size_t max_results, size_t *out_count);

// Create playlist by genre
esp_err_t sx_sd_music_create_genre_playlist(const char *genre, sx_sd_playlist_t *playlist);

// Free playlist memory
void sx_sd_music_free_playlist(sx_sd_playlist_t *playlist);

// Get cover art metadata (offset, size, MIME type) - if available in ID3v2
esp_err_t sx_sd_music_get_cover_art_info(const char *file_path, uint32_t *offset, 
                                         uint32_t *size, char *mime_type, size_t mime_type_size);

#ifdef __cplusplus
}
#endif




















