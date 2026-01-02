#include "screen_screensaver.h"

#include <esp_log.h>
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "ui_theme_tokens.h"

static const char *TAG = "screen_screensaver";

static lv_obj_t *s_content = NULL;
static lv_obj_t *s_screensaver_display = NULL;
static lv_obj_t *s_container = NULL;

static void on_create(void) {
    ESP_LOGI(TAG, "Screensaver screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Set background (full screen, no top bar for screensaver) - keep black for screensaver
    lv_obj_set_style_bg_color(container, lv_color_hex(0x000000), LV_PART_MAIN);
    
    // Create content area (full screen)
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100));
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Screensaver display (clock/image/animation)
    s_screensaver_display = lv_obj_create(s_content);
    lv_obj_set_size(s_screensaver_display, 200, 200);
    lv_obj_set_style_bg_color(s_screensaver_display, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_screensaver_display, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_screensaver_display, 10, LV_PART_MAIN);
    
    // Clock display (placeholder)
    lv_obj_t *clock_label = lv_label_create(s_screensaver_display);
    lv_label_set_text(clock_label, "12:00");
    lv_obj_set_style_text_font(clock_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(clock_label, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(clock_label);
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_SCREENSAVER, "Screensaver", container, s_screensaver_display);
    #endif
}

static void on_show(void) {
    ESP_LOGI(TAG, "Screensaver screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_SCREENSAVER);
    #endif
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Screensaver screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_SCREENSAVER);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Screensaver screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_SCREENSAVER);
    #endif
    
    if (s_content != NULL) {
        lv_obj_del(s_content);
        s_content = NULL;
    }
}

// Register this screen
void screen_screensaver_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
    };
    ui_screen_registry_register(SCREEN_ID_SCREENSAVER, &callbacks);
}
