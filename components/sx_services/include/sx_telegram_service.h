#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Telegram Service
// Telegram bot integration for sending messages

// Telegram message types
typedef enum {
    SX_TELEGRAM_MSG_TEXT = 0,
    SX_TELEGRAM_MSG_PHOTO,
    SX_TELEGRAM_MSG_AUDIO,
    SX_TELEGRAM_MSG_DOCUMENT,
} sx_telegram_msg_type_t;

// Telegram configuration
typedef struct {
    const char *bot_token;       // Telegram bot token
    const char *chat_id;          // Target chat ID
    uint32_t timeout_ms;          // Request timeout in milliseconds
} sx_telegram_config_t;

// Message callback
typedef void (*sx_telegram_message_cb_t)(const char *message, void *user_data);

// Initialize Telegram service
// config: Telegram configuration
// Returns: ESP_OK on success
esp_err_t sx_telegram_service_init(const sx_telegram_config_t *config);

// Deinitialize Telegram service
void sx_telegram_service_deinit(void);

// Send text message
// message: Message text
// Returns: ESP_OK on success
esp_err_t sx_telegram_send_message(const char *message);

// Send photo
// photo_data: Photo data (JPEG/PNG)
// photo_size: Size of photo data
// caption: Optional caption
// Returns: ESP_OK on success
esp_err_t sx_telegram_send_photo(const uint8_t *photo_data, size_t photo_size, const char *caption);

// Send audio
// audio_data: Audio data
// audio_size: Size of audio data
// caption: Optional caption
// Returns: ESP_OK on success
esp_err_t sx_telegram_send_audio(const uint8_t *audio_data, size_t audio_size, const char *caption);

// Set message callback (for receiving messages)
void sx_telegram_set_message_callback(sx_telegram_message_cb_t callback, void *user_data);

// Check if Telegram service is initialized
bool sx_telegram_is_initialized(void);

#ifdef __cplusplus
}
#endif




