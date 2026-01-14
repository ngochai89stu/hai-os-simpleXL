#pragma once

#include "sx_event_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

// Phase 3: All handlers return uint32_t (dirty_mask) instead of bool
// UI input handler
uint32_t sx_event_handler_ui_input(const sx_event_t *evt, sx_state_t *state);

// Chatbot handlers
uint32_t sx_event_handler_chatbot_stt(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_chatbot_tts_sentence(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_chatbot_emotion(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_chatbot_tts_start(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_chatbot_tts_stop(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_chatbot_audio_channel_opened(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_chatbot_audio_channel_closed(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_chatbot_connected(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_chatbot_disconnected(const sx_event_t *evt, sx_state_t *state);

// System command handlers
uint32_t sx_event_handler_system_reboot(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_system_command(const sx_event_t *evt, sx_state_t *state);

// Alert handler
uint32_t sx_event_handler_alert(const sx_event_t *evt, sx_state_t *state);

// Protocol error and handshake handlers
uint32_t sx_event_handler_protocol_error(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_protocol_timeout(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_protocol_hello_sent(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_protocol_hello_received(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_protocol_hello_timeout(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_protocol_reconnecting(const sx_event_t *evt, sx_state_t *state);

// Audio handlers
uint32_t sx_event_handler_audio_playback_stopped(const sx_event_t *evt, sx_state_t *state);

// Radio handlers
uint32_t sx_event_handler_radio_error(const sx_event_t *evt, sx_state_t *state);

// WiFi handlers (Phase 1: break circular dependency)
uint32_t sx_event_handler_wifi_scan_request(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_wifi_connect_request(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_wifi_state_update(const sx_event_t *evt, sx_state_t *state);

// STT/TTS handlers (Phase 1: break circular dependency)
uint32_t sx_event_handler_stt_state_update(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_tts_state_update(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_tts_speak_request(const sx_event_t *evt, sx_state_t *state);

// OTA/Activation handlers
uint32_t sx_event_handler_wifi_connected(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_activation_required(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_activation_done(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_activation_pending(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_activation_timeout(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_ota_finished(const sx_event_t *evt, sx_state_t *state);
uint32_t sx_event_handler_ota_error(const sx_event_t *evt, sx_state_t *state);

#ifdef __cplusplus
}
#endif




