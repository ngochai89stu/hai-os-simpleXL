#include "ui_theme_helpers.h"

#include <esp_log.h>
#include "ui_theme_tokens.h"

static const char *TAG = "ui_theme_helpers";

void sx_ui_theme_apply_to_object(lv_obj_t *obj, bool recursive) {
    if (obj == NULL) {
        return;
    }

    // Get current theme colors
    const sx_theme_colors_t *colors = sx_theme_get_colors();
    if (colors == NULL) {
        ESP_LOGW(TAG, "Theme colors not available, using default tokens");
        // Fallback to default theme tokens
        lv_obj_set_style_bg_color(obj, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
        lv_obj_set_style_text_color(obj, UI_COLOR_TEXT_PRIMARY, LV_PART_MAIN);
        
        if (recursive) {
            uint32_t child_cnt = lv_obj_get_child_cnt(obj);
            for (uint32_t i = 0; i < child_cnt; i++) {
                lv_obj_t *child = lv_obj_get_child(obj, i);
                sx_ui_theme_apply_to_object(child, true);
            }
        }
        return;
    }

    // Apply background color
    lv_obj_set_style_bg_color(obj, lv_color_hex(colors->bg_primary), LV_PART_MAIN);
    
    // Apply text color (if object supports text)
    lv_obj_set_style_text_color(obj, lv_color_hex(colors->text_primary), LV_PART_MAIN);

    // Apply to children if recursive
    if (recursive) {
        uint32_t child_cnt = lv_obj_get_child_cnt(obj);
        for (uint32_t i = 0; i < child_cnt; i++) {
            lv_obj_t *child = lv_obj_get_child(obj, i);
            sx_ui_theme_apply_to_object(child, true);
        }
    }

    ESP_LOGD(TAG, "Applied theme to object (recursive=%d)", recursive);
}






