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

#define SX_UI_MESSAGE_MAX_LEN 256
#define SX_UI_SESSION_ID_MAX_LEN 64
#define SX_UI_ERROR_MSG_MAX_LEN 128
#define SX_UI_ALERT_STATUS_MAX_LEN 64
#define SX_UI_ALERT_MSG_MAX_LEN 256
#define SX_UI_ALERT_EMOTION_MAX_LEN 32
#define SX_UI_WIFI_SSID_MAX_LEN 32

typedef struct {
    // UI renders these fields; UI never mutates them.
    sx_device_state_t device_state;
    const char *status_text;   // points to constant strings for now
    const char *emotion_id;    // points to constant strings for now

    // Chat message buffers
    char last_user_message[SX_UI_MESSAGE_MAX_LEN];
    char last_assistant_message[SX_UI_MESSAGE_MAX_LEN];
    
    // Chatbot state
    bool chatbot_connected;
    bool audio_channel_opened;
    uint32_t server_sample_rate;
    uint32_t server_frame_duration;
    char session_id[SX_UI_SESSION_ID_MAX_LEN];
    
    // Error state
    bool has_error;
    char error_message[SX_UI_ERROR_MSG_MAX_LEN];
    uint32_t error_code;
    
    // Alert state
    bool has_alert;
    char alert_status[SX_UI_ALERT_STATUS_MAX_LEN];
    char alert_message[SX_UI_ALERT_MSG_MAX_LEN];
    char alert_emotion[SX_UI_ALERT_EMOTION_MAX_LEN];
    
    // Audio state (detailed)
    bool audio_playing;
    bool audio_recording;
    uint8_t volume;
    bool volume_muted;
    
    // WiFi state (detailed)
    bool wifi_connected;
    int8_t wifi_rssi;
    char wifi_ssid[SX_UI_WIFI_SSID_MAX_LEN];
} sx_ui_state_t;

// P0.4: State version and dirty mask (Section 5.1 SIMPLEXL_ARCH v1.3)
// Dirty mask bits (bit per domain)
#define SX_STATE_DIRTY_WIFI      (1U << 0)
#define SX_STATE_DIRTY_AUDIO    (1U << 1)
#define SX_STATE_DIRTY_UI       (1U << 2)
#define SX_STATE_DIRTY_SYSTEM   (1U << 3)
// Reserved for future domains (4-31)

typedef struct {
    uint32_t version;      // P0.4: Monotonically increasing version (Section 5.1)
    uint32_t dirty_mask;   // P0.4: Bitmask indicating which domains changed (Section 5.1)
    uint32_t seq;          // Legacy: monotonically increasing snapshot sequence (kept for compatibility)
    sx_wifi_state_t wifi;
    sx_audio_state_t audio;
    sx_ui_state_t ui;
} sx_state_t;

#ifdef __cplusplus
}
#endif
