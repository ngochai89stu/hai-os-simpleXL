#include "screen_touch_debug.h"

#include <esp_log.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"

static const char *TAG = "screen_touch_debug";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_touch_area = NULL;
static lv_obj_t *s_coord_label = NULL;
static lv_obj_t *s_event_label = NULL;
static lv_obj_t *s_container = NULL;

// Touch event handler
static void touch_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();
    
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_RELEASED || code == LV_EVENT_PRESSING) {
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        
        if (s_coord_label != NULL) {
            char coord_text[64];
            snprintf(coord_text, sizeof(coord_text), "Coordinates: (%ld, %ld)", (long)point.x, (long)point.y);
            lv_label_set_text(s_coord_label, coord_text);
        }
        
        if (s_event_label != NULL) {
            const char *event_name = code == LV_EVENT_PRESSED ? "PRESSED" : 
                                     code == LV_EVENT_RELEASED ? "RELEASED" : "PRESSING";
            char event_text[64];
            snprintf(event_text, sizeof(event_text), "Event: %s", event_name);
            lv_label_set_text(s_event_label, event_text);
        }
    }
}

static void on_create(void) {
    ESP_LOGI(TAG, "Touch Debug screen onCreate");
    
    if (!lvgl_port_lock(0)) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock");
        return;
    }
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        lvgl_port_unlock();
        return;
    }
    
    s_container = container;
    
    // Set background
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "Touch Debug");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Touch area (large interactive area)
    s_touch_area = lv_obj_create(s_content);
    lv_obj_set_size(s_touch_area, LV_PCT(100), LV_PCT(100) - 100);
    lv_obj_set_style_bg_color(s_touch_area, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_touch_area, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_touch_area, lv_color_hex(0x5b7fff), LV_PART_MAIN);
    lv_obj_set_style_radius(s_touch_area, 10, LV_PART_MAIN);
    
    lv_obj_t *touch_hint = lv_label_create(s_touch_area);
    lv_label_set_text(touch_hint, "Touch here to test");
    lv_obj_set_style_text_font(touch_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(touch_hint, lv_color_hex(0x888888), 0);
    lv_obj_center(touch_hint);
    
    // Coordinate display
    s_coord_label = lv_label_create(s_content);
    lv_label_set_text(s_coord_label, "Coordinates: (0, 0)");
    lv_obj_set_style_text_font(s_coord_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_coord_label, lv_color_hex(0xFFFFFF), 0);
    
    // Event display
    s_event_label = lv_label_create(s_content);
    lv_label_set_text(s_event_label, "Event: None");
    lv_obj_set_style_text_font(s_event_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_event_label, lv_color_hex(0x888888), 0);
    
    // Attach touch event handler
    lv_obj_add_event_cb(s_touch_area, touch_event_cb, LV_EVENT_ALL, NULL);
    
    lvgl_port_unlock();
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_TOUCH_DEBUG, "Touch Debug", container, s_touch_area);
    #endif
}

static void on_show(void) {
    ESP_LOGI(TAG, "Touch Debug screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_TOUCH_DEBUG);
    #endif
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Touch Debug screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_TOUCH_DEBUG);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Touch Debug screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_TOUCH_DEBUG);
    #endif
    
    if (lvgl_port_lock(0)) {
        if (s_top_bar != NULL) {
            lv_obj_del(s_top_bar);
            s_top_bar = NULL;
        }
        if (s_content != NULL) {
            lv_obj_del(s_content);
            s_content = NULL;
        }
        lvgl_port_unlock();
    }
}

// Register this screen
void screen_touch_debug_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
    };
    ui_screen_registry_register(SCREEN_ID_TOUCH_DEBUG, &callbacks);
}
