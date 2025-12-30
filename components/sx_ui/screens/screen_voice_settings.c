#include "screen_voice_settings.h"

#include <esp_log.h>
#include <string.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "sx_audio_afe.h"
#include "sx_stt_service.h"
#include "sx_tts_service.h"
#include "sx_settings_service.h"

static const char *TAG = "screen_voice_settings";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_vad_slider = NULL;
static lv_obj_t *s_aec_switch = NULL;
static lv_obj_t *s_ns_switch = NULL;
static lv_obj_t *s_mic_gain_slider = NULL;
static lv_obj_t *s_container = NULL;

// Forward declarations
static void vad_slider_cb(lv_event_t *e);
static void aec_switch_cb(lv_event_t *e);
static void ns_switch_cb(lv_event_t *e);
static void mic_gain_slider_cb(lv_event_t *e);
static void load_settings(void);
static void save_settings(void);

static void on_create(void) {
    ESP_LOGI(TAG, "Voice Settings screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Set background
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "Voice Settings");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 20, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // VAD (Voice Activity Detection) slider
    lv_obj_t *vad_label = lv_label_create(s_content);
    lv_label_set_text(vad_label, "VAD Sensitivity");
    lv_obj_set_style_text_font(vad_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(vad_label, lv_color_hex(0xFFFFFF), 0);
    
    s_vad_slider = lv_slider_create(s_content);
    lv_obj_set_size(s_vad_slider, LV_PCT(100), 20);
    lv_obj_set_style_bg_color(s_vad_slider, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_vad_slider, lv_color_hex(0x5b7fff), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_vad_slider, lv_color_hex(0x5b7fff), LV_PART_KNOB);
    lv_slider_set_value(s_vad_slider, 50, LV_ANIM_OFF);
    lv_slider_set_range(s_vad_slider, 0, 100);
    lv_obj_add_event_cb(s_vad_slider, vad_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // AEC (Acoustic Echo Cancellation) switch
    lv_obj_t *aec_label = lv_label_create(s_content);
    lv_label_set_text(aec_label, "AEC (Echo Cancellation)");
    lv_obj_set_style_text_font(aec_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(aec_label, lv_color_hex(0xFFFFFF), 0);
    
    s_aec_switch = lv_switch_create(s_content);
    lv_obj_set_style_bg_color(s_aec_switch, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_aec_switch, lv_color_hex(0x5b7fff), LV_PART_INDICATOR);
    lv_obj_add_state(s_aec_switch, LV_STATE_CHECKED);
    lv_obj_add_event_cb(s_aec_switch, aec_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // NS (Noise Suppression) switch
    lv_obj_t *ns_label = lv_label_create(s_content);
    lv_label_set_text(ns_label, "NS (Noise Suppression)");
    lv_obj_set_style_text_font(ns_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ns_label, lv_color_hex(0xFFFFFF), 0);
    
    s_ns_switch = lv_switch_create(s_content);
    lv_obj_set_style_bg_color(s_ns_switch, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ns_switch, lv_color_hex(0x5b7fff), LV_PART_INDICATOR);
    lv_obj_add_state(s_ns_switch, LV_STATE_CHECKED);
    lv_obj_add_event_cb(s_ns_switch, ns_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Mic Gain slider
    lv_obj_t *mic_gain_label = lv_label_create(s_content);
    lv_label_set_text(mic_gain_label, "Microphone Gain");
    lv_obj_set_style_text_font(mic_gain_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mic_gain_label, lv_color_hex(0xFFFFFF), 0);
    
    s_mic_gain_slider = lv_slider_create(s_content);
    lv_obj_set_size(s_mic_gain_slider, LV_PCT(100), 20);
    lv_obj_set_style_bg_color(s_mic_gain_slider, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_mic_gain_slider, lv_color_hex(0x5b7fff), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_mic_gain_slider, lv_color_hex(0x5b7fff), LV_PART_KNOB);
    lv_slider_set_value(s_mic_gain_slider, 50, LV_ANIM_OFF);
    lv_slider_set_range(s_mic_gain_slider, 0, 100);
    lv_obj_add_event_cb(s_mic_gain_slider, mic_gain_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Load current settings
    load_settings();
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_VOICE_SETTINGS, "Voice Settings", container, s_content);
    #endif
}

static void load_settings(void) {
    if (!lvgl_port_lock(0)) {
        return;
    }
    
    // Load AFE settings
    bool aec_enabled = sx_audio_afe_is_aec_enabled();
    
    // Load from settings service (with defaults)
    bool ns_enabled = true;
    int32_t vad_sensitivity = 50;
    int32_t mic_gain = 50;
    
    sx_settings_get_bool_default("voice_ns_enabled", &ns_enabled, true);
    sx_settings_get_int_default("voice_vad_sensitivity", &vad_sensitivity, 50);
    sx_settings_get_int_default("voice_mic_gain", &mic_gain, 50);
    
    // Update UI
    if (s_aec_switch != NULL) {
        if (aec_enabled) {
            lv_obj_add_state(s_aec_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(s_aec_switch, LV_STATE_CHECKED);
        }
    }
    
    if (s_ns_switch != NULL) {
        if (ns_enabled) {
            lv_obj_add_state(s_ns_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(s_ns_switch, LV_STATE_CHECKED);
        }
    }
    
    if (s_vad_slider != NULL) {
        lv_slider_set_value(s_vad_slider, vad_sensitivity, LV_ANIM_OFF);
    }
    
    if (s_mic_gain_slider != NULL) {
        lv_slider_set_value(s_mic_gain_slider, mic_gain, LV_ANIM_OFF);
    }
    
    lvgl_port_unlock();
}

static void save_settings(void) {
    if (!lvgl_port_lock(0)) {
        return;
    }
    
    bool aec_enabled = lv_obj_has_state(s_aec_switch, LV_STATE_CHECKED);
    bool ns_enabled = lv_obj_has_state(s_ns_switch, LV_STATE_CHECKED);
    int vad_sensitivity = lv_slider_get_value(s_vad_slider);
    int mic_gain = lv_slider_get_value(s_mic_gain_slider);
    
    lvgl_port_unlock();
    
    // Save to settings
    sx_settings_set_bool("voice_aec_enabled", aec_enabled);
    sx_settings_set_bool("voice_ns_enabled", ns_enabled);
    sx_settings_set_int("voice_vad_sensitivity", vad_sensitivity);
    sx_settings_set_int("voice_mic_gain", mic_gain);
    sx_settings_commit();
    
    // Update AFE config
    sx_audio_afe_config_t config = {
        .enable_aec = aec_enabled,
        .enable_vad = true, // VAD is enabled if slider is used
        .enable_ns = ns_enabled,
        .enable_agc = false, // AGC not exposed in UI yet
        .sample_rate_hz = 16000
    };
    
    // Re-init AFE with new config (this will update the service)
    // Note: In production, this should be done more efficiently
    esp_err_t ret = sx_audio_afe_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update AFE config: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "AFE config updated: AEC=%d, NS=%d, VAD sensitivity=%d", 
                 aec_enabled, ns_enabled, vad_sensitivity);
    }
}

static void vad_slider_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        int value = lv_slider_get_value(s_vad_slider);
        ESP_LOGI(TAG, "VAD sensitivity changed: %d", value);
        save_settings();
    }
}

static void aec_switch_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        bool enabled = lv_obj_has_state(s_aec_switch, LV_STATE_CHECKED);
        ESP_LOGI(TAG, "AEC %s", enabled ? "enabled" : "disabled");
        save_settings();
    }
}

static void ns_switch_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        bool enabled = lv_obj_has_state(s_ns_switch, LV_STATE_CHECKED);
        ESP_LOGI(TAG, "NS %s", enabled ? "enabled" : "disabled");
        save_settings();
    }
}

static void mic_gain_slider_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        int value = lv_slider_get_value(s_mic_gain_slider);
        ESP_LOGI(TAG, "Mic gain changed: %d", value);
        save_settings();
        // Note: Mic gain adjustment should be done at hardware/audio service level
        // This is a placeholder - actual implementation depends on audio service API
    }
}

static void on_show(void) {
    ESP_LOGI(TAG, "Voice Settings screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_VOICE_SETTINGS);
    #endif
    
    // Reload settings when showing screen
    load_settings();
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Voice Settings screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_VOICE_SETTINGS);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Voice Settings screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_VOICE_SETTINGS);
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
void screen_voice_settings_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
    };
    ui_screen_registry_register(SCREEN_ID_VOICE_SETTINGS, &callbacks);
}
