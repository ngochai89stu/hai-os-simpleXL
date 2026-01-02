#include "sx_protocol_ws.h"
#include "sx_protocol_base.h"
#include "sx_protocol_audio.h"
#include "sx_protocol_common.h"
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
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "esp_timer.h"

static const char *TAG = "sx_ws";

// WebSocket implementation structure
typedef struct {
    esp_websocket_client_handle_t client;
    bool connected;
    sx_protocol_ws_config_t cfg;
    int protocol_version;
    uint32_t server_sample_rate;
    uint32_t server_frame_duration;
    char session_id[64];
    sx_protocol_ws_message_cb_t message_callback;
    sx_protocol_audio_callback_t audio_callback;
    uint32_t last_incoming_time;  // For timeout detection
    bool has_error;
    char error_message[128];
} sx_protocol_ws_impl_t;

// Global instance (singleton pattern for backward compatibility)
static sx_protocol_ws_impl_t s_ws_impl = {0};
static sx_protocol_base_t s_ws_base = {0};

// Reconnection state
static void schedule_ws_reconnect(void);
static bool s_reconnecting = false;
static uint32_t s_reconnect_attempts = 0;
static const uint32_t s_max_reconnect_attempts = 10;
static SemaphoreHandle_t s_reconnect_mutex = NULL;
static esp_timer_handle_t s_reconnect_timer = NULL;

// Legacy static variables (for backward compatibility)
static esp_websocket_client_handle_t s_client = NULL;
static bool s_connected = false;
static sx_protocol_ws_config_t s_cfg = {0};
static int s_protocol_version = 2;  // Default protocol version
static uint32_t s_server_sample_rate = 16000;  // Default sample rate
static uint32_t s_server_frame_duration = 60;  // Default frame duration (ms)

// Static buffer for audio packet sending (optimization: avoid malloc/free per packet)
#define MAX_AUDIO_PACKET_SIZE (4000 + sizeof(sx_binary_protocol_v2_t))  // Max Opus frame + protocol header
static uint8_t s_ws_audio_send_buffer[MAX_AUDIO_PACKET_SIZE];
static SemaphoreHandle_t s_ws_audio_buffer_mutex = NULL;

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
            schedule_ws_reconnect();
            ESP_LOGW(TAG, "WebSocket disconnected");
            s_connected = false;
            s_ws_impl.connected = false;
            
            // Check if it's an error disconnect
            if (s_ws_impl.has_error) {
                char *error_str = sx_event_alloc_string(s_ws_impl.error_message);
                if (error_str) {
                    sx_event_t evt_error = {
                        .type = SX_EVT_PROTOCOL_ERROR,
                        .arg0 = 0,
                        .ptr = error_str,
                    };
                    sx_dispatcher_post_event(&evt_error);
                }
            }
            
            // Emit disconnected event
            sx_event_t evt_disc = {
                .type = SX_EVT_CHATBOT_DISCONNECTED,
                .arg0 = 0,
                .ptr = NULL,
            };
            sx_dispatcher_post_event(&evt_disc);
            break;
        case WEBSOCKET_EVENT_ERROR:
            schedule_ws_reconnect();
            ESP_LOGE(TAG, "WebSocket error");
            s_ws_impl.has_error = true;
            strncpy(s_ws_impl.error_message, "WebSocket connection error", sizeof(s_ws_impl.error_message) - 1);
            s_ws_impl.error_message[sizeof(s_ws_impl.error_message) - 1] = '\0';
            
            // Emit error event
            char *error_str = sx_event_alloc_string("WebSocket connection error");
            if (error_str) {
                sx_event_t evt = {
                    .type = SX_EVT_PROTOCOL_ERROR,
                    .arg0 = 0,
                    .ptr = error_str,
                };
                sx_dispatcher_post_event(&evt);
            }
            break;
        case WEBSOCKET_EVENT_DATA:
                // Phase 2: Use common function to update last incoming time
                sx_protocol_common_update_last_incoming_time(&s_ws_impl.last_incoming_time);
            
            if (data->op_code == 0x1) { // text frame
                // Security: Limit payload size to prevent DoS
                #define MAX_WS_PAYLOAD_SIZE 8192
                size_t payload_size = (data->data_len > MAX_WS_PAYLOAD_SIZE) ? MAX_WS_PAYLOAD_SIZE : data->data_len;
                
                char *payload = strndup((const char *)data->data_ptr, payload_size);
                if (payload) {
                    // Security: Limit JSON size to prevent heap exhaustion
                    #define MAX_JSON_SIZE 4096
                    if (payload_size > MAX_JSON_SIZE) {
                        ESP_LOGW(TAG, "JSON payload too large: %zu bytes, truncating to %d", payload_size, MAX_JSON_SIZE);
                        payload[MAX_JSON_SIZE] = '\0';  // Truncate
                    }
                    
                    // Parse JSON message
                    cJSON *root = cJSON_Parse(payload);
                    if (root != NULL) {
                        bool handled = sx_chatbot_handle_json_message(root, payload);

                        if (!handled) {
                            cJSON *type = cJSON_GetObjectItem(root, "type");
                            const char *msg_type = cJSON_IsString(type) ? type->valuestring : NULL;

                            if (msg_type && strcmp(msg_type, "hello") == 0) {
                                // Server hello message - parse audio params and emit event
                                typedef struct {
                                    uint32_t server_sample_rate;
                                    uint32_t server_frame_duration;
                                    char session_id[64];
                                } hello_data_t;
                                
                                hello_data_t *hello = (hello_data_t *)malloc(sizeof(hello_data_t));
                                if (hello) {
                                    // Default values
                                    hello->server_sample_rate = 16000;
                                    hello->server_frame_duration = 60;
                                    hello->session_id[0] = '\0';
                                    
                                    // Parse audio_params
                                    cJSON *audio_params = cJSON_GetObjectItem(root, "audio_params");
                                    if (audio_params != NULL) {
                                        cJSON *sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");
                                        if (cJSON_IsNumber(sample_rate)) {
                                            hello->server_sample_rate = (uint32_t)sample_rate->valueint;
                                            s_server_sample_rate = hello->server_sample_rate;
                                            ESP_LOGI(TAG, "Server sample rate: %lu Hz", hello->server_sample_rate);
                                        }
                                        cJSON *frame_duration = cJSON_GetObjectItem(audio_params, "frame_duration");
                                        if (cJSON_IsNumber(frame_duration)) {
                                            hello->server_frame_duration = (uint32_t)frame_duration->valueint;
                                            s_server_frame_duration = hello->server_frame_duration;
                                            ESP_LOGI(TAG, "Server frame duration: %lu ms", hello->server_frame_duration);
                                        }
                                    }
                                    
                                    // Parse session_id
                                    cJSON *session_id = cJSON_GetObjectItem(root, "session_id");
                                    if (cJSON_IsString(session_id)) {
                                        strncpy(hello->session_id, session_id->valuestring, sizeof(hello->session_id) - 1);
                                        hello->session_id[sizeof(hello->session_id) - 1] = '\0';
                                        // Also update impl session_id
                                        strncpy(s_ws_impl.session_id, session_id->valuestring, sizeof(s_ws_impl.session_id) - 1);
                                        s_ws_impl.session_id[sizeof(s_ws_impl.session_id) - 1] = '\0';
                                        ESP_LOGI(TAG, "Server session_id: %s", hello->session_id);
                                    }
                                    
                                    // Emit hello received event
                                    sx_event_t evt = {
                                        .type = SX_EVT_PROTOCOL_HELLO_RECEIVED,
                                        .arg0 = 0,
                                        .ptr = hello,
                                    };
                                    sx_dispatcher_post_event(&evt);
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
                // Phase 2: Use common function to update last incoming time
                sx_protocol_common_update_last_incoming_time(&s_ws_impl.last_incoming_time);
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
    size_t total_size = 0;
    uint8_t *buffer = NULL;
    bool use_static_buffer = false;
    
    if (s_protocol_version == 2) {
        // BinaryProtocol2
        total_size = sizeof(sx_binary_protocol_v2_t) + packet->payload_size;
        
        // Try to use static buffer first (optimization: avoid malloc/free)
        if (total_size <= sizeof(s_ws_audio_send_buffer)) {
            if (s_ws_audio_buffer_mutex == NULL) {
                s_ws_audio_buffer_mutex = xSemaphoreCreateMutex();
            }
            
            if (s_ws_audio_buffer_mutex != NULL && 
                xSemaphoreTake(s_ws_audio_buffer_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                buffer = s_ws_audio_send_buffer;
                use_static_buffer = true;
            }
        }
        
        // Fallback to malloc if static buffer not available or too small
        if (buffer == NULL) {
            buffer = (uint8_t *)malloc(total_size);
            if (buffer == NULL) {
                return ESP_ERR_NO_MEM;
            }
        }
        
        sx_binary_protocol_v2_t *bp2 = (sx_binary_protocol_v2_t *)buffer;
        bp2->version = htons(2);
        bp2->type = 0;  // 0 = OPUS audio
        bp2->reserved = 0;
        bp2->timestamp = htonl(packet->timestamp);
        bp2->payload_size = htonl(packet->payload_size);
        memcpy(bp2->payload, packet->payload, packet->payload_size);
        
        ret = esp_websocket_client_send_bin(s_client, (const char *)buffer, total_size, portMAX_DELAY);
        
        if (use_static_buffer) {
            xSemaphoreGive(s_ws_audio_buffer_mutex);
        } else {
            free(buffer);
        }
    } else if (s_protocol_version == 3) {
        // BinaryProtocol3
        total_size = sizeof(sx_binary_protocol_v3_t) + packet->payload_size;
        
        // Try to use static buffer first
        if (total_size <= sizeof(s_ws_audio_send_buffer)) {
            if (s_ws_audio_buffer_mutex == NULL) {
                s_ws_audio_buffer_mutex = xSemaphoreCreateMutex();
            }
            
            if (s_ws_audio_buffer_mutex != NULL && 
                xSemaphoreTake(s_ws_audio_buffer_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                buffer = s_ws_audio_send_buffer;
                use_static_buffer = true;
            }
        }
        
        // Fallback to malloc if static buffer not available or too small
        if (buffer == NULL) {
            buffer = (uint8_t *)malloc(total_size);
            if (buffer == NULL) {
                return ESP_ERR_NO_MEM;
            }
        }
        
        sx_binary_protocol_v3_t *bp3 = (sx_binary_protocol_v3_t *)buffer;
        bp3->type = 0;  // 0 = OPUS audio
        bp3->reserved = 0;
        bp3->payload_size = htons(packet->payload_size);
        memcpy(bp3->payload, packet->payload, packet->payload_size);
        
        ret = esp_websocket_client_send_bin(s_client, (const char *)buffer, total_size, portMAX_DELAY);
        
        if (use_static_buffer) {
            xSemaphoreGive(s_ws_audio_buffer_mutex);
        } else {
            free(buffer);
        }
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

// ============================================================================
// Reconnection helper functions
// ============================================================================

static void ws_reconnect_timer_cb(void *arg) {
    (void)arg;
    if (sx_protocol_ws_is_connected()) {
        s_reconnecting = false;
        return; // already connected
    }
    ESP_LOGI(TAG, "Reconnecting to WebSocket (attempt %lu)", (unsigned long)s_reconnect_attempts);
    esp_err_t ret = sx_protocol_ws_start(&s_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WebSocket reconnected");
        s_reconnecting = false;
        s_reconnect_attempts = 0;
    } else {
        // schedule next attempt
        s_reconnecting = false;
        // Will be rescheduled in schedule_ws_reconnect()
        sx_event_t evt_err = {
            .type = SX_EVT_PROTOCOL_ERROR,
            .arg0 = 0,
            .ptr = sx_event_alloc_string("Reconnect failed"),
        };
        sx_dispatcher_post_event(&evt_err);
        schedule_ws_reconnect();
    }
}

static void schedule_ws_reconnect(void) {
    if (!s_reconnect_mutex) {
        s_reconnect_mutex = xSemaphoreCreateMutex();
    }
    if (!xSemaphoreTake(s_reconnect_mutex, 0)) return;
    if (s_reconnecting || s_reconnect_attempts >= s_max_reconnect_attempts) {
        xSemaphoreGive(s_reconnect_mutex);
        return;
    }
    s_reconnecting = true;
    s_reconnect_attempts++;
    uint32_t attempt = s_reconnect_attempts;
    uint64_t delay_us = (uint64_t)(5000 * attempt) * 1000ULL; // exponential back-off (ms→us)

    // Emit reconnecting event
    sx_event_t evt = {
        .type = SX_EVT_PROTOCOL_RECONNECTING,
        .arg0 = attempt,
        .ptr = NULL,
    };
    sx_dispatcher_post_event(&evt);

    if (!s_reconnect_timer) {
        esp_timer_create_args_t tmr_cfg = {
            .callback = ws_reconnect_timer_cb,
            .arg = NULL,
            .name = "ws_reconnect"
        };
        esp_timer_create(&tmr_cfg, &s_reconnect_timer);
    }
    esp_timer_start_once(s_reconnect_timer, delay_us);
    xSemaphoreGive(s_reconnect_mutex);
}

// ============================================================================
// Protocol Base Interface Implementation
// ============================================================================

static esp_err_t ws_base_start(sx_protocol_base_t *self, const void *config) {
    if (!self || !config) return ESP_ERR_INVALID_ARG;
    sx_protocol_ws_impl_t *impl = (sx_protocol_ws_impl_t *)self->impl;
    const sx_protocol_ws_config_t *cfg = (const sx_protocol_ws_config_t *)config;
    
    // Update impl config
    impl->cfg = *cfg;
    impl->protocol_version = cfg->protocol_version > 0 ? cfg->protocol_version : 2;
    
    // Sync with legacy static variables
    s_cfg = *cfg;
    s_protocol_version = impl->protocol_version;
    
    esp_err_t ret = sx_protocol_ws_start(cfg);
    
    // Sync back after start
    impl->client = s_client;
    impl->connected = s_connected;
    
    return ret;
}

static esp_err_t ws_base_stop(sx_protocol_base_t *self) {
    if (!self) return ESP_ERR_INVALID_ARG;
    sx_protocol_ws_impl_t *impl = (sx_protocol_ws_impl_t *)self->impl;
    
    esp_err_t ret = sx_protocol_ws_stop();
    
    // Sync back
    impl->client = s_client;
    impl->connected = s_connected;
    
    return ret;
}

static bool ws_base_is_connected(const sx_protocol_base_t *self) {
    if (!self) return false;
    sx_protocol_ws_impl_t *impl = (sx_protocol_ws_impl_t *)self->impl;
    
    // Sync from legacy
    impl->connected = sx_protocol_ws_is_connected();
    impl->client = s_client;
    
    return impl->connected;
}

static esp_err_t ws_base_send_text(sx_protocol_base_t *self, const char *text) {
    if (!self || !text) return ESP_ERR_INVALID_ARG;
    return sx_protocol_ws_send_text(text);
}

static esp_err_t ws_base_send_audio(sx_protocol_base_t *self, const sx_audio_stream_packet_t *packet) {
    if (!self || !packet) return ESP_ERR_INVALID_ARG;
    return sx_protocol_ws_send_audio(packet);
}

// Build hello message with device info
static char* build_hello_message(void) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;
    
    cJSON_AddStringToObject(root, "type", "hello");
    cJSON_AddStringToObject(root, "device", "ESP32");
    cJSON_AddStringToObject(root, "version", "1.0");
    
    // Add device capabilities
    cJSON *capabilities = cJSON_CreateObject();
    if (capabilities) {
        cJSON_AddBoolToObject(capabilities, "audio", true);
        cJSON_AddBoolToObject(capabilities, "mcp", true);
        cJSON_AddItemToObject(root, "capabilities", capabilities);
    }
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    return json_str;
}

static esp_err_t ws_base_open_audio_channel(sx_protocol_base_t *self) {
    if (!self) return ESP_ERR_INVALID_ARG;
    
    // WebSocket doesn't have separate audio channel, it's always open when connected
    // But we send hello message when opening audio channel
    
    if (!sx_protocol_ws_is_connected()) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Build and send hello message
    char *hello_msg = build_hello_message();
    if (!hello_msg) {
        ESP_LOGE(TAG, "Failed to build hello message");
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t ret = sx_protocol_ws_send_text(hello_msg);
    free(hello_msg);
    
    if (ret == ESP_OK) {
        // Emit hello sent event
        sx_event_t evt = {
            .type = SX_EVT_PROTOCOL_HELLO_SENT,
            .arg0 = 0,
            .ptr = NULL,
        };
        sx_dispatcher_post_event(&evt);
        
        ESP_LOGI(TAG, "Hello message sent to server");
    } else {
        ESP_LOGE(TAG, "Failed to send hello message: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

static void ws_base_close_audio_channel(sx_protocol_base_t *self) {
    if (!self) return;
    // WebSocket doesn't have separate audio channel
    // Emit event
    sx_event_t evt = {
        .type = SX_EVT_CHATBOT_AUDIO_CHANNEL_CLOSED,
        .arg0 = 0,
        .ptr = NULL,
    };
    sx_dispatcher_post_event(&evt);
}

static bool ws_base_is_audio_channel_opened(const sx_protocol_base_t *self) {
    if (!self) return false;
    return sx_protocol_ws_is_connected();
}

static uint32_t ws_base_get_server_sample_rate(const sx_protocol_base_t *self) {
    if (!self) return 0;
    sx_protocol_ws_impl_t *impl = (sx_protocol_ws_impl_t *)self->impl;
    impl->server_sample_rate = sx_protocol_ws_get_server_sample_rate();
    return impl->server_sample_rate;
}

static uint32_t ws_base_get_server_frame_duration(const sx_protocol_base_t *self) {
    if (!self) return 0;
    sx_protocol_ws_impl_t *impl = (sx_protocol_ws_impl_t *)self->impl;
    impl->server_frame_duration = sx_protocol_ws_get_server_frame_duration();
    return impl->server_frame_duration;
}

static const char* ws_base_get_session_id(const sx_protocol_base_t *self) {
    if (!self) return NULL;
    sx_protocol_ws_impl_t *impl = (sx_protocol_ws_impl_t *)self->impl;
    return impl->session_id[0] != '\0' ? impl->session_id : NULL;
}

static bool ws_base_is_timeout(const sx_protocol_base_t *self) {
    if (!self) return false;
    sx_protocol_ws_impl_t *impl = (sx_protocol_ws_impl_t *)self->impl;
    
    // Phase 2: Use common timeout detection (30 seconds)
    return sx_protocol_common_check_timeout(impl->last_incoming_time, 30000);
}

static void ws_base_set_error(sx_protocol_base_t *self, const char *error_msg) {
    if (!self || !error_msg) return;
    sx_protocol_ws_impl_t *impl = (sx_protocol_ws_impl_t *)self->impl;
    
    impl->has_error = true;
    strncpy(impl->error_message, error_msg, sizeof(impl->error_message) - 1);
    impl->error_message[sizeof(impl->error_message) - 1] = '\0';
    
    // Emit error event
    char *error_str = sx_event_alloc_string(error_msg);
    if (error_str) {
        sx_event_t evt = {
            .type = SX_EVT_PROTOCOL_ERROR,
            .arg0 = 0,
            .ptr = error_str,
        };
        sx_dispatcher_post_event(&evt);
    }
}

static esp_err_t ws_base_set_message_callback(sx_protocol_base_t *self, void *callback) {
    if (!self) return ESP_ERR_INVALID_ARG;
    sx_protocol_ws_impl_t *impl = (sx_protocol_ws_impl_t *)self->impl;
    impl->message_callback = (sx_protocol_ws_message_cb_t)callback;
    return sx_protocol_ws_set_message_callback((sx_protocol_ws_message_cb_t)callback);
}

static esp_err_t ws_base_set_audio_callback(sx_protocol_base_t *self, sx_protocol_audio_callback_t callback) {
    if (!self) return ESP_ERR_INVALID_ARG;
    sx_protocol_ws_impl_t *impl = (sx_protocol_ws_impl_t *)self->impl;
    impl->audio_callback = callback;
    return sx_protocol_ws_set_audio_callback(callback);
}

// Chatbot control methods (from reference repo)
// Phase 2: Refactor to use sx_protocol_common to reduce duplicate code
static esp_err_t ws_base_send_wake_word_detected(sx_protocol_base_t *self, const char *wake_word) {
    if (!self || !wake_word) return ESP_ERR_INVALID_ARG;
    if (!sx_protocol_ws_is_connected()) return ESP_ERR_INVALID_STATE;
    
    // Get session_id from impl
    sx_protocol_ws_impl_t *impl = (sx_protocol_ws_impl_t *)self->impl;
    const char *session_id = impl->session_id[0] != '\0' ? impl->session_id : "";
    
    // Phase 2: Use common JSON builder (but WS uses different format: "listen" type)
    // Note: WS uses {"type":"listen","state":"detect","text":"wake_word"} format
    // while common uses {"type":"wake_word_detected","wake_word":"..."} format
    // For now, keep WS-specific format but extract to helper
    cJSON *json = cJSON_CreateObject();
    if (!json) return ESP_ERR_NO_MEM;
    
    cJSON_AddStringToObject(json, "session_id", session_id);
    cJSON_AddStringToObject(json, "type", "listen");
    cJSON_AddStringToObject(json, "state", "detect");
    cJSON_AddStringToObject(json, "text", wake_word);
    
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    if (!json_str) return ESP_ERR_NO_MEM;
    
    esp_err_t ret = sx_protocol_ws_send_text(json_str);
    free(json_str);
    
    return ret;
}

static esp_err_t ws_base_send_start_listening(sx_protocol_base_t *self, sx_listening_mode_t mode) {
    if (!self) return ESP_ERR_INVALID_ARG;
    if (!sx_protocol_ws_is_connected()) return ESP_ERR_INVALID_STATE;
    
    sx_protocol_ws_impl_t *impl = (sx_protocol_ws_impl_t *)self->impl;
    const char *session_id = impl->session_id[0] != '\0' ? impl->session_id : "";
    
    // Phase 2: Use common JSON builder (but WS uses different format)
    // WS format: {"type":"listen","state":"start","mode":"auto|manual|realtime"}
    // Common format: {"type":"start_listening","mode":"auto_stop|manual_stop|realtime"}
    // For now, keep WS-specific format but could be refactored later
    cJSON *json = cJSON_CreateObject();
    if (!json) return ESP_ERR_NO_MEM;
    
    cJSON_AddStringToObject(json, "session_id", session_id);
    cJSON_AddStringToObject(json, "type", "listen");
    cJSON_AddStringToObject(json, "state", "start");
    
    const char *mode_str = "auto";
    if (mode == SX_LISTENING_MODE_MANUAL_STOP) {
        mode_str = "manual";
    } else if (mode == SX_LISTENING_MODE_REALTIME) {
        mode_str = "realtime";
    }
    cJSON_AddStringToObject(json, "mode", mode_str);
    
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    if (!json_str) return ESP_ERR_NO_MEM;
    
    esp_err_t ret = sx_protocol_ws_send_text(json_str);
    free(json_str);
    
    return ret;
}

static esp_err_t ws_base_send_stop_listening(sx_protocol_base_t *self) {
    if (!self) return ESP_ERR_INVALID_ARG;
    if (!sx_protocol_ws_is_connected()) return ESP_ERR_INVALID_STATE;
    
    sx_protocol_ws_impl_t *impl = (sx_protocol_ws_impl_t *)self->impl;
    const char *session_id = impl->session_id[0] != '\0' ? impl->session_id : "";
    
    // Phase 2: WS uses different format, keep for now
    // WS format: {"type":"listen","state":"stop"}
    // Common format: {"type":"stop_listening"}
    cJSON *json = cJSON_CreateObject();
    if (!json) return ESP_ERR_NO_MEM;
    
    cJSON_AddStringToObject(json, "session_id", session_id);
    cJSON_AddStringToObject(json, "type", "listen");
    cJSON_AddStringToObject(json, "state", "stop");
    
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    if (!json_str) return ESP_ERR_NO_MEM;
    
    esp_err_t ret = sx_protocol_ws_send_text(json_str);
    free(json_str);
    
    return ret;
}

static esp_err_t ws_base_send_abort_speaking(sx_protocol_base_t *self, sx_abort_reason_t reason) {
    if (!self) return ESP_ERR_INVALID_ARG;
    if (!sx_protocol_ws_is_connected()) return ESP_ERR_INVALID_STATE;
    
    sx_protocol_ws_impl_t *impl = (sx_protocol_ws_impl_t *)self->impl;
    const char *session_id = impl->session_id[0] != '\0' ? impl->session_id : "";
    
    // Phase 2: Use common JSON builder (format matches!)
    char *json_str = sx_protocol_common_build_abort_speaking_json(session_id, reason);
    if (!json_str) return ESP_ERR_NO_MEM;
    
    esp_err_t ret = sx_protocol_ws_send_text(json_str);
    free(json_str);
    
    return ret;
}

static esp_err_t ws_base_send_mcp_message(sx_protocol_base_t *self, const char *payload) {
    if (!self || !payload) return ESP_ERR_INVALID_ARG;
    if (!sx_protocol_ws_is_connected()) return ESP_ERR_INVALID_STATE;
    
    sx_protocol_ws_impl_t *impl = (sx_protocol_ws_impl_t *)self->impl;
    const char *session_id = impl->session_id[0] != '\0' ? impl->session_id : "";
    
    // Phase 2: Use common JSON builder (format matches!)
    char *json_str = sx_protocol_common_build_mcp_message_json(session_id, payload);
    if (!json_str) return ESP_ERR_NO_MEM;
    
    esp_err_t ret = sx_protocol_ws_send_text(json_str);
    free(json_str);
    
    return ret;
}

// Protocol operations structure
static const sx_protocol_ops_t s_ws_ops = {
    .start = ws_base_start,
    .stop = ws_base_stop,
    .is_connected = ws_base_is_connected,
    .send_text = ws_base_send_text,
    .send_audio = ws_base_send_audio,
    .open_audio_channel = ws_base_open_audio_channel,
    .close_audio_channel = ws_base_close_audio_channel,
    .is_audio_channel_opened = ws_base_is_audio_channel_opened,
    .get_server_sample_rate = ws_base_get_server_sample_rate,
    .get_server_frame_duration = ws_base_get_server_frame_duration,
    .get_session_id = ws_base_get_session_id,
    .is_timeout = ws_base_is_timeout,
    .set_error = ws_base_set_error,
    .set_message_callback = ws_base_set_message_callback,
    .set_audio_callback = ws_base_set_audio_callback,
    .send_wake_word_detected = ws_base_send_wake_word_detected,
    .send_start_listening = ws_base_send_start_listening,
    .send_stop_listening = ws_base_send_stop_listening,
    .send_abort_speaking = ws_base_send_abort_speaking,
    .send_mcp_message = ws_base_send_mcp_message,
};

// Get base interface for WebSocket protocol
sx_protocol_base_t* sx_protocol_ws_get_base(void) {
    // Initialize if not already done
    if (s_ws_base.ops == NULL) {
        s_ws_impl.client = s_client;
        s_ws_impl.connected = s_connected;
        s_ws_impl.cfg = s_cfg;
        s_ws_impl.protocol_version = s_protocol_version;
        s_ws_impl.server_sample_rate = s_server_sample_rate;
        s_ws_impl.server_frame_duration = s_server_frame_duration;
        s_ws_impl.session_id[0] = '\0';
        s_ws_impl.message_callback = s_message_callback;
        s_ws_impl.audio_callback = s_audio_callback;
        s_ws_impl.last_incoming_time = 0;
        s_ws_impl.has_error = false;
        s_ws_impl.error_message[0] = '\0';
        
        s_ws_base.ops = &s_ws_ops;
        s_ws_base.impl = &s_ws_impl;
    } else {
        // Sync state from legacy variables
        s_ws_impl.client = s_client;
        s_ws_impl.connected = s_connected;
        s_ws_impl.server_sample_rate = s_server_sample_rate;
        s_ws_impl.server_frame_duration = s_server_frame_duration;
    }
    
    return &s_ws_base;
}

