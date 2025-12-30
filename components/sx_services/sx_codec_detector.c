#include "sx_codec_detector.h"
#include <esp_log.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// static const char *TAG = "sx_codec_detector"; // Reserved for future logging

// Magic numbers for various codec formats
// FLAC: "fLaC" (0x66 0x4C 0x61 0x43)
#define FLAC_MAGIC "fLaC"
// WAV: "RIFF" (0x52 0x49 0x46 0x46)
#define WAV_MAGIC_RIFF "RIFF"
// MP3: ID3v2 tag "ID3" (0x49 0x44 0x33) or sync word 0xFF 0xFB/0xF3
// OGG: "OggS" (0x4F 0x67 0x67 0x53)

esp_err_t sx_codec_detect_from_content_type(const char *content_type, sx_codec_detect_result_t *result) {
    if (!content_type || !result) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(result, 0, sizeof(*result));
    result->detect_source = SX_CODEC_DETECT_SOURCE_CONTENT_TYPE;
    result->confidence = 0.9f; // High confidence from Content-Type
    
    // Convert to lowercase for comparison
    char content_type_lower[128];
    size_t len = strlen(content_type);
    if (len >= sizeof(content_type_lower)) len = sizeof(content_type_lower) - 1;
    for (size_t i = 0; i < len; i++) {
        content_type_lower[i] = tolower((unsigned char)content_type[i]);
    }
    content_type_lower[len] = '\0';
    
    // Detect format
    if (strstr(content_type_lower, "audio/aac") != NULL || 
        strstr(content_type_lower, "audio/aacp") != NULL ||
        strstr(content_type_lower, "audio/mp4") != NULL) {
        result->codec_type = SX_CODEC_TYPE_AAC;
        strncpy(result->detected_format, "AAC", sizeof(result->detected_format) - 1);
    } else if (strstr(content_type_lower, "audio/mpeg") != NULL ||
               strstr(content_type_lower, "audio/mp3") != NULL) {
        result->codec_type = SX_CODEC_TYPE_MP3;
        strncpy(result->detected_format, "MP3", sizeof(result->detected_format) - 1);
    } else if (strstr(content_type_lower, "audio/flac") != NULL ||
               strstr(content_type_lower, "audio/x-flac") != NULL) {
        result->codec_type = SX_CODEC_TYPE_FLAC;
        strncpy(result->detected_format, "FLAC", sizeof(result->detected_format) - 1);
    } else if (strstr(content_type_lower, "audio/ogg") != NULL ||
               strstr(content_type_lower, "audio/vorbis") != NULL ||
               strstr(content_type_lower, "audio/opus") != NULL) {
        if (strstr(content_type_lower, "opus") != NULL) {
            result->codec_type = SX_CODEC_TYPE_OPUS;
            strncpy(result->detected_format, "Opus", sizeof(result->detected_format) - 1);
        } else {
            result->codec_type = SX_CODEC_TYPE_OGG;
            strncpy(result->detected_format, "OGG/Vorbis", sizeof(result->detected_format) - 1);
        }
    } else if (strstr(content_type_lower, "audio/wav") != NULL ||
               strstr(content_type_lower, "audio/wave") != NULL ||
               strstr(content_type_lower, "audio/x-wav") != NULL) {
        result->codec_type = SX_CODEC_TYPE_WAV;
        strncpy(result->detected_format, "WAV", sizeof(result->detected_format) - 1);
    } else if (strstr(content_type_lower, "audio/pcm") != NULL ||
               strstr(content_type_lower, "audio/l16") != NULL) {
        result->codec_type = SX_CODEC_TYPE_PCM;
        strncpy(result->detected_format, "PCM", sizeof(result->detected_format) - 1);
    } else {
        result->codec_type = SX_CODEC_TYPE_UNKNOWN;
        strncpy(result->detected_format, "Unknown", sizeof(result->detected_format) - 1);
        result->confidence = 0.5f; // Lower confidence for unknown
    }
    
    return ESP_OK;
}

esp_err_t sx_codec_detect_from_extension(const char *file_path, sx_codec_detect_result_t *result) {
    if (!file_path || !result) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(result, 0, sizeof(*result));
    result->detect_source = SX_CODEC_DETECT_SOURCE_FILE_EXTENSION;
    result->confidence = 0.8f; // Good confidence from extension
    
    // Find extension
    const char *ext = strrchr(file_path, '.');
    if (!ext || ext[1] == '\0') {
        result->codec_type = SX_CODEC_TYPE_UNKNOWN;
        strncpy(result->detected_format, "Unknown", sizeof(result->detected_format) - 1);
        result->confidence = 0.3f;
        return ESP_OK;
    }
    
    ext++; // Skip '.'
    
    // Convert to lowercase
    char ext_lower[16];
    size_t len = strlen(ext);
    if (len >= sizeof(ext_lower)) len = sizeof(ext_lower) - 1;
    for (size_t i = 0; i < len; i++) {
        ext_lower[i] = tolower((unsigned char)ext[i]);
    }
    ext_lower[len] = '\0';
    
    // Detect from extension
    if (strcmp(ext_lower, "aac") == 0 || strcmp(ext_lower, "m4a") == 0) {
        result->codec_type = SX_CODEC_TYPE_AAC;
        strncpy(result->detected_format, "AAC", sizeof(result->detected_format) - 1);
    } else if (strcmp(ext_lower, "mp3") == 0) {
        result->codec_type = SX_CODEC_TYPE_MP3;
        strncpy(result->detected_format, "MP3", sizeof(result->detected_format) - 1);
    } else if (strcmp(ext_lower, "flac") == 0) {
        result->codec_type = SX_CODEC_TYPE_FLAC;
        strncpy(result->detected_format, "FLAC", sizeof(result->detected_format) - 1);
    } else if (strcmp(ext_lower, "ogg") == 0 || strcmp(ext_lower, "oga") == 0) {
        result->codec_type = SX_CODEC_TYPE_OGG;
        strncpy(result->detected_format, "OGG/Vorbis", sizeof(result->detected_format) - 1);
    } else if (strcmp(ext_lower, "opus") == 0) {
        result->codec_type = SX_CODEC_TYPE_OPUS;
        strncpy(result->detected_format, "Opus", sizeof(result->detected_format) - 1);
    } else if (strcmp(ext_lower, "wav") == 0 || strcmp(ext_lower, "wave") == 0) {
        result->codec_type = SX_CODEC_TYPE_WAV;
        strncpy(result->detected_format, "WAV", sizeof(result->detected_format) - 1);
    } else if (strcmp(ext_lower, "pcm") == 0 || strcmp(ext_lower, "raw") == 0) {
        result->codec_type = SX_CODEC_TYPE_PCM;
        strncpy(result->detected_format, "PCM", sizeof(result->detected_format) - 1);
    } else {
        result->codec_type = SX_CODEC_TYPE_UNKNOWN;
        strncpy(result->detected_format, "Unknown", sizeof(result->detected_format) - 1);
        result->confidence = 0.3f;
    }
    
    return ESP_OK;
}

esp_err_t sx_codec_detect_from_magic(const uint8_t *data, size_t data_size, sx_codec_detect_result_t *result) {
    if (!data || !result || data_size < 4) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(result, 0, sizeof(*result));
    result->detect_source = SX_CODEC_DETECT_SOURCE_MAGIC_NUMBER;
    result->confidence = 0.95f; // Very high confidence from magic numbers
    
    // Check FLAC: "fLaC" at offset 0
    if (data_size >= 4 && memcmp(data, FLAC_MAGIC, 4) == 0) {
        result->codec_type = SX_CODEC_TYPE_FLAC;
        strncpy(result->detected_format, "FLAC", sizeof(result->detected_format) - 1);
        return ESP_OK;
    }
    
    // Check WAV: "RIFF" at offset 0, "WAVE" at offset 8
    if (data_size >= 12 && memcmp(data, WAV_MAGIC_RIFF, 4) == 0) {
        if (data_size >= 12 && memcmp(data + 8, "WAVE", 4) == 0) {
            result->codec_type = SX_CODEC_TYPE_WAV;
            strncpy(result->detected_format, "WAV", sizeof(result->detected_format) - 1);
            return ESP_OK;
        }
    }
    
    // Check OGG: "OggS" at offset 0
    if (data_size >= 4 && memcmp(data, "OggS", 4) == 0) {
        result->codec_type = SX_CODEC_TYPE_OGG;
        strncpy(result->detected_format, "OGG/Vorbis", sizeof(result->detected_format) - 1);
        return ESP_OK;
    }
    
    // Check MP3: ID3v2 tag "ID3" at offset 0
    if (data_size >= 3 && memcmp(data, "ID3", 3) == 0) {
        result->codec_type = SX_CODEC_TYPE_MP3;
        strncpy(result->detected_format, "MP3", sizeof(result->detected_format) - 1);
        return ESP_OK;
    }
    
    // Check MP3: Sync word 0xFF 0xFB/0xF3/0xFA/0xF2 at offset 0
    if (data_size >= 2 && data[0] == 0xFF && 
        (data[1] == 0xFB || data[1] == 0xF3 || data[1] == 0xFA || data[1] == 0xF2)) {
        result->codec_type = SX_CODEC_TYPE_MP3;
        strncpy(result->detected_format, "MP3", sizeof(result->detected_format) - 1);
        return ESP_OK;
    }
    
    // Check AAC: ADTS header 0xFF 0xF1-0xF9 (but not MP3 sync word)
    // AAC ADTS sync word is 0xFF 0xF1-0xF9, frame sync is 12 bits
    if (data_size >= 2 && data[0] == 0xFF && 
        (data[1] & 0xF0) == 0xF0 && (data[1] & 0x0F) >= 0x01 && (data[1] & 0x0F) <= 0x09) {
        result->codec_type = SX_CODEC_TYPE_AAC;
        strncpy(result->detected_format, "AAC", sizeof(result->detected_format) - 1);
        return ESP_OK;
    }
    
    // Unknown
    result->codec_type = SX_CODEC_TYPE_UNKNOWN;
    strncpy(result->detected_format, "Unknown", sizeof(result->detected_format) - 1);
    result->confidence = 0.5f;
    
    return ESP_OK;
}

esp_err_t sx_codec_detect_from_url(const char *url, sx_codec_detect_result_t *result) {
    if (!url || !result) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(result, 0, sizeof(*result));
    result->detect_source = SX_CODEC_DETECT_SOURCE_URL;
    result->confidence = 0.7f; // Moderate confidence from URL
    
    // Try to detect from URL path/extension
    return sx_codec_detect_from_extension(url, result);
}

esp_err_t sx_codec_detect_smart(const char *content_type, const char *file_path,
                                const char *url, const uint8_t *magic_data, size_t magic_size,
                                sx_codec_detect_result_t *result) {
    if (!result) {
        return ESP_ERR_INVALID_ARG;
    }
    
    sx_codec_detect_result_t results[4] = {0};
    int result_count = 0;
    
    // Try Content-Type first (highest priority for HTTP streams)
    if (content_type && content_type[0] != '\0') {
        if (sx_codec_detect_from_content_type(content_type, &results[result_count]) == ESP_OK) {
            result_count++;
        }
    }
    
    // Try magic numbers (very reliable)
    if (magic_data && magic_size >= 4) {
        if (sx_codec_detect_from_magic(magic_data, magic_size, &results[result_count]) == ESP_OK) {
            result_count++;
        }
    }
    
    // Try file extension
    if (file_path && file_path[0] != '\0') {
        if (sx_codec_detect_from_extension(file_path, &results[result_count]) == ESP_OK) {
            result_count++;
        }
    }
    
    // Try URL
    if (url && url[0] != '\0' && (!file_path || strcmp(url, file_path) != 0)) {
        if (sx_codec_detect_from_url(url, &results[result_count]) == ESP_OK) {
            result_count++;
        }
    }
    
    // Choose best result (highest confidence)
    if (result_count == 0) {
        memset(result, 0, sizeof(*result));
        result->codec_type = SX_CODEC_TYPE_UNKNOWN;
        result->confidence = 0.0f;
        strncpy(result->detected_format, "Unknown", sizeof(result->detected_format) - 1);
        return ESP_OK;
    }
    
    // Find result with highest confidence
    *result = results[0];
    for (int i = 1; i < result_count; i++) {
        if (results[i].confidence > result->confidence) {
            *result = results[i];
        } else if (results[i].confidence == result->confidence &&
                   results[i].codec_type != SX_CODEC_TYPE_UNKNOWN &&
                   result->codec_type == SX_CODEC_TYPE_UNKNOWN) {
            // Prefer non-unknown if confidence is same
            *result = results[i];
        }
    }
    
    // If we have multiple results, increase confidence if they agree
    if (result_count > 1) {
        int agreement_count = 0;
        for (int i = 0; i < result_count; i++) {
            if (results[i].codec_type == result->codec_type) {
                agreement_count++;
            }
        }
        if (agreement_count > 1) {
            result->confidence = fminf(1.0f, result->confidence + 0.1f * (agreement_count - 1));
        }
    }
    
    return ESP_OK;
}

const char *sx_codec_type_to_string(sx_codec_type_t codec_type) {
    switch (codec_type) {
        case SX_CODEC_TYPE_AAC: return "AAC";
        case SX_CODEC_TYPE_MP3: return "MP3";
        case SX_CODEC_TYPE_FLAC: return "FLAC";
        case SX_CODEC_TYPE_OGG: return "OGG";
        case SX_CODEC_TYPE_WAV: return "WAV";
        case SX_CODEC_TYPE_OPUS: return "Opus";
        case SX_CODEC_TYPE_PCM: return "PCM";
        case SX_CODEC_TYPE_UNKNOWN: return "Unknown";
        default: return "Unknown";
    }
}

bool sx_codec_type_is_supported(sx_codec_type_t codec_type) {
    switch (codec_type) {
        case SX_CODEC_TYPE_AAC:
        case SX_CODEC_TYPE_MP3:
        case SX_CODEC_TYPE_FLAC:
        case SX_CODEC_TYPE_WAV:
        case SX_CODEC_TYPE_PCM:
            return true; // These have decoders implemented
        case SX_CODEC_TYPE_OPUS:
            // Opus decoder may be available via esp-opus-encoder
            #ifdef CONFIG_SX_CODEC_OPUS_ENABLE
            return true;
            #else
            return false;
            #endif
        case SX_CODEC_TYPE_OGG:
        case SX_CODEC_TYPE_UNKNOWN:
        default:
            return false; // Not yet supported
    }
}

// Convert sx_codec_type_t to sx_audio_file_format_t
static sx_audio_file_format_t codec_type_to_file_format(sx_codec_type_t codec_type) {
    switch (codec_type) {
        case SX_CODEC_TYPE_MP3:
            return SX_AUDIO_FILE_FORMAT_MP3;
        case SX_CODEC_TYPE_AAC:
            return SX_AUDIO_FILE_FORMAT_AAC;
        case SX_CODEC_TYPE_FLAC:
            return SX_AUDIO_FILE_FORMAT_FLAC;
        case SX_CODEC_TYPE_WAV:
            return SX_AUDIO_FILE_FORMAT_WAV;
        case SX_CODEC_TYPE_OGG:
            return SX_AUDIO_FILE_FORMAT_OGG;
        case SX_CODEC_TYPE_OPUS:
            return SX_AUDIO_FILE_FORMAT_OPUS;
        case SX_CODEC_TYPE_PCM:
            return SX_AUDIO_FILE_FORMAT_RAW_PCM;
        case SX_CODEC_TYPE_UNKNOWN:
        default:
            return SX_AUDIO_FILE_FORMAT_UNKNOWN;
    }
}

sx_audio_file_format_t sx_codec_detect_file_format(const char *file_path, FILE *file_handle) {
    sx_codec_detect_result_t result = {0};
    uint8_t magic_data[16] = {0};
    size_t magic_size = 0;
    
    // Try to read magic bytes from file if handle is provided
    if (file_handle != NULL) {
        long current_pos = ftell(file_handle);
        if (current_pos >= 0) {
            size_t read = fread(magic_data, 1, sizeof(magic_data), file_handle);
            if (read > 0) {
                magic_size = read;
            }
            // Restore file position
            fseek(file_handle, current_pos, SEEK_SET);
        }
    }
    
    // Use smart detection
    esp_err_t ret = sx_codec_detect_smart(
        NULL,           // content_type
        file_path,      // file_path
        NULL,           // url
        magic_size > 0 ? magic_data : NULL,  // magic_data
        magic_size,     // magic_size
        &result
    );
    
    if (ret != ESP_OK) {
        return SX_AUDIO_FILE_FORMAT_UNKNOWN;
    }
    
    return codec_type_to_file_format(result.codec_type);
}

bool sx_codec_is_wav_header(const uint8_t *hdr) {
    if (hdr == NULL) {
        return false;
    }
    // Check for "RIFF" magic number at offset 0
    return (hdr[0] == 'R' && hdr[1] == 'I' && hdr[2] == 'F' && hdr[3] == 'F');
}

