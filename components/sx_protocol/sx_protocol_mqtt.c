#include "sx_protocol_mqtt.h"
#include "sx_protocol_base.h"
#include "sx_protocol_audio.h"
#include "sx_protocol_common.h"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_chatbot_service.h"
#include "sx_event_string_pool.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <cJSON.h>

static const char *TAG = "sx_protocol_mqtt";

// Forward declarations for UDP functions
extern esp_err_t sx_protocol_mqtt_udp_init(const char *udp_server, int udp_port,
                                          const char *aes_key_hex, const char *aes_nonce_hex);
extern void sx_protocol_mqtt_udp_close(void);
extern esp_err_t sx_protocol_mqtt_udp_send_audio(const sx_audio_stream_packet_t *packet);
extern void sx_protocol_mqtt_udp_set_audio_callback(sx_protocol_audio_callback_t callback);
extern void sx_protocol_mqtt_udp_set_server_params(uint32_t sample_rate, uint32_t frame_duration);
extern bool sx_protocol_mqtt_udp_is_opened(void);

// MQTT implementation structure
typedef struct {
    bool initialized;
    bool connected;
    esp_mqtt_client_handle_t client;
    sx_protocol_mqtt_config_t config;
    sx_protocol_mqtt_message_cb_t message_callback;
    SemaphoreHandle_t mutex;
    uint32_t server_sample_rate;
    uint32_t server_frame_duration;
    char session_id[64];
    uint32_t last_incoming_time;  // For timeout detection
    bool has_error;
    char error_message[128];
} sx_protocol_mqtt_impl_t;

// Global instance (singleton pattern for backward compatibility)
static sx_protocol_mqtt_impl_t s_mqtt_impl = {0};
static sx_protocol_base_t s_mqtt_base = {0};

// MQTT client state (legacy static variables for backward compatibility)
static bool s_initialized = false;
static bool s_connected = false;
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static sx_protocol_mqtt_config_t s_config = {0};
static sx_protocol_mqtt_message_cb_t s_message_callback = NULL;
static SemaphoreHandle_t s_mqtt_mutex = NULL;

// MQTT event handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            s_connected = true;
            // Emit connected event
            sx_event_t evt_conn = {
                .type = SX_EVT_CHATBOT_CONNECTED,
                .arg0 = 0,
                .ptr = NULL,
            };
            sx_dispatcher_post_event(&evt_conn);
            
            // Subscribe to default topics if configured
            if (s_config.topic_prefix != NULL) {
                char subscribe_topic[256];
                snprintf(subscribe_topic, sizeof(subscribe_topic), "%smessages", s_config.topic_prefix);
                esp_mqtt_client_subscribe(client, subscribe_topic, 1);
                ESP_LOGI(TAG, "Subscribed to: %s", subscribe_topic);
            }
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
            s_connected = false;
            s_mqtt_impl.connected = false;
            
            // Check if it's an error disconnect
            if (s_mqtt_impl.has_error) {
                char *error_str = sx_event_alloc_string(s_mqtt_impl.error_message);
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
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            s_mqtt_impl.has_error = true;
            strncpy(s_mqtt_impl.error_message, "MQTT connection error", sizeof(s_mqtt_impl.error_message) - 1);
            s_mqtt_impl.error_message[sizeof(s_mqtt_impl.error_message) - 1] = '\0';
            
            // Emit error event
            char *error_str = sx_event_alloc_string("MQTT connection error");
            if (error_str) {
                sx_event_t evt = {
                    .type = SX_EVT_PROTOCOL_ERROR,
                    .arg0 = 0,
                    .ptr = error_str,
                };
                sx_dispatcher_post_event(&evt);
            }
            break;
            
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT subscribed, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT unsubscribed, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT published, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_DATA:
            // Phase 2: Use common function to update last incoming time
            sx_protocol_common_update_last_incoming_time(&s_mqtt_impl.last_incoming_time);
            
            ESP_LOGI(TAG, "MQTT data received, topic=%.*s, data_len=%d",
                     event->topic_len, event->topic, event->data_len);
            
            // Parse JSON message nếu là text
            // Security: Limit payload size to prevent DoS
            #define MAX_MQTT_PAYLOAD_SIZE 8192
            #define MAX_JSON_SIZE 4096
            if (event->data_len > 0 && event->data_len < MAX_MQTT_PAYLOAD_SIZE) {
                size_t payload_size = (event->data_len > MAX_JSON_SIZE) ? MAX_JSON_SIZE : event->data_len;
                char *payload = strndup((const char *)event->data, payload_size);
                if (payload) {
                    // Ensure null termination
                    payload[payload_size] = '\0';
                    
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
                                    if (cJSON_IsObject(audio_params)) {
                                        cJSON *sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");
                                        cJSON *frame_duration = cJSON_GetObjectItem(audio_params, "frame_duration");
                                        
                                        if (cJSON_IsNumber(sample_rate)) {
                                            hello->server_sample_rate = (uint32_t)sample_rate->valueint;
                                        }
                                        if (cJSON_IsNumber(frame_duration)) {
                                            hello->server_frame_duration = (uint32_t)frame_duration->valueint;
                                        }
                                    }
                                    
                                    // Parse session_id
                                    cJSON *session_id = cJSON_GetObjectItem(root, "session_id");
                                    if (cJSON_IsString(session_id)) {
                                        strncpy(hello->session_id, session_id->valuestring, sizeof(hello->session_id) - 1);
                                        hello->session_id[sizeof(hello->session_id) - 1] = '\0';
                                    }
                                    
                                    // Parse transport for UDP (MQTT specific)
                                    cJSON *transport = cJSON_GetObjectItem(root, "transport");
                                    if (transport != NULL && cJSON_IsString(transport) &&
                                        strcmp(transport->valuestring, "udp") == 0) {
                                        
                                        // Get UDP parameters
                                        cJSON *udp_server = cJSON_GetObjectItem(root, "udp_server");
                                        cJSON *udp_port = cJSON_GetObjectItem(root, "udp_port");
                                        cJSON *aes_key = cJSON_GetObjectItem(root, "aes_key");
                                        cJSON *aes_nonce = cJSON_GetObjectItem(root, "aes_nonce");
                                        
                                        if (cJSON_IsString(udp_server) && cJSON_IsNumber(udp_port)) {
                                            // Initialize UDP channel
                                            const char *key = cJSON_IsString(aes_key) ? aes_key->valuestring : NULL;
                                            const char *nonce = cJSON_IsString(aes_nonce) ? aes_nonce->valuestring : NULL;
                                            
                                            sx_protocol_mqtt_udp_init(udp_server->valuestring, 
                                                                     (int)udp_port->valueint,
                                                                     key, nonce);
                                            
                                            // Set server params
                                            sx_protocol_mqtt_udp_set_server_params(hello->server_sample_rate, 
                                                                                   hello->server_frame_duration);
                                            
                                            // Emit audio channel opened event
                                            sx_event_t evt_channel = {
                                                .type = SX_EVT_CHATBOT_AUDIO_CHANNEL_OPENED,
                                                .arg0 = 0,
                                                .ptr = NULL,
                                            };
                                            sx_dispatcher_post_event(&evt_channel);
                                        }
                                    }
                                    
                                    // Emit hello received event
                                    sx_event_t evt = {
                                        .type = SX_EVT_PROTOCOL_HELLO_RECEIVED,
                                        .arg0 = 0,
                                        .ptr = hello,
                                    };
                                    sx_dispatcher_post_event(&evt);
                                    
                                    ESP_LOGI(TAG, "Server hello received: sample_rate=%lu, frame_duration=%lu, session_id=%s",
                                             hello->server_sample_rate, hello->server_frame_duration, hello->session_id);
                                }
                            } else if (msg_type && strcmp(msg_type, "goodbye") == 0) {
                                // Close UDP audio channel
                                sx_protocol_mqtt_udp_close();
                                ESP_LOGI(TAG, "UDP audio channel closed (goodbye)");
                            } else if (!msg_type) {
                                // Không có type mà JSON parse được: chuyển sang MCP
                                sx_chatbot_handle_mcp_message(payload);
                            }
                        }
                        cJSON_Delete(root);
                    } else {
                        // Not JSON - try MCP handler
                        sx_chatbot_handle_mcp_message(payload);
                    }
                    free(payload);
                }
            }
            
            // Call message callback if set (for custom handling)
            if (s_message_callback != NULL) {
                char topic[256] = {0};
                if (event->topic_len < sizeof(topic)) {
                    strncpy(topic, event->topic, event->topic_len);
                    topic[event->topic_len] = '\0';
                }
                s_message_callback(topic, (const uint8_t *)event->data, event->data_len);
            }
            break;
            
        default:
            ESP_LOGD(TAG, "MQTT event: %d", event_id);
            break;
    }
}

esp_err_t sx_protocol_mqtt_init(const sx_protocol_mqtt_config_t *config) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    if (config == NULL || config->broker_uri == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    s_mqtt_mutex = xSemaphoreCreateMutex();
    if (s_mqtt_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    // Copy configuration
    memset(&s_config, 0, sizeof(s_config));
    s_config.broker_uri = config->broker_uri;
    s_config.client_id = config->client_id;
    s_config.username = config->username;
    s_config.password = config->password;
    s_config.topic_prefix = config->topic_prefix;
    s_config.keepalive_sec = (config->keepalive_sec > 0) ? config->keepalive_sec : 60;
    s_config.clean_session = config->clean_session;
    
    // Create MQTT client configuration
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = s_config.broker_uri;
    mqtt_cfg.credentials.client_id = s_config.client_id;
    mqtt_cfg.credentials.username = s_config.username;
    mqtt_cfg.credentials.authentication.password = s_config.password;
    mqtt_cfg.session.keepalive = s_config.keepalive_sec;
    // Note: clean_session field may not exist in ESP-IDF v5.5 MQTT API
    // mqtt_cfg.session.clean_session = s_config.clean_session;
    
    // Create MQTT client
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to create MQTT client");
        vSemaphoreDelete(s_mqtt_mutex);
        s_mqtt_mutex = NULL;
        return ESP_FAIL;
    }
    
    // Register event handler
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    s_initialized = true;
    s_connected = false;
    
    ESP_LOGI(TAG, "MQTT protocol initialized (broker: %s)", s_config.broker_uri);
    return ESP_OK;
}

esp_err_t sx_protocol_mqtt_start(void) {
    if (!s_initialized || s_mqtt_client == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_connected) {
        return ESP_OK; // Already connected
    }
    
    esp_err_t ret = esp_mqtt_client_start(s_mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "MQTT client started");
    return ESP_OK;
}

esp_err_t sx_protocol_mqtt_stop(void) {
    if (!s_initialized || s_mqtt_client == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = esp_mqtt_client_stop(s_mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop MQTT client: %s", esp_err_to_name(ret));
        return ret;
    }
    
    s_connected = false;
    ESP_LOGI(TAG, "MQTT client stopped");
    return ESP_OK;
}

bool sx_protocol_mqtt_is_connected(void) {
    return s_connected;
}

esp_err_t sx_protocol_mqtt_subscribe(const char *topic, int qos) {
    if (!s_initialized || s_mqtt_client == NULL || topic == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    
    int msg_id = esp_mqtt_client_subscribe(s_mqtt_client, topic, qos);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to topic: %s", topic);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Subscribed to topic: %s (qos=%d, msg_id=%d)", topic, qos, msg_id);
    return ESP_OK;
}

esp_err_t sx_protocol_mqtt_unsubscribe(const char *topic) {
    if (!s_initialized || s_mqtt_client == NULL || topic == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    
    int msg_id = esp_mqtt_client_unsubscribe(s_mqtt_client, topic);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to unsubscribe from topic: %s", topic);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Unsubscribed from topic: %s (msg_id=%d)", topic, msg_id);
    return ESP_OK;
}

esp_err_t sx_protocol_mqtt_publish(const char *topic, const char *data, int len, int qos, bool retain) {
    if (!s_initialized || s_mqtt_client == NULL || topic == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    
    int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, data, len, qos, retain);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish to topic: %s", topic);
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "Published to topic: %s (len=%d, qos=%d, msg_id=%d)", topic, len, qos, msg_id);
    return ESP_OK;
}

esp_err_t sx_protocol_mqtt_set_message_callback(sx_protocol_mqtt_message_cb_t callback) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    s_message_callback = callback;
    ESP_LOGI(TAG, "Message callback %s", callback ? "set" : "cleared");
    return ESP_OK;
}

esp_err_t sx_protocol_mqtt_send_audio(const sx_audio_stream_packet_t *packet) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return sx_protocol_mqtt_udp_send_audio(packet);
}

esp_err_t sx_protocol_mqtt_set_audio_callback(sx_protocol_audio_callback_t callback) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    sx_protocol_mqtt_udp_set_audio_callback(callback);
    ESP_LOGI(TAG, "Audio callback %s", callback ? "set" : "cleared");
    return ESP_OK;
}

uint32_t sx_protocol_mqtt_get_server_sample_rate(void) {
    // This will be set from hello message
    return 16000;  // Default
}

uint32_t sx_protocol_mqtt_get_server_frame_duration(void) {
    // This will be set from hello message
    return 60;  // Default
}

bool sx_protocol_mqtt_is_audio_channel_opened(void) {
    return sx_protocol_mqtt_udp_is_opened();
}

// ============================================================================
// Protocol Base Interface Implementation
// ============================================================================

static esp_err_t mqtt_base_start(sx_protocol_base_t *self, const void *config) {
    if (!self || !config) return ESP_ERR_INVALID_ARG;
    sx_protocol_mqtt_impl_t *impl = (sx_protocol_mqtt_impl_t *)self->impl;
    const sx_protocol_mqtt_config_t *cfg = (const sx_protocol_mqtt_config_t *)config;
    
    // Update impl config
    impl->config = *cfg;
    
    // Sync with legacy static variables
    s_config = *cfg;
    
    // Initialize if not already done
    if (!s_initialized) {
        esp_err_t ret = sx_protocol_mqtt_init(cfg);
        if (ret != ESP_OK) return ret;
    }
    
    esp_err_t ret = sx_protocol_mqtt_start();
    
    // Sync back after start
    impl->client = s_mqtt_client;
    impl->connected = s_connected;
    impl->initialized = s_initialized;
    impl->message_callback = s_message_callback;
    impl->mutex = s_mqtt_mutex;
    
    return ret;
}

static esp_err_t mqtt_base_stop(sx_protocol_base_t *self) {
    if (!self) return ESP_ERR_INVALID_ARG;
    sx_protocol_mqtt_impl_t *impl = (sx_protocol_mqtt_impl_t *)self->impl;
    
    esp_err_t ret = sx_protocol_mqtt_stop();
    
    // Sync back
    impl->client = s_mqtt_client;
    impl->connected = s_connected;
    impl->initialized = s_initialized;
    
    return ret;
}

static bool mqtt_base_is_connected(const sx_protocol_base_t *self) {
    if (!self) return false;
    sx_protocol_mqtt_impl_t *impl = (sx_protocol_mqtt_impl_t *)self->impl;
    
    // Sync from legacy
    impl->connected = sx_protocol_mqtt_is_connected();
    impl->client = s_mqtt_client;
    
    return impl->connected;
}

static esp_err_t mqtt_base_send_text(sx_protocol_base_t *self, const char *text) {
    if (!self || !text) return ESP_ERR_INVALID_ARG;
    sx_protocol_mqtt_impl_t *impl = (sx_protocol_mqtt_impl_t *)self->impl;
    
    if (!impl->initialized || !impl->connected) return ESP_ERR_INVALID_STATE;
    
    // Publish to messages topic
    char topic[256];
    if (impl->config.topic_prefix) {
        snprintf(topic, sizeof(topic), "%smessages", impl->config.topic_prefix);
    } else {
        snprintf(topic, sizeof(topic), "sx/device/messages");
    }
    
    return sx_protocol_mqtt_publish(topic, text, strlen(text), 1, false);
}

static esp_err_t mqtt_base_send_audio(sx_protocol_base_t *self, const sx_audio_stream_packet_t *packet) {
    if (!self || !packet) return ESP_ERR_INVALID_ARG;
    return sx_protocol_mqtt_send_audio(packet);
}

// Build hello message with device info
static char* mqtt_build_hello_message(void) {
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

static esp_err_t mqtt_base_open_audio_channel(sx_protocol_base_t *self) {
    if (!self) return ESP_ERR_INVALID_ARG;
    sx_protocol_mqtt_impl_t *impl = (sx_protocol_mqtt_impl_t *)self->impl;
    
    if (!impl->initialized || !impl->connected) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Build and send hello message
    char *hello_msg = mqtt_build_hello_message();
    if (!hello_msg) {
        ESP_LOGE(TAG, "Failed to build hello message");
        return ESP_ERR_NO_MEM;
    }
    
    // Publish hello message
    char topic[256];
    if (impl->config.topic_prefix) {
        snprintf(topic, sizeof(topic), "%smessages", impl->config.topic_prefix);
    } else {
        snprintf(topic, sizeof(topic), "sx/device/messages");
    }
    
    esp_err_t ret = sx_protocol_mqtt_publish(topic, hello_msg, strlen(hello_msg), 1, false);
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

static void mqtt_base_close_audio_channel(sx_protocol_base_t *self) {
    if (!self) return;
    (void)self;  // impl not used in this function
    
    // Close UDP channel
    sx_protocol_mqtt_udp_close();
    
    // Emit event
    sx_event_t evt = {
        .type = SX_EVT_CHATBOT_AUDIO_CHANNEL_CLOSED,
        .arg0 = 0,
        .ptr = NULL,
    };
    sx_dispatcher_post_event(&evt);
}

static bool mqtt_base_is_audio_channel_opened(const sx_protocol_base_t *self) {
    if (!self) return false;
    return sx_protocol_mqtt_is_audio_channel_opened();
}

static uint32_t mqtt_base_get_server_sample_rate(const sx_protocol_base_t *self) {
    if (!self) return 0;
    sx_protocol_mqtt_impl_t *impl = (sx_protocol_mqtt_impl_t *)self->impl;
    impl->server_sample_rate = sx_protocol_mqtt_get_server_sample_rate();
    return impl->server_sample_rate;
}

static uint32_t mqtt_base_get_server_frame_duration(const sx_protocol_base_t *self) {
    if (!self) return 0;
    sx_protocol_mqtt_impl_t *impl = (sx_protocol_mqtt_impl_t *)self->impl;
    impl->server_frame_duration = sx_protocol_mqtt_get_server_frame_duration();
    return impl->server_frame_duration;
}

static const char* mqtt_base_get_session_id(const sx_protocol_base_t *self) {
    if (!self) return NULL;
    sx_protocol_mqtt_impl_t *impl = (sx_protocol_mqtt_impl_t *)self->impl;
    return impl->session_id[0] != '\0' ? impl->session_id : NULL;
}

static bool mqtt_base_is_timeout(const sx_protocol_base_t *self) {
    if (!self) return false;
    sx_protocol_mqtt_impl_t *impl = (sx_protocol_mqtt_impl_t *)self->impl;
    
    // Phase 2: Use common timeout detection (30 seconds)
    return sx_protocol_common_check_timeout(impl->last_incoming_time, 30000);
}

static void mqtt_base_set_error(sx_protocol_base_t *self, const char *error_msg) {
    if (!self || !error_msg) return;
    sx_protocol_mqtt_impl_t *impl = (sx_protocol_mqtt_impl_t *)self->impl;
    
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

static esp_err_t mqtt_base_set_message_callback(sx_protocol_base_t *self, void *callback) {
    if (!self) return ESP_ERR_INVALID_ARG;
    sx_protocol_mqtt_impl_t *impl = (sx_protocol_mqtt_impl_t *)self->impl;
    impl->message_callback = (sx_protocol_mqtt_message_cb_t)callback;
    return sx_protocol_mqtt_set_message_callback((sx_protocol_mqtt_message_cb_t)callback);
}

static esp_err_t mqtt_base_set_audio_callback(sx_protocol_base_t *self, sx_protocol_audio_callback_t callback) {
    if (!self) return ESP_ERR_INVALID_ARG;
    (void)self;  // impl not used in this function
    // Note: MQTT audio callback is handled via UDP, not directly
    return sx_protocol_mqtt_set_audio_callback(callback);
}

// Phase 2: Implement missing base functions for MQTT using common utilities
static esp_err_t mqtt_base_send_wake_word_detected(sx_protocol_base_t *self, const char *wake_word) {
    if (!self || !wake_word) return ESP_ERR_INVALID_ARG;
    if (!sx_protocol_mqtt_is_connected()) return ESP_ERR_INVALID_STATE;
    
    sx_protocol_mqtt_impl_t *impl = (sx_protocol_mqtt_impl_t *)self->impl;
    const char *session_id = impl->session_id[0] != '\0' ? impl->session_id : "";
    
    // Phase 2: Use common JSON builder
    char *json_str = sx_protocol_common_build_wake_word_json(session_id, wake_word);
    if (!json_str) return ESP_ERR_NO_MEM;
    
    // MQTT uses topic-based messaging
    char topic[256];
    snprintf(topic, sizeof(topic), "%smessages", impl->config.topic_prefix ? impl->config.topic_prefix : "sx/device/");
    
    esp_err_t ret = sx_protocol_mqtt_publish(topic, json_str, strlen(json_str), 1, false);
    free(json_str);
    
    return ret;
}

static esp_err_t mqtt_base_send_start_listening(sx_protocol_base_t *self, sx_listening_mode_t mode) {
    if (!self) return ESP_ERR_INVALID_ARG;
    if (!sx_protocol_mqtt_is_connected()) return ESP_ERR_INVALID_STATE;
    
    sx_protocol_mqtt_impl_t *impl = (sx_protocol_mqtt_impl_t *)self->impl;
    const char *session_id = impl->session_id[0] != '\0' ? impl->session_id : "";
    
    // Phase 2: Use common JSON builder
    char *json_str = sx_protocol_common_build_start_listening_json(session_id, mode);
    if (!json_str) return ESP_ERR_NO_MEM;
    
    char topic[256];
    snprintf(topic, sizeof(topic), "%smessages", impl->config.topic_prefix ? impl->config.topic_prefix : "sx/device/");
    
    esp_err_t ret = sx_protocol_mqtt_publish(topic, json_str, strlen(json_str), 1, false);
    free(json_str);
    
    return ret;
}

static esp_err_t mqtt_base_send_stop_listening(sx_protocol_base_t *self) {
    if (!self) return ESP_ERR_INVALID_ARG;
    if (!sx_protocol_mqtt_is_connected()) return ESP_ERR_INVALID_STATE;
    
    sx_protocol_mqtt_impl_t *impl = (sx_protocol_mqtt_impl_t *)self->impl;
    const char *session_id = impl->session_id[0] != '\0' ? impl->session_id : "";
    
    // Phase 2: Use common JSON builder
    char *json_str = sx_protocol_common_build_stop_listening_json(session_id);
    if (!json_str) return ESP_ERR_NO_MEM;
    
    char topic[256];
    snprintf(topic, sizeof(topic), "%smessages", impl->config.topic_prefix ? impl->config.topic_prefix : "sx/device/");
    
    esp_err_t ret = sx_protocol_mqtt_publish(topic, json_str, strlen(json_str), 1, false);
    free(json_str);
    
    return ret;
}

static esp_err_t mqtt_base_send_abort_speaking(sx_protocol_base_t *self, sx_abort_reason_t reason) {
    if (!self) return ESP_ERR_INVALID_ARG;
    if (!sx_protocol_mqtt_is_connected()) return ESP_ERR_INVALID_STATE;
    
    sx_protocol_mqtt_impl_t *impl = (sx_protocol_mqtt_impl_t *)self->impl;
    const char *session_id = impl->session_id[0] != '\0' ? impl->session_id : "";
    
    // Phase 2: Use common JSON builder
    char *json_str = sx_protocol_common_build_abort_speaking_json(session_id, reason);
    if (!json_str) return ESP_ERR_NO_MEM;
    
    char topic[256];
    snprintf(topic, sizeof(topic), "%smessages", impl->config.topic_prefix ? impl->config.topic_prefix : "sx/device/");
    
    esp_err_t ret = sx_protocol_mqtt_publish(topic, json_str, strlen(json_str), 1, false);
    free(json_str);
    
    return ret;
}

static esp_err_t mqtt_base_send_mcp_message(sx_protocol_base_t *self, const char *payload) {
    if (!self || !payload) return ESP_ERR_INVALID_ARG;
    if (!sx_protocol_mqtt_is_connected()) return ESP_ERR_INVALID_STATE;
    
    sx_protocol_mqtt_impl_t *impl = (sx_protocol_mqtt_impl_t *)self->impl;
    const char *session_id = impl->session_id[0] != '\0' ? impl->session_id : "";
    
    // Phase 2: Use common JSON builder
    char *json_str = sx_protocol_common_build_mcp_message_json(session_id, payload);
    if (!json_str) return ESP_ERR_NO_MEM;
    
    char topic[256];
    snprintf(topic, sizeof(topic), "%smessages", impl->config.topic_prefix ? impl->config.topic_prefix : "sx/device/");
    
    esp_err_t ret = sx_protocol_mqtt_publish(topic, json_str, strlen(json_str), 1, false);
    free(json_str);
    
    return ret;
}

// Protocol operations structure
static const sx_protocol_ops_t s_mqtt_ops = {
    .start = mqtt_base_start,
    .stop = mqtt_base_stop,
    .is_connected = mqtt_base_is_connected,
    .send_text = mqtt_base_send_text,
    .send_audio = mqtt_base_send_audio,
    .open_audio_channel = mqtt_base_open_audio_channel,
    .close_audio_channel = mqtt_base_close_audio_channel,
    .is_audio_channel_opened = mqtt_base_is_audio_channel_opened,
    .get_server_sample_rate = mqtt_base_get_server_sample_rate,
    .get_server_frame_duration = mqtt_base_get_server_frame_duration,
    .get_session_id = mqtt_base_get_session_id,
    .is_timeout = mqtt_base_is_timeout,
    .set_error = mqtt_base_set_error,
    .set_message_callback = mqtt_base_set_message_callback,
    .set_audio_callback = mqtt_base_set_audio_callback,
    .send_wake_word_detected = mqtt_base_send_wake_word_detected,
    .send_start_listening = mqtt_base_send_start_listening,
    .send_stop_listening = mqtt_base_send_stop_listening,
    .send_abort_speaking = mqtt_base_send_abort_speaking,
    .send_mcp_message = mqtt_base_send_mcp_message,
};

// Get base interface for MQTT protocol
sx_protocol_base_t* sx_protocol_mqtt_get_base(void) {
    // Initialize if not already done
    if (s_mqtt_base.ops == NULL) {
        s_mqtt_impl.initialized = s_initialized;
        s_mqtt_impl.connected = s_connected;
        s_mqtt_impl.client = s_mqtt_client;
        s_mqtt_impl.config = s_config;
        s_mqtt_impl.message_callback = s_message_callback;
        s_mqtt_impl.mutex = s_mqtt_mutex;
        s_mqtt_impl.server_sample_rate = 16000;  // Default
        s_mqtt_impl.server_frame_duration = 60;  // Default
        s_mqtt_impl.session_id[0] = '\0';
        s_mqtt_impl.last_incoming_time = 0;
        s_mqtt_impl.has_error = false;
        s_mqtt_impl.error_message[0] = '\0';
        
        s_mqtt_base.ops = &s_mqtt_ops;
        s_mqtt_base.impl = &s_mqtt_impl;
    } else {
        // Sync state from legacy variables
        s_mqtt_impl.client = s_mqtt_client;
        s_mqtt_impl.connected = s_connected;
        s_mqtt_impl.initialized = s_initialized;
        s_mqtt_impl.server_sample_rate = sx_protocol_mqtt_get_server_sample_rate();
        s_mqtt_impl.server_frame_duration = sx_protocol_mqtt_get_server_frame_duration();
    }
    
    return &s_mqtt_base;
}



