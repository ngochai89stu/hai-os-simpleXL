#pragma once

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: Media Metadata Parser
// Supports ID3v2 (MP3) and Vorbis Comments (OGG/FLAC)

// Track metadata structure
typedef struct {
    char title[256];           // Track title
    char artist[256];           // Artist name
    char genre[64];            // Genre
    uint32_t duration_ms;      // Duration in milliseconds (0 if unknown)
    char cover_hint[512];      // Optional: path to cover image or hint
    bool has_metadata;         // True if metadata was successfully parsed
} sx_track_meta_t;

// Initialize metadata parser
esp_err_t sx_meta_init(void);

// Parse metadata from file
// Returns ESP_OK if metadata found, ESP_ERR_NOT_FOUND if no metadata, ESP_FAIL on error
esp_err_t sx_meta_parse_file(const char *file_path, sx_track_meta_t *out);

// Get duration estimate from file size and bitrate (fallback)
// Returns duration in milliseconds, 0 if cannot estimate
uint32_t sx_meta_estimate_duration(const char *file_path);

// Find cover image in same directory as track
// Searches for: cover.jpg, folder.jpg, album.jpg, cover.png, etc.
// Returns ESP_OK if found, ESP_ERR_NOT_FOUND if not found
esp_err_t sx_meta_find_cover(const char *track_path, char *cover_path, size_t cover_path_len);

#ifdef __cplusplus
}
#endif









