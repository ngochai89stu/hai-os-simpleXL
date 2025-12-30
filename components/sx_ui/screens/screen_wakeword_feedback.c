#include "screen_wakeword_feedback.h"

#include <esp_log.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "sx_wake_word_service.h"

static const char *TAG = "screen_wakeword_feedback";

static lv_obj_t *s_content = NULL;
static lv_obj_t *s_animation_obj = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_anim_t s_pulse_anim;
static lv_obj_t *s_container = NULL;
static bool s_wake_word_detected = false;

// Forward declaration
static void wake_word_detected_cb(void *user_data);

static void pulse_anim_cb(void *var, int32_t value) {
    lv_obj_t *obj = (lv_obj_t *)var;
    if (obj != NULL) {
        lv_obj_set_style_transform_scale(obj, value, 0);
    }
}

static void on_create(void) {
    ESP_LOGI(TAG, "Wakeword Feedback screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Set background (full screen, no top bar for wakeword feedback)
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    
    // Create content area (full screen)
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100));
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Visual feedback animation (matching web demo)
    s_animation_obj = lv_obj_create(s_content);
    lv_obj_set_size(s_animation_obj, 150, 150);
    lv_obj_set_style_bg_color(s_animation_obj, lv_color_hex(0x5b7fff), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_animation_obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_animation_obj, 75, LV_PART_MAIN);
    
    // Pulse animation (matching web demo)
    lv_anim_init(&s_pulse_anim);
    lv_anim_set_var(&s_pulse_anim, s_animation_obj);
    lv_anim_set_values(&s_pulse_anim, 100, 150);
    lv_anim_set_time(&s_pulse_anim, 1000);
    lv_anim_set_repeat_count(&s_pulse_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_playback_time(&s_pulse_anim, 1000);
    lv_anim_set_exec_cb(&s_pulse_anim, pulse_anim_cb);
    lv_anim_start(&s_pulse_anim);
    
    // Status text (matching web demo)
    s_status_label = lv_label_create(s_content);
    lv_label_set_text(s_status_label, "Listening...");
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xFFFFFF), 0);
    
    // Register wake word callback
    // Note: This screen should be shown when wake word detection is active
    // The callback will be called when wake word is detected
    if (sx_wake_word_is_active()) {
        // Wake word is already active, register callback
        sx_wake_word_start(wake_word_detected_cb, NULL);
    }
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_WAKEWORD_FEEDBACK, "Wakeword Feedback", container, s_content);
    #endif
}

static void wake_word_detected_cb(void *user_data) {
    (void)user_data;
    ESP_LOGI(TAG, "Wake word detected! Triggering feedback animation");
    
    s_wake_word_detected = true;
    
    // Update UI in LVGL task context
    if (lvgl_port_lock(0)) {
        if (s_status_label != NULL) {
            lv_label_set_text(s_status_label, "Wake word detected!");
        }
        
        // Enhance animation when detected
        if (s_animation_obj != NULL) {
            // Change color to indicate detection
            lv_obj_set_style_bg_color(s_animation_obj, lv_color_hex(0x00ff00), LV_PART_MAIN);
            
            // Restart animation with faster pulse
            lv_anim_set_time(&s_pulse_anim, 500);
            lv_anim_set_playback_time(&s_pulse_anim, 500);
            lv_anim_start(&s_pulse_anim);
        }
        
        lvgl_port_unlock();
    }
}

static void on_show(void) {
    ESP_LOGI(TAG, "Wakeword Feedback screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_WAKEWORD_FEEDBACK);
    #endif
    
    // Reset detection state
    s_wake_word_detected = false;
    
    // Start wake word detection if not already active
    if (!sx_wake_word_is_active()) {
        esp_err_t ret = sx_wake_word_start(wake_word_detected_cb, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start wake word detection: %s", esp_err_to_name(ret));
            if (lvgl_port_lock(0)) {
                if (s_status_label != NULL) {
                    lv_label_set_text(s_status_label, "Wake word unavailable");
                }
                lvgl_port_unlock();
            }
        } else {
            ESP_LOGI(TAG, "Wake word detection started");
        }
    } else {
        // Already active, just register callback
        sx_wake_word_start(wake_word_detected_cb, NULL);
    }
    
    // Restart pulse animation
    if (lvgl_port_lock(0)) {
        // Reset animation to normal speed
        lv_anim_set_time(&s_pulse_anim, 1000);
        lv_anim_set_playback_time(&s_pulse_anim, 1000);
        
        // Reset color to blue
        if (s_animation_obj != NULL) {
            lv_obj_set_style_bg_color(s_animation_obj, lv_color_hex(0x5b7fff), LV_PART_MAIN);
        }
        
        if (s_status_label != NULL) {
            lv_label_set_text(s_status_label, "Listening...");
        }
        
        lv_anim_start(&s_pulse_anim);
        lvgl_port_unlock();
    }
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Wakeword Feedback screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_WAKEWORD_FEEDBACK);
    #endif
    
    // Stop wake word detection when hiding screen
    // Note: This might stop wake word for other screens too
    // In production, you might want to keep it running
    // sx_wake_word_stop();
    
    // Stop pulse animation
    if (lvgl_port_lock(0)) {
        lv_anim_del(s_animation_obj, pulse_anim_cb);
        lvgl_port_unlock();
    }
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Wakeword Feedback screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_WAKEWORD_FEEDBACK);
    #endif
    
    if (s_content != NULL) {
        lv_obj_del(s_content);
        s_content = NULL;
    }
}

// Register this screen
void screen_wakeword_feedback_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
    };
    ui_screen_registry_register(SCREEN_ID_WAKEWORD_FEEDBACK, &callbacks);
}
