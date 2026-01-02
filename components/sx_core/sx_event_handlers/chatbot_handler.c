#include "sx_event_handler.h"
#include "sx_event_string_pool.h"
#include "sx_chatbot_service.h"
#include "sx_protocol_ws.h"
#include "sx_protocol_mqtt.h"
#include "sx_audio_protocol_bridge.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>

static const char *TAG = "evt_handler_chatbot";

// Map raw emotion string from chatbot to a stable UI emotion_id
static const char *map_emotion_to_id(const char *emotion) {
    if (emotion == NULL) {
        return "neutral";
    }

    // Simple substring matching for common emotions
    if (strstr(emotion, "happy") || strstr(emotion, "joy") || strstr(emotion, "smile")) {
        return "happy";
    }
    if (strstr(emotion, "sad") || strstr(emotion, "down")) {
        return "sad";
    }
    if (strstr(emotion, "angry") || strstr(emotion, "mad")) {
        return "angry";
    }
    if (strstr(emotion, "surprise") || strstr(emotion, "wow")) {
        return "surprised";
    }
    if (strstr(emotion, "fear") || strstr(emotion, "scared")) {
        return "afraid";
    }

    // Default fallback
    return "neutral";
}

bool sx_event_handler_chatbot_stt(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_CHATBOT_STT) {
        return false;
    }
    
    const char *text = (const char *)evt->ptr;
    if (text != NULL) {
        ESP_LOGI(TAG, "Chatbot STT: %s", text);
        // Update state - UI will display user message
        state->seq++;
        // Use a stable status_text to indicate new STT result
        state->ui.status_text = "stt_result";
        // Copy the message to the state buffer
        strncpy(state->ui.last_user_message, text, SX_UI_MESSAGE_MAX_LEN - 1);
        state->ui.last_user_message[SX_UI_MESSAGE_MAX_LEN - 1] = '\0'; // Ensure null termination
        // Clear the other message buffer to avoid showing stale data
        state->ui.last_assistant_message[0] = '\0';
        // Free text copy (may be from pool or malloc)
        sx_event_free_string((char *)evt->ptr);
        return true;
    }
    return false;
}

bool sx_event_handler_chatbot_tts_sentence(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_CHATBOT_TTS_SENTENCE) {
        return false;
    }
    
    const char *text = (const char *)evt->ptr;
    if (text != NULL) {
        ESP_LOGI(TAG, "Chatbot TTS sentence: %s", text);
        // Update state - UI will display assistant message
        state->seq++;
        // Use a stable status_text to indicate new TTS sentence
        state->ui.status_text = "tts_sentence";
        // Copy the message to the state buffer
        strncpy(state->ui.last_assistant_message, text, SX_UI_MESSAGE_MAX_LEN - 1);
        state->ui.last_assistant_message[SX_UI_MESSAGE_MAX_LEN - 1] = '\0'; // Ensure null termination
        // Clear the other message buffer
        state->ui.last_user_message[0] = '\0';
        // Free text copy (may be from pool or malloc)
        sx_event_free_string((char *)evt->ptr);
        return true;
    }
    return false;
}

bool sx_event_handler_chatbot_emotion(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_CHATBOT_EMOTION) {
        return false;
    }
    
    const char *emotion = (const char *)evt->ptr;
    if (emotion != NULL) {
        ESP_LOGI(TAG, "Chatbot emotion: %s", emotion);
        // Update state - UI will update emotion display
        state->seq++;
        state->ui.emotion_id = map_emotion_to_id(emotion);
        // Free emotion copy (may be from pool or malloc)
        sx_event_free_string((char *)evt->ptr);
        return true;
    }
    return false;
}

bool sx_event_handler_chatbot_tts_start(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_CHATBOT_TTS_START) {
        return false;
    }
    
    ESP_LOGI(TAG, "Chatbot TTS started");
    // Update state - UI can show speaking indicator
    state->seq++;
    return true;
}

bool sx_event_handler_chatbot_tts_stop(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_CHATBOT_TTS_STOP) {
        return false;
    }
    
    ESP_LOGI(TAG, "Chatbot TTS stopped");
    // Update state - UI can hide speaking indicator
    state->seq++;
    return true;
}

bool sx_event_handler_chatbot_audio_channel_opened(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_CHATBOT_AUDIO_CHANNEL_OPENED) {
        return false;
    }
    
    // Handle audio channel opened (MQTT UDP or WebSocket)
    ESP_LOGI(TAG, "Audio channel opened, enabling audio receiving");
    
    // Update state
    state->ui.audio_channel_opened = true;
    
    // Get server params from protocol
    if (sx_protocol_ws_is_connected()) {
        state->ui.server_sample_rate = sx_protocol_ws_get_server_sample_rate();
        state->ui.server_frame_duration = sx_protocol_ws_get_server_frame_duration();
    } else if (sx_protocol_mqtt_is_connected()) {
        state->ui.server_sample_rate = sx_protocol_mqtt_get_server_sample_rate();
        state->ui.server_frame_duration = sx_protocol_mqtt_get_server_frame_duration();
    }
    
    sx_audio_protocol_bridge_enable_receive(true);
    return true;
}

bool sx_event_handler_chatbot_connected(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_CHATBOT_CONNECTED) {
        return false;
    }
    
    ESP_LOGI(TAG, "Chatbot connected");
    
    // Update state
    state->ui.chatbot_connected = true;
    
    // Check which protocol is actually connected and set availability
    // Note: Bootstrap already sets this, but we update here for runtime reconnections
    if (sx_protocol_ws_is_connected()) {
        sx_chatbot_set_protocol_ws_available(true);
        // Enable audio bridge for audio streaming (WebSocket)
        sx_audio_protocol_bridge_enable_receive(true);
        ESP_LOGI(TAG, "Audio receiving enabled (WebSocket connected)");
    }
    if (sx_protocol_mqtt_is_connected()) {
        sx_chatbot_set_protocol_mqtt_available(true);
        // Note: For MQTT, audio receiving is enabled when UDP channel opens
        // (handled by SX_EVT_CHATBOT_AUDIO_CHANNEL_OPENED event)
    }
    return true;
}

bool sx_event_handler_chatbot_disconnected(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_CHATBOT_DISCONNECTED) {
        return false;
    }
    
    ESP_LOGI(TAG, "Chatbot disconnected");
    
    // Update state
    state->ui.chatbot_connected = false;
    state->ui.audio_channel_opened = false;
    state->ui.session_id[0] = '\0';
    
    // Check which protocol disconnected and update availability
    if (!sx_protocol_ws_is_connected()) {
        sx_chatbot_set_protocol_ws_available(false);
    }
    if (!sx_protocol_mqtt_is_connected()) {
        sx_chatbot_set_protocol_mqtt_available(false);
    }
    // Disable audio bridge if both protocols disconnected
    if (!sx_protocol_ws_is_connected() && !sx_protocol_mqtt_is_connected()) {
        sx_audio_protocol_bridge_enable_receive(false);
        sx_audio_protocol_bridge_enable_send(false);
        ESP_LOGI(TAG, "Audio streaming disabled (all protocols disconnected)");
    }
    return true;
}

bool sx_event_handler_chatbot_audio_channel_closed(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_CHATBOT_AUDIO_CHANNEL_CLOSED) {
        return false;
    }
    
    ESP_LOGI(TAG, "Audio channel closed");
    
    // Update state
    state->ui.audio_channel_opened = false;
    
    // Cleanup alert state
    state->ui.has_alert = false;
    state->ui.alert_status[0] = '\0';
    state->ui.alert_message[0] = '\0';
    state->ui.alert_emotion[0] = '\0';
    
    // Disable audio bridge
    sx_audio_protocol_bridge_enable_receive(false);
    sx_audio_protocol_bridge_enable_send(false);
    
    // Note: Power management will be handled in orchestrator if needed
    // (sx_platform_set_power_save_mode(true))
    
    return true;
}

bool sx_event_handler_system_reboot(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_SYSTEM_REBOOT) {
        return false;
    }
    
    ESP_LOGI(TAG, "System reboot requested");
    
    // Update state to show rebooting
    state->ui.device_state = SX_DEV_BUSY;
    state->ui.status_text = "rebooting";
    
    // Delay để cleanup và log
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Restart system
    esp_restart();
    
    return true; // Will not reach here, but for consistency
}

bool sx_event_handler_system_command(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_SYSTEM_COMMAND) {
        return false;
    }
    
    const char *command = (const char *)evt->ptr;
    if (command != NULL) {
        ESP_LOGI(TAG, "System command received: %s", command);
        
        // Update state
        state->ui.status_text = "system_command";
        
        // Free command string
        sx_event_free_string((char *)evt->ptr);
        
        // Note: Future system commands can be handled here
        // For now, only reboot is implemented
        
        return true;
    }
    return false;
}

// Alert data structure (must match what's allocated in chatbot service)
typedef struct {
    char status[64];
    char message[256];
    char emotion[32];
} alert_data_t;

bool sx_event_handler_alert(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_ALERT) {
        return false;
    }
    
    alert_data_t *alert = (alert_data_t *)evt->ptr;
    if (alert != NULL) {
        ESP_LOGI(TAG, "Alert: status=%s, message=%s, emotion=%s", 
                 alert->status, alert->message, alert->emotion);
        
        // Update state
        state->ui.has_alert = true;
        strncpy(state->ui.alert_status, alert->status, sizeof(state->ui.alert_status) - 1);
        state->ui.alert_status[sizeof(state->ui.alert_status) - 1] = '\0';
        strncpy(state->ui.alert_message, alert->message, sizeof(state->ui.alert_message) - 1);
        state->ui.alert_message[sizeof(state->ui.alert_message) - 1] = '\0';
        strncpy(state->ui.alert_emotion, alert->emotion, sizeof(state->ui.alert_emotion) - 1);
        state->ui.alert_emotion[sizeof(state->ui.alert_emotion) - 1] = '\0';
        
        // Update emotion_id for UI
        state->ui.emotion_id = map_emotion_to_id(alert->emotion);
        
        // Free alert data
        free(alert);
        
        return true;
    }
    return false;
}

bool sx_event_handler_protocol_error(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_PROTOCOL_ERROR) {
        return false;
    }
    
    const char *error_msg = (const char *)evt->ptr;
    if (error_msg != NULL) {
        ESP_LOGE(TAG, "Protocol error: %s", error_msg);
        
        // Update state
        state->ui.has_error = true;
        strncpy(state->ui.error_message, error_msg, sizeof(state->ui.error_message) - 1);
        state->ui.error_message[sizeof(state->ui.error_message) - 1] = '\0';
        state->ui.device_state = SX_DEV_ERROR;
        
        // Free error message string
        sx_event_free_string((char *)evt->ptr);
        
        return true;
    }
    return false;
}

bool sx_event_handler_protocol_timeout(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_PROTOCOL_TIMEOUT) {
        return false;
    }
    
    ESP_LOGW(TAG, "Protocol timeout");
    
    // Update state
    state->ui.has_error = true;
    strncpy(state->ui.error_message, "Connection timeout", sizeof(state->ui.error_message) - 1);
    state->ui.error_message[sizeof(state->ui.error_message) - 1] = '\0';
    state->ui.device_state = SX_DEV_ERROR;
    
    return true;
}

// Hello data structure (must match what's allocated in protocol layer)
typedef struct {
    uint32_t server_sample_rate;
    uint32_t server_frame_duration;
    char session_id[64];
} hello_data_t;

bool sx_event_handler_protocol_hello_received(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_PROTOCOL_HELLO_RECEIVED) {
        return false;
    }
    
    hello_data_t *hello = (hello_data_t *)evt->ptr;
    if (hello != NULL) {
        ESP_LOGI(TAG, "Server hello received: sample_rate=%lu, frame_duration=%lu, session_id=%s",
                 hello->server_sample_rate, hello->server_frame_duration, hello->session_id);
        
        // Update state với server params
        state->ui.server_sample_rate = hello->server_sample_rate;
        state->ui.server_frame_duration = hello->server_frame_duration;
        strncpy(state->ui.session_id, hello->session_id, sizeof(state->ui.session_id) - 1);
        state->ui.session_id[sizeof(state->ui.session_id) - 1] = '\0';
        
        // Update audio bridge frame duration from server (optimization: dynamic frame duration)
        sx_audio_protocol_bridge_update_frame_duration(hello->server_frame_duration);
        
        // Note: Sample rate validation will be done in orchestrator
        // if we have access to audio codec sample_rate
        
        // Free hello data
        free(hello);
        
        return true;
    }
    return false;
}

bool sx_event_handler_protocol_hello_sent(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_PROTOCOL_HELLO_SENT) {
        return false;
    }
    
    ESP_LOGI(TAG, "Hello message sent to server");
    // No state update needed, just log
    return false; // No state change
}

bool sx_event_handler_protocol_hello_timeout(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_PROTOCOL_HELLO_TIMEOUT) {
        return false;
    }
    
    ESP_LOGW(TAG, "Server hello timeout");
    
    // Update state
    state->ui.has_error = true;
    strncpy(state->ui.error_message, "Server hello timeout", sizeof(state->ui.error_message) - 1);
    state->ui.error_message[sizeof(state->ui.error_message) - 1] = '\0';
    state->ui.device_state = SX_DEV_ERROR;
    
    return true;
}

bool sx_event_handler_protocol_reconnecting(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_PROTOCOL_RECONNECTING) {
        return false;
    }
    
    uint32_t attempt = evt->arg0;
    ESP_LOGI(TAG, "Reconnecting to server (attempt %lu)", attempt);
    
    // Update state
    state->ui.status_text = "reconnecting";
    state->ui.device_state = SX_DEV_BUSY;
    
    return true;
}

