#include "screen_about.h"

#include <esp_log.h>
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "ui_theme_tokens.h"
#include "ui_list.h"

static const char *TAG = "screen_about";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_info_list = NULL;
static lv_obj_t *s_container = NULL;

static void on_create(void) {
    ESP_LOGI(TAG, "About screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Set background using token
    lv_obj_set_style_bg_color(container, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "About");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, UI_SPACE_XL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_content, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // Device/System info list (scrollable) - using shared component
    s_info_list = ui_scrollable_list_create(s_content);
    lv_obj_set_size(s_info_list, LV_PCT(100), LV_PCT(100));
    
    // Device info items using shared component
    const char* info_items[] = {
        "Device: H.A.I OS Device",
        "Firmware: v1.0.0",
        "ESP-IDF: v5.5.1",
        "Free Memory: 512 KB",
        "Uptime: 0:00:00"
    };
    for (int i = 0; i < 5; i++) {
        ui_list_item_two_line_create(
            s_info_list,
            NULL,  // No icon
            info_items[i],
            NULL,  // No subtitle
            NULL,  // No extra text
            NULL,  // No callback (read-only info)
            NULL
        );
    }
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_ABOUT, "About", container, s_info_list);
    #endif
}

static void on_show(void) {
    ESP_LOGI(TAG, "About screen onShow");
}

static void on_hide(void) {
    ESP_LOGI(TAG, "About screen onHide");
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "About screen onDestroy");
    
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
void screen_about_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
    };
    ui_screen_registry_register(SCREEN_ID_ABOUT, &callbacks);
}
