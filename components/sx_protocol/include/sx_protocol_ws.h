#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include "sx_protocol_audio.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *url;          // ws or wss URL
    const char *auth_token;   // optional HTTP header "Authorization: Bearer <token>"
    int reconnect_ms;         // reconnect delay (ms)
    int protocol_version;     // Protocol version (2 or 3, default 2)
} sx_protocol_ws_config_t;

// Initialize and connect WebSocket client
esp_err_t sx_protocol_ws_start(const sx_protocol_ws_config_t *cfg);

// Stop and cleanup
esp_err_t sx_protocol_ws_stop(void);

// Send UTF-8 text (JSON) to server
esp_err_t sx_protocol_ws_send_text(const char *text);

// Send audio packet (Opus encoded) to server
esp_err_t sx_protocol_ws_send_audio(const sx_audio_stream_packet_t *packet);

// Return true if connected
bool sx_protocol_ws_is_connected(void);

// Get server sample rate (from hello message)
uint32_t sx_protocol_ws_get_server_sample_rate(void);

// Get server frame duration (from hello message)
uint32_t sx_protocol_ws_get_server_frame_duration(void);

// Set audio callback for incoming audio packets
esp_err_t sx_protocol_ws_set_audio_callback(sx_protocol_audio_callback_t callback);

// Custom message callback for unknown message types
typedef void (*sx_protocol_ws_message_cb_t)(const char *type, const char *payload);
esp_err_t sx_protocol_ws_set_message_callback(sx_protocol_ws_message_cb_t callback);

#ifdef __cplusplus
}
#endif



