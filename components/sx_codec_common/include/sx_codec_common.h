#pragma once

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

// centralized codec-related type definitions for SimpleXL

/**
 * @brief Codec types supported by SimpleXL (audio format types)
 */
typedef enum {
    SX_CODEC_TYPE_UNKNOWN = 0,
    SX_CODEC_TYPE_AAC,
    SX_CODEC_TYPE_MP3,
    SX_CODEC_TYPE_FLAC,
    SX_CODEC_TYPE_OGG,
    SX_CODEC_TYPE_WAV,
    SX_CODEC_TYPE_OPUS,
    SX_CODEC_TYPE_PCM,  // Raw PCM
} sx_codec_type_t;

/**
 * @brief Hardware codec chip types (for hardware volume control)
 */
typedef enum {
    SX_HW_CODEC_NONE = 0,
    SX_HW_CODEC_ES8388,      // ES8388 audio codec chip
    SX_HW_CODEC_ES8311,      // ES8311 audio codec chip
    SX_HW_CODEC_PCM5102A,    // PCM5102A DAC (no hardware volume control)
} sx_hw_codec_chip_t;

// Backward compatibility aliases (deprecated, use sx_hw_codec_chip_t instead)
#define SX_CODEC_NONE        SX_HW_CODEC_NONE
#define SX_CODEC_ES8388       SX_HW_CODEC_ES8388
#define SX_CODEC_ES8311       SX_HW_CODEC_ES8311
#define SX_CODEC_PCM5102A    SX_HW_CODEC_PCM5102A

/**
 * @brief Audio file formats for local playback
 */
typedef enum {
    SX_AUDIO_FILE_FORMAT_UNKNOWN = 0,
    SX_AUDIO_FILE_FORMAT_WAV,
    SX_AUDIO_FILE_FORMAT_MP3,
    SX_AUDIO_FILE_FORMAT_AAC,
    SX_AUDIO_FILE_FORMAT_FLAC,
    SX_AUDIO_FILE_FORMAT_OGG,
    SX_AUDIO_FILE_FORMAT_OPUS,
    SX_AUDIO_FILE_FORMAT_RAW_PCM,
} sx_audio_file_format_t;

/**
 * @brief Codec error types
 * Mapped from esp_audio_err_t or other underlying libraries
 */
typedef enum {
    SX_CODEC_ERR_OK = ESP_OK,                           /*!< Success */
    SX_CODEC_ERR_FAIL = ESP_FAIL,                         /*!< Generic failure */
    SX_CODEC_ERR_MEM_LACK = ESP_ERR_NO_MEM,             /*!< Memory allocation failed */
    SX_CODEC_ERR_INVALID_ARG = ESP_ERR_INVALID_ARG,     /*!< Invalid argument */
    SX_CODEC_ERR_NOT_SUPPORTED = ESP_ERR_NOT_SUPPORTED, /*!< Feature not supported */
    SX_CODEC_ERR_NEEDS_MORE_DATA,                       /*!< Decoder needs more input data */
    SX_CODEC_ERR_OUTPUT_BUFFER_TOO_SMALL,               /*!< Output buffer is not large enough */
    SX_CODEC_ERR_EOF,                                   /*!< End of file or stream */
} sx_codec_err_t;

// Note: ESP_AUDIO_ERR_EOF and ESP_AUDIO_ERR_OK are defined in esp_audio_types.h
// from esp_audio_codec component. Use esp_audio_err_t directly when working with
// esp_audio_codec APIs. Use sx_codec_err_t for SimpleXL internal APIs.

#ifdef __cplusplus
}
#endif

