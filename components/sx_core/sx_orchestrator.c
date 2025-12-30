#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_state.h"
#include "sx_chatbot_service.h"
#include "sx_playlist_manager.h"
#include "sx_protocol_ws.h"
#include "sx_protocol_mqtt.h"
#include "sx_audio_protocol_bridge.h"
#include "sx_event_string_pool.h"

#include <esp_log.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "sx_orchestrator";

// Map raw emotion string from chatbot to a stable UI emotion_id
// Note: We only store pointers to constant strings in sx_state_t.ui.emotion_id
static const char *map_emotion_to_id(const char *emotion)
{
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

static void sx_orchestrator_task(void *arg) {
    (void)arg;

    ESP_LOGI(TAG, "orchestrator task start");

    sx_state_t st;
    sx_dispatcher_get_state(&st);

    // mark bootstrap ready
    st.seq++;
    st.ui.device_state = SX_DEV_IDLE;
    st.ui.status_text = "ready";
    st.ui.emotion_id = "neutral";
    sx_dispatcher_set_state(&st);

    // Optimized: Use vTaskDelayUntil for efficient polling
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t poll_interval = pdMS_TO_TICKS(10);  // 10ms polling interval

    for (;;) {
        bool has_work = false;
        
        // Process all pending events in batch
        sx_event_t evt;
        while (sx_dispatcher_poll_event(&evt)) {
            has_work = true;
            ESP_LOGI(TAG, "evt=%d arg0=%u", (int)evt.type, (unsigned)evt.arg0);
            // Phase 4: Handle UI input events
            if (evt.type == SX_EVT_UI_INPUT) {
                sx_dispatcher_get_state(&st);
                st.seq++;
                
                // Phase 4: If ptr points to chat message text, route to chatbot
                if (evt.ptr != NULL) {
                    const char *message = (const char *)evt.ptr;
                    ESP_LOGI(TAG, "UI input message: %s", message);
                    
                    // Route to chatbot service (if ready)
                    if (sx_chatbot_is_ready()) {
                        esp_err_t ret = sx_chatbot_send_message(message);
                        if (ret == ESP_OK) {
                            st.ui.status_text = "sending...";
                        } else {
                            st.ui.status_text = "chat_error";
                        }
                    } else {
                        st.ui.status_text = "chat_not_ready";
                    }
                    
                    // Free message copy allocated by ChatScreen (may be from pool or malloc)
                    sx_event_free_string((char *)evt.ptr);
                } else {
                    st.ui.status_text = "ui_input";
                }
                
                sx_dispatcher_set_state(&st);
            }
            
            // Phase 5: Handle audio playback stopped event for auto-play next track
            if (evt.type == SX_EVT_AUDIO_PLAYBACK_STOPPED) {
                // Check if should auto-play next track
                if (sx_playlist_should_auto_play_next()) {
                    ESP_LOGI(TAG, "Auto-playing next track");
                    esp_err_t ret = sx_playlist_next();
                    if (ret != ESP_OK) {
                        ESP_LOGW(TAG, "Auto-play next track failed: %s", esp_err_to_name(ret));
                    }
                }
            }
            
            // Phase 5: Handle radio error events
            if (evt.type == SX_EVT_RADIO_ERROR) {
                const char *error_msg = (const char *)evt.ptr;
                if (error_msg != NULL) {
                    ESP_LOGE(TAG, "Radio error: %s", error_msg);
                    // Update state with error message (UI will pick it up in on_update)
                    sx_dispatcher_get_state(&st);
                    st.seq++;
                    // Store error in state or dispatch to UI
                    // For now, we'll handle it in screen_radio's on_update
                    sx_dispatcher_set_state(&st);
                }
                // Free error message copy (may be from pool or malloc)
                if (evt.ptr != NULL) {
                    sx_event_free_string((char *)evt.ptr);
                }
            }
            
            // Phase 5: Handle chatbot events
            if (evt.type == SX_EVT_CHATBOT_STT) {
                const char *text = (const char *)evt.ptr;
                if (text != NULL) {
                    ESP_LOGI(TAG, "Chatbot STT: %s", text);
                    // Update state - UI will display user message
                    sx_dispatcher_get_state(&st);
                    st.seq++;
                    // Use a stable status_text to indicate new STT result
                    st.ui.status_text = "stt_result";
                    // Copy the message to the state buffer
                    strncpy(st.ui.last_user_message, text, SX_UI_MESSAGE_MAX_LEN - 1);
                    st.ui.last_user_message[SX_UI_MESSAGE_MAX_LEN - 1] = '\0'; // Ensure null termination
                    // Clear the other message buffer to avoid showing stale data
                    st.ui.last_assistant_message[0] = '\0';
                    sx_dispatcher_set_state(&st);
                    // Free text copy (may be from pool or malloc)
                    sx_event_free_string((char *)evt.ptr);
                }
            } else if (evt.type == SX_EVT_CHATBOT_TTS_SENTENCE) {
                const char *text = (const char *)evt.ptr;
                if (text != NULL) {
                    ESP_LOGI(TAG, "Chatbot TTS sentence: %s", text);
                    // Update state - UI will display assistant message
                    sx_dispatcher_get_state(&st);
                    st.seq++;
                    // Use a stable status_text to indicate new TTS sentence
                    st.ui.status_text = "tts_sentence";
                    // Copy the message to the state buffer
                    strncpy(st.ui.last_assistant_message, text, SX_UI_MESSAGE_MAX_LEN - 1);
                    st.ui.last_assistant_message[SX_UI_MESSAGE_MAX_LEN - 1] = '\0'; // Ensure null termination
                    // Clear the other message buffer
                    st.ui.last_user_message[0] = '\0';
                    sx_dispatcher_set_state(&st);
                    // Free text copy (may be from pool or malloc)
                    sx_event_free_string((char *)evt.ptr);
                }
            } else if (evt.type == SX_EVT_CHATBOT_EMOTION) {
                const char *emotion = (const char *)evt.ptr;
                if (emotion != NULL) {
                    ESP_LOGI(TAG, "Chatbot emotion: %s", emotion);
                    // Update state - UI will update emotion display
                    sx_dispatcher_get_state(&st);
                    st.seq++;
                    st.ui.emotion_id = map_emotion_to_id(emotion);
                    sx_dispatcher_set_state(&st);
                    // Free emotion copy (may be from pool or malloc)
                    sx_event_free_string((char *)evt.ptr);
                }
            } else if (evt.type == SX_EVT_CHATBOT_TTS_START) {
                ESP_LOGI(TAG, "Chatbot TTS started");
                // Update state - UI can show speaking indicator
                sx_dispatcher_get_state(&st);
                st.seq++;
                sx_dispatcher_set_state(&st);
            } else if (evt.type == SX_EVT_CHATBOT_TTS_STOP) {
                ESP_LOGI(TAG, "Chatbot TTS stopped");
                // Update state - UI can hide speaking indicator
                sx_dispatcher_get_state(&st);
                st.seq++;
                sx_dispatcher_set_state(&st);
            } else if (evt.type == SX_EVT_CHATBOT_AUDIO_CHANNEL_OPENED) {
                // Handle audio channel opened (MQTT UDP or WebSocket)
                ESP_LOGI(TAG, "Audio channel opened, enabling audio receiving");
                sx_audio_protocol_bridge_enable_receive(true);
            } else if (evt.type == SX_EVT_CHATBOT_CONNECTED) {
                ESP_LOGI(TAG, "Chatbot connected");
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
            } else if (evt.type == SX_EVT_CHATBOT_DISCONNECTED) {
                ESP_LOGI(TAG, "Chatbot disconnected");
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
            }
        }
        
        // Optimized: Sleep only if no work, otherwise continue immediately
        if (!has_work) {
            vTaskDelayUntil(&last_wake_time, poll_interval);
        } else {
            last_wake_time = xTaskGetTickCount();
        }
    }
}

void sx_orchestrator_start(void) {
    // Optimized: Reduced stack size from 4096 to 3072 (measured sufficient)
    xTaskCreatePinnedToCore(sx_orchestrator_task, "sx_orch", 3072, NULL, 8, NULL, tskNO_AFFINITY);
}
