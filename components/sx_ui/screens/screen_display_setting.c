#include "screen_display_setting.h"

#include <esp_log.h>
#include <string.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "sx_state.h"
#include "sx_platform.h"
#include "sx_settings_service.h"
#include "sx_theme_service.h"

static const char *TAG = "screen_display_setting";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_brightness_slider = NULL;
static lv_obj_t *s_theme_selector = NULL;
static lv_obj_t *s_timeout_setting = NULL;
static lv_obj_t *s_container = NULL;

static void brightness_slider_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *slider = lv_event_get_target(e);
        int32_t value = lv_slider_get_value(slider);
        ESP_LOGI(TAG, "Brightness changed to %ld%%", value);
        
        // Set brightness via platform API
        esp_err_t ret = sx_platform_set_brightness((uint8_t)value);
        if (ret == ESP_OK) {
            // Save to settings
            sx_settings_set_int("display_brightness", value);
            sx_settings_commit();
        } else {
            ESP_LOGE(TAG, "Failed to set brightness: %s", esp_err_to_name(ret));
        }
    }
}

// Helper function to apply theme to current screen
static void apply_theme_to_current_screen(void) {
    if (!lvgl_port_lock(0)) {
        return;
    }
    
    lv_obj_t *container = ui_router_get_container();
    if (container != NULL) {
        const sx_theme_colors_t *colors = sx_theme_get_colors();
        
        // Apply theme to container
        sx_theme_apply_to_object(container, true);
        
        // Apply theme to common elements (top bar, content areas)
        // Note: Individual screens should apply theme to their specific elements in on_show
        // This is a basic application for immediate feedback
        
        ESP_LOGI(TAG, "Theme applied to current screen");
    }
    
    lvgl_port_unlock();
}

static void theme_selector_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *dropdown = lv_event_get_target(e);
        uint16_t selected = lv_dropdown_get_selected(dropdown);
        
        sx_theme_type_t theme_type = (sx_theme_type_t)selected;
        esp_err_t ret = sx_theme_set_type(theme_type);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Theme changed to: %d", theme_type);
            
            // Apply theme to current screen immediately
            apply_theme_to_current_screen();
            
            // Note: Other screens will apply theme when they are shown (in on_show)
            // Theme service callback mechanism can be used for more advanced scenarios
        } else {
            ESP_LOGE(TAG, "Failed to set theme: %s", esp_err_to_name(ret));
        }
    }
}

static void on_create(void) {
    ESP_LOGI(TAG, "Display Settings screen onCreate");
    
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
    s_top_bar = screen_common_create_top_bar_with_back(container, "Display Settings");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 20, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Brightness slider (matching web demo)
    lv_obj_t *brightness_label = lv_label_create(s_content);
    lv_label_set_text(brightness_label, "Brightness");
    lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(brightness_label, lv_color_hex(0xFFFFFF), 0);
    
    s_brightness_slider = lv_slider_create(s_content);
    lv_obj_set_size(s_brightness_slider, LV_PCT(100), 20);
    lv_obj_set_style_bg_color(s_brightness_slider, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_brightness_slider, lv_color_hex(0x5b7fff), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_brightness_slider, lv_color_hex(0x5b7fff), LV_PART_KNOB);
    // Load brightness from settings
    int32_t saved_brightness = 80;
    sx_settings_get_int_default("display_brightness", &saved_brightness, 80);
    if (saved_brightness < 0) saved_brightness = 0;
    if (saved_brightness > 100) saved_brightness = 100;
    
    lv_slider_set_value(s_brightness_slider, (int32_t)saved_brightness, LV_ANIM_OFF);
    lv_slider_set_range(s_brightness_slider, 0, 100);
    
    // Add brightness change handler
    // (Handler will be added in on_show)
    
    // Theme selector (matching web demo)
    lv_obj_t *theme_label = lv_label_create(s_content);
    lv_label_set_text(theme_label, "Theme");
    lv_obj_set_style_text_font(theme_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(theme_label, lv_color_hex(0xFFFFFF), 0);
    
    s_theme_selector = lv_dropdown_create(s_content);
    lv_dropdown_set_options(s_theme_selector, "Dark\nLight\nAuto");
    lv_obj_set_size(s_theme_selector, LV_PCT(100), 40);
    lv_obj_set_style_bg_color(s_theme_selector, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_theme_selector, &lv_font_montserrat_14, 0);
    
    // Timeout setting (matching web demo)
    lv_obj_t *timeout_label = lv_label_create(s_content);
    lv_label_set_text(timeout_label, "Screen Timeout");
    lv_obj_set_style_text_font(timeout_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(timeout_label, lv_color_hex(0xFFFFFF), 0);
    
    s_timeout_setting = lv_dropdown_create(s_content);
    lv_dropdown_set_options(s_timeout_setting, "30s\n1min\n5min\n10min\nNever");
    lv_obj_set_size(s_timeout_setting, LV_PCT(100), 40);
    lv_obj_set_style_bg_color(s_timeout_setting, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_timeout_setting, &lv_font_montserrat_14, 0);
    
    lvgl_port_unlock();
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_DISPLAY_SETTING, "Display Setting", container, s_content);
    #endif
}

static void on_show(void) {
    ESP_LOGI(TAG, "Display Settings screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_DISPLAY_SETTING);
    #endif
    
    // Load and update brightness slider from current brightness
    if (s_brightness_slider != NULL && lvgl_port_lock(0)) {
        uint8_t current_brightness = sx_platform_get_brightness();
        lv_slider_set_value(s_brightness_slider, current_brightness, LV_ANIM_OFF);
        
        // Add event handler if not already added
        lv_obj_add_event_cb(s_brightness_slider, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lvgl_port_unlock();
    }
    
    // Load and update theme selector
    if (s_theme_selector != NULL && lvgl_port_lock(0)) {
        sx_theme_type_t current_theme = sx_theme_get_type();
        lv_dropdown_set_selected(s_theme_selector, current_theme);
        
        // Add event handler if not already added
        lv_obj_add_event_cb(s_theme_selector, theme_selector_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lvgl_port_unlock();
    }
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Display Settings screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_DISPLAY_SETTING);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Display Settings screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_DISPLAY_SETTING);
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

static void on_update(const sx_state_t *state) {
    // Update settings from state if needed
    // For now, settings are controlled directly by user interaction
}

// Register this screen
void screen_display_setting_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
        .on_update = on_update,
    };
    ui_screen_registry_register(SCREEN_ID_DISPLAY_SETTING, &callbacks);
}
