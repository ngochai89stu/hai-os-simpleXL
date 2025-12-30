#pragma once

#include <stdbool.h>
#include "sx_state.h"

#ifdef __cplusplus
extern "C" {
#endif

// Defines all screens in the application (29 screens total)
// Based on: Web Demo (18) + Legacy Essential (2) + Legacy Advanced (2) + Legacy Debug (7)
// See: reports/UI_SCREEN_ANALYSIS_AND_RECOMMENDATION.md
typedef enum {
    // P0 - Core Product Screens (20 screens)
    SCREEN_ID_BOOT,                    // Web Demo: BootScreen
    SCREEN_ID_FLASH,                   // Web Demo: FlashScreen (optional welcome splash)
    SCREEN_ID_HOME,                    // Web Demo: HomeScreen (main menu)
    SCREEN_ID_CHAT,                    // Web Demo: ChatScreen
    SCREEN_ID_WAKEWORD_FEEDBACK,        // Web Demo: WakewordFeedbackScreen
    SCREEN_ID_MUSIC_ONLINE_LIST,       // Web Demo: MusicOnlineListScreen
    SCREEN_ID_MUSIC_PLAYER,            // Web Demo: MusicPlayerScreen
    SCREEN_ID_RADIO,                   // Web Demo: RadioScreen
    SCREEN_ID_SD_CARD_MUSIC,           // Web Demo: SDCardMusicScreen
    SCREEN_ID_IR_CONTROL,              // Web Demo: IRControlScreen
    SCREEN_ID_SETTINGS,                // Web Demo: SettingsScreen
    SCREEN_ID_WIFI_SETUP,              // Web Demo: WifiSetupScreen
    SCREEN_ID_BLUETOOTH_SETTING,       // Web Demo: BluetoothSettingScreen
    SCREEN_ID_DISPLAY_SETTING,         // Web Demo: DisplaySettingScreen
    SCREEN_ID_EQUALIZER,               // Web Demo: EqualizerScreen
    SCREEN_ID_OTA,                     // Web Demo: OtaScreen
    SCREEN_ID_ABOUT,                   // Web Demo: AboutScreen
    SCREEN_ID_GOOGLE_NAVIGATION,       // Web Demo: GoogleNavigationScreen
    SCREEN_ID_PERMISSION,               // Legacy: PermissionScreen (essential privacy)
    SCREEN_ID_SCREENSAVER,              // Legacy: ScreensaverScreen (core UX)
    SCREEN_ID_SCREENSAVER_SETTING,     // Legacy: ScreensaverSettingScreen
    
    // P1 - Advanced Feature Screens (2 screens)
    SCREEN_ID_AUDIO_EFFECTS,            // Legacy: AudioEffectsScreen (merge into Equalizer in Phase 4)
    SCREEN_ID_STARTUP_IMAGE_PICKER,     // Legacy: StartupImagePickerScreen (user customization)
    
    // P2 - Developer & Diagnostic Screens (7 screens - Hidden/Debug Only)
    SCREEN_ID_VOICE_SETTINGS,           // Legacy: VoiceSettingsScreen (low-level audio tuning)
    SCREEN_ID_NETWORK_DIAGNOSTIC,       // Legacy: NetworkDiagnosticScreen
    SCREEN_ID_SNAPSHOT_MANAGER,         // Legacy: SnapshotManagerScreen
    SCREEN_ID_DIAGNOSTICS,              // Legacy: DiagnosticsScreen (system metrics)
    SCREEN_ID_INTROSPECTION,            // Legacy: IntrospectionScreen (runtime state)
    SCREEN_ID_DEV_CONSOLE,              // Legacy: DevConsoleScreen (command console)
    SCREEN_ID_TOUCH_DEBUG,              // Legacy: TouchDebugScreen
    
    SCREEN_ID_MAX,                      // Total: 29 screens
} ui_screen_id_t;

// Screen lifecycle callbacks (C-style, no business logic)
typedef struct {
    void (*on_create)(void);      // Called once when screen is created
    void (*on_show)(void);        // Called when screen becomes visible
    void (*on_hide)(void);        // Called when screen is hidden
    void (*on_destroy)(void);     // Called when screen is destroyed
    void (*on_update)(const sx_state_t *state);  // Called periodically to update UI from state (optional)
} ui_screen_callbacks_t;

// Screen registry API
void ui_screen_registry_init(void);
bool ui_screen_registry_register(ui_screen_id_t screen_id, const ui_screen_callbacks_t *callbacks);
const ui_screen_callbacks_t* ui_screen_registry_get(ui_screen_id_t screen_id);
const char* ui_screen_registry_get_name(ui_screen_id_t screen_id);

#ifdef __cplusplus
}
#endif


