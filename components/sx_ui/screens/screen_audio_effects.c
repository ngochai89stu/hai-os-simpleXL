#include "screen_audio_effects.h"

#include <esp_log.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"

static const char *TAG = "screen_audio_effects";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_bass_slider = NULL;
static lv_obj_t *s_treble_slider = NULL;
static lv_obj_t *s_reverb_slider = NULL;
static lv_obj_t *s_container = NULL;

static void on_create(void) {
    ESP_LOGI(TAG, "Audio Effects screen onCreate");
    
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
    s_top_bar = screen_common_create_top_bar_with_back(container, "Audio Effects");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 20, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Bass slider
    lv_obj_t *bass_label = lv_label_create(s_content);
    lv_label_set_text(bass_label, "Bass");
    lv_obj_set_style_text_font(bass_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(bass_label, lv_color_hex(0xFFFFFF), 0);
    
    s_bass_slider = lv_slider_create(s_content);
    lv_obj_set_size(s_bass_slider, LV_PCT(100), 20);
    lv_obj_set_style_bg_color(s_bass_slider, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bass_slider, lv_color_hex(0x5b7fff), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_bass_slider, lv_color_hex(0x5b7fff), LV_PART_KNOB);
    lv_slider_set_value(s_bass_slider, 50, LV_ANIM_OFF);
    lv_slider_set_range(s_bass_slider, 0, 100);
    
    // Treble slider
    lv_obj_t *treble_label = lv_label_create(s_content);
    lv_label_set_text(treble_label, "Treble");
    lv_obj_set_style_text_font(treble_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(treble_label, lv_color_hex(0xFFFFFF), 0);
    
    s_treble_slider = lv_slider_create(s_content);
    lv_obj_set_size(s_treble_slider, LV_PCT(100), 20);
    lv_obj_set_style_bg_color(s_treble_slider, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_treble_slider, lv_color_hex(0x5b7fff), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_treble_slider, lv_color_hex(0x5b7fff), LV_PART_KNOB);
    lv_slider_set_value(s_treble_slider, 50, LV_ANIM_OFF);
    lv_slider_set_range(s_treble_slider, 0, 100);
    
    // Reverb slider
    lv_obj_t *reverb_label = lv_label_create(s_content);
    lv_label_set_text(reverb_label, "Reverb");
    lv_obj_set_style_text_font(reverb_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(reverb_label, lv_color_hex(0xFFFFFF), 0);
    
    s_reverb_slider = lv_slider_create(s_content);
    lv_obj_set_size(s_reverb_slider, LV_PCT(100), 20);
    lv_obj_set_style_bg_color(s_reverb_slider, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_reverb_slider, lv_color_hex(0x5b7fff), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_reverb_slider, lv_color_hex(0x5b7fff), LV_PART_KNOB);
    lv_slider_set_value(s_reverb_slider, 0, LV_ANIM_OFF);
    lv_slider_set_range(s_reverb_slider, 0, 100);
    
    lvgl_port_unlock();
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_AUDIO_EFFECTS, "Audio Effects", container, s_content);
    #endif
}

static void on_show(void) {
    ESP_LOGI(TAG, "Audio Effects screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_AUDIO_EFFECTS);
    #endif
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Audio Effects screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_AUDIO_EFFECTS);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Audio Effects screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_AUDIO_EFFECTS);
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
void screen_audio_effects_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
    };
    ui_screen_registry_register(SCREEN_ID_AUDIO_EFFECTS, &callbacks);
}
