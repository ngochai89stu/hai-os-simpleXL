#pragma once

#include <esp_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 2: Intent Engine v2 – Expanded voice-command set and routing

// Intent types (expandable)
typedef enum {
    SX_INTENT_UNKNOWN = 0,

    // ─── Media – Music ─────────────────────────────────────────────────────────
    SX_INTENT_MUSIC_PLAY,
    SX_INTENT_MUSIC_STOP,
    SX_INTENT_MUSIC_PAUSE,
    SX_INTENT_MUSIC_RESUME,
    SX_INTENT_MUSIC_NEXT,
    SX_INTENT_MUSIC_PREVIOUS,

    // ─── Media – Radio ─────────────────────────────────────────────────────────
    SX_INTENT_RADIO_PLAY,
    SX_INTENT_RADIO_STOP,

    // ─── Volume ───────────────────────────────────────────────────────────────
    SX_INTENT_VOLUME_UP,
    SX_INTENT_VOLUME_DOWN,
    SX_INTENT_VOLUME_SET,        // value = absolute level 0-100
    SX_INTENT_VOLUME_MUTE,
    SX_INTENT_VOLUME_UNMUTE,

    // ─── Playback seek ────────────────────────────────────────────────────────
    SX_INTENT_SEEK_POSITION,     // value = seconds (or % if entity ends with "%")

    // ─── Connectivity ─────────────────────────────────────────────────────────
    SX_INTENT_WIFI_CONNECT,
    SX_INTENT_WIFI_DISCONNECT,

    // ─── IoT / IR / AC ────────────────────────────────────────────────────────
    SX_INTENT_IR_SEND,
    SX_INTENT_AC_CONTROL,

    SX_INTENT_MAX
} sx_intent_type_t;

// Intent structure
typedef struct {
    sx_intent_type_t type;
    char entity[128];      // Extracted entity (e.g., song name, station name)
    int value;             // Numeric value (e.g., volume level, seconds)
} sx_intent_t;

// Intent callback function type
typedef esp_err_t (*sx_intent_handler_t)(const sx_intent_t *intent);

// Initialize intent service
esp_err_t sx_intent_service_init(void);

// Register intent handler (returns ESP_OK / error)
esp_err_t sx_intent_register_handler(sx_intent_type_t type, sx_intent_handler_t handler);

// Parse text input and extract intent (fills intent struct)
esp_err_t sx_intent_parse(const char *text, sx_intent_t *intent);

// Parse and immediately dispatch intent to registered handlers
esp_err_t sx_intent_execute(const char *text);

#ifdef __cplusplus
}
#endif
