/**
 * @file sx_media_metadata.c
 * @brief Media metadata parser for MP3 (ID3v2) and OGG/FLAC (Vorbis Comments)
 * Phase 5: Backend Enablement
 */

#include "sx_media_metadata.h"
#include <esp_log.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

static const char *TAG = "sx_meta";

// Forward declarations for Vorbis comment parsing
static esp_err_t parse_flac_vorbis_comment(FILE *f, sx_track_meta_t *meta);
static esp_err_t parse_ogg_vorbis_comment(FILE *f, sx_track_meta_t *meta);
static esp_err_t parse_vorbis_comment_data(FILE *f, uint32_t block_size, sx_track_meta_t *meta);
static esp_err_t parse_vorbis_comment_data_from_buffer(const uint8_t *data, size_t data_size, sx_track_meta_t *meta);

// Helper: Read 4 bytes as big-endian uint32_t
static uint32_t read_be32(const uint8_t *data) {
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | 
           ((uint32_t)data[2] << 8) | data[3];
}

// Helper: Read 3 bytes as big-endian uint32_t (ID3v2 sync-safe)
static uint32_t read_be32_syncsafe(const uint8_t *data) {
    return ((uint32_t)data[0] << 21) | ((uint32_t)data[1] << 14) | 
           ((uint32_t)data[2] << 7) | data[3];
}

// Helper: Read null-terminated string from buffer
static size_t read_string(const uint8_t *data, size_t max_len, char *out, size_t out_size) {
    size_t len = 0;
    // Skip encoding byte (0 = ISO-8859-1, 1 = UTF-16, 2 = UTF-16BE, 3 = UTF-8)
    if (max_len > 0 && data[0] < 4) {
        uint8_t encoding = data[0];
        const uint8_t *text = data + 1;
        size_t text_max = max_len - 1;
        
        if (encoding == 0 || encoding == 3) { // ISO-8859-1 or UTF-8
            len = strnlen((const char *)text, text_max);
            if (len >= out_size) len = out_size - 1;
            memcpy(out, text, len);
            out[len] = '\0';
        } else if (encoding == 1 || encoding == 2) { // UTF-16
            // Simplified: convert UTF-16 to UTF-8 (basic implementation)
            // For now, just skip UTF-16 frames
            len = 0;
        }
    }
    return len;
}

// Parse ID3v2 tag from MP3 file
static esp_err_t parse_id3v2(FILE *f, sx_track_meta_t *meta) {
    uint8_t header[10];
    if (fread(header, 1, 10, f) != 10) {
        return ESP_ERR_NOT_FOUND;
    }
    
    // Check ID3v2 header
    if (memcmp(header, "ID3", 3) != 0) {
        return ESP_ERR_NOT_FOUND;
    }
    
    uint8_t version_major = header[3];
    uint8_t version_minor = header[4];
    if (version_major != 3 && version_major != 4) {
        ESP_LOGW(TAG, "ID3v2.%d.%d not fully supported, trying anyway", version_major, version_minor);
    }
    
    // Get tag size (sync-safe)
    uint32_t tag_size = read_be32_syncsafe(header + 6);
    ESP_LOGD(TAG, "ID3v2 tag size: %lu bytes", (unsigned long)tag_size);
    
    // Read tag data
    uint8_t *tag_data = (uint8_t *)malloc(tag_size);
    if (tag_data == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    if (fread(tag_data, 1, tag_size, f) != tag_size) {
        free(tag_data);
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    // Parse frames
    size_t pos = 0;
    bool found_title = false;
    bool found_artist = false;
    bool found_genre = false;
    
    while (pos + 10 < tag_size) {
        // Frame header: 4-byte ID, 4-byte size, 2-byte flags
        const uint8_t *frame = tag_data + pos;
        char frame_id[5] = {0};
        memcpy(frame_id, frame, 4);
        
        uint32_t frame_size;
        if (version_major == 4) {
            frame_size = read_be32_syncsafe(frame + 4);
        } else {
            frame_size = read_be32(frame + 4);
        }
        
        if (frame_size == 0 || pos + 10 + frame_size > tag_size) {
            break;
        }
        
        const uint8_t *frame_data = frame + 10;
        
        // TIT2 = Title
        if (strcmp(frame_id, "TIT2") == 0 && !found_title) {
            read_string(frame_data, frame_size, meta->title, sizeof(meta->title));
            found_title = true;
            ESP_LOGD(TAG, "Found title: %s", meta->title);
        }
        // TPE1 = Artist
        else if (strcmp(frame_id, "TPE1") == 0 && !found_artist) {
            read_string(frame_data, frame_size, meta->artist, sizeof(meta->artist));
            found_artist = true;
            ESP_LOGD(TAG, "Found artist: %s", meta->artist);
        }
        // TCON = Genre
        else if (strcmp(frame_id, "TCON") == 0 && !found_genre) {
            read_string(frame_data, frame_size, meta->genre, sizeof(meta->genre));
            found_genre = true;
            ESP_LOGD(TAG, "Found genre: %s", meta->genre);
        }
        // TLEN = Length (in milliseconds)
        else if (strcmp(frame_id, "TLEN") == 0 && meta->duration_ms == 0) {
            // TLEN is text field containing duration in milliseconds
            char len_str[32] = {0};
            read_string(frame_data, frame_size, len_str, sizeof(len_str));
            meta->duration_ms = (uint32_t)atoi(len_str);
            ESP_LOGD(TAG, "Found duration: %lu ms", (unsigned long)meta->duration_ms);
        }
        
        pos += 10 + frame_size;
    }
    
    free(tag_data);
    return (found_title || found_artist || found_genre) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

// Parse Vorbis Comments from OGG/FLAC file
static esp_err_t parse_vorbis_comment(FILE *f, sx_track_meta_t *meta) {
    // Vorbis comments structure:
    // - OGG: In first/second OGG page after identification header
    // - FLAC: In METADATA_BLOCK_VORBIS_COMMENT (type 4)
    
    // For OGG: Look for "OggS" header, then find comment page
    // For FLAC: Look for "fLaC" header, then find comment block
    
    uint8_t header[4];
    if (fread(header, 1, 4, f) != 4) {
        return ESP_ERR_NOT_FOUND;
    }
    
    fseek(f, 0, SEEK_SET);
    
    // Check for FLAC
    if (memcmp(header, "fLaC", 4) == 0) {
        return parse_flac_vorbis_comment(f, meta);
    }
    
    // Check for OGG
    if (memcmp(header, "OggS", 4) == 0) {
        return parse_ogg_vorbis_comment(f, meta);
    }
    
    return ESP_ERR_NOT_FOUND;
}

// Parse FLAC Vorbis Comments
static esp_err_t parse_flac_vorbis_comment(FILE *f, sx_track_meta_t *meta) {
    // FLAC structure: "fLaC" (4 bytes) + STREAMINFO block + other blocks
    // Vorbis comment block: type 4, last bit indicates if it's last block
    
    fseek(f, 4, SEEK_SET); // Skip "fLaC"
    
    // Read metadata blocks until we find Vorbis comment (type 4)
    while (!feof(f)) {
        uint8_t block_header[4];
        if (fread(block_header, 1, 4, f) != 4) {
            break;
        }
        
        bool is_last = (block_header[0] & 0x80) != 0;
        uint8_t block_type = block_header[0] & 0x7F;
        uint32_t block_size = ((uint32_t)block_header[1] << 16) | 
                             ((uint32_t)block_header[2] << 8) | 
                             block_header[3];
        
        if (block_type == 4) { // Vorbis comment block
            return parse_vorbis_comment_data(f, block_size, meta);
        } else {
            // Skip this block
            fseek(f, block_size, SEEK_CUR);
        }
        
        if (is_last) break;
    }
    
    return ESP_ERR_NOT_FOUND;
}

// Parse OGG Vorbis Comments
static esp_err_t parse_ogg_vorbis_comment(FILE *f, sx_track_meta_t *meta) {
    // OGG structure: Pages with headers
    // Vorbis comment is usually in second page (after identification header)
    
    uint8_t page_header[27];
    if (fread(page_header, 1, 27, f) != 27) {
        return ESP_ERR_NOT_FOUND;
    }
    
    // Check capture pattern
    if (memcmp(page_header, "OggS", 4) != 0) {
        return ESP_ERR_NOT_FOUND;
    }
    
    uint8_t page_segments = page_header[26];
    if (page_segments == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    
    // Read segment table
    uint8_t segment_table[255];
    if (fread(segment_table, 1, page_segments, f) != page_segments) {
        return ESP_ERR_NOT_FOUND;
    }
    
    // Calculate page data size
    size_t page_data_size = 0;
    for (int i = 0; i < page_segments; i++) {
        page_data_size += segment_table[i];
    }
    
    // Skip identification header page, read comment page
    fseek(f, page_data_size, SEEK_CUR);
    
    // Read next page (comment page)
    if (fread(page_header, 1, 27, f) != 27) {
        return ESP_ERR_NOT_FOUND;
    }
    
    if (memcmp(page_header, "OggS", 4) != 0) {
        return ESP_ERR_NOT_FOUND;
    }
    
    page_segments = page_header[26];
    if (fread(segment_table, 1, page_segments, f) != page_segments) {
        return ESP_ERR_NOT_FOUND;
    }
    
    page_data_size = 0;
    for (int i = 0; i < page_segments; i++) {
        page_data_size += segment_table[i];
    }
    
    // Read comment page data
    uint8_t *page_data = (uint8_t *)malloc(page_data_size);
    if (page_data == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    if (fread(page_data, 1, page_data_size, f) != page_data_size) {
        free(page_data);
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    // Parse vorbis comment from page data
    // Vorbis comment format: vendor_length (4 bytes LE) + vendor_string + user_comment_list_length (4 bytes LE) + comments
    size_t pos = 0;
    if (page_data_size < 8) {
        free(page_data);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Skip vendor string
    uint32_t vendor_len = page_data[0] | (page_data[1] << 8) | (page_data[2] << 16) | (page_data[3] << 24);
    pos = 4 + vendor_len;
    
    if (pos + 4 > page_data_size) {
        free(page_data);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Read comment count (skip for now, not used in parsing)
    pos += 4;  // Skip comment count field
    
    // Parse comments
    esp_err_t ret = parse_vorbis_comment_data_from_buffer(page_data + pos, page_data_size - pos, meta);
    
    free(page_data);
    return ret;
}

// Parse Vorbis comment data from FLAC block
static esp_err_t parse_vorbis_comment_data(FILE *f, uint32_t block_size, sx_track_meta_t *meta) {
    // Read block data
    uint8_t *block_data = (uint8_t *)malloc(block_size);
    if (block_data == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    if (fread(block_data, 1, block_size, f) != block_size) {
        free(block_data);
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    esp_err_t ret = parse_vorbis_comment_data_from_buffer(block_data, block_size, meta);
    free(block_data);
    return ret;
}

// Parse Vorbis comment data from buffer
static esp_err_t parse_vorbis_comment_data_from_buffer(const uint8_t *data, size_t data_size, sx_track_meta_t *meta) {
    size_t pos = 0;
    bool found_title = false;
    bool found_artist = false;
    bool found_genre = false;
    
    // Skip vendor string (already skipped in OGG case)
    if (data_size < 4) {
        return ESP_ERR_NOT_FOUND;
    }
    
    // If first 4 bytes look like length, skip vendor
    uint32_t vendor_len = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    if (vendor_len < data_size) {
        pos = 4 + vendor_len;
    }
    
    // Read comment count
    if (pos + 4 > data_size) {
        return ESP_ERR_NOT_FOUND;
    }
    
    uint32_t comment_count = data[pos] | (data[pos+1] << 8) | 
                            (data[pos+2] << 16) | (data[pos+3] << 24);
    pos += 4;
    
    // Parse each comment: length (4 bytes LE) + "TAG=value"
    for (uint32_t i = 0; i < comment_count && pos < data_size; i++) {
        if (pos + 4 > data_size) break;
        
        uint32_t comment_len = data[pos] | (data[pos+1] << 8) | 
                              (data[pos+2] << 16) | (data[pos+3] << 24);
        pos += 4;
        
        if (pos + comment_len > data_size) break;
        
        // Parse "TAG=value" format
        char comment[512];
        size_t copy_len = comment_len < sizeof(comment) - 1 ? comment_len : sizeof(comment) - 1;
        memcpy(comment, data + pos, copy_len);
        comment[copy_len] = '\0';
        
        // Find '=' separator
        char *eq = strchr(comment, '=');
        if (eq != NULL) {
            *eq = '\0';
            char *tag = comment;
            char *value = eq + 1;
            
            // Convert tag to uppercase for comparison
            for (char *p = tag; *p; p++) {
                if (*p >= 'a' && *p <= 'z') *p -= 32;
            }
            
            if (strcmp(tag, "TITLE") == 0 && !found_title) {
                strncpy(meta->title, value, sizeof(meta->title) - 1);
                meta->title[sizeof(meta->title) - 1] = '\0';
                found_title = true;
                ESP_LOGD(TAG, "Found title: %s", meta->title);
            } else if (strcmp(tag, "ARTIST") == 0 && !found_artist) {
                strncpy(meta->artist, value, sizeof(meta->artist) - 1);
                meta->artist[sizeof(meta->artist) - 1] = '\0';
                found_artist = true;
                ESP_LOGD(TAG, "Found artist: %s", meta->artist);
            } else if (strcmp(tag, "GENRE") == 0 && !found_genre) {
                strncpy(meta->genre, value, sizeof(meta->genre) - 1);
                meta->genre[sizeof(meta->genre) - 1] = '\0';
                found_genre = true;
                ESP_LOGD(TAG, "Found genre: %s", meta->genre);
            }
        }
        
        pos += comment_len;
    }
    
    return (found_title || found_artist || found_genre) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t sx_meta_init(void) {
    // No initialization needed for now
    return ESP_OK;
}

esp_err_t sx_meta_parse_file(const char *file_path, sx_track_meta_t *out) {
    if (file_path == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize output
    memset(out, 0, sizeof(sx_track_meta_t));
    
    FILE *f = fopen(file_path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s", file_path);
        return ESP_ERR_NOT_FOUND;
    }
    
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    
    // Try ID3v2 (MP3)
    fseek(f, 0, SEEK_SET);
    ret = parse_id3v2(f, out);
    if (ret == ESP_OK) {
        out->has_metadata = true;
        fclose(f);
        return ESP_OK;
    }
    
    // Try Vorbis Comments (OGG/FLAC)
    fseek(f, 0, SEEK_SET);
    ret = parse_vorbis_comment(f, out);
    if (ret == ESP_OK) {
        out->has_metadata = true;
        fclose(f);
        return ESP_OK;
    }
    
    fclose(f);
    return ESP_ERR_NOT_FOUND;
}

uint32_t sx_meta_estimate_duration(const char *file_path) {
    if (file_path == NULL) {
        return 0;
    }
    
    struct stat st;
    if (stat(file_path, &st) != 0) {
        return 0;
    }
    
    // Estimate based on file size and typical bitrate
    // For MP3: assume 128 kbps average
    // For FLAC: assume 1000 kbps average
    // This is very rough estimate
    
    const char *ext = strrchr(file_path, '.');
    if (ext == NULL) {
        return 0;
    }
    
    uint32_t bitrate_kbps = 128; // Default MP3 bitrate
    if (strcasecmp(ext, ".flac") == 0 || strcasecmp(ext, ".ogg") == 0) {
        bitrate_kbps = 1000; // FLAC/OGG typically higher
    }
    
    // Duration (ms) = (file_size_bytes * 8) / (bitrate_kbps * 1000) * 1000
    uint64_t file_size_bytes = st.st_size;
    uint64_t duration_ms = (file_size_bytes * 8 * 1000) / (bitrate_kbps * 1000);
    
    return (uint32_t)duration_ms;
}

esp_err_t sx_meta_find_cover(const char *track_path, char *cover_path, size_t cover_path_len) {
    if (track_path == NULL || cover_path == NULL || cover_path_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Get directory from track path
    char dir_path[512];
    strncpy(dir_path, track_path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';
    
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash == NULL) {
        last_slash = strrchr(dir_path, '\\');
    }
    
    if (last_slash != NULL) {
        *(last_slash + 1) = '\0'; // Terminate at directory
    } else {
        // No directory, use current directory
        dir_path[0] = '.';
        dir_path[1] = '/';
        dir_path[2] = '\0';
    }
    
    // Search for common cover image filenames
    const char *cover_names[] = {
        "cover.jpg",
        "folder.jpg",
        "album.jpg",
        "cover.png",
        "folder.png",
        "album.png",
        "FRONT_COVER.jpg",
        "FRONT_COVER.png"
    };
    
    for (size_t i = 0; i < sizeof(cover_names) / sizeof(cover_names[0]); i++) {
        snprintf(cover_path, cover_path_len, "%s%s", dir_path, cover_names[i]);
        
        FILE *f = fopen(cover_path, "rb");
        if (f != NULL) {
            fclose(f);
            ESP_LOGD(TAG, "Found cover: %s", cover_path);
            return ESP_OK;
        }
    }
    
    return ESP_ERR_NOT_FOUND;
}

