#include "screen_settings.h"

#include <esp_log.h>
#include <string.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "sx_ui_verify.h"
#include "screen_common.h"
#include "sx_state.h"

static const char *TAG = "screen_settings";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_container = NULL;


static void on_create(void) {
    ESP_LOGI(TAG, "Settings screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Set background
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "Settings");
    
    // Create content area - Phone-style settings list
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(s_content, 2, LV_PART_MAIN);  // Small gap between items
    
    // Phone-style settings list (clean list, no section headers)
    // Display Settings
    screen_common_create_list_item(s_content, "Display Settings", SCREEN_ID_DISPLAY_SETTING);
    
    // Bluetooth Settings
    screen_common_create_list_item(s_content, "Bluetooth Settings", SCREEN_ID_BLUETOOTH_SETTING);
    
    // Screensaver Settings
    screen_common_create_list_item(s_content, "Screensaver Settings", SCREEN_ID_SCREENSAVER_SETTING);
    
    // Equalizer (includes Audio Effects: Bass, Treble, Reverb)
    screen_common_create_list_item(s_content, "Equalizer", SCREEN_ID_EQUALIZER);
    
    // Wi-Fi Setup
    screen_common_create_list_item(s_content, "Wi-Fi Setup", SCREEN_ID_WIFI_SETUP);
    
    // Voice Settings
    screen_common_create_list_item(s_content, "Voice Settings", SCREEN_ID_VOICE_SETTINGS);
    
    // About
    screen_common_create_list_item(s_content, "About", SCREEN_ID_ABOUT);
    
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_SETTINGS, "Settings", container, s_content);
    #endif
}

static void on_show(void) {
    ESP_LOGI(TAG, "Settings screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_SETTINGS);
    #endif
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Settings screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_SETTINGS);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Settings screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_SETTINGS);
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
    // Settings list doesn't need state updates
    (void)state;
}

// Register this screen
void screen_settings_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
        .on_update = on_update,
    };
    ui_screen_registry_register(SCREEN_ID_SETTINGS, &callbacks);
}
