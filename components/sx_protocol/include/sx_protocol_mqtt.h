#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: MQTT Protocol support for MCP communication

// MQTT protocol configuration
typedef struct {
    const char *broker_uri;      // MQTT broker URI (e.g., "mqtt://broker.example.com:1883")
    const char *client_id;        // MQTT client ID (NULL for auto-generated)
    const char *username;         // MQTT username (NULL if not required)
    const char *password;         // MQTT password (NULL if not required)
    const char *topic_prefix;     // Topic prefix (e.g., "sx/device/")
    uint32_t keepalive_sec;       // Keepalive interval in seconds (default: 60)
    bool clean_session;           // Clean session flag (default: true)
} sx_protocol_mqtt_config_t;

// Initialize MQTT protocol
esp_err_t sx_protocol_mqtt_init(const sx_protocol_mqtt_config_t *config);

// Start MQTT connection
esp_err_t sx_protocol_mqtt_start(void);

// Stop MQTT connection
esp_err_t sx_protocol_mqtt_stop(void);

// Check if MQTT is connected
bool sx_protocol_mqtt_is_connected(void);

// Subscribe to a topic
esp_err_t sx_protocol_mqtt_subscribe(const char *topic, int qos);

// Unsubscribe from a topic
esp_err_t sx_protocol_mqtt_unsubscribe(const char *topic);

// Publish message to a topic
esp_err_t sx_protocol_mqtt_publish(const char *topic, const char *data, int len, int qos, bool retain);

// Set message callback (called when message received)
typedef void (*sx_protocol_mqtt_message_cb_t)(const char *topic, const uint8_t *data, int len);
esp_err_t sx_protocol_mqtt_set_message_callback(sx_protocol_mqtt_message_cb_t callback);

// Audio streaming support (UDP)
#include "sx_protocol_audio.h"

// Send audio packet (Opus encoded) via UDP
esp_err_t sx_protocol_mqtt_send_audio(const sx_audio_stream_packet_t *packet);

// Set audio callback for incoming audio packets (from UDP)
esp_err_t sx_protocol_mqtt_set_audio_callback(sx_protocol_audio_callback_t callback);

// Get server sample rate (from hello message)
uint32_t sx_protocol_mqtt_get_server_sample_rate(void);

// Get server frame duration (from hello message)
uint32_t sx_protocol_mqtt_get_server_frame_duration(void);

// Check if audio channel (UDP) is opened
bool sx_protocol_mqtt_is_audio_channel_opened(void);

#ifdef __cplusplus
}
#endif



