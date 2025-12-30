#include "sx_chatbot_service.h"

#include <esp_log.h>
#include <string.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_radio_service.h"
#include "sx_intent_service.h"
#include "sx_mcp_server.h"
#include "sx_mcp_tools.h"
#include "sx_protocol_ws.h"
#include "sx_protocol_mqtt.h"
#include <cJSON.h>

static const char *TAG = "sx_chatbot";

static bool s_initialized = false;
static bool s_ready = false;
static bool s_intent_parsing_enabled = true; // Enable by default
static sx_chatbot_config_t s_cfg = {0};
static bool s_protocol_ws_available = false;
static bool s_protocol_mqtt_available = false;

// Message queue for chatbot
#define CHATBOT_QUEUE_SIZE 10
static QueueHandle_t s_message_queue = NULL;

typedef struct {
    char text[256];
} chatbot_message_t;

static void sx_chatbot_task(void *arg);

esp_err_t sx_chatbot_service_init(const sx_chatbot_config_t *cfg) {
    if (s_initialized) {
        return ESP_OK;
    }
    if (cfg != NULL) {
        s_cfg = *cfg;
    }
    
    // Initialize MCP server
    esp_err_t ret = sx_mcp_server_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MCP server");
        return ret;
    }
    
    // Register MCP tools
    ret = sx_mcp_tools_register_all();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MCP tools");
        return ret;
    }
    
    // Create message queue
    s_message_queue = xQueueCreate(CHATBOT_QUEUE_SIZE, sizeof(chatbot_message_t));
    if (s_message_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create message queue");
        return ESP_ERR_NO_MEM;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "Chatbot service initialized with MCP support, endpoint=%s", (s_cfg.endpoint ? s_cfg.endpoint : "(none)"));
    return ESP_OK;
}

esp_err_t sx_chatbot_service_start(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    // Tối ưu: Giảm stack size từ 4096 xuống 3072 để tiết kiệm memory
    xTaskCreatePinnedToCore(sx_chatbot_task, "sx_chatbot", 3072, NULL, 5, NULL, tskNO_AFFINITY);
    ESP_LOGI(TAG, "Chatbot service started (stub)");
    return ESP_OK;
}

esp_err_t sx_chatbot_service_stop(void) {
    // Stop chatbot task
    // Note: Task handle is not stored, so we can't directly delete it
    // In a full implementation, we would store the task handle and delete it here
    // For now, the task will continue running until service is deinitialized
    ESP_LOGI(TAG, "Chatbot service stop requested (task cleanup not fully implemented)");
    return ESP_OK;
}

// Helper function to check if message is a radio/music command
static bool sx_chatbot_is_music_command(const char *text, char *song_name, size_t song_name_size) {
    if (text == NULL) {
        return false;
    }
    
    // Convert to lowercase for comparison
    char lower_text[256];
    size_t len = strlen(text);
    if (len >= sizeof(lower_text)) {
        len = sizeof(lower_text) - 1;
    }
    for (size_t i = 0; i < len; i++) {
        lower_text[i] = tolower((unsigned char)text[i]);
    }
    lower_text[len] = '\0';
    
    // Check for patterns: "play music:", "play radio:", "phát nhạc:", etc.
    const char *patterns[] = {
        "play music:",
        "play radio:",
        "phát nhạc:",
        "phát radio:",
        "music:",
        "radio:",
    };
    
    for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); i++) {
        const char *pattern = patterns[i];
        size_t pattern_len = strlen(pattern);
        
        if (strncmp(lower_text, pattern, pattern_len) == 0) {
            // Extract song name after pattern
            const char *song_start = text + pattern_len;
            // Skip whitespace
            while (*song_start == ' ' || *song_start == '\t') {
                song_start++;
            }
            
            if (*song_start != '\0') {
                size_t song_len = strlen(song_start);
                if (song_len >= song_name_size) {
                    song_len = song_name_size - 1;
                }
                strncpy(song_name, song_start, song_len);
                song_name[song_len] = '\0';
                return true;
            }
        }
        
        // Also check if pattern appears anywhere in the text
        char *found = strstr(lower_text, pattern);
        if (found != NULL) {
            const char *song_start = text + (found - lower_text) + pattern_len;
            while (*song_start == ' ' || *song_start == '\t') {
                song_start++;
            }
            if (*song_start != '\0') {
                size_t song_len = strlen(song_start);
                if (song_len >= song_name_size) {
                    song_len = song_name_size - 1;
                }
                strncpy(song_name, song_start, song_len);
                song_name[song_len] = '\0';
                return true;
            }
        }
    }
    
    return false;
}

// Helper function to build radio stream URL from song name
static void sx_chatbot_build_radio_url(const char *song_name, char *url, size_t url_size) {
    // Primary music API (from xiaozhi-esp32)
    const char *api_base = "http://14.225.204.77:5005/stream_pcm?song=";
    
    // URL encode song name (simple version - replace spaces with %20)
    char encoded_song[256];
    size_t song_len = strlen(song_name);
    if (song_len >= sizeof(encoded_song)) {
        song_len = sizeof(encoded_song) - 1;
    }
    
    size_t encoded_idx = 0;
    for (size_t i = 0; i < song_len && encoded_idx < sizeof(encoded_song) - 1; i++) {
        if (song_name[i] == ' ') {
            if (encoded_idx + 3 < sizeof(encoded_song) - 1) {
                encoded_song[encoded_idx++] = '%';
                encoded_song[encoded_idx++] = '2';
                encoded_song[encoded_idx++] = '0';
            }
        } else {
            encoded_song[encoded_idx++] = song_name[i];
        }
    }
    encoded_song[encoded_idx] = '\0';
    
    snprintf(url, url_size, "%s%s", api_base, encoded_song);
}

static void sx_chatbot_task(void *arg) {
    (void)arg;
    // Simulate ready after short delay
    vTaskDelay(pdMS_TO_TICKS(500));
    s_ready = true;
    ESP_LOGI(TAG, "Chatbot ready endpoint=%s", (s_cfg.endpoint ? s_cfg.endpoint : "(none)"));

    chatbot_message_t msg;
    while (1) {
        // Process messages from queue
        if (xQueueReceive(s_message_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "Processing message: %s", msg.text);
            
            // Phase 5: Try intent parsing first
            if (s_intent_parsing_enabled) {
                esp_err_t intent_ret = sx_intent_execute(msg.text);
                if (intent_ret == ESP_OK) {
                    ESP_LOGI(TAG, "Intent executed successfully: %s", msg.text);
                    continue; // Intent handled, skip to next message
                }
            }
            
            // Fallback: Check if message is a music/radio command (legacy)
            char song_name[256];
            if (sx_chatbot_is_music_command(msg.text, song_name, sizeof(song_name))) {
                ESP_LOGI(TAG, "Detected music command, song: %s", song_name);
                
                // Build radio stream URL
                char stream_url[512];
                sx_chatbot_build_radio_url(song_name, stream_url, sizeof(stream_url));
                
                // Play radio stream
                esp_err_t ret = sx_radio_play_station(stream_url);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "Radio stream started: %s", stream_url);
                } else {
                    ESP_LOGE(TAG, "Failed to start radio stream: %s", esp_err_to_name(ret));
                }
            } else {
                // Phase 5: Send to protocol layer (WebSocket or MQTT)
                if (s_protocol_ws_available && sx_protocol_ws_is_connected()) {
                    // Build JSON message
                    cJSON *json = cJSON_CreateObject();
                    cJSON_AddStringToObject(json, "type", "user_message");
                    cJSON_AddStringToObject(json, "text", msg.text);
                    
                    char *json_str = cJSON_PrintUnformatted(json);
                    if (json_str) {
                        esp_err_t ret = sx_protocol_ws_send_text(json_str);
                        if (ret == ESP_OK) {
                            ESP_LOGI(TAG, "Message sent via WebSocket: %s", msg.text);
                        } else {
                            ESP_LOGW(TAG, "Failed to send message via WebSocket");
                        }
                        free(json_str);
                    }
                    cJSON_Delete(json);
                } else if (s_protocol_mqtt_available && sx_protocol_mqtt_is_connected()) {
                    // Build JSON message
                    cJSON *json = cJSON_CreateObject();
                    cJSON_AddStringToObject(json, "type", "user_message");
                    cJSON_AddStringToObject(json, "text", msg.text);
                    
                    char *json_str = cJSON_PrintUnformatted(json);
                    if (json_str) {
                        // Use topic prefix from config or default
                        const char *topic = s_cfg.publish_topic ? s_cfg.publish_topic : "chatbot/message";
                        esp_err_t ret = sx_protocol_mqtt_publish(topic, json_str, strlen(json_str), 1, false);
                        if (ret == ESP_OK) {
                            ESP_LOGI(TAG, "Message sent via MQTT: %s", msg.text);
                        } else {
                            ESP_LOGW(TAG, "Failed to send message via MQTT");
                        }
                        free(json_str);
                    }
                    cJSON_Delete(json);
                } else {
                    ESP_LOGW(TAG, "No protocol available, message not sent: %s", msg.text);
                }
            }
        }
    }
}

esp_err_t sx_chatbot_send_message(const char *text) {
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (text == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_message_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // Queue message for processing
    chatbot_message_t msg;
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';
    
    if (xQueueSend(s_message_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Message queue full, dropping message");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Message queued: %s", text);
    return ESP_OK;
}

bool sx_chatbot_is_ready(void) {
    return s_ready;
}

esp_err_t sx_chatbot_enable_intent_parsing(bool enable) {
    s_intent_parsing_enabled = enable;
    ESP_LOGI(TAG, "Intent parsing %s", enable ? "enabled" : "disabled");
    return ESP_OK;
}

esp_err_t sx_chatbot_set_protocol_ws_available(bool available) {
    s_protocol_ws_available = available;
    ESP_LOGI(TAG, "WebSocket protocol %s", available ? "available" : "unavailable");
    return ESP_OK;
}

esp_err_t sx_chatbot_set_protocol_mqtt_available(bool available) {
    s_protocol_mqtt_available = available;
    ESP_LOGI(TAG, "MQTT protocol %s", available ? "available" : "unavailable");
    return ESP_OK;
}

esp_err_t sx_chatbot_handle_mcp_message(const char *message) {
    if (!s_initialized || message == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Parse and handle MCP message
    char *response = sx_mcp_server_parse_message(message);
    if (response != NULL) {
        // Send response via protocol layer
        if (s_protocol_ws_available && sx_protocol_ws_is_connected()) {
            // Build MCP message wrapper
            cJSON *mcp_msg = cJSON_CreateObject();
            cJSON_AddStringToObject(mcp_msg, "type", "mcp");
            cJSON_AddStringToObject(mcp_msg, "payload", response);
            
            char *json_str = cJSON_PrintUnformatted(mcp_msg);
            if (json_str) {
                esp_err_t ret = sx_protocol_ws_send_text(json_str);
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to send MCP response via WebSocket");
                }
                free(json_str);
            }
            cJSON_Delete(mcp_msg);
        } else if (s_protocol_mqtt_available && sx_protocol_mqtt_is_connected()) {
            // Build MCP message wrapper
            cJSON *mcp_msg = cJSON_CreateObject();
            cJSON_AddStringToObject(mcp_msg, "type", "mcp");
            cJSON_AddStringToObject(mcp_msg, "payload", response);
            
            char *json_str = cJSON_PrintUnformatted(mcp_msg);
            if (json_str) {
                const char *topic = s_cfg.publish_topic ? s_cfg.publish_topic : "chatbot/message";
                esp_err_t ret = sx_protocol_mqtt_publish(topic, json_str, strlen(json_str), 1, false);
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to send MCP response via MQTT");
                }
                free(json_str);
            }
            cJSON_Delete(mcp_msg);
        } else {
            ESP_LOGW(TAG, "No protocol available, MCP response not sent");
        }
        free(response);
    }
    
    return ESP_OK;
}

