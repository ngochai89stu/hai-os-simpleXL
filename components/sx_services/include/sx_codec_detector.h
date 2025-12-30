#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Smart Codec Engine
// Auto-detect codec type from data/URL/headers and select appropriate decoder

#include "sx_codec_common.h"

// Supported codec types are defined in sx_codec_common.h

// Detection source
typedef enum {
    SX_CODEC_DETECT_SOURCE_NONE = 0,
    SX_CODEC_DETECT_SOURCE_CONTENT_TYPE,  // From HTTP Content-Type header
    SX_CODEC_DETECT_SOURCE_FILE_EXTENSION, // From file extension
    SX_CODEC_DETECT_SOURCE_MAGIC_NUMBER,   // From file header magic numbers
    SX_CODEC_DETECT_SOURCE_URL,            // From URL pattern
} sx_codec_detect_source_t;

// Codec detection result
typedef struct {
    sx_codec_type_t codec_type;
    sx_codec_detect_source_t detect_source;
    float confidence;  // 0.0-1.0, confidence in detection
    char detected_format[32];  // Human-readable format string
} sx_codec_detect_result_t;

// Detect codec from Content-Type header (HTTP)
// content_type: HTTP Content-Type header value (e.g., "audio/aac")
// result: Output detection result
// Returns: ESP_OK on success
esp_err_t sx_codec_detect_from_content_type(const char *content_type, sx_codec_detect_result_t *result);

// Detect codec from file extension
// file_path: File path or filename
// result: Output detection result
// Returns: ESP_OK on success
esp_err_t sx_codec_detect_from_extension(const char *file_path, sx_codec_detect_result_t *result);

// Detect codec from magic numbers (file header)
// data: File header data (at least 16 bytes recommended)
// data_size: Size of data buffer
// result: Output detection result
// Returns: ESP_OK on success
esp_err_t sx_codec_detect_from_magic(const uint8_t *data, size_t data_size, sx_codec_detect_result_t *result);

// Detect codec from URL pattern
// url: URL string
// result: Output detection result
// Returns: ESP_OK on success
esp_err_t sx_codec_detect_from_url(const char *url, sx_codec_detect_result_t *result);

// Smart detection - tries multiple methods
// content_type: HTTP Content-Type (optional, can be NULL)
// file_path: File path (optional, can be NULL)
// url: URL (optional, can be NULL)
// magic_data: File header data (optional, can be NULL)
// magic_size: Size of magic_data (0 if not provided)
// result: Output detection result
// Returns: ESP_OK on success
esp_err_t sx_codec_detect_smart(const char *content_type, const char *file_path,
                                const char *url, const uint8_t *magic_data, size_t magic_size,
                                sx_codec_detect_result_t *result);

// Get codec type name string
const char *sx_codec_type_to_string(sx_codec_type_t codec_type);

// Check if codec type is supported (decoder available)
bool sx_codec_type_is_supported(sx_codec_type_t codec_type);

// Detect file format from file path and file handle
// This is a convenience function that combines multiple detection methods
// file_path: Path to the file (optional, can be NULL)
// file_handle: FILE* handle to the file (optional, can be NULL)
// Returns: Detected file format
sx_audio_file_format_t sx_codec_detect_file_format(const char *file_path, FILE *file_handle);

// Check if data buffer contains a WAV header
// hdr: Buffer containing at least 4 bytes
// Returns: true if WAV header detected
bool sx_codec_is_wav_header(const uint8_t *hdr);

#ifdef __cplusplus
}
#endif


