// Common utilities for all screens
#include "screen_common.h"
#include <esp_log.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "ui_screen_registry.h"
#include "sx_ui_verify.h"
#include "ui_icons.h"

static const char *TAG = "screen_common";

// Back button event handler
static void back_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        if (lvgl_port_lock(0)) {
            ui_router_navigate_to(SCREEN_ID_HOME);
            lvgl_port_unlock();
        }
    }
}

lv_obj_t* screen_common_create_top_bar(lv_obj_t *parent, const char *title) {
    if (!lvgl_port_lock(0)) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock");
        return NULL;
    }
    
    lv_obj_t *top_bar = lv_obj_create(parent);
    lv_obj_set_size(top_bar, LV_PCT(100), 48);  // Slightly taller for better touch targets
    lv_obj_align(top_bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x1a1a1a), LV_PART_MAIN);  // Darker background
    lv_obj_set_style_border_width(top_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(top_bar, 0, LV_PART_MAIN);
    
    lv_obj_t *title_label = lv_label_create(top_bar);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);  // Larger font
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 15, 0);
    
    lvgl_port_unlock();
    return top_bar;
}

lv_obj_t* screen_common_create_top_bar_with_back(lv_obj_t *parent, const char *title) {
    if (!lvgl_port_lock(0)) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock");
        return NULL;
    }
    
    lv_obj_t *top_bar = screen_common_create_top_bar(parent, title);
    if (top_bar == NULL) {
        lvgl_port_unlock();
        return NULL;
    }
    
    // Create back button with icon (web demo style)
    lv_obj_t *back_btn = lv_btn_create(top_bar);
    lv_obj_set_size(back_btn, 40, 40);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x3a3a3a), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(back_btn, 20, LV_PART_MAIN);
    lv_obj_set_style_border_width(back_btn, 0, LV_PART_MAIN);
    
    lv_obj_t *back_icon = ui_icon_create(back_btn, UI_ICON_BACK, 18);
    if (back_icon != NULL) {
        lv_obj_set_style_text_color(back_icon, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(back_icon);
    }
    
    lv_obj_add_event_cb(back_btn, back_btn_event_cb, LV_EVENT_CLICKED, NULL);
    
    // Adjust title position to make room for back button
    lv_obj_t *title_label = lv_obj_get_child(top_bar, 0);
    if (title_label != NULL) {
        lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 55, 0);
    }
    
    lvgl_port_unlock();
    return top_bar;
}

// List item click handler
static void list_item_click_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *item = lv_event_get_target(e);
        ui_screen_id_t screen_id = (ui_screen_id_t)(intptr_t)lv_obj_get_user_data(item);
        if (lvgl_port_lock(0)) {
            ui_router_navigate_to(screen_id);
            lvgl_port_unlock();
        }
    }
}

lv_obj_t* screen_common_create_list_item(lv_obj_t *parent, const char *title, ui_screen_id_t target_screen) {
    if (!lvgl_port_lock(0)) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock");
        return NULL;
    }
    
    lv_obj_t *item = lv_obj_create(parent);
    lv_obj_set_size(item, LV_PCT(100), 56);  // Slightly taller for better touch target
    lv_obj_set_style_bg_color(item, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(item, lv_color_hex(0x3a3a3a), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(item, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(item, 15, LV_PART_MAIN);
    lv_obj_set_style_pad_left(item, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_right(item, 20, LV_PART_MAIN);
    lv_obj_set_style_radius(item, 0, LV_PART_MAIN);  // No radius for cleaner look
    lv_obj_set_user_data(item, (void *)(intptr_t)target_screen);
    lv_obj_add_event_cb(item, list_item_click_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *label = lv_label_create(item);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);  // Slightly larger font
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
    
    // Arrow icon (chevron right)
    lv_obj_t *arrow = ui_icon_create(item, UI_ICON_NEXT, 16);
    if (arrow != NULL) {
        lv_obj_set_style_text_color(arrow, lv_color_hex(0x888888), 0);
        lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -5, 0);
    }
    
    lvgl_port_unlock();
    return item;
}

#if SX_UI_VERIFY_MODE
// Touch probe event handler
static void touch_probe_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *obj = lv_event_get_target(e);
        ui_screen_id_t screen_id = (ui_screen_id_t)(intptr_t)lv_obj_get_user_data(obj);
        void *obj_ptr = (void *)obj;
        
        ESP_LOGI(TAG, "[TOUCH] screen=%d clicked obj=%p", screen_id, obj_ptr);
        sx_ui_verify_mark_touch_ok(screen_id);
    }
}

lv_obj_t* screen_common_add_touch_probe(lv_obj_t *parent, const char *label, ui_screen_id_t screen_id) {
    if (!lvgl_port_lock(0)) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock");
        return NULL;
    }
    
    // Create a touch probe button (full-width bar at bottom)
    lv_obj_t *probe = lv_btn_create(parent);
    lv_obj_set_size(probe, LV_PCT(100), 40);
    lv_obj_align(probe, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(probe, lv_color_hex(0x5b7fff), LV_PART_MAIN);
    lv_obj_set_style_bg_color(probe, lv_color_hex(0x4a6fff), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(probe, 5, LV_PART_MAIN);
    lv_obj_set_user_data(probe, (void *)(intptr_t)screen_id);
    lv_obj_add_event_cb(probe, touch_probe_event_cb, LV_EVENT_CLICKED, NULL);
    
    // Add label
    char probe_label[64];
    snprintf(probe_label, sizeof(probe_label), "[TOUCH TEST] %s", label ? label : "Tap here");
    lv_obj_t *probe_label_obj = lv_label_create(probe);
    lv_label_set_text(probe_label_obj, probe_label);
    lv_obj_set_style_text_font(probe_label_obj, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(probe_label_obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(probe_label_obj);
    
    lvgl_port_unlock();
    return probe;
}
#endif // SX_UI_VERIFY_MODE
