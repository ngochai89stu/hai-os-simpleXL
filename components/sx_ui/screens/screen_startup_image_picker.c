#include "screen_startup_image_picker.h"

#include <esp_log.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"

static const char *TAG = "screen_startup_image_picker";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_image_list = NULL;
static lv_obj_t *s_preview = NULL;
static lv_obj_t *s_container = NULL;

static void on_create(void) {
    ESP_LOGI(TAG, "Startup Image screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Set background
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "Startup Image");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Preview area
    s_preview = lv_obj_create(s_content);
    lv_obj_set_size(s_preview, LV_PCT(100), 150);
    lv_obj_set_style_bg_color(s_preview, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_preview, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_preview, 10, LV_PART_MAIN);
    
    lv_obj_t *preview_icon = lv_label_create(s_preview);
    lv_label_set_text(preview_icon, "🖼️");
    lv_obj_set_style_text_font(preview_icon, &lv_font_montserrat_14, 0);
    lv_obj_center(preview_icon);
    
    // Image list (scrollable)
    s_image_list = lv_obj_create(s_content);
    lv_obj_set_size(s_image_list, LV_PCT(100), LV_PCT(100) - 170);
    lv_obj_set_style_bg_opa(s_image_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_image_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_image_list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_image_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_image_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Sample images (placeholder - will be populated from SD/assets)
    const char* sample_images[] = {"Default", "Image 1", "Image 2", "Image 3"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *image_item = lv_obj_create(s_image_list);
        lv_obj_set_size(image_item, LV_PCT(100), 50);
        lv_obj_set_style_bg_color(image_item, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        lv_obj_set_style_bg_color(image_item, lv_color_hex(0x3a3a3a), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_border_width(image_item, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(image_item, 10, LV_PART_MAIN);
        lv_obj_set_style_radius(image_item, 5, LV_PART_MAIN);
        
        lv_obj_t *label = lv_label_create(image_item);
        lv_label_set_text(label, sample_images[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
    }
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_STARTUP_IMAGE_PICKER, "Startup Image Picker", container, s_image_list);
    #endif
}

static void on_show(void) {
    ESP_LOGI(TAG, "Startup Image screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_STARTUP_IMAGE_PICKER);
    #endif
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Startup Image screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_STARTUP_IMAGE_PICKER);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Startup Image screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_STARTUP_IMAGE_PICKER);
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
void screen_startup_image_picker_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
    };
    ui_screen_registry_register(SCREEN_ID_STARTUP_IMAGE_PICKER, &callbacks);
}
