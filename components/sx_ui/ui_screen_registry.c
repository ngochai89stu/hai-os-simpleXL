#include "ui_screen_registry.h"

#include <esp_log.h>
#include <string.h>

static const char *TAG = "ui_screen_registry";

#define MAX_SCREENS SCREEN_ID_MAX

// Screen registry storage
static ui_screen_callbacks_t s_screen_callbacks[MAX_SCREENS];
static bool s_registry_initialized = false;

// Screen name mapping (for logging/debugging)
// Total: 29 screens (P0: 20, P1: 2, P2: 7)
static const char* s_screen_names[MAX_SCREENS] = {
    // P0 - Core Product Screens (20)
    [SCREEN_ID_BOOT] = "boot",
    [SCREEN_ID_FLASH] = "flash",
    [SCREEN_ID_HOME] = "home",
    [SCREEN_ID_CHAT] = "chat",
    [SCREEN_ID_WAKEWORD_FEEDBACK] = "wakeword-feedback",
    [SCREEN_ID_MUSIC_ONLINE_LIST] = "music-online-list",
    [SCREEN_ID_MUSIC_PLAYER] = "music-player",
    [SCREEN_ID_RADIO] = "radio",
    [SCREEN_ID_SD_CARD_MUSIC] = "sd-card-music",
    [SCREEN_ID_IR_CONTROL] = "ir-control",
    [SCREEN_ID_SETTINGS] = "settings",
    [SCREEN_ID_WIFI_SETUP] = "wifi-setup",
    [SCREEN_ID_BLUETOOTH_SETTING] = "settings-bluetooth",
    [SCREEN_ID_DISPLAY_SETTING] = "settings-display",
    [SCREEN_ID_EQUALIZER] = "settings-equalizer",
    [SCREEN_ID_OTA] = "ota-update",
    [SCREEN_ID_ABOUT] = "about",
    [SCREEN_ID_GOOGLE_NAVIGATION] = "google-navigation",
    [SCREEN_ID_PERMISSION] = "settings-permission",
    [SCREEN_ID_SCREENSAVER] = "screensaver",
    [SCREEN_ID_SCREENSAVER_SETTING] = "settings-screensaver",
    
    // P1 - Advanced Feature Screens (2)
    [SCREEN_ID_AUDIO_EFFECTS] = "settings-audio-effects",
    [SCREEN_ID_STARTUP_IMAGE_PICKER] = "settings-startup-image",
    
    // P2 - Developer & Diagnostic Screens (7)
    [SCREEN_ID_VOICE_SETTINGS] = "settings-voice",
    [SCREEN_ID_NETWORK_DIAGNOSTIC] = "settings-network-diagnostic",
    [SCREEN_ID_SNAPSHOT_MANAGER] = "settings-snapshot-manager",
    [SCREEN_ID_DIAGNOSTICS] = "debug-diagnostics",
    [SCREEN_ID_INTROSPECTION] = "debug-introspection",
    [SCREEN_ID_DEV_CONSOLE] = "debug-dev-console",
    [SCREEN_ID_TOUCH_DEBUG] = "debug-touch",
};

void ui_screen_registry_init(void) {
    if (s_registry_initialized) {
        ESP_LOGW(TAG, "Registry already initialized");
        return;
    }
    
    memset(s_screen_callbacks, 0, sizeof(s_screen_callbacks));
    s_registry_initialized = true;
    ESP_LOGI(TAG, "Screen registry initialized (max screens: %d)", MAX_SCREENS);
}

bool ui_screen_registry_register(ui_screen_id_t screen_id, const ui_screen_callbacks_t *callbacks) {
    if (!s_registry_initialized) {
        ESP_LOGE(TAG, "Registry not initialized");
        return false;
    }
    
    if (screen_id >= SCREEN_ID_MAX) {
        ESP_LOGE(TAG, "Invalid screen_id: %d (max: %d)", screen_id, SCREEN_ID_MAX);
        return false;
    }
    
    if (callbacks == NULL) {
        ESP_LOGE(TAG, "callbacks is NULL for screen_id: %d", screen_id);
        return false;
    }
    
    s_screen_callbacks[screen_id] = *callbacks;
    ESP_LOGI(TAG, "Registered screen: %s (id: %d)", 
             ui_screen_registry_get_name(screen_id), screen_id);
    return true;
}

const ui_screen_callbacks_t* ui_screen_registry_get(ui_screen_id_t screen_id) {
    if (!s_registry_initialized || screen_id >= SCREEN_ID_MAX) {
        return NULL;
    }
    
    // Check if callbacks are set (at least one callback must be non-NULL)
    if (s_screen_callbacks[screen_id].on_create == NULL &&
        s_screen_callbacks[screen_id].on_show == NULL &&
        s_screen_callbacks[screen_id].on_hide == NULL &&
        s_screen_callbacks[screen_id].on_destroy == NULL) {
        return NULL;
    }
    
    return &s_screen_callbacks[screen_id];
}

const char* ui_screen_registry_get_name(ui_screen_id_t screen_id) {
    if (screen_id >= SCREEN_ID_MAX) {
        return "unknown";
    }
    return s_screen_names[screen_id] ? s_screen_names[screen_id] : "unnamed";
}


