#include "sx_protocol_ws.h"
#include "sx_protocol_audio.h"
#include "sx_mcp_server.h"
#include "sx_chatbot_service.h"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_event_string_pool.h"
#include <esp_websocket_client.h>
#include <esp_log.h>
#include <string.h>
#include <cJSON.h>
#include <arpa/inet.h>
#include <stdlib.h>

static const char *TAG = "sx_ws";

static esp_websocket_client_handle_t s_client = NULL;
static bool s_connected = false;
static sx_protocol_ws_config_t s_cfg = {0};
static int s_protocol_version = 2;  // Default protocol version
static uint32_t s_server_sample_rate = 16000;  // Default sample rate
static uint32_t s_server_frame_duration = 60;  // Default frame duration (ms)

// Callbacks for message handling
static sx_protocol_ws_message_cb_t s_message_callback = NULL;
static sx_protocol_audio_callback_t s_audio_callback = NULL;

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected");
            s_connected = true;
            // Emit connected event
            sx_event_t evt_conn = {
                .type = SX_EVT_CHATBOT_CONNECTED,
                .arg0 = 0,
                .ptr = NULL,
            };
            sx_dispatcher_post_event(&evt_conn);
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WebSocket disconnected");
            s_connected = false;
            // Emit disconnected event
            sx_event_t evt_disc = {
                .type = SX_EVT_CHATBOT_DISCONNECTED,
                .arg0 = 0,
                .ptr = NULL,
            };
            sx_dispatcher_post_event(&evt_disc);
            break;
        case WEBSOCKET_EVENT_DATA:
            if (data->op_code == 0x1) { // text frame
                char *payload = strndup((const char *)data->data_ptr, data->data_len);
                if (payload) {
                    // Parse JSON message
                    cJSON *root = cJSON_Parse(payload);
                    if (root != NULL) {
                        bool handled = sx_chatbot_handle_json_message(root, payload);

                        if (!handled) {
                            cJSON *type = cJSON_GetObjectItem(root, "type");
                            const char *msg_type = cJSON_IsString(type) ? type->valuestring : NULL;

                            if (msg_type && strcmp(msg_type, "hello") == 0) {
                                // Server hello message - parse audio params
                                cJSON *audio_params = cJSON_GetObjectItem(root, "audio_params");
                                if (audio_params != NULL) {
                                    cJSON *sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");
                                    if (cJSON_IsNumber(sample_rate)) {
                                        s_server_sample_rate = (uint32_t)sample_rate->valueint;
                                        ESP_LOGI(TAG, "Server sample rate: %lu Hz", s_server_sample_rate);
                                    }
                                    cJSON *frame_duration = cJSON_GetObjectItem(audio_params, "frame_duration");
                                    if (cJSON_IsNumber(frame_duration)) {
                                        s_server_frame_duration = (uint32_t)frame_duration->valueint;
                                        ESP_LOGI(TAG, "Server frame duration: %lu ms", s_server_frame_duration);
                                    }
                                }
                            } else {
                                // Unknown message type - pass to custom callback if set
                                if (msg_type && s_message_callback != NULL) {
                                    char *msg_str = cJSON_PrintUnformatted(root);
                                    if (msg_str) {
                                        s_message_callback(msg_type, msg_str);
                                        free(msg_str);
                                    }
                                } else if (msg_type) {
                                    ESP_LOGW(TAG, "Unknown message type: %s", msg_type);
                                }
                            }
                        }
                        cJSON_Delete(root);
                    } else {
                        // Not JSON - try MCP handler
                        sx_chatbot_handle_mcp_message(payload);
                    }
                    free(payload);
                }
            } else if (data->op_code == 0x2) { // binary frame
                // Handle binary audio data
                if (s_audio_callback != NULL && data->data_len > 0) {
                    if (s_protocol_version == 2) {
                        // Parse BinaryProtocol2
                        if (data->data_len >= sizeof(sx_binary_protocol_v2_t)) {
                            sx_binary_protocol_v2_t *bp2 = (sx_binary_protocol_v2_t *)data->data_ptr;
                            uint16_t version = ntohs(bp2->version);
                            uint32_t timestamp = ntohl(bp2->timestamp);
                            uint32_t payload_size = ntohl(bp2->payload_size);
                            
                            if (version == 2 && payload_size > 0 && 
                                data->data_len >= sizeof(sx_binary_protocol_v2_t) + payload_size) {
                                // Allocate packet
                                sx_audio_stream_packet_t packet = {0};
                                packet.sample_rate = s_server_sample_rate;
                                packet.frame_duration = s_server_frame_duration;
                                packet.timestamp = timestamp;
                                packet.payload_size = payload_size;
                                packet.payload = (uint8_t *)malloc(payload_size);
                                
                                if (packet.payload != NULL) {
                                    memcpy(packet.payload, bp2->payload, payload_size);
                                    s_audio_callback(&packet);
                                    free(packet.payload);
                                } else {
                                    ESP_LOGE(TAG, "Failed to allocate memory for audio packet");
                                }
                            }
                        }
                    } else if (s_protocol_version == 3) {
                        // Parse BinaryProtocol3
                        if (data->data_len >= sizeof(sx_binary_protocol_v3_t)) {
                            sx_binary_protocol_v3_t *bp3 = (sx_binary_protocol_v3_t *)data->data_ptr;
                            uint16_t payload_size = ntohs(bp3->payload_size);
                            
                            if (payload_size > 0 && 
                                data->data_len >= sizeof(sx_binary_protocol_v3_t) + payload_size) {
                                // Allocate packet
                                sx_audio_stream_packet_t packet = {0};
                                packet.sample_rate = s_server_sample_rate;
                                packet.frame_duration = s_server_frame_duration;
                                packet.timestamp = 0;  // v3 doesn't have timestamp
                                packet.payload_size = payload_size;
                                packet.payload = (uint8_t *)malloc(payload_size);
                                
                                if (packet.payload != NULL) {
                                    memcpy(packet.payload, bp3->payload, payload_size);
                                    s_audio_callback(&packet);
                                    free(packet.payload);
                                } else {
                                    ESP_LOGE(TAG, "Failed to allocate memory for audio packet");
                                }
                            }
                        }
                    } else {
                        // Default: raw Opus data
                        sx_audio_stream_packet_t packet = {0};
                        packet.sample_rate = s_server_sample_rate;
                        packet.frame_duration = s_server_frame_duration;
                        packet.timestamp = 0;
                        packet.payload_size = data->data_len;
                        packet.payload = (uint8_t *)malloc(data->data_len);
                        
                        if (packet.payload != NULL) {
                            memcpy(packet.payload, data->data_ptr, data->data_len);
                            s_audio_callback(&packet);
                            free(packet.payload);
                        }
                    }
                } else {
                    ESP_LOGD(TAG, "Binary frame received, len=%d (no audio callback)", data->data_len);
                }
            }
            break;
        default:
            break;
    }
}

// Callback for MCP server to send JSON back to cloud
static void mcp_send_cb(const char *payload) {
    if (s_client && s_connected && payload) {
        // Wrap MCP response in message format
        cJSON *mcp_msg = cJSON_CreateObject();
        cJSON_AddStringToObject(mcp_msg, "type", "mcp");
        cJSON_AddStringToObject(mcp_msg, "payload", payload);
        
        char *msg_str = cJSON_PrintUnformatted(mcp_msg);
        if (msg_str) {
            esp_err_t ret = esp_websocket_client_send_text(s_client, msg_str, strlen(msg_str), portMAX_DELAY);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send MCP response: %s", esp_err_to_name(ret));
            } else {
                ESP_LOGD(TAG, "Sent MCP response via WebSocket");
            }
            free(msg_str);
        }
        cJSON_Delete(mcp_msg);
    }
}

esp_err_t sx_protocol_ws_start(const sx_protocol_ws_config_t *cfg) {
    if (s_client) return ESP_OK;
    if (!cfg || !cfg->url) return ESP_ERR_INVALID_ARG;
    s_cfg = *cfg;
    s_protocol_version = (cfg->protocol_version > 0) ? cfg->protocol_version : 2;

    esp_websocket_client_config_t ws_cfg = {
        .uri = s_cfg.url,
        .disable_auto_reconnect = false,
        .reconnect_timeout_ms = (s_cfg.reconnect_ms > 0) ? s_cfg.reconnect_ms : 5000,
    };
    // Note: auth_token support requires custom header setup via esp_websocket_client_set_headers()
    //       Currently not needed, but can be added if authentication is required
    s_client = esp_websocket_client_init(&ws_cfg);
    if (!s_client) return ESP_ERR_NO_MEM;
    esp_websocket_register_events(s_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL);

    esp_err_t ret = esp_websocket_client_start(s_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ws start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register send callback to MCP server so responses get out
    sx_mcp_server_set_send_callback(mcp_send_cb);

    return ESP_OK;
}

esp_err_t sx_protocol_ws_stop(void) {
    if (!s_client) return ESP_OK;
    esp_websocket_client_stop(s_client);
    esp_websocket_client_destroy(s_client);
    s_client = NULL;
    s_connected = false;
    return ESP_OK;
}

esp_err_t sx_protocol_ws_send_text(const char *text) {
    if (!s_client || !s_connected || !text) return ESP_ERR_INVALID_STATE;
    esp_websocket_client_send_text(s_client, text, strlen(text), portMAX_DELAY);
    return ESP_OK;
}

bool sx_protocol_ws_is_connected(void) {
    return s_connected;
}

// Set custom message callback for unknown message types
esp_err_t sx_protocol_ws_set_message_callback(sx_protocol_ws_message_cb_t callback) {
    s_message_callback = callback;
    return ESP_OK;
}

// Send audio packet (Opus encoded) to server
esp_err_t sx_protocol_ws_send_audio(const sx_audio_stream_packet_t *packet) {
    if (!s_client || !s_connected || !packet || !packet->payload || packet->payload_size == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = ESP_OK;
    
    if (s_protocol_version == 2) {
        // BinaryProtocol2
        size_t total_size = sizeof(sx_binary_protocol_v2_t) + packet->payload_size;
        uint8_t *buffer = (uint8_t *)malloc(total_size);
        if (buffer == NULL) {
            return ESP_ERR_NO_MEM;
        }
        
        sx_binary_protocol_v2_t *bp2 = (sx_binary_protocol_v2_t *)buffer;
        bp2->version = htons(2);
        bp2->type = 0;  // 0 = OPUS audio
        bp2->reserved = 0;
        bp2->timestamp = htonl(packet->timestamp);
        bp2->payload_size = htonl(packet->payload_size);
        memcpy(bp2->payload, packet->payload, packet->payload_size);
        
        ret = esp_websocket_client_send_bin(s_client, (const char *)buffer, total_size, portMAX_DELAY);
        free(buffer);
    } else if (s_protocol_version == 3) {
        // BinaryProtocol3
        size_t total_size = sizeof(sx_binary_protocol_v3_t) + packet->payload_size;
        uint8_t *buffer = (uint8_t *)malloc(total_size);
        if (buffer == NULL) {
            return ESP_ERR_NO_MEM;
        }
        
        sx_binary_protocol_v3_t *bp3 = (sx_binary_protocol_v3_t *)buffer;
        bp3->type = 0;  // 0 = OPUS audio
        bp3->reserved = 0;
        bp3->payload_size = htons(packet->payload_size);
        memcpy(bp3->payload, packet->payload, packet->payload_size);
        
        ret = esp_websocket_client_send_bin(s_client, (const char *)buffer, total_size, portMAX_DELAY);
        free(buffer);
    } else {
        // Default: raw Opus data
        ret = esp_websocket_client_send_bin(s_client, (const char *)packet->payload, packet->payload_size, portMAX_DELAY);
    }
    
    return ret;
}

// Get server sample rate
uint32_t sx_protocol_ws_get_server_sample_rate(void) {
    return s_server_sample_rate;
}

// Get server frame duration
uint32_t sx_protocol_ws_get_server_frame_duration(void) {
    return s_server_frame_duration;
}

// Set audio callback for incoming audio packets
esp_err_t sx_protocol_ws_set_audio_callback(sx_protocol_audio_callback_t callback) {
    s_audio_callback = callback;
    return ESP_OK;
}

