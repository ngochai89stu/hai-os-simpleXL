#include "ui_icons.h"
#include <esp_log.h>
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

static const char *TAG = "ui_icons";

// Map icon types to LVGL symbols
// Using LVGL built-in symbols for better compatibility
static const char* s_icon_symbols[] = {
    [UI_ICON_MUSIC_PLAYER] = LV_SYMBOL_AUDIO,      // Music player
    [UI_ICON_MUSIC_ONLINE] = LV_SYMBOL_FILE,       // Online music (file icon)
    [UI_ICON_RADIO] = LV_SYMBOL_AUDIO,            // Radio (use audio symbol)
    [UI_ICON_SD_CARD] = LV_SYMBOL_DRIVE,          // SD card
    [UI_ICON_IR_CONTROL] = LV_SYMBOL_REFRESH,     // IR control (refresh icon)
    [UI_ICON_SETTINGS] = LV_SYMBOL_SETTINGS,      // Settings
    [UI_ICON_CHAT] = LV_SYMBOL_EDIT,              // Chat (edit icon)
    [UI_ICON_DISPLAY] = LV_SYMBOL_VIDEO,         // Display
    [UI_ICON_BLUETOOTH] = LV_SYMBOL_BLUETOOTH,    // Bluetooth
    [UI_ICON_SCREENSAVER] = LV_SYMBOL_IMAGE,      // Screensaver
    [UI_ICON_EQUALIZER] = LV_SYMBOL_VOLUME_MID,   // Equalizer
    [UI_ICON_WIFI] = LV_SYMBOL_WIFI,              // WiFi
    [UI_ICON_VOICE] = LV_SYMBOL_AUDIO,            // Voice (use audio symbol)
    [UI_ICON_ABOUT] = LV_SYMBOL_SETTINGS,         // About (use settings symbol)
    [UI_ICON_PLAY] = LV_SYMBOL_PLAY,              // Play
    [UI_ICON_PAUSE] = LV_SYMBOL_PAUSE,            // Pause
    [UI_ICON_STOP] = LV_SYMBOL_STOP,              // Stop
    [UI_ICON_PREV] = LV_SYMBOL_PREV,              // Previous
    [UI_ICON_NEXT] = LV_SYMBOL_NEXT,              // Next
    [UI_ICON_VOLUME] = LV_SYMBOL_VOLUME_MAX,      // Volume
    [UI_ICON_BRIGHTNESS] = LV_SYMBOL_TINT,       // Brightness (use tint symbol)
    [UI_ICON_BACK] = LV_SYMBOL_LEFT,              // Back
    [UI_ICON_MENU] = LV_SYMBOL_LIST,              // Menu
    [UI_ICON_CLOSE] = LV_SYMBOL_CLOSE,            // Close
    [UI_ICON_CHECK] = LV_SYMBOL_OK,               // Check
};

const char* ui_icon_get_symbol(ui_icon_type_t icon_type) {
    if (icon_type >= 0 && icon_type < UI_ICON_MAX) {
        return s_icon_symbols[icon_type];
    }
    ESP_LOGW(TAG, "Invalid icon type: %d", icon_type);
    return LV_SYMBOL_DUMMY;
}

lv_obj_t* ui_icon_create(lv_obj_t *parent, ui_icon_type_t icon_type, uint32_t size) {
    if (!lvgl_port_lock(0)) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock");
        return NULL;
    }
    
    lv_obj_t *icon = lv_label_create(parent);
    const char *symbol = ui_icon_get_symbol(icon_type);
    lv_label_set_text(icon, symbol);
    
    // Set font size based on size parameter
    // Default to montserrat_14, can be scaled
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_14, 0);
    
    // Set icon color (primary blue)
    lv_obj_set_style_text_color(icon, lv_color_hex(0x5b7fff), 0);
    
    // Scale icon if size is specified
    if (size > 0 && size != 14) {
        // For different sizes, we'd need different fonts or scaling
        // For now, use transform scale
        lv_obj_set_style_transform_scale(icon, (size * 100) / 14, 0);
    }
    
    lvgl_port_unlock();
    return icon;
}

lv_obj_t* ui_icon_button_create(lv_obj_t *parent, ui_icon_type_t icon_type, uint32_t size) {
    if (!lvgl_port_lock(0)) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock");
        return NULL;
    }
    
    // Create button container
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, size + 20, size + 20);  // Add padding
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x3a3a3a), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    
    // Create icon inside button
    lv_obj_t *icon = ui_icon_create(btn, icon_type, size);
    if (icon != NULL) {
        lv_obj_center(icon);
    }
    
    lvgl_port_unlock();
    return btn;
}

