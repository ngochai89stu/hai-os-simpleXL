#include "sx_protocol_mqtt.h"
#include "sx_protocol_audio.h"
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

// MQTT client state
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
            // Emit disconnected event
            sx_event_t evt_disc = {
                .type = SX_EVT_CHATBOT_DISCONNECTED,
                .arg0 = 0,
                .ptr = NULL,
            };
            sx_dispatcher_post_event(&evt_disc);
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
                                // Parse server hello message for UDP audio channel
                                cJSON *transport = cJSON_GetObjectItem(root, "transport");
                                if (transport != NULL && cJSON_IsString(transport) &&
                                    strcmp(transport->valuestring, "udp") == 0) {
                                    
                                    // Get audio parameters
                                    cJSON *audio_params = cJSON_GetObjectItem(root, "audio_params");
                                    if (cJSON_IsObject(audio_params)) {
                                        cJSON *sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");
                                        cJSON *frame_duration = cJSON_GetObjectItem(audio_params, "frame_duration");
                                        
                                        uint32_t sr = 16000;
                                        uint32_t fd = 60;
                                        
                                        if (cJSON_IsNumber(sample_rate)) {
                                            sr = (uint32_t)sample_rate->valueint;
                                        }
                                        if (cJSON_IsNumber(frame_duration)) {
                                            fd = (uint32_t)frame_duration->valueint;
                                        }
                                        
                                        sx_protocol_mqtt_udp_set_server_params(sr, fd);
                                        ESP_LOGI(TAG, "Server audio params: %lu Hz, %lu ms", sr, fd);
                                    }
                                    
                                    // Get UDP configuration
                                    cJSON *udp = cJSON_GetObjectItem(root, "udp");
                                    if (cJSON_IsObject(udp)) {
                                        cJSON *server = cJSON_GetObjectItem(udp, "server");
                                        cJSON *port = cJSON_GetObjectItem(udp, "port");
                                        cJSON *key = cJSON_GetObjectItem(udp, "key");
                                        cJSON *nonce = cJSON_GetObjectItem(udp, "nonce");
                                        
                                        if (cJSON_IsString(server) && cJSON_IsNumber(port) &&
                                            cJSON_IsString(key) && cJSON_IsString(nonce)) {
                                            
                                            esp_err_t ret = sx_protocol_mqtt_udp_init(
                                                server->valuestring,
                                                port->valueint,
                                                key->valuestring,
                                                nonce->valuestring
                                            );
                                            
                                            if (ret == ESP_OK) {
                                                ESP_LOGI(TAG, "UDP audio channel opened");
                                                // Emit audio channel opened event
                                                sx_event_t evt = {
                                                    .type = SX_EVT_CHATBOT_AUDIO_CHANNEL_OPENED,
                                                    .arg0 = 0,
                                                    .ptr = NULL,
                                                };
                                                sx_dispatcher_post_event(&evt);
                                            } else {
                                                ESP_LOGE(TAG, "Failed to open UDP audio channel: %s", 
                                                        esp_err_to_name(ret));
                                            }
                                        }
                                    }
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
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error: %s", esp_err_to_name(event->error_handle->error_type));
            s_connected = false;
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



