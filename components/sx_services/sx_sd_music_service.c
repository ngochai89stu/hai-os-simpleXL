#include "sx_sd_music_service.h"
#include "sx_sd_service.h"
#include <esp_log.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

// static const char *TAG = "sx_sd_music"; // Reserved for future logging

// ID3v1 tag structure (128 bytes at end of file)
typedef struct {
    char tag[3];      // "TAG"
    char title[30];
    char artist[30];
    char album[30];
    char year[4];
    char comment[30];
    uint8_t genre;
} __attribute__((packed)) id3v1_tag_t;

// ID3v2 header (10 bytes at start of file)
typedef struct {
    char identifier[3];  // "ID3"
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t flags;
    uint32_t size;      // Synchsafe integer
} __attribute__((packed)) id3v2_header_t;

// ID3v2 frame header (10 bytes)
typedef struct {
    char frame_id[4];
    uint32_t size;      // Synchsafe integer
    uint16_t flags;
} __attribute__((packed)) id3v2_frame_header_t;

// Helper: Read synchsafe integer (7 bits per byte)
static uint32_t read_synchsafe_uint32(const uint8_t *data) {
    return ((uint32_t)data[0] << 21) | ((uint32_t)data[1] << 14) | 
           ((uint32_t)data[2] << 7) | (uint32_t)data[3];
}

// Helper: Check if file has ID3v1 tag
static bool has_id3v1(FILE *f) {
    fseek(f, -128, SEEK_END);
    char tag[4] = {0};
    if (fread(tag, 1, 3, f) != 3) {
        return false;
    }
    return (strncmp(tag, "TAG", 3) == 0);
}

// Helper: Parse ID3v1 tag
static esp_err_t parse_id3v1(FILE *f, sx_sd_music_metadata_t *metadata) {
    id3v1_tag_t tag;
    fseek(f, -128, SEEK_END);
    if (fread(&tag, sizeof(tag), 1, f) != 1) {
        return ESP_FAIL;
    }
    
    // Copy fields (null-terminate)
    strncpy(metadata->title, tag.title, sizeof(metadata->title) - 1);
    metadata->title[sizeof(metadata->title) - 1] = '\0';
    strncpy(metadata->artist, tag.artist, sizeof(metadata->artist) - 1);
    metadata->artist[sizeof(metadata->artist) - 1] = '\0';
    strncpy(metadata->album, tag.album, sizeof(metadata->album) - 1);
    metadata->album[sizeof(metadata->album) - 1] = '\0';
    
    // Parse year
    char year_str[5] = {0};
    strncpy(year_str, tag.year, 4);
    metadata->year = (uint16_t)atoi(year_str);
    
    // Parse track number from comment (if available)
    if (tag.comment[28] == 0 && tag.comment[29] != 0) {
        metadata->track = tag.comment[29];
    } else {
        metadata->track = 0;
    }
    
    // Genre mapping (ID3v1 has 148 genres)
    static const char *genre_table[] = {
        "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge",
        "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B",
        "Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska",
        "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient", "Trip-Hop",
        "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical", "Instrumental",
        "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise", "Alt Rock",
        "Bass", "Soul", "Punk", "Space", "Meditative", "Instrumental Pop",
        "Instrumental Rock", "Ethnic", "Gothic", "Darkwave", "Techno-Industrial",
        "Electronic", "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy",
        "Cult", "Gangsta", "Top 40", "Christian Rap", "Pop/Funk", "Jungle",
        "Native US", "Cabaret", "New Wave", "Psychadelic", "Rave", "Showtunes",
        "Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz", "Polka", "Retro",
        "Musical", "Rock & Roll", "Hard Rock", "Folk", "Folk-Rock", "National Folk",
        "Swing", "Fast Fusion", "Bebob", "Latin", "Revival", "Celtic", "Bluegrass",
        "Avantgarde", "Gothic Rock", "Progressive Rock", "Psychedelic Rock",
        "Symphonic Rock", "Slow Rock", "Big Band", "Chorus", "Easy Listening",
        "Acoustic", "Humour", "Speech", "Chanson", "Opera", "Chamber Music",
        "Sonata", "Symphony", "Booty Bass", "Primus", "Porn Groove", "Satire",
        "Slow Jam", "Club", "Tango", "Samba", "Folklore", "Ballad", "Power Ballad",
        "Rhythmic Soul", "Freestyle", "Duet", "Punk Rock", "Drum Solo", "A capella",
        "Euro-House", "Dance Hall", "Goa", "Drum & Bass", "Club-House", "Hardcore",
        "Terror", "Indie", "BritPop", "Negerpunk", "Polsk Punk", "Beat",
        "Christian Gangsta Rap", "Heavy Metal", "Black Metal", "Crossover",
        "Contemporary Christian", "Christian Rock", "Merengue", "Salsa", "Thrash Metal",
        "Anime", "Jpop", "Synthpop"
    };
    
    if (tag.genre < sizeof(genre_table) / sizeof(genre_table[0])) {
        strncpy(metadata->genre, genre_table[tag.genre], sizeof(metadata->genre) - 1);
        metadata->genre[sizeof(metadata->genre) - 1] = '\0';
    } else {
        metadata->genre[0] = '\0';
    }
    
    metadata->has_metadata = true;
    return ESP_OK;
}

// Helper: Check if file has ID3v2 tag
static bool has_id3v2(FILE *f) {
    fseek(f, 0, SEEK_SET);
    char identifier[4] = {0};
    if (fread(identifier, 1, 3, f) != 3) {
        return false;
    }
    return (strncmp(identifier, "ID3", 3) == 0);
}

// Helper: Parse ID3v2 tag (simplified - only parse TIT2, TPE1, TALB, TYER, TRCK, TCON)
static esp_err_t parse_id3v2(FILE *f, sx_sd_music_metadata_t *metadata) {
    id3v2_header_t header;
    fseek(f, 0, SEEK_SET);
    if (fread(&header, sizeof(header), 1, f) != 1) {
        return ESP_FAIL;
    }
    
    uint32_t tag_size = read_synchsafe_uint32((uint8_t *)&header.size);
    if (tag_size > 1024 * 1024) {  // Sanity check: max 1MB tag
        return ESP_ERR_INVALID_SIZE;
    }
    
    uint8_t *tag_data = (uint8_t *)malloc(tag_size);
    if (!tag_data) {
        return ESP_ERR_NO_MEM;
    }
    
    if (fread(tag_data, 1, tag_size, f) != tag_size) {
        free(tag_data);
        return ESP_FAIL;
    }
    
    // Parse frames
    size_t pos = 0;
    while (pos + 10 <= tag_size) {
        id3v2_frame_header_t *frame = (id3v2_frame_header_t *)(tag_data + pos);
        
        // Check for valid frame ID (4 ASCII characters)
        if (frame->frame_id[0] < 0x20 || frame->frame_id[0] > 0x7E) {
            break;  // Invalid frame, stop parsing
        }
        
        uint32_t frame_size = read_synchsafe_uint32((uint8_t *)&frame->size);
        pos += 10;
        
        if (pos + frame_size > tag_size) {
            break;  // Frame extends beyond tag
        }
        
        // Parse text frames (skip encoding byte, read UTF-8 string)
        if (frame_size > 1) {
            // uint8_t encoding = tag_data[pos]; // Reserved for future encoding support
            size_t text_start = pos + 1;
            size_t text_len = frame_size - 1;
            
            // Find null terminator
            size_t null_pos = text_start;
            while (null_pos < pos + frame_size && tag_data[null_pos] != 0) {
                null_pos++;
            }
            if (null_pos < pos + frame_size) {
                text_len = null_pos - text_start;
            }
            
            char *text = (char *)malloc(text_len + 1);
            if (text) {
                memcpy(text, tag_data + text_start, text_len);
                text[text_len] = '\0';
                
                // Map frame IDs to metadata fields
                if (strncmp((char *)frame->frame_id, "TIT2", 4) == 0) {
                    strncpy(metadata->title, text, sizeof(metadata->title) - 1);
                    metadata->title[sizeof(metadata->title) - 1] = '\0';
                } else if (strncmp((char *)frame->frame_id, "TPE1", 4) == 0) {
                    strncpy(metadata->artist, text, sizeof(metadata->artist) - 1);
                    metadata->artist[sizeof(metadata->artist) - 1] = '\0';
                } else if (strncmp((char *)frame->frame_id, "TALB", 4) == 0) {
                    strncpy(metadata->album, text, sizeof(metadata->album) - 1);
                    metadata->album[sizeof(metadata->album) - 1] = '\0';
                } else if (strncmp((char *)frame->frame_id, "TYER", 4) == 0 || 
                          strncmp((char *)frame->frame_id, "TDRC", 4) == 0) {
                    metadata->year = (uint16_t)atoi(text);
                } else if (strncmp((char *)frame->frame_id, "TRCK", 4) == 0) {
                    metadata->track = (uint8_t)atoi(text);
                } else if (strncmp((char *)frame->frame_id, "TCON", 4) == 0) {
                    strncpy(metadata->genre, text, sizeof(metadata->genre) - 1);
                    metadata->genre[sizeof(metadata->genre) - 1] = '\0';
                }
                
                free(text);
            }
        }
        
        pos += frame_size;
    }
    
    free(tag_data);
    metadata->has_metadata = true;
    return ESP_OK;
}

// Get metadata for a music file
esp_err_t sx_sd_music_get_metadata(const char *file_path, sx_sd_music_metadata_t *metadata) {
    if (!metadata || !file_path) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(metadata, 0, sizeof(*metadata));
    
    const char *mount_point = sx_sd_get_mount_point();
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s%s", mount_point, file_path);
    
    FILE *f = fopen(full_path, "rb");
    if (!f) {
        return ESP_FAIL;
    }
    
    // Try ID3v2 first (at start of file)
    if (has_id3v2(f)) {
        parse_id3v2(f, metadata);
    }
    
    // Try ID3v1 (at end of file) - will overwrite ID3v2 if ID3v1 exists
    if (has_id3v1(f)) {
        parse_id3v1(f, metadata);
    }
    
    fclose(f);
    return ESP_OK;
}

// List music files with metadata
esp_err_t sx_sd_music_list_files(const char *dir_path, sx_sd_music_entry_t *entries, 
                                  size_t max_count, size_t *out_count) {
    if (!entries || max_count == 0 || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    sx_sd_file_entry_t *file_entries = (sx_sd_file_entry_t *)malloc(max_count * sizeof(sx_sd_file_entry_t));
    if (!file_entries) {
        return ESP_ERR_NO_MEM;
    }
    
    size_t file_count = 0;
    esp_err_t ret = sx_sd_list_files(dir_path, file_entries, max_count, &file_count);
    if (ret != ESP_OK) {
        free(file_entries);
        return ret;
    }
    
    const char *mount_point = sx_sd_get_mount_point();
    size_t music_count = 0;
    
    for (size_t i = 0; i < file_count && music_count < max_count; i++) {
        memset(&entries[music_count], 0, sizeof(sx_sd_music_entry_t));
        
        // Build full path
        char full_path[512];
        if (dir_path[strlen(dir_path) - 1] == '/') {
            snprintf(full_path, sizeof(full_path), "%s%s%s", mount_point, dir_path, file_entries[i].name);
        } else {
            snprintf(full_path, sizeof(full_path), "%s%s/%s", mount_point, dir_path, file_entries[i].name);
        }
        
        // Copy path and name
        strncpy(entries[music_count].path, full_path + strlen(mount_point), sizeof(entries[music_count].path) - 1);
        strncpy(entries[music_count].name, file_entries[i].name, sizeof(entries[music_count].name) - 1);
        entries[music_count].is_dir = file_entries[i].is_dir;
        
        // Parse metadata for music files (not directories)
        if (!file_entries[i].is_dir) {
            const char *ext = strrchr(file_entries[i].name, '.');
            if (ext && (strcasecmp(ext, ".mp3") == 0 || strcasecmp(ext, ".wav") == 0)) {
                sx_sd_music_get_metadata(entries[music_count].path, &entries[music_count].metadata);
            }
        }
        
        music_count++;
    }
    
    free(file_entries);
    *out_count = music_count;
    return ESP_OK;
}

// Case-insensitive string comparison (reserved for future UTF-8 support)
// static int strcasecmp_utf8(const char *s1, const char *s2) {
//     while (*s1 && *s2) {
//         int c1 = tolower((unsigned char)*s1);
//         int c2 = tolower((unsigned char)*s2);
//         if (c1 != c2) {
//             return c1 - c2;
//         }
//         s1++;
//         s2++;
//     }
//     return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
// }

// Search music files
esp_err_t sx_sd_music_search(const char *query, sx_sd_music_entry_t *results, 
                              size_t max_results, size_t *out_count) {
    if (!query || !results || max_results == 0 || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Simple recursive search (can be optimized later)
    sx_sd_music_entry_t *all_entries = (sx_sd_music_entry_t *)malloc(max_results * sizeof(sx_sd_music_entry_t));
    if (!all_entries) {
        return ESP_ERR_NO_MEM;
    }
    
    size_t all_count = 0;
    esp_err_t ret = sx_sd_music_list_files("/", all_entries, max_results, &all_count);
    if (ret != ESP_OK) {
        free(all_entries);
        return ret;
    }
    
    size_t match_count = 0;
    for (size_t i = 0; i < all_count && match_count < max_results; i++) {
        if (all_entries[i].is_dir) {
            continue;  // Skip directories
        }
        
        // Search in name, title, artist, album
        bool matches = false;
        if (strcasestr(all_entries[i].name, query) != NULL) {
            matches = true;
        } else if (all_entries[i].metadata.has_metadata) {
            if (strcasestr(all_entries[i].metadata.title, query) != NULL ||
                strcasestr(all_entries[i].metadata.artist, query) != NULL ||
                strcasestr(all_entries[i].metadata.album, query) != NULL) {
                matches = true;
            }
        }
        
        if (matches) {
            memcpy(&results[match_count], &all_entries[i], sizeof(sx_sd_music_entry_t));
            match_count++;
        }
    }
    
    free(all_entries);
    *out_count = match_count;
    return ESP_OK;
}

// Get tracks by genre
esp_err_t sx_sd_music_get_by_genre(const char *genre, sx_sd_music_entry_t *results, 
                                    size_t max_results, size_t *out_count) {
    if (!genre || !results || max_results == 0 || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    sx_sd_music_entry_t *all_entries = (sx_sd_music_entry_t *)malloc(max_results * sizeof(sx_sd_music_entry_t));
    if (!all_entries) {
        return ESP_ERR_NO_MEM;
    }
    
    size_t all_count = 0;
    esp_err_t ret = sx_sd_music_list_files("/", all_entries, max_results, &all_count);
    if (ret != ESP_OK) {
        free(all_entries);
        return ret;
    }
    
    size_t match_count = 0;
    for (size_t i = 0; i < all_count && match_count < max_results; i++) {
        if (all_entries[i].is_dir || !all_entries[i].metadata.has_metadata) {
            continue;
        }
        
        if (strcasecmp(all_entries[i].metadata.genre, genre) == 0) {
            memcpy(&results[match_count], &all_entries[i], sizeof(sx_sd_music_entry_t));
            match_count++;
        }
    }
    
    free(all_entries);
    *out_count = match_count;
    return ESP_OK;
}

// Create playlist by genre
esp_err_t sx_sd_music_create_genre_playlist(const char *genre, sx_sd_playlist_t *playlist) {
    if (!genre || !playlist) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(playlist, 0, sizeof(*playlist));
    strncpy(playlist->name, genre, sizeof(playlist->name) - 1);
    strncpy(playlist->genre, genre, sizeof(playlist->genre) - 1);
    
    sx_sd_music_entry_t *entries = (sx_sd_music_entry_t *)malloc(256 * sizeof(sx_sd_music_entry_t));
    if (!entries) {
        return ESP_ERR_NO_MEM;
    }
    
    size_t count = 0;
    esp_err_t ret = sx_sd_music_get_by_genre(genre, entries, 256, &count);
    if (ret != ESP_OK) {
        free(entries);
        return ret;
    }
    
    playlist->track_count = count;
    playlist->track_paths = (char **)malloc(count * sizeof(char *));
    if (!playlist->track_paths) {
        free(entries);
        return ESP_ERR_NO_MEM;
    }
    
    for (size_t i = 0; i < count; i++) {
        size_t path_len = strlen(entries[i].path) + 1;
        playlist->track_paths[i] = (char *)malloc(path_len);
        if (playlist->track_paths[i]) {
            // Security: Use strncpy with explicit size (already malloc'd exact size, so safe)
            strncpy(playlist->track_paths[i], entries[i].path, path_len - 1);
            playlist->track_paths[i][path_len - 1] = '\0';  // Ensure null termination
        }
    }
    
    free(entries);
    return ESP_OK;
}

// Free playlist memory
void sx_sd_music_free_playlist(sx_sd_playlist_t *playlist) {
    if (!playlist) {
        return;
    }
    
    if (playlist->track_paths) {
        for (size_t i = 0; i < playlist->track_count; i++) {
            if (playlist->track_paths[i]) {
                free(playlist->track_paths[i]);
            }
        }
        free(playlist->track_paths);
        playlist->track_paths = NULL;
    }
    playlist->track_count = 0;
}

// Get cover art info (simplified - would need full ID3v2 APIC frame parsing)
esp_err_t sx_sd_music_get_cover_art_info(const char *file_path, uint32_t *offset, 
                                         uint32_t *size, char *mime_type, size_t mime_type_size) {
    // Future: Implement full ID3v2 APIC frame parsing
    // This requires parsing ID3v2 tag structure to find APIC frame
    // and extract cover art data (JPEG/PNG) with proper MIME type detection
    // For now, return not found (simplified implementation)
    return ESP_ERR_NOT_FOUND;
}

// Initialize service (no-op for now)
esp_err_t sx_sd_music_service_init(void) {
    return ESP_OK;
}



