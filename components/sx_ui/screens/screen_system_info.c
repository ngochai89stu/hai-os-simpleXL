#include "screen_system_info.h"

#include <esp_log.h>
#include <string.h>
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "sx_state.h"
#include "ui_theme_tokens.h"
#include "ui_list.h"
#include "esp_system.h"
#include "esp_chip_info.h"

static const char *TAG = "screen_system_info";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_info_list = NULL;

static void on_create(void) {
    ESP_LOGI(TAG, "System Info screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    // Set background using token
    lv_obj_set_style_bg_color(container, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "System Info");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, UI_SPACE_XL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_content, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // System info list using shared component
    s_info_list = ui_scrollable_list_create(s_content);
    lv_obj_set_size(s_info_list, LV_PCT(100), LV_PCT(100));
    
    // Get system information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t min_free_heap = esp_get_minimum_free_heap_size();
    
    char chip_model[32];
    snprintf(chip_model, sizeof(chip_model), "ESP32-S3 (%d cores)", chip_info.cores);
    
    char free_heap_str[32];
    snprintf(free_heap_str, sizeof(free_heap_str), "%lu KB", free_heap / 1024);
    
    char min_heap_str[32];
    snprintf(min_heap_str, sizeof(min_heap_str), "%lu KB", min_free_heap / 1024);
    
    // System info items
    struct {
        const char *title;
        const char *subtitle;
    } items[] = {
        {"Chip Model", chip_model},
        {"Free Heap", free_heap_str},
        {"Min Free Heap", min_heap_str},
        {"CPU Frequency", "240 MHz"},
        {"Flash Size", "16 MB"},
    };
    
    for (size_t i = 0; i < sizeof(items)/sizeof(items[0]); i++) {
        ui_list_item_two_line_create(
            s_info_list,
            NULL,  // No icon
            items[i].title,
            items[i].subtitle,
            NULL,  // No extra text
            NULL,  // No callback (read-only info)
            NULL
        );
    }
    
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_SYSTEM_INFO, "System Info", container, s_content);
    #endif
}

static void on_show(void) {
    ESP_LOGI(TAG, "System Info screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_SYSTEM_INFO);
    #endif
}

static void on_hide(void) {
    ESP_LOGI(TAG, "System Info screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_SYSTEM_INFO);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "System Info screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_SYSTEM_INFO);
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

static void on_update(const sx_state_t *state) {
    // Update system info if needed
    (void)state;
}

// Register this screen
void screen_system_info_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
        .on_update = on_update,
    };
    ui_screen_registry_register(SCREEN_ID_SYSTEM_INFO, &callbacks);
}






