#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_state.h"
#include "sx_state_helper.h"  // P0.4: State version and dirty_mask helpers
#include "sx_event_handler.h"
#include "sx_metrics.h"  // P2.6: Metrics collection
#include "event_handlers.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "sx_orchestrator";

static void sx_orchestrator_task(void *arg) {
    (void)arg;

    ESP_LOGI(TAG, "orchestrator task start");

    // Initialize event handler system
    sx_event_handler_init();

    // Register all event handlers
    sx_event_handler_register(SX_EVT_UI_INPUT, sx_event_handler_ui_input);
    sx_event_handler_register(SX_EVT_CHATBOT_STT, sx_event_handler_chatbot_stt);
    sx_event_handler_register(SX_EVT_CHATBOT_TTS_SENTENCE, sx_event_handler_chatbot_tts_sentence);
    sx_event_handler_register(SX_EVT_CHATBOT_EMOTION, sx_event_handler_chatbot_emotion);
    sx_event_handler_register(SX_EVT_CHATBOT_TTS_START, sx_event_handler_chatbot_tts_start);
    sx_event_handler_register(SX_EVT_CHATBOT_TTS_STOP, sx_event_handler_chatbot_tts_stop);
    sx_event_handler_register(SX_EVT_CHATBOT_AUDIO_CHANNEL_OPENED, sx_event_handler_chatbot_audio_channel_opened);
    sx_event_handler_register(SX_EVT_CHATBOT_AUDIO_CHANNEL_CLOSED, sx_event_handler_chatbot_audio_channel_closed);
    sx_event_handler_register(SX_EVT_CHATBOT_CONNECTED, sx_event_handler_chatbot_connected);
    sx_event_handler_register(SX_EVT_CHATBOT_DISCONNECTED, sx_event_handler_chatbot_disconnected);
    sx_event_handler_register(SX_EVT_AUDIO_PLAYBACK_STOPPED, sx_event_handler_audio_playback_stopped);
    sx_event_handler_register(SX_EVT_RADIO_ERROR, sx_event_handler_radio_error);
    
    // System command handlers
    sx_event_handler_register(SX_EVT_SYSTEM_REBOOT, sx_event_handler_system_reboot);
    sx_event_handler_register(SX_EVT_SYSTEM_COMMAND, sx_event_handler_system_command);
    
    // Alert handler
    sx_event_handler_register(SX_EVT_ALERT, sx_event_handler_alert);
    
    // Protocol error and handshake handlers
    sx_event_handler_register(SX_EVT_PROTOCOL_ERROR, sx_event_handler_protocol_error);
    sx_event_handler_register(SX_EVT_PROTOCOL_TIMEOUT, sx_event_handler_protocol_timeout);
    sx_event_handler_register(SX_EVT_PROTOCOL_HELLO_SENT, sx_event_handler_protocol_hello_sent);
    sx_event_handler_register(SX_EVT_PROTOCOL_HELLO_RECEIVED, sx_event_handler_protocol_hello_received);
    sx_event_handler_register(SX_EVT_PROTOCOL_HELLO_TIMEOUT, sx_event_handler_protocol_hello_timeout);
    sx_event_handler_register(SX_EVT_PROTOCOL_RECONNECTING, sx_event_handler_protocol_reconnecting);

    // OTA/Activation handlers
    sx_event_handler_register(SX_EVT_WIFI_CONNECTED, sx_event_handler_wifi_connected);
    sx_event_handler_register(SX_EVT_ACTIVATION_REQUIRED, sx_event_handler_activation_required);
    sx_event_handler_register(SX_EVT_ACTIVATION_PENDING, sx_event_handler_activation_pending);
    sx_event_handler_register(SX_EVT_ACTIVATION_DONE, sx_event_handler_activation_done);
    sx_event_handler_register(SX_EVT_ACTIVATION_TIMEOUT, sx_event_handler_activation_timeout);
    sx_event_handler_register(SX_EVT_OTA_FINISHED, sx_event_handler_ota_finished);
    sx_event_handler_register(SX_EVT_OTA_ERROR, sx_event_handler_ota_error);

    sx_state_t st;
    sx_dispatcher_get_state(&st);

    // mark bootstrap ready
    // P0.4: Update version and dirty_mask (Section 5.1)
    st.ui.device_state = SX_DEV_IDLE;
    st.ui.status_text = "ready";
    st.ui.emotion_id = "neutral";
    
    // Initialize new state fields
    st.ui.chatbot_connected = false;
    st.ui.audio_channel_opened = false;
    st.ui.server_sample_rate = 0;
    st.ui.server_frame_duration = 0;
    st.ui.session_id[0] = '\0';
    st.ui.has_error = false;
    st.ui.error_message[0] = '\0';
    st.ui.error_code = 0;
    st.ui.has_alert = false;
    st.ui.alert_status[0] = '\0';
    st.ui.alert_message[0] = '\0';
    st.ui.alert_emotion[0] = '\0';
    st.ui.audio_playing = false;
    st.ui.audio_recording = false;
    st.ui.volume = 0;
    st.ui.volume_muted = false;
    st.ui.wifi_connected = false;
    st.ui.wifi_rssi = 0;
    st.ui.wifi_ssid[0] = '\0';
    
    // P0.4: Update version and dirty_mask for UI domain changes
    sx_state_update_version_and_dirty(&st, SX_STATE_DIRTY_UI | SX_STATE_DIRTY_SYSTEM);
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
            // Optimized: Rate-limited logging (only log every 100 events to reduce overhead)
            static uint32_t event_log_counter = 0;
            if (++event_log_counter % 100 == 0) {
                ESP_LOGI(TAG, "Processed %lu events (last: evt=%d arg0=%u)", 
                        event_log_counter, (int)evt.type, (unsigned)evt.arg0);
            }
            
            sx_dispatcher_get_state(&st);
            
            // Process event using registry
            // P0.4: Event handlers should return dirty_mask indicating which domains changed
            uint32_t dirty_mask = 0;
            if (sx_event_handler_process(&evt, &st)) {
                // P0.4: Determine dirty_mask based on event type
                // This is a simplified approach - in production, event handlers should return dirty_mask
                switch (evt.type) {
                    case SX_EVT_WIFI_CONNECTED:
                    case SX_EVT_WIFI_DISCONNECTED:
                    case SX_EVT_WIFI_SCAN_COMPLETE:
                        dirty_mask = SX_STATE_DIRTY_WIFI;
                        break;
                    case SX_EVT_AUDIO_PLAYBACK_STARTED:
                    case SX_EVT_AUDIO_PLAYBACK_STOPPED:
                    case SX_EVT_AUDIO_PLAYBACK_PAUSED:
                    case SX_EVT_AUDIO_PLAYBACK_RESUMED:
                    case SX_EVT_AUDIO_RECORDING_STARTED:
                    case SX_EVT_AUDIO_RECORDING_STOPPED:
                    case SX_EVT_AUDIO_ERROR:
                        dirty_mask = SX_STATE_DIRTY_AUDIO;
                        break;
                    case SX_EVT_UI_INPUT:
                    case SX_EVT_UI_READY:
                    case SX_EVT_CHATBOT_STT:
                    case SX_EVT_CHATBOT_TTS_SENTENCE:
                    case SX_EVT_CHATBOT_EMOTION:
                    case SX_EVT_CHATBOT_CONNECTED:
                    case SX_EVT_CHATBOT_DISCONNECTED:
                    case SX_EVT_ALERT:
                    case SX_EVT_ERROR:
                        dirty_mask = SX_STATE_DIRTY_UI;
                        break;
                    case SX_EVT_SYSTEM_REBOOT:
                    case SX_EVT_SYSTEM_COMMAND:
                    case SX_EVT_BOOTSTRAP_STARTED:
                    case SX_EVT_BOOTSTRAP_READY:
                        dirty_mask = SX_STATE_DIRTY_SYSTEM;
                        break;
                    default:
                        // Default: assume UI domain changed
                        dirty_mask = SX_STATE_DIRTY_UI;
                        break;
                }
                
                // P0.4: Update version and dirty_mask
                sx_state_update_version_and_dirty(&st, dirty_mask);
                sx_dispatcher_set_state(&st);
                
                // P2.6: Update metrics - state version and updates
                sx_metrics_set_state_version(st.version);
                sx_metrics_inc_state_updates();
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
