#include "screen_dev_console.h"

#include <esp_log.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"

static const char *TAG = "screen_dev_console";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_output_area = NULL;
static lv_obj_t *s_input_textarea = NULL;
static lv_obj_t *s_execute_btn = NULL;
static lv_obj_t *s_container = NULL;

static void on_create(void) {
    ESP_LOGI(TAG, "Dev Console screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Set background
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "Dev Console");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Output area (scrollable text area)
    s_output_area = lv_textarea_create(s_content);
    lv_obj_set_size(s_output_area, LV_PCT(100), LV_PCT(100) - 80);
    lv_textarea_set_text(s_output_area, "> Dev Console Ready\n> Type commands and press Execute\n");
    lv_textarea_set_placeholder_text(s_output_area, "Console output will appear here...");
    lv_obj_set_style_bg_color(s_output_area, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_output_area, lv_color_hex(0x00ff00), 0);
    lv_obj_set_style_text_font(s_output_area, &lv_font_montserrat_14, 0);
    lv_textarea_set_text_selection(s_output_area, false);
    
    // Input area
    lv_obj_t *input_container = lv_obj_create(s_content);
    lv_obj_set_size(input_container, LV_PCT(100), 60);
    lv_obj_set_style_bg_opa(input_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(input_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(input_container, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(input_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(input_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(input_container, 10, LV_PART_MAIN);
    
    s_input_textarea = lv_textarea_create(input_container);
    lv_obj_set_flex_grow(s_input_textarea, 1);
    lv_obj_set_size(s_input_textarea, LV_PCT(75), 50);
    lv_textarea_set_placeholder_text(s_input_textarea, "Enter command...");
    lv_obj_set_style_bg_color(s_input_textarea, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_input_textarea, &lv_font_montserrat_14, 0);
    
    s_execute_btn = lv_btn_create(input_container);
    lv_obj_set_size(s_execute_btn, LV_PCT(20), 50);
    lv_obj_set_style_bg_color(s_execute_btn, lv_color_hex(0x5b7fff), LV_PART_MAIN);
    lv_obj_set_style_radius(s_execute_btn, 5, LV_PART_MAIN);
    lv_obj_t *execute_label = lv_label_create(s_execute_btn);
    lv_label_set_text(execute_label, "Exec");
    lv_obj_set_style_text_font(execute_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(execute_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(execute_label);
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_DEV_CONSOLE, "Dev Console", container, s_output_area);
    #endif
}

static void on_show(void) {
    ESP_LOGI(TAG, "Dev Console screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_DEV_CONSOLE);
    #endif
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Dev Console screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_DEV_CONSOLE);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Dev Console screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_DEV_CONSOLE);
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
void screen_dev_console_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
    };
    ui_screen_registry_register(SCREEN_ID_DEV_CONSOLE, &callbacks);
}
