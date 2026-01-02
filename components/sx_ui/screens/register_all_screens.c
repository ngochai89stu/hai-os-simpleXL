// Register all screens with the screen registry
// This file centralizes all screen registration calls

#include "ui_screen_registry.h"

// P0 - Core Product Screens
#include "screen_boot.h"
#include "screen_flash.h"
#include "screen_home.h"
#include "screen_chat.h"
#include "screen_wakeword_feedback.h"
#include "screen_music_online_list.h"
#include "screen_music_player.h"
#include "screen_radio.h"
#include "screen_sd_card_music.h"
#include "screen_ir_control.h"
#include "screen_settings.h"
#include "screen_wifi_setup.h"
#include "screen_bluetooth_setting.h"
#include "screen_display_setting.h"
#include "screen_equalizer.h"
#include "screen_ota.h"
#include "screen_about.h"
#include "screen_google_navigation.h"
#include "screen_permission.h"
#include "screen_screensaver.h"
#include "screen_screensaver_setting.h"
#include "screen_ac_control.h"
#include "screen_system_info.h"
#include "screen_quick_settings.h"

// P1 - Advanced Feature Screens
// screen_audio_effects.h removed - merged into Equalizer
#include "screen_startup_image_picker.h"

// P2 - Developer & Diagnostic Screens
#include "screen_voice_settings.h"
#include "screen_network_diagnostic.h"
#include "screen_snapshot_manager.h"
#include "screen_diagnostics.h"
#include "screen_introspection.h"
#include "screen_dev_console.h"
#include "screen_touch_debug.h"

void register_all_screens(void) {
    // P0 - Core Product Screens (20)
    screen_boot_register();
    screen_flash_register();
    screen_home_register();
    screen_chat_register();
    screen_wakeword_feedback_register();
    screen_music_online_list_register();
    screen_music_player_register();
    screen_radio_register();
    screen_sd_card_music_register();
    screen_ir_control_register();
    screen_settings_register();
    screen_wifi_setup_register();
    screen_bluetooth_setting_register();
    screen_display_setting_register();
    screen_equalizer_register();
    screen_ota_register();
    screen_about_register();
    screen_google_navigation_register();
    screen_permission_register();
    screen_screensaver_register();
    screen_screensaver_setting_register();
    screen_ac_control_register();
    screen_system_info_register();
    screen_quick_settings_register();
    
    // P1 - Advanced Feature Screens (1)
    // screen_audio_effects_register() removed - merged into Equalizer
    screen_startup_image_picker_register();
    
    // P2 - Developer & Diagnostic Screens (7)
    screen_voice_settings_register();
    screen_network_diagnostic_register();
    screen_snapshot_manager_register();
    screen_diagnostics_register();
    screen_introspection_register();
    screen_dev_console_register();
    screen_touch_debug_register();
}






