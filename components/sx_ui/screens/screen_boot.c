#include "screen_boot.h"

#include <esp_log.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "sx_ui_verify.h"
#include "sx_assets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "screen_boot";

static lv_obj_t *s_bootscreen_img = NULL;
static lv_obj_t *s_container = NULL;
static lv_timer_t *s_boot_timer = NULL;

#define BOOTSCREEN_DISPLAY_TIME_MS 3000  // 3 seconds

static void on_create(void) {
    ESP_LOGI(TAG, "Boot screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Get bootscreen image from assets
    const lv_img_dsc_t *bootscreen_img = sx_assets_get_bootscreen_img();
    if (bootscreen_img == NULL) {
        ESP_LOGE(TAG, "Bootscreen image not available");
        // Fallback: black background
        lv_obj_set_style_bg_color(container, lv_color_hex(0x000000), LV_PART_MAIN);
        return;
    }
    
    ESP_LOGI(TAG, "[BOOT] bootscreen image found: w=%d h=%d data_size=%d", 
             bootscreen_img->header.w, bootscreen_img->header.h, bootscreen_img->data_size);
    
    // Set container to fill screen
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(container, 0, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Set container background to black (in case image doesn't cover fully)
    lv_obj_set_style_bg_color(container, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(container, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    
    // Create image object to display bootscreen
    s_bootscreen_img = lv_img_create(container);
    if (s_bootscreen_img == NULL) {
        ESP_LOGE(TAG, "Failed to create image object");
        return;
    }
    
    // Remove padding and border from image object
    lv_obj_set_style_pad_all(s_bootscreen_img, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_bootscreen_img, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_bootscreen_img, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(s_bootscreen_img, LV_OBJ_FLAG_SCROLLABLE);
    
    // Set image source FIRST
    lv_img_set_src(s_bootscreen_img, bootscreen_img);
    
    // Set size to match image dimensions (320x480) - must be exact
    lv_obj_set_size(s_bootscreen_img, 320, 480);
    
    // Position at top-left (0, 0)
    lv_obj_set_pos(s_bootscreen_img, 0, 0);
    
    // Ensure no scaling - image should be exactly 320x480
    lv_img_set_zoom(s_bootscreen_img, 256); // 256 = 100% zoom (no scaling)
    lv_img_set_angle(s_bootscreen_img, 0); // No rotation
    
    // Force refresh
    lv_obj_invalidate(s_bootscreen_img);
    lv_obj_invalidate(container);
    
    ESP_LOGI(TAG, "[BOOT] bootscreen image applied: 320x480 BGR565, img_obj=%p, container=%p", 
             s_bootscreen_img, container);
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_BOOT, "Boot", container, s_bootscreen_img);
    #endif
}

static void boot_timer_cb(lv_timer_t *timer) {
    (void)timer;
    ESP_LOGE(TAG, "[TRACE] boot_timer_cb() FIRED - timer=%p navigating to FLASH", (void*)timer);
    // Navigate to flash screen after bootscreen display time
    ui_router_navigate_to(SCREEN_ID_FLASH);
    // Timer will be deleted in on_hide
}

static void on_show(void) {
    ESP_LOGI(TAG, "Boot screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_BOOT);
    #endif
    
    // Create timer to auto-navigate to flash screen after 3 seconds
    if (s_boot_timer == NULL) {
        s_boot_timer = lv_timer_create(boot_timer_cb, BOOTSCREEN_DISPLAY_TIME_MS, NULL);
        if (s_boot_timer != NULL) {
            lv_timer_set_repeat_count(s_boot_timer, 1); // Run once
            ESP_LOGI(TAG, "Boot screen timer created: %d ms", BOOTSCREEN_DISPLAY_TIME_MS);
        } else {
            ESP_LOGE(TAG, "Failed to create boot screen timer");
        }
    }
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Boot screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_BOOT);
    #endif
    
    // Delete timer if it exists
    if (s_boot_timer != NULL) {
        lv_timer_del(s_boot_timer);
        s_boot_timer = NULL;
        ESP_LOGI(TAG, "Boot screen timer deleted");
    }
}

static void on_destroy(void) {
    ESP_LOGE(TAG, "[TRACE] Boot screen onDestroy - timer=%p", (void*)s_boot_timer);
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_BOOT);
    #endif
    
    // Ensure timer is deleted
    if (s_boot_timer != NULL) {
        ESP_LOGE(TAG, "[TRACE] Deleting boot timer in onDestroy: %p", (void*)s_boot_timer);
        lv_timer_del(s_boot_timer);
        s_boot_timer = NULL;
    }
    
    if (s_bootscreen_img != NULL) {
        lv_obj_del(s_bootscreen_img);
        s_bootscreen_img = NULL;
    }
}

// Register this screen
void screen_boot_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
    };
    ui_screen_registry_register(SCREEN_ID_BOOT, &callbacks);
}


