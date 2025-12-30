#include "screen_about.h"

#include <esp_log.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"

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
    
    // Set background
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "About");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 20, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Device/System info list (scrollable) - matching web demo
    s_info_list = lv_obj_create(s_content);
    lv_obj_set_size(s_info_list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_info_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_info_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_info_list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_info_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_info_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Device info items (matching web demo)
    const char* info_items[] = {
        "Device: H.A.I OS Device",
        "Firmware: v1.0.0",
        "ESP-IDF: v5.5.1",
        "Free Memory: 512 KB",
        "Uptime: 0:00:00"
    };
    for (int i = 0; i < 5; i++) {
        lv_obj_t *info_item = lv_obj_create(s_info_list);
        lv_obj_set_size(info_item, LV_PCT(100), 40);
        lv_obj_set_style_bg_color(info_item, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        lv_obj_set_style_border_width(info_item, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(info_item, 10, LV_PART_MAIN);
        lv_obj_set_style_radius(info_item, 5, LV_PART_MAIN);
        
        lv_obj_t *label = lv_label_create(info_item);
        lv_label_set_text(label, info_items[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
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
