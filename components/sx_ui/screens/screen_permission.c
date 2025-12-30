#include "screen_permission.h"

#include <esp_log.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"

static const char *TAG = "screen_permission";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_permission_list = NULL;
static lv_obj_t *s_container = NULL;

static void on_create(void) {
    ESP_LOGI(TAG, "Permissions screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Set background
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "Permissions");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Permission list (scrollable)
    s_permission_list = lv_obj_create(s_content);
    lv_obj_set_size(s_permission_list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_permission_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_permission_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_permission_list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_permission_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_permission_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Permission items with toggle switches
    const char* permissions[] = {"Microphone", "Storage", "Location", "Bluetooth", "WiFi"};
    for (int i = 0; i < 5; i++) {
        lv_obj_t *perm_item = lv_obj_create(s_permission_list);
        lv_obj_set_size(perm_item, LV_PCT(100), 50);
        lv_obj_set_style_bg_color(perm_item, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        lv_obj_set_style_border_width(perm_item, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(perm_item, 10, LV_PART_MAIN);
        lv_obj_set_style_radius(perm_item, 5, LV_PART_MAIN);
        
        lv_obj_t *label = lv_label_create(perm_item);
        lv_label_set_text(label, permissions[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
        
        // Toggle switch
        lv_obj_t *sw = lv_switch_create(perm_item);
        lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -10, 0);
        lv_obj_set_style_bg_color(sw, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
        lv_obj_set_style_bg_color(sw, lv_color_hex(0x5b7fff), LV_PART_INDICATOR);
        if (i < 2) { // First 2 enabled by default
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        }
    }
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_PERMISSION, "Permission", container, s_permission_list);
    #endif
}

static void on_show(void) {
    ESP_LOGI(TAG, "Permissions screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_PERMISSION);
    #endif
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Permissions screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_PERMISSION);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Permissions screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_PERMISSION);
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
void screen_permission_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
    };
    ui_screen_registry_register(SCREEN_ID_PERMISSION, &callbacks);
}
