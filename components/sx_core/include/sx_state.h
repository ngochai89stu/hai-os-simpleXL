#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Immutable snapshot-style state.
// - Single writer: orchestrator (sx_core)
// - Multiple readers: UI task and any service that needs read-only view

typedef enum {
    SX_DEV_UNKNOWN = 0,
    SX_DEV_BOOTING,
    SX_DEV_IDLE,
    SX_DEV_BUSY,
    SX_DEV_ERROR,
} sx_device_state_t;

typedef struct {
    bool initialized;
    bool connected;
    int8_t rssi;
} sx_wifi_state_t;

typedef struct {
    bool initialized;
    bool playing;
    uint8_t volume;
} sx_audio_state_t;

typedef struct {
    // UI renders these fields; UI never mutates them.
    sx_device_state_t device_state;
    const char *status_text;   // points to constant strings for now
    const char *emotion_id;    // points to constant strings for now
} sx_ui_state_t;

typedef struct {
    uint32_t seq; // monotonically increasing snapshot sequence
    sx_wifi_state_t wifi;
    sx_audio_state_t audio;
    sx_ui_state_t ui;
} sx_state_t;

#ifdef __cplusplus
}
#endif
