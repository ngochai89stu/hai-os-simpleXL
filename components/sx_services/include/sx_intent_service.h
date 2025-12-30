#pragma once

#include <esp_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: Intent Service - Basic voice command parsing and routing

// Intent types
typedef enum {
    SX_INTENT_UNKNOWN = 0,
    SX_INTENT_MUSIC_PLAY,
    SX_INTENT_MUSIC_STOP,
    SX_INTENT_MUSIC_PAUSE,
    SX_INTENT_RADIO_PLAY,
    SX_INTENT_RADIO_STOP,
    SX_INTENT_VOLUME_UP,
    SX_INTENT_VOLUME_DOWN,
    SX_INTENT_VOLUME_SET,
    SX_INTENT_WIFI_CONNECT,
    SX_INTENT_WIFI_DISCONNECT,
    SX_INTENT_IR_SEND,
    SX_INTENT_AC_CONTROL,
} sx_intent_type_t;

// Intent structure
typedef struct {
    sx_intent_type_t type;
    char entity[128];      // Extracted entity (e.g., song name, station name)
    int value;             // Numeric value (e.g., volume level)
} sx_intent_t;

// Intent callback function type
typedef esp_err_t (*sx_intent_handler_t)(const sx_intent_t *intent);

// Initialize intent service
esp_err_t sx_intent_service_init(void);

// Register intent handler
esp_err_t sx_intent_register_handler(sx_intent_type_t type, sx_intent_handler_t handler);

// Parse text input and extract intent
esp_err_t sx_intent_parse(const char *text, sx_intent_t *intent);

// Execute intent (parse and route to handler)
esp_err_t sx_intent_execute(const char *text);

#ifdef __cplusplus
}
#endif







