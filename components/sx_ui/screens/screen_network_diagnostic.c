#include "screen_network_diagnostic.h"

#include <esp_log.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"

static const char *TAG = "screen_network_diagnostic";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_status_list = NULL;
static lv_obj_t *s_test_btn = NULL;
static lv_obj_t *s_container = NULL;

static void on_create(void) {
    ESP_LOGI(TAG, "Network Diagnostic screen onCreate");
    
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
    s_top_bar = screen_common_create_top_bar_with_back(container, "Network Diagnostic");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Test button
    s_test_btn = lv_btn_create(s_content);
    lv_obj_set_size(s_test_btn, LV_PCT(100), 50);
    lv_obj_set_style_bg_color(s_test_btn, lv_color_hex(0x5b7fff), LV_PART_MAIN);
    lv_obj_set_style_radius(s_test_btn, 5, LV_PART_MAIN);
    lv_obj_t *test_label = lv_label_create(s_test_btn);
    lv_label_set_text(test_label, "Run Network Test");
    lv_obj_set_style_text_font(test_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(test_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(test_label);
    
    // Network status list (scrollable)
    s_status_list = lv_obj_create(s_content);
    lv_obj_set_size(s_status_list, LV_PCT(100), LV_PCT(100) - 70);
    lv_obj_set_style_bg_opa(s_status_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_status_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_status_list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_status_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_status_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Network status items
    const char* status_items[] = {
        "WiFi: Connected",
        "IP: 192.168.1.100",
        "Gateway: 192.168.1.1",
        "DNS: 8.8.8.8",
        "Signal: -45 dBm",
        "Latency: 15 ms"
    };
    for (int i = 0; i < 6; i++) {
        lv_obj_t *status_item = lv_obj_create(s_status_list);
        lv_obj_set_size(status_item, LV_PCT(100), 40);
        lv_obj_set_style_bg_color(status_item, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        lv_obj_set_style_border_width(status_item, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(status_item, 10, LV_PART_MAIN);
        lv_obj_set_style_radius(status_item, 5, LV_PART_MAIN);
        
        lv_obj_t *label = lv_label_create(status_item);
        lv_label_set_text(label, status_items[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
    }
    
    lvgl_port_unlock();
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_NETWORK_DIAGNOSTIC, "Network Diagnostic", container, s_status_list);
    #endif
}

static void on_show(void) {
    ESP_LOGI(TAG, "Network Diagnostic screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_NETWORK_DIAGNOSTIC);
    #endif
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Network Diagnostic screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_NETWORK_DIAGNOSTIC);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Network Diagnostic screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_NETWORK_DIAGNOSTIC);
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
void screen_network_diagnostic_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
    };
    ui_screen_registry_register(SCREEN_ID_NETWORK_DIAGNOSTIC, &callbacks);
}
