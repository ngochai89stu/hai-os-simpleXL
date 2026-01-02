#include "sx_chatbot_service.h"

#include <esp_log.h>
#include <string.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_event_string_pool.h"
#include "sx_radio_service.h"
#include "sx_intent_service.h"
#include "sx_mcp_server.h"
#include "sx_mcp_tools.h"
#include "sx_protocol_ws.h"
#include "sx_protocol_mqtt.h"
#include "sx_protocol_base.h"
#include "sx_protocol_factory.h"
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

// Helper function to get current protocol base interface
// Phase 1: Migrate to use protocol factory
static sx_protocol_base_t* sx_chatbot_get_protocol_base(void) {
    // Use factory for auto-detection
    sx_protocol_base_t *protocol = sx_protocol_factory_get_current();
    if (protocol && protocol->ops && protocol->ops->is_connected) {
        if (protocol->ops->is_connected(protocol)) {
            return protocol;
        }
    }
    return NULL;
}

esp_err_t sx_chatbot_service_init(const sx_chatbot_config_t *cfg) {
    if (s_initialized) {
        return ESP_OK;
    }
    if (cfg != NULL) {
        s_cfg = *cfg;
    }
    
    // Phase 1: Initialize protocol factory
    sx_protocol_factory_init();
    
    // MCP server is initialized in bootstrap now
    
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
                // Phase 1: Migrate to use protocol base interface (eliminate duplicate code)
                sx_protocol_base_t *protocol = sx_chatbot_get_protocol_base();
                if (protocol && protocol->ops && protocol->ops->is_connected && protocol->ops->is_connected(protocol)) {
                    // Build JSON message (common code, no duplication)
                    cJSON *json = cJSON_CreateObject();
                    cJSON_AddStringToObject(json, "type", "user_message");
                    cJSON_AddStringToObject(json, "text", msg.text);
                    
                    char *json_str = cJSON_PrintUnformatted(json);
                    if (json_str && protocol->ops->send_text) {
                        esp_err_t ret = protocol->ops->send_text(protocol, json_str);
                        if (ret == ESP_OK) {
                            ESP_LOGI(TAG, "Message sent via protocol: %s", msg.text);
                        } else {
                            ESP_LOGW(TAG, "Failed to send message via protocol: %s", esp_err_to_name(ret));
                        }
                        free(json_str);
                    } else if (json_str) {
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

bool sx_chatbot_handle_json_message(cJSON *root, const char *raw_fallback) {
    if (root == NULL) {
        return false;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    const char *msg_type = cJSON_IsString(type) ? type->valuestring : NULL;

    // Không có type: coi như MCP raw
    if (!msg_type) {
        if (raw_fallback != NULL) {
            sx_chatbot_handle_mcp_message(raw_fallback);
        } else {
            char *msg_str = cJSON_PrintUnformatted(root);
            if (msg_str) {
                sx_chatbot_handle_mcp_message(msg_str);
                free(msg_str);
            }
        }
        return true;
    }

    if (strcmp(msg_type, "stt") == 0) {
        cJSON *text = cJSON_GetObjectItem(root, "text");
        if (cJSON_IsString(text)) {
            ESP_LOGI(TAG, "STT: %s", text->valuestring);
            sx_event_t evt = {
                .type = SX_EVT_CHATBOT_STT,
                .arg0 = 0,
                .ptr = sx_event_alloc_string(text->valuestring),
            };
            sx_dispatcher_post_event(&evt);
        }
        return true;
    }

    if (strcmp(msg_type, "tts") == 0) {
        cJSON *state = cJSON_GetObjectItem(root, "state");
        if (cJSON_IsString(state)) {
            const char *tts_state = state->valuestring;

            if (strcmp(tts_state, "start") == 0) {
                sx_event_t evt = {
                    .type = SX_EVT_CHATBOT_TTS_START,
                    .arg0 = 0,
                    .ptr = NULL,
                };
                sx_dispatcher_post_event(&evt);
            } else if (strcmp(tts_state, "stop") == 0) {
                sx_event_t evt = {
                    .type = SX_EVT_CHATBOT_TTS_STOP,
                    .arg0 = 0,
                    .ptr = NULL,
                };
                sx_dispatcher_post_event(&evt);
            } else if (strcmp(tts_state, "sentence_start") == 0) {
                cJSON *text = cJSON_GetObjectItem(root, "text");
                if (cJSON_IsString(text)) {
                    ESP_LOGI(TAG, "TTS sentence: %s", text->valuestring);
                    sx_event_t evt = {
                        .type = SX_EVT_CHATBOT_TTS_SENTENCE,
                        .arg0 = 0,
                        .ptr = sx_event_alloc_string(text->valuestring),
                    };
                    sx_dispatcher_post_event(&evt);
                }
            }
        }
        return true;
    }

    if (strcmp(msg_type, "llm") == 0) {
        cJSON *emotion = cJSON_GetObjectItem(root, "emotion");
        if (cJSON_IsString(emotion)) {
            ESP_LOGI(TAG, "LLM emotion: %s", emotion->valuestring);
            sx_event_t evt = {
                .type = SX_EVT_CHATBOT_EMOTION,
                .arg0 = 0,
                .ptr = sx_event_alloc_string(emotion->valuestring),
            };
            sx_dispatcher_post_event(&evt);
        }
        return true;
    }

    if (strcmp(msg_type, "mcp") == 0) {
        cJSON *payload = cJSON_GetObjectItem(root, "payload");
        if (payload != NULL) {
            char *payload_str = cJSON_PrintUnformatted(payload);
            if (payload_str) {
                sx_chatbot_handle_mcp_message(payload_str);
                free(payload_str);
            }
        } else {
            // Fallback: pass entire message
            if (raw_fallback != NULL) {
                sx_chatbot_handle_mcp_message(raw_fallback);
            } else {
                char *msg_str = cJSON_PrintUnformatted(root);
                if (msg_str) {
                    sx_chatbot_handle_mcp_message(msg_str);
                    free(msg_str);
                }
            }
        }
        return true;
    }

    // System commands
    if (strcmp(msg_type, "system") == 0) {
        cJSON *command = cJSON_GetObjectItem(root, "command");
        if (cJSON_IsString(command)) {
            if (strcmp(command->valuestring, "reboot") == 0) {
                sx_event_t evt = {
                    .type = SX_EVT_SYSTEM_REBOOT,
                    .arg0 = 0,
                    .ptr = NULL,
                };
                sx_dispatcher_post_event(&evt);
            } else {
                // Generic system command
                sx_event_t evt = {
                    .type = SX_EVT_SYSTEM_COMMAND,
                    .arg0 = 0,
                    .ptr = sx_event_alloc_string(command->valuestring),
                };
                sx_dispatcher_post_event(&evt);
            }
        }
        return true;
    }

    // Alert messages
    if (strcmp(msg_type, "alert") == 0) {
        cJSON *status = cJSON_GetObjectItem(root, "status");
        cJSON *message = cJSON_GetObjectItem(root, "message");
        cJSON *emotion = cJSON_GetObjectItem(root, "emotion");
        
        if (cJSON_IsString(status) && cJSON_IsString(message) && cJSON_IsString(emotion)) {
            typedef struct {
                char status[64];
                char message[256];
                char emotion[32];
            } alert_data_t;
            
            alert_data_t *alert = (alert_data_t *)malloc(sizeof(alert_data_t));
            if (alert) {
                strncpy(alert->status, status->valuestring, sizeof(alert->status) - 1);
                alert->status[sizeof(alert->status) - 1] = '\0';
                strncpy(alert->message, message->valuestring, sizeof(alert->message) - 1);
                alert->message[sizeof(alert->message) - 1] = '\0';
                strncpy(alert->emotion, emotion->valuestring, sizeof(alert->emotion) - 1);
                alert->emotion[sizeof(alert->emotion) - 1] = '\0';
                
                sx_event_t evt = {
                    .type = SX_EVT_ALERT,
                    .arg0 = 0,
                    .ptr = alert,
                };
                sx_dispatcher_post_event(&evt);
            }
        }
        return true;
    }

    // Các type khác (hello, goodbye, ...) sẽ do protocol layer xử lý
    return false;
}

esp_err_t sx_chatbot_handle_mcp_message(const char *message) {
    if (!s_initialized || message == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Parse and handle MCP message
    char *response = sx_mcp_server_parse_message(message);
    if (response != NULL) {
        // Use protocol base interface to send MCP message
        sx_protocol_base_t *protocol = sx_chatbot_get_protocol_base();
        if (protocol != NULL) {
            esp_err_t ret = SX_PROTOCOL_SEND_MCP_MESSAGE(protocol, response);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to send MCP response via protocol");
            }
        } else {
            ESP_LOGW(TAG, "No protocol available, MCP response not sent");
        }
        free(response);
    }
    
    return ESP_OK;
}

// Audio channel control (from reference repo)
esp_err_t sx_chatbot_open_audio_channel(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    sx_protocol_base_t *protocol = sx_chatbot_get_protocol_base();
    if (protocol == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return SX_PROTOCOL_OPEN_AUDIO_CHANNEL(protocol);
}

esp_err_t sx_chatbot_close_audio_channel(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    sx_protocol_base_t *protocol = sx_chatbot_get_protocol_base();
    if (protocol == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    SX_PROTOCOL_CLOSE_AUDIO_CHANNEL(protocol);
    return ESP_OK;
}

bool sx_chatbot_is_audio_channel_opened(void) {
    if (!s_initialized) {
        return false;
    }
    
    sx_protocol_base_t *protocol = sx_chatbot_get_protocol_base();
    if (protocol == NULL) {
        return false;
    }
    
    return SX_PROTOCOL_IS_AUDIO_CHANNEL_OPENED(protocol);
}

// Listening control (from reference repo)
esp_err_t sx_chatbot_send_wake_word_detected(const char *wake_word) {
    if (!s_initialized || wake_word == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    sx_protocol_base_t *protocol = sx_chatbot_get_protocol_base();
    if (protocol == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return SX_PROTOCOL_SEND_WAKE_WORD_DETECTED(protocol, wake_word);
}

esp_err_t sx_chatbot_send_start_listening(sx_listening_mode_t mode) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    sx_protocol_base_t *protocol = sx_chatbot_get_protocol_base();
    if (protocol == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return SX_PROTOCOL_SEND_START_LISTENING(protocol, mode);
}

esp_err_t sx_chatbot_send_stop_listening(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    sx_protocol_base_t *protocol = sx_chatbot_get_protocol_base();
    if (protocol == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return SX_PROTOCOL_SEND_STOP_LISTENING(protocol);
}

esp_err_t sx_chatbot_send_abort_speaking(sx_abort_reason_t reason) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    sx_protocol_base_t *protocol = sx_chatbot_get_protocol_base();
    if (protocol == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return SX_PROTOCOL_SEND_ABORT_SPEAKING(protocol, reason);
}

