#include "ui_router.h"
#include "ui_screen_registry.h"

#include <esp_log.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"

#include "sx_dispatcher.h"
#include "sx_state.h"

static const char *TAG = "ui_router";

// Current active screen (use SCREEN_ID_MAX to indicate no screen active yet)
static ui_screen_id_t s_current_screen = SCREEN_ID_MAX;
static lv_obj_t *s_screen_container = NULL;
static bool s_router_initialized = false;

void ui_router_init(void) {
    if (s_router_initialized) {
        ESP_LOGW(TAG, "Router already initialized");
        return;
    }
    
    // Initialize screen registry first
    ui_screen_registry_init();
    
    // Create base container for screens (must be called from UI task with LVGL lock)
    if (lvgl_port_lock(0)) {
        // Get default screen
        lv_obj_t *scr = lv_scr_act();
        
        // Create a container that will hold all screen content
        s_screen_container = lv_obj_create(scr);
        lv_obj_set_size(s_screen_container, LV_PCT(100), LV_PCT(100));
        lv_obj_align(s_screen_container, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_bg_opa(s_screen_container, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_screen_container, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(s_screen_container, 0, LV_PART_MAIN);
        
        lvgl_port_unlock();
        
        s_router_initialized = true;
        ESP_LOGI(TAG, "UI router initialized");
    } else {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock during router init");
    }
}

void ui_router_navigate_to(ui_screen_id_t screen_id) {
    // TRACE: Log caller of navigate_to
    void *caller_addr = __builtin_return_address(0);
    ESP_LOGE(TAG, "[TRACE] navigate_to() called: screen_id=%d caller_addr=%p", screen_id, caller_addr);
    
    if (!s_router_initialized) {
        ESP_LOGE(TAG, "Router not initialized");
        return;
    }
    
    if (screen_id >= SCREEN_ID_MAX) {
        ESP_LOGE(TAG, "Invalid screen_id: %d", screen_id);
        return;
    }
    
    // Get screen callbacks
    const ui_screen_callbacks_t *callbacks = ui_screen_registry_get(screen_id);
    if (callbacks == NULL) {
        ESP_LOGW(TAG, "Screen %s (id: %d) not registered, skipping navigation",
                 ui_screen_registry_get_name(screen_id), screen_id);
        return;
    }
    
    // Hide current screen if different and valid
    if (s_current_screen != SCREEN_ID_MAX && s_current_screen != screen_id) {
        const ui_screen_callbacks_t *old_callbacks = ui_screen_registry_get(s_current_screen);
        if (old_callbacks && old_callbacks->on_hide) {
            old_callbacks->on_hide();
        }
    }
    
    // Prevent duplicate navigation to same screen
    if (s_current_screen == screen_id) {
        ESP_LOGD(TAG, "Already on screen %s (id: %d), skipping navigation",
                 ui_screen_registry_get_name(screen_id), screen_id);
        return;
    }
    
    // Show new screen (must be called from UI task with LVGL lock)
    if (lvgl_port_lock(0)) {
        // Hide current screen if different and valid
        if (s_current_screen != SCREEN_ID_MAX) {
            const ui_screen_callbacks_t *old_callbacks = ui_screen_registry_get(s_current_screen);
            if (old_callbacks && old_callbacks->on_hide) {
                old_callbacks->on_hide();
            }
        }
        
        // Clear container content BEFORE destroying old screen
        if (s_screen_container != NULL) {
            lv_obj_clean(s_screen_container);
        }
        
        lvgl_port_unlock();
        
        // Destroy old screen if different and valid
        if (s_current_screen != SCREEN_ID_MAX) {
            const ui_screen_callbacks_t *old_callbacks = ui_screen_registry_get(s_current_screen);
            if (old_callbacks && old_callbacks->on_destroy) {
                old_callbacks->on_destroy();
            }
        }
        
        // Create new screen (always call on_create when switching screens)
        if (callbacks->on_create) {
            callbacks->on_create();
        }
        
        // Update current screen BEFORE calling on_show
        s_current_screen = screen_id;
        
        // Show new screen (only once, after create)
        if (callbacks->on_show) {
            // TRACE: Log caller address to identify who calls onShow
            void *caller_addr = __builtin_return_address(0);
            ESP_LOGE(TAG, "[TRACE] onShow() caller: screen=%s (id:%d) caller_addr=%p router_navigate_to=%p",
                     ui_screen_registry_get_name(screen_id), screen_id, caller_addr, (void*)ui_router_navigate_to);
            callbacks->on_show();
        }
        
        ESP_LOGI(TAG, "Navigated to screen: %s (id: %d)", 
                 ui_screen_registry_get_name(screen_id), screen_id);
    } else {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock during navigation");
    }
}

ui_screen_id_t ui_router_get_current_screen(void) {
    return s_current_screen;
}

lv_obj_t* ui_router_get_container(void) {
    return s_screen_container;
}


