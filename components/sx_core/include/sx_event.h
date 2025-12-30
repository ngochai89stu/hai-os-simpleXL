#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// System-level event types. Keep stable and forward-compatible.
typedef enum {
    SX_EVT_NONE = 0,

    // lifecycle
    SX_EVT_BOOTSTRAP_STARTED,
    SX_EVT_BOOTSTRAP_READY,

    // UI
    SX_EVT_UI_READY,

    // platform
    SX_EVT_PLATFORM_READY,

    // services
    SX_EVT_SERVICES_READY,

    // input
    SX_EVT_UI_INPUT, // payload: sx_ui_input_event_t (defined in sx_ui_state.h)

    // audio service (Phase 4)
    SX_EVT_AUDIO_PLAYBACK_STARTED,
    SX_EVT_AUDIO_PLAYBACK_STOPPED,
    SX_EVT_AUDIO_PLAYBACK_PAUSED,
    SX_EVT_AUDIO_PLAYBACK_RESUMED,
    SX_EVT_AUDIO_RECORDING_STARTED,
    SX_EVT_AUDIO_RECORDING_STOPPED,
    SX_EVT_AUDIO_ERROR,

    // radio service (Phase 4 - online streaming)
    SX_EVT_RADIO_STARTED,
    SX_EVT_RADIO_STOPPED,
    SX_EVT_RADIO_METADATA,
    SX_EVT_RADIO_ERROR,

    // wifi service (Phase 5)
    SX_EVT_WIFI_CONNECTED,
    SX_EVT_WIFI_DISCONNECTED,
    SX_EVT_WIFI_SCAN_COMPLETE,

    // chatbot service (Phase 5)
    SX_EVT_CHATBOT_STT,              // STT result from server (ptr: text string)
    SX_EVT_CHATBOT_TTS_START,        // TTS started
    SX_EVT_CHATBOT_TTS_STOP,         // TTS stopped
    SX_EVT_CHATBOT_TTS_SENTENCE,     // TTS sentence (ptr: text string)
    SX_EVT_CHATBOT_EMOTION,          // LLM emotion (ptr: emotion string)
    SX_EVT_CHATBOT_CONNECTED,        // Chatbot connected
    SX_EVT_CHATBOT_DISCONNECTED,     // Chatbot disconnected
    SX_EVT_CHATBOT_AUDIO_CHANNEL_OPENED,  // Audio channel (UDP) opened (MQTT only)

    // diagnostics
    SX_EVT_ERROR,
} sx_event_type_t;

typedef struct {
    sx_event_type_t type;
    uint32_t arg0;
    uint32_t arg1;
    const void *ptr; // optional pointer payload (ownership rules documented)
} sx_event_t;

#ifdef __cplusplus
}
#endif
