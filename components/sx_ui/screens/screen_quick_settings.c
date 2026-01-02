#include "screen_quick_settings.h"

#include <esp_log.h>
#include <string.h>
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "sx_state.h"
#include "ui_theme_tokens.h"
#include "ui_buttons.h"
#include "ui_slider.h"

static const char *TAG = "screen_quick_settings";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_brightness_slider = NULL;
static lv_obj_t *s_volume_slider = NULL;
static lv_obj_t *s_wifi_btn = NULL;
static lv_obj_t *s_bluetooth_btn = NULL;

// Forward declarations
static void brightness_slider_cb(lv_event_t *e);
static void volume_slider_cb(lv_event_t *e);
static void wifi_btn_cb(lv_event_t *e);
static void bluetooth_btn_cb(lv_event_t *e);

static void brightness_slider_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        int32_t value = lv_slider_get_value(s_brightness_slider);
        // Set brightness (if service available)
        // sx_display_set_brightness(value);
        ESP_LOGI(TAG, "Brightness set to %d", value);
    }
}

static void volume_slider_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        int32_t value = lv_slider_get_value(s_volume_slider);
        // Set volume (if service available)
        // sx_audio_set_volume((uint8_t)value);
        ESP_LOGI(TAG, "Volume set to %d", value);
    }
}

static void wifi_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Navigate to WiFi setup
        if (lvgl_port_lock(0)) {
            ui_router_navigate_to(SCREEN_ID_WIFI_SETUP);
            lvgl_port_unlock();
        }
    }
}

static void bluetooth_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Navigate to Bluetooth settings
        if (lvgl_port_lock(0)) {
            ui_router_navigate_to(SCREEN_ID_BLUETOOTH_SETTING);
            lvgl_port_unlock();
        }
    }
}

static void on_create(void) {
    ESP_LOGI(TAG, "Quick Settings screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    // Set background using token
    lv_obj_set_style_bg_color(container, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "Quick Settings");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, UI_SPACE_XL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_content, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Brightness slider
    lv_obj_t *brightness_label = lv_label_create(s_content);
    lv_label_set_text(brightness_label, "Brightness");
    lv_obj_set_style_text_font(brightness_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(brightness_label, UI_COLOR_TEXT_PRIMARY, 0);
    
    s_brightness_slider = ui_gradient_slider_create(s_content, brightness_slider_cb, NULL);
    lv_obj_set_size(s_brightness_slider, LV_PCT(100), UI_SIZE_SLIDER_HEIGHT_THICK);
    lv_slider_set_range(s_brightness_slider, 0, 100);
    lv_slider_set_value(s_brightness_slider, 80, LV_ANIM_OFF);
    
    // Volume slider
    lv_obj_t *volume_label = lv_label_create(s_content);
    lv_label_set_text(volume_label, "Volume");
    lv_obj_set_style_text_font(volume_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(volume_label, UI_COLOR_TEXT_PRIMARY, 0);
    
    s_volume_slider = ui_gradient_slider_create(s_content, volume_slider_cb, NULL);
    lv_obj_set_size(s_volume_slider, LV_PCT(100), UI_SIZE_SLIDER_HEIGHT_THICK);
    lv_slider_set_range(s_volume_slider, 0, 100);
    lv_slider_set_value(s_volume_slider, 50, LV_ANIM_OFF);
    
    // Quick action buttons
    lv_obj_t *actions_label = lv_label_create(s_content);
    lv_label_set_text(actions_label, "Quick Actions");
    lv_obj_set_style_text_font(actions_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(actions_label, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_pad_top(actions_label, UI_SPACE_LG, LV_PART_MAIN);
    
    // WiFi button
    s_wifi_btn = lv_btn_create(s_content);
    lv_obj_set_size(s_wifi_btn, LV_PCT(100), UI_SIZE_BUTTON_HEIGHT);
    lv_obj_set_style_bg_color(s_wifi_btn, UI_COLOR_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_radius(s_wifi_btn, 5, LV_PART_MAIN);
    lv_obj_add_event_cb(s_wifi_btn, wifi_btn_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *wifi_label = lv_label_create(s_wifi_btn);
    lv_label_set_text(wifi_label, "WiFi Settings");
    lv_obj_set_style_text_font(wifi_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(wifi_label, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(wifi_label);
    
    // Bluetooth button
    s_bluetooth_btn = lv_btn_create(s_content);
    lv_obj_set_size(s_bluetooth_btn, LV_PCT(100), UI_SIZE_BUTTON_HEIGHT);
    lv_obj_set_style_bg_color(s_bluetooth_btn, UI_COLOR_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_radius(s_bluetooth_btn, 5, LV_PART_MAIN);
    lv_obj_add_event_cb(s_bluetooth_btn, bluetooth_btn_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *bluetooth_label = lv_label_create(s_bluetooth_btn);
    lv_label_set_text(bluetooth_label, "Bluetooth Settings");
    lv_obj_set_style_text_font(bluetooth_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(bluetooth_label, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(bluetooth_label);
    
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_QUICK_SETTINGS, "Quick Settings", container, s_content);
    #endif
}

static void on_show(void) {
    ESP_LOGI(TAG, "Quick Settings screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_QUICK_SETTINGS);
    #endif
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Quick Settings screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_QUICK_SETTINGS);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Quick Settings screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_QUICK_SETTINGS);
    #endif
    
    if (s_top_bar != NULL) {
        lv_obj_del(s_top_bar);
        s_top_bar = NULL;
    }
    if (s_content != NULL) {
        lv_obj_del(s_content);
        s_content = NULL;
    }
}

static void on_update(const sx_state_t *state) {
    // Update UI from state if needed
    (void)state;
}

// Register this screen
void screen_quick_settings_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
        .on_update = on_update,
    };
    ui_screen_registry_register(SCREEN_ID_QUICK_SETTINGS, &callbacks);
}






