#pragma once

#include "lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Icon system for UI screens
// Uses LVGL symbols and custom icon rendering

// Icon types for different screens/features
typedef enum {
    UI_ICON_MUSIC_PLAYER,
    UI_ICON_MUSIC_ONLINE,
    UI_ICON_RADIO,
    UI_ICON_SD_CARD,
    UI_ICON_IR_CONTROL,
    UI_ICON_SETTINGS,
    UI_ICON_CHAT,
    UI_ICON_DISPLAY,
    UI_ICON_BLUETOOTH,
    UI_ICON_SCREENSAVER,
    UI_ICON_EQUALIZER,
    UI_ICON_WIFI,
    UI_ICON_VOICE,
    UI_ICON_ABOUT,
    UI_ICON_PLAY,
    UI_ICON_PAUSE,
    UI_ICON_STOP,
    UI_ICON_PREV,
    UI_ICON_NEXT,
    UI_ICON_VOLUME,
    UI_ICON_BRIGHTNESS,
    UI_ICON_BACK,
    UI_ICON_MENU,
    UI_ICON_CLOSE,
    UI_ICON_CHECK,
    UI_ICON_MAX
} ui_icon_type_t;

// Create an icon object using LVGL symbols
// Returns lv_obj_t* (lv_label) that can be styled and positioned
lv_obj_t* ui_icon_create(lv_obj_t *parent, ui_icon_type_t icon_type, uint32_t size);

// Get LVGL symbol string for an icon type
const char* ui_icon_get_symbol(ui_icon_type_t icon_type);

// Create icon button (icon + background)
lv_obj_t* ui_icon_button_create(lv_obj_t *parent, ui_icon_type_t icon_type, uint32_t size);

#ifdef __cplusplus
}
#endif



