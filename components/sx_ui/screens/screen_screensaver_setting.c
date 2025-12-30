#include "screen_screensaver_setting.h"

#include <esp_log.h>
#include <string.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "sx_settings_service.h"

static const char *TAG = "screen_screensaver_setting";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_timeout_dropdown = NULL;
static lv_obj_t *s_image_selector = NULL;
static lv_obj_t *s_enable_switch = NULL;
static lv_obj_t *s_container = NULL;

// Forward declaration
static void image_selector_cb(lv_event_t *e);

static void on_create(void) {
    ESP_LOGI(TAG, "Screensaver Settings screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Set background
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "Screensaver Settings");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 20, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Enable screensaver switch
    lv_obj_t *enable_label = lv_label_create(s_content);
    lv_label_set_text(enable_label, "Enable Screensaver");
    lv_obj_set_style_text_font(enable_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(enable_label, lv_color_hex(0xFFFFFF), 0);
    
    s_enable_switch = lv_switch_create(s_content);
    lv_obj_set_style_bg_color(s_enable_switch, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_enable_switch, lv_color_hex(0x5b7fff), LV_PART_INDICATOR);
    lv_obj_add_state(s_enable_switch, LV_STATE_CHECKED);
    
    // Timeout setting
    lv_obj_t *timeout_label = lv_label_create(s_content);
    lv_label_set_text(timeout_label, "Timeout");
    lv_obj_set_style_text_font(timeout_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(timeout_label, lv_color_hex(0xFFFFFF), 0);
    
    s_timeout_dropdown = lv_dropdown_create(s_content);
    lv_dropdown_set_options(s_timeout_dropdown, "30s\n1min\n5min\n10min\n30min\nNever");
    lv_obj_set_size(s_timeout_dropdown, LV_PCT(100), 40);
    lv_obj_set_style_bg_color(s_timeout_dropdown, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_timeout_dropdown, &lv_font_montserrat_14, 0);
    
    // Image selector
    lv_obj_t *image_label = lv_label_create(s_content);
    lv_label_set_text(image_label, "Screensaver Image");
    lv_obj_set_style_text_font(image_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(image_label, lv_color_hex(0xFFFFFF), 0);
    
    s_image_selector = lv_dropdown_create(s_content);
    lv_dropdown_set_options(s_image_selector, "Default\nGradient\nImage 1\nImage 2\nCustom");
    lv_obj_set_size(s_image_selector, LV_PCT(100), 40);
    lv_obj_set_style_bg_color(s_image_selector, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_image_selector, &lv_font_montserrat_14, 0);
    
    // Load saved background image setting
    char saved_bg[64] = {0};
    if (sx_settings_get_string_default("screensaver_bg_image", saved_bg, sizeof(saved_bg), "Default") == ESP_OK) {
        // Find index in dropdown
        const char *options[] = {"Default", "Gradient", "Image 1", "Image 2", "Custom"};
        for (int i = 0; i < 5; i++) {
            if (strcmp(saved_bg, options[i]) == 0) {
                lv_dropdown_set_selected(s_image_selector, i);
                break;
            }
        }
    }
    
    // Add event handler for image selector
    lv_obj_add_event_cb(s_image_selector, image_selector_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_SCREENSAVER_SETTING, "Screensaver Setting", container, s_content);
    #endif
}

static void image_selector_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *dropdown = lv_event_get_target(e);
        uint16_t selected = lv_dropdown_get_selected(dropdown);
        
        const char *options[] = {"Default", "Gradient", "Image 1", "Image 2", "Custom"};
        if (selected < 5) {
            const char *bg_name = options[selected];
            ESP_LOGI(TAG, "Background image changed to: %s", bg_name);
            
            // Save to settings
            sx_settings_set_string("screensaver_bg_image", bg_name);
            sx_settings_commit();

            // Trigger flash screen (screensaver) to reload background:
            // - Flash screen always reads "screensaver_bg_image" from settings in load_background_image()
            // - When user updates this setting, we want the next time flash shows to use the new image.
            //   This is already handled by screen_flash on_show(), so no additional navigation is needed here.
        }
    }
}

static void on_show(void) {
    ESP_LOGI(TAG, "Screensaver Settings screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_SCREENSAVER_SETTING);
    #endif
    
    // Reload settings
    if (s_image_selector != NULL) {
        char saved_bg[64] = {0};
        if (sx_settings_get_string_default("screensaver_bg_image", saved_bg, sizeof(saved_bg), "Default") == ESP_OK) {
            const char *options[] = {"Default", "Gradient", "Image 1", "Image 2", "Custom"};
            for (int i = 0; i < 5; i++) {
                if (strcmp(saved_bg, options[i]) == 0) {
                    lv_dropdown_set_selected(s_image_selector, i);
                    break;
                }
            }
        }
    }
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Screensaver Settings screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_SCREENSAVER_SETTING);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Screensaver Settings screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_SCREENSAVER_SETTING);
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

// Register this screen
void screen_screensaver_setting_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
    };
    ui_screen_registry_register(SCREEN_ID_SCREENSAVER_SETTING, &callbacks);
}
