#pragma once

#include "sx_event_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

// UI input handler
bool sx_event_handler_ui_input(const sx_event_t *evt, sx_state_t *state);

// Chatbot handlers
bool sx_event_handler_chatbot_stt(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_chatbot_tts_sentence(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_chatbot_emotion(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_chatbot_tts_start(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_chatbot_tts_stop(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_chatbot_audio_channel_opened(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_chatbot_audio_channel_closed(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_chatbot_connected(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_chatbot_disconnected(const sx_event_t *evt, sx_state_t *state);

// System command handlers
bool sx_event_handler_system_reboot(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_system_command(const sx_event_t *evt, sx_state_t *state);

// Alert handler
bool sx_event_handler_alert(const sx_event_t *evt, sx_state_t *state);

// Protocol error and handshake handlers
bool sx_event_handler_protocol_error(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_protocol_timeout(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_protocol_hello_sent(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_protocol_hello_received(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_protocol_hello_timeout(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_protocol_reconnecting(const sx_event_t *evt, sx_state_t *state);

// Audio handlers
bool sx_event_handler_audio_playback_stopped(const sx_event_t *evt, sx_state_t *state);

// Radio handlers
bool sx_event_handler_radio_error(const sx_event_t *evt, sx_state_t *state);

// OTA/Activation handlers
bool sx_event_handler_wifi_connected(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_activation_required(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_activation_done(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_activation_pending(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_activation_timeout(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_ota_finished(const sx_event_t *evt, sx_state_t *state);
bool sx_event_handler_ota_error(const sx_event_t *evt, sx_state_t *state);

#ifdef __cplusplus
}
#endif




