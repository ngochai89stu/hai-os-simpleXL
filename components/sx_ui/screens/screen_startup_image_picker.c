#include "screen_startup_image_picker.h"

#include <esp_log.h>
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "ui_theme_tokens.h"
#include "ui_list.h"

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
    
    // Set background using token
    lv_obj_set_style_bg_color(container, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "Startup Image");
    
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
    
    // Preview area
    s_preview = lv_obj_create(s_content);
    lv_obj_set_size(s_preview, LV_PCT(100), 150);
    lv_obj_set_style_bg_color(s_preview, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_preview, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_preview, 10, LV_PART_MAIN);
    
    lv_obj_t *preview_icon = lv_label_create(s_preview);
    lv_label_set_text(preview_icon, "🖼️");
    lv_obj_set_style_text_font(preview_icon, UI_FONT_MEDIUM, 0);
    lv_obj_center(preview_icon);
    
    // Image list (scrollable) - using shared component
    s_image_list = ui_scrollable_list_create(s_content);
    lv_obj_set_size(s_image_list, LV_PCT(100), LV_PCT(100) - 170);
    
    // Sample images (placeholder - will be populated from SD/assets) - using shared component
    const char* sample_images[] = {"Default", "Image 1", "Image 2", "Image 3"};
    for (int i = 0; i < 4; i++) {
        ui_list_item_two_line_create(
            s_image_list,
            NULL,  // No icon
            sample_images[i],
            NULL,  // No subtitle
            NULL,  // No extra text
            NULL,  // No callback for now
            (void*)(intptr_t)i
        );
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
