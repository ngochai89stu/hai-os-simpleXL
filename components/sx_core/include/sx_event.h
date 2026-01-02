#pragma once

#include <stdint.h>
#include "sx_event_payloads.h"  // Event payload structs (P0.1)

#ifdef __cplusplus
extern "C" {
#endif

// Event priority (higher = more important)
typedef enum {
    SX_EVT_PRIORITY_LOW = 0,
    SX_EVT_PRIORITY_NORMAL = 1,
    SX_EVT_PRIORITY_HIGH = 2,
    SX_EVT_PRIORITY_CRITICAL = 3,
} sx_event_priority_t;

// Helper macro to get default priority for event type
#define SX_EVT_DEFAULT_PRIORITY(type) \
    ((type) == SX_EVT_ERROR || (type) == SX_EVT_PROTOCOL_ERROR || (type) == SX_EVT_ALERT ? SX_EVT_PRIORITY_CRITICAL : \
     (type) == SX_EVT_CHATBOT_CONNECTED || (type) == SX_EVT_CHATBOT_DISCONNECTED ? SX_EVT_PRIORITY_HIGH : \
     (type) == SX_EVT_RADIO_ERROR || (type) == SX_EVT_AUDIO_ERROR || (type) == SX_EVT_PROTOCOL_TIMEOUT ? SX_EVT_PRIORITY_HIGH : \
     SX_EVT_PRIORITY_NORMAL)

// P1.2: Event taxonomy with range reservation (Section 4.1 SIMPLEXL_ARCH v1.3)
// Each domain gets 0x0100 (256) IDs to avoid collisions over time
// Range format: BASE = 0xNN00, range = 0xNN00-0xNNFF

// Event ID ranges by domain:
#define SX_EVT_LIFECYCLE_BASE    0x0000  // Range: 0x0000-0x00FF (256 IDs)
#define SX_EVT_UI_BASE           0x0100  // Range: 0x0100-0x01FF (256 IDs)
#define SX_EVT_AUDIO_BASE        0x0200  // Range: 0x0200-0x02FF (256 IDs)
#define SX_EVT_RADIO_BASE        0x0300  // Range: 0x0300-0x03FF (256 IDs)
#define SX_EVT_WIFI_BASE         0x0400  // Range: 0x0400-0x04FF (256 IDs)
#define SX_EVT_CHATBOT_BASE      0x0500  // Range: 0x0500-0x05FF (256 IDs)
#define SX_EVT_SYSTEM_BASE       0x0600  // Range: 0x0600-0x06FF (256 IDs)
#define SX_EVT_PROTOCOL_BASE     0x0700  // Range: 0x0700-0x07FF (256 IDs)
#define SX_EVT_OTA_BASE          0x0800  // Range: 0x0800-0x08FF (256 IDs)
#define SX_EVT_DISPLAY_BASE      0x0900  // Range: 0x0900-0x09FF (256 IDs)
#define SX_EVT_THEME_BASE        0x0A00  // Range: 0x0A00-0x0AFF (256 IDs)
#define SX_EVT_IMAGE_BASE        0x0B00  // Range: 0x0B00-0x0BFF (256 IDs)
#define SX_EVT_QRCODE_BASE       0x0C00  // Range: 0x0C00-0x0CFF (256 IDs)
// Reserved ranges: 0x0D00-0x0FFF for future domains

// System-level event types. Keep stable and forward-compatible.
// P1.2: Reorganized with range reservation (Section 4.1)
typedef enum {
    SX_EVT_NONE = 0,

    // ========================================
    // LIFECYCLE domain (0x0000-0x00FF)
    // ========================================
    SX_EVT_BOOTSTRAP_STARTED = SX_EVT_LIFECYCLE_BASE + 0,
    SX_EVT_BOOTSTRAP_READY,
    SX_EVT_PLATFORM_READY,
    SX_EVT_SERVICES_READY,
    SX_EVT_UI_READY,
    // Reserved: SX_EVT_LIFECYCLE_BASE + 5 to +255

    // ========================================
    // UI domain (0x0100-0x01FF)
    // ========================================
    SX_EVT_UI_INPUT = SX_EVT_UI_BASE + 0,  // payload: sx_ui_input_event_t (defined in sx_ui_state.h)
    // Reserved: SX_EVT_UI_BASE + 1 to +255

    // ========================================
    // AUDIO domain (0x0200-0x02FF)
    // ========================================
    SX_EVT_AUDIO_PLAYBACK_STARTED = SX_EVT_AUDIO_BASE + 0,
    SX_EVT_AUDIO_PLAYBACK_STOPPED,
    SX_EVT_AUDIO_PLAYBACK_PAUSED,
    SX_EVT_AUDIO_PLAYBACK_RESUMED,
    SX_EVT_AUDIO_RECORDING_STARTED,
    SX_EVT_AUDIO_RECORDING_STOPPED,
    SX_EVT_AUDIO_ERROR,
    // Reserved: SX_EVT_AUDIO_BASE + 7 to +255

    // ========================================
    // RADIO domain (0x0300-0x03FF)
    // ========================================
    SX_EVT_RADIO_STARTED = SX_EVT_RADIO_BASE + 0,
    SX_EVT_RADIO_STOPPED,
    SX_EVT_RADIO_METADATA,
    SX_EVT_RADIO_ERROR,
    // Reserved: SX_EVT_RADIO_BASE + 4 to +255

    // ========================================
    // WIFI domain (0x0400-0x04FF)
    // ========================================
    SX_EVT_WIFI_CONNECTED = SX_EVT_WIFI_BASE + 0,
    SX_EVT_WIFI_DISCONNECTED,
    SX_EVT_WIFI_SCAN_COMPLETE,
    // Reserved: SX_EVT_WIFI_BASE + 3 to +255

    // ========================================
    // CHATBOT domain (0x0500-0x05FF)
    // ========================================
    SX_EVT_CHATBOT_STT = SX_EVT_CHATBOT_BASE + 0,              // STT result from server (ptr: text string)
    SX_EVT_CHATBOT_TTS_START,        // TTS started
    SX_EVT_CHATBOT_TTS_STOP,         // TTS stopped
    SX_EVT_CHATBOT_TTS_SENTENCE,     // TTS sentence (ptr: text string)
    SX_EVT_CHATBOT_EMOTION,          // LLM emotion (ptr: emotion string)
    SX_EVT_CHATBOT_CONNECTED,        // Chatbot connected
    SX_EVT_CHATBOT_DISCONNECTED,     // Chatbot disconnected
    SX_EVT_CHATBOT_AUDIO_CHANNEL_OPENED,  // Audio channel (UDP) opened (MQTT only)
    SX_EVT_CHATBOT_AUDIO_CHANNEL_CLOSED,  // Audio channel closed
    // Reserved: SX_EVT_CHATBOT_BASE + 9 to +255

    // ========================================
    // SYSTEM domain (0x0600-0x06FF)
    // ========================================
    SX_EVT_SYSTEM_REBOOT = SX_EVT_SYSTEM_BASE + 0,            // System reboot command
    SX_EVT_SYSTEM_COMMAND,           // Generic system command (ptr: command string)
    SX_EVT_ALERT,                    // Alert message (ptr: alert_data_t*)
    SX_EVT_ERROR,                    // Generic error
    // Reserved: SX_EVT_SYSTEM_BASE + 4 to +255

    // ========================================
    // PROTOCOL domain (0x0700-0x07FF)
    // ========================================
    SX_EVT_PROTOCOL_ERROR = SX_EVT_PROTOCOL_BASE + 0,           // Protocol error (ptr: error message string)
    SX_EVT_PROTOCOL_TIMEOUT,         // Protocol timeout
    SX_EVT_PROTOCOL_HELLO_SENT,      // Hello message sent
    SX_EVT_PROTOCOL_HELLO_RECEIVED,  // Server hello received (ptr: hello_data_t*)
    SX_EVT_PROTOCOL_HELLO_TIMEOUT,   // Server hello timeout
    SX_EVT_PROTOCOL_RECONNECTING,     // Reconnection in progress (arg0: attempt number)
    // Reserved: SX_EVT_PROTOCOL_BASE + 6 to +255

    // ========================================
    // OTA/ACTIVATION domain (0x0800-0x08FF)
    // ========================================
    SX_EVT_OTA_PROGRESS = SX_EVT_OTA_BASE + 0,              // arg0 = percent, arg1 = speed KB/s
    SX_EVT_OTA_FINISHED,              // ptr = version string
    SX_EVT_OTA_ERROR,                 // ptr = error message
    SX_EVT_ACTIVATION_REQUIRED,       // ptr = activation code string
    SX_EVT_ACTIVATION_PENDING,        // activation pending (HTTP 202), arg0=attempt, arg1=delay_ms
    SX_EVT_ACTIVATION_DONE,           // activation success
    SX_EVT_ACTIVATION_TIMEOUT,        // activation timed out after polling
    // Reserved: SX_EVT_OTA_BASE + 7 to +255

    // ========================================
    // DISPLAY domain (0x0900-0x09FF) - P0.1
    // ========================================
    SX_EVT_DISPLAY_CAPTURE_REQUEST = SX_EVT_DISPLAY_BASE + 0,   // Service → UI: request screen capture (arg0=width, arg1=height)
    SX_EVT_DISPLAY_CAPTURE_DONE,       // UI → Service: capture done (ptr: sx_display_capture_payload_t*)
    SX_EVT_DISPLAY_PREVIEW_REQUEST,     // Service → UI: show preview (ptr: sx_display_preview_payload_t*)
    SX_EVT_DISPLAY_PREVIEW_DONE,       // UI → Service: preview shown
    SX_EVT_DISPLAY_HIDE_REQUEST,        // Service → UI: hide preview
    // Reserved: SX_EVT_DISPLAY_BASE + 5 to +255

    // ========================================
    // THEME domain (0x0A00-0x0AFF) - P0.1
    // ========================================
    SX_EVT_THEME_CHANGED = SX_EVT_THEME_BASE + 0,              // Service → UI: theme changed (ptr: sx_theme_data_t*)
    // Reserved: SX_EVT_THEME_BASE + 1 to +255

    // ========================================
    // IMAGE domain (0x0B00-0x0BFF) - P0.1
    // ========================================
    SX_EVT_IMAGE_LOAD_REQUEST = SX_EVT_IMAGE_BASE + 0,          // UI → Service: load image (ptr: path string)
    SX_EVT_IMAGE_LOADED,                // Service → UI: image loaded (ptr: sx_image_load_payload_t*)
    // Reserved: SX_EVT_IMAGE_BASE + 2 to +255

    // ========================================
    // QRCODE domain (0x0C00-0x0CFF) - P0.1
    // ========================================
    SX_EVT_QR_CODE_GENERATE_REQUEST = SX_EVT_QRCODE_BASE + 0,    // UI → Service: generate QR (ptr: text string)
    SX_EVT_QR_CODE_GENERATED,           // Service → UI: QR generated (ptr: sx_qr_code_result_t*)
    // Reserved: SX_EVT_QRCODE_BASE + 2 to +255

    // ========================================
    // Reserved ranges for future domains:
    // 0x0D00-0x0FFF: Reserved for future expansion
    // ========================================
} sx_event_type_t;

typedef struct {
    sx_event_type_t type;
    sx_event_priority_t priority;  // Default to NORMAL if not set (0 = not set, use default)
    uint32_t arg0;
    uint32_t arg1;
    const void *ptr; // optional pointer payload (ownership rules documented)
} sx_event_t;

#ifdef __cplusplus
}
#endif
