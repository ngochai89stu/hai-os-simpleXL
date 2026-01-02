#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include "sx_protocol_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

// Listening modes (from reference repo)
typedef enum {
    SX_LISTENING_MODE_AUTO_STOP = 0,
    SX_LISTENING_MODE_MANUAL_STOP,
    SX_LISTENING_MODE_REALTIME,
} sx_listening_mode_t;

// Abort reasons (from reference repo)
typedef enum {
    SX_ABORT_REASON_NONE = 0,
    SX_ABORT_REASON_WAKE_WORD_DETECTED,
} sx_abort_reason_t;

typedef struct sx_protocol_base sx_protocol_base_t;

typedef struct {
    esp_err_t (*start)(sx_protocol_base_t *self, const void *config);
    esp_err_t (*stop)(sx_protocol_base_t *self);
    bool (*is_connected)(const sx_protocol_base_t *self);

    esp_err_t (*send_text)(sx_protocol_base_t *self, const char *text);
    esp_err_t (*send_audio)(sx_protocol_base_t *self, const sx_audio_stream_packet_t *packet);

    esp_err_t (*open_audio_channel)(sx_protocol_base_t *self);
    void (*close_audio_channel)(sx_protocol_base_t *self);
    bool (*is_audio_channel_opened)(const sx_protocol_base_t *self);

    uint32_t (*get_server_sample_rate)(const sx_protocol_base_t *self);
    uint32_t (*get_server_frame_duration)(const sx_protocol_base_t *self);
    const char* (*get_session_id)(const sx_protocol_base_t *self);

    bool (*is_timeout)(const sx_protocol_base_t *self);
    void (*set_error)(sx_protocol_base_t *self, const char *error_msg);

    esp_err_t (*set_message_callback)(sx_protocol_base_t *self, void *callback);
    esp_err_t (*set_audio_callback)(sx_protocol_base_t *self, sx_protocol_audio_callback_t callback);

    esp_err_t (*send_wake_word_detected)(sx_protocol_base_t *self, const char *wake_word);
    esp_err_t (*send_start_listening)(sx_protocol_base_t *self, sx_listening_mode_t mode);
    esp_err_t (*send_stop_listening)(sx_protocol_base_t *self);
    esp_err_t (*send_abort_speaking)(sx_protocol_base_t *self, sx_abort_reason_t reason);
    esp_err_t (*send_mcp_message)(sx_protocol_base_t *self, const char *payload);
} sx_protocol_ops_t;

struct sx_protocol_base {
    const sx_protocol_ops_t *ops;
    void *impl;
};

// Convenience macros (used by chatbot service)
#define SX_PROTOCOL_OPEN_AUDIO_CHANNEL(self) \
    ((self) && (self)->ops && (self)->ops->open_audio_channel ? (self)->ops->open_audio_channel((self)) : ESP_ERR_INVALID_ARG)

#define SX_PROTOCOL_CLOSE_AUDIO_CHANNEL(self) \
    do { if ((self) && (self)->ops && (self)->ops->close_audio_channel) (self)->ops->close_audio_channel((self)); } while(0)

#define SX_PROTOCOL_IS_AUDIO_CHANNEL_OPENED(self) \
    ((self) && (self)->ops && (self)->ops->is_audio_channel_opened ? (self)->ops->is_audio_channel_opened((self)) : false)

#define SX_PROTOCOL_SEND_MCP_MESSAGE(self, payload) \
    ((self) && (self)->ops && (self)->ops->send_mcp_message ? (self)->ops->send_mcp_message((self), (payload)) : ESP_ERR_INVALID_ARG)

#define SX_PROTOCOL_SEND_WAKE_WORD_DETECTED(self, wk) \
    ((self) && (self)->ops && (self)->ops->send_wake_word_detected ? (self)->ops->send_wake_word_detected((self), (wk)) : ESP_ERR_INVALID_ARG)

#define SX_PROTOCOL_SEND_START_LISTENING(self, mode) \
    ((self) && (self)->ops && (self)->ops->send_start_listening ? (self)->ops->send_start_listening((self), (mode)) : ESP_ERR_INVALID_ARG)

#define SX_PROTOCOL_SEND_STOP_LISTENING(self) \
    ((self) && (self)->ops && (self)->ops->send_stop_listening ? (self)->ops->send_stop_listening((self)) : ESP_ERR_INVALID_ARG)

#define SX_PROTOCOL_SEND_ABORT_SPEAKING(self, reason) \
    ((self) && (self)->ops && (self)->ops->send_abort_speaking ? (self)->ops->send_abort_speaking((self), (reason)) : ESP_ERR_INVALID_ARG)

#ifdef __cplusplus
}
#endif
