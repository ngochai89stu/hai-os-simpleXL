#include "screen_equalizer.h"

#include <esp_log.h>
#include <string.h>
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "sx_state.h"
#include "sx_audio_eq.h"
#include "sx_audio_reverb.h"
#include "ui_theme_tokens.h"
#include "ui_slider.h"

#define SX_AUDIO_EQ_NUM_BANDS 10

static const char *TAG = "screen_equalizer";

// EQ preset values (10 bands, -12dB to +12dB range, centered at 0)
static const int16_t s_eq_presets[][10] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           // Flat
    {2, 4, 6, 4, 2, 0, -2, -4, -2, 0},        // Pop
    {4, 2, -2, -4, -2, 2, 4, 6, 4, 2},       // Rock
    {0, 2, 4, 2, 0, -2, -4, -2, 0, 2},       // Jazz
    {-2, -4, -2, 0, 2, 4, 6, 4, 2, 0},       // Classical
};

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_preset_selector = NULL;
static lv_obj_t *s_band_sliders[10] = {NULL};
static lv_obj_t *s_apply_btn = NULL;
static lv_obj_t *s_reverb_slider = NULL;
static lv_obj_t *s_container = NULL;

// Forward declarations
static void reverb_slider_cb(lv_event_t *e);

static void on_create(void) {
    ESP_LOGI(TAG, "Equalizer screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Set background using token
    lv_obj_set_style_bg_color(container, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "Equalizer");
    
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
    
    // Preset selector (matching web demo)
    lv_obj_t *preset_label = lv_label_create(s_content);
    lv_label_set_text(preset_label, "Preset");
    lv_obj_set_style_text_font(preset_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(preset_label, UI_COLOR_TEXT_PRIMARY, 0);
    
    s_preset_selector = lv_dropdown_create(s_content);
    lv_dropdown_set_options(s_preset_selector, "Flat\nPop\nRock\nJazz\nClassical\nCustom");
    lv_obj_set_size(s_preset_selector, LV_PCT(100), UI_SIZE_BUTTON_HEIGHT);
    lv_obj_set_style_bg_color(s_preset_selector, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_preset_selector, UI_FONT_MEDIUM, 0);
    
    // Band sliders (10 bands) - matching web demo
    lv_obj_t *bands_label = lv_label_create(s_content);
    lv_label_set_text(bands_label, "Bands");
    lv_obj_set_style_text_font(bands_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(bands_label, UI_COLOR_TEXT_PRIMARY, 0);
    
    // Create grid for band sliders
    lv_obj_t *bands_grid = lv_obj_create(s_content);
    lv_obj_set_size(bands_grid, LV_PCT(100), 200);
    lv_obj_set_style_bg_opa(bands_grid, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(bands_grid, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bands_grid, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(bands_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(bands_grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(bands_grid, UI_SPACE_XL, LV_PART_MAIN);
    lv_obj_set_style_pad_column(bands_grid, UI_SPACE_SM, LV_PART_MAIN);
    
    // 10 band sliders (vertical)
    const char* band_labels[] = {"31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"};
    for (int i = 0; i < 10; i++) {
        lv_obj_t *band_container = lv_obj_create(bands_grid);
        lv_obj_set_size(band_container, 40, 180);
        lv_obj_set_style_bg_opa(band_container, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(band_container, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(band_container, UI_SPACE_SM, LV_PART_MAIN);
        lv_obj_set_flex_flow(band_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(band_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        // Band label
        lv_obj_t *label = lv_label_create(band_container);
        lv_label_set_text(label, band_labels[i]);
        lv_obj_set_style_text_font(label, UI_FONT_MEDIUM, 0);
        lv_obj_set_style_text_color(label, UI_COLOR_TEXT_SECONDARY, 0);
        
        // Vertical slider - using shared component (note: vertical sliders need special handling)
        s_band_sliders[i] = ui_gradient_slider_create(band_container, NULL, NULL);
        lv_obj_set_size(s_band_sliders[i], 20, 120);
        lv_slider_set_value(s_band_sliders[i], 50, LV_ANIM_OFF); // 50 = 0dB (center)
        lv_slider_set_range(s_band_sliders[i], 0, 100); // 0 = -12dB, 50 = 0dB, 100 = +12dB
        lv_obj_set_style_transform_angle(s_band_sliders[i], 2700, 0); // Rotate 270 degrees (vertical)
    }
    
    // Reverb control (merged from Audio Effects)
    lv_obj_t *reverb_label = lv_label_create(s_content);
    lv_label_set_text(reverb_label, "Reverb");
    lv_obj_set_style_text_font(reverb_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(reverb_label, UI_COLOR_TEXT_PRIMARY, 0);
    
    // Reverb slider using shared component
    s_reverb_slider = ui_gradient_slider_create(s_content, reverb_slider_cb, NULL);
    lv_obj_set_size(s_reverb_slider, LV_PCT(100), UI_SIZE_SLIDER_HEIGHT_THICK);
    lv_slider_set_value(s_reverb_slider, 0, LV_ANIM_OFF);
    lv_slider_set_range(s_reverb_slider, 0, 100);
    
    // Apply button (matching web demo)
    s_apply_btn = lv_btn_create(s_content);
    lv_obj_set_size(s_apply_btn, LV_PCT(100), UI_SIZE_BUTTON_HEIGHT);
    lv_obj_set_style_bg_color(s_apply_btn, UI_COLOR_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_radius(s_apply_btn, 5, LV_PART_MAIN);
    lv_obj_t *apply_label = lv_label_create(s_apply_btn);
    lv_label_set_text(apply_label, "Apply");
    lv_obj_set_style_text_font(apply_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(apply_label, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(apply_label);
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_EQUALIZER, "Equalizer", container, s_content);
    #endif
}


static void on_hide(void) {
    ESP_LOGI(TAG, "Equalizer screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_EQUALIZER);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Equalizer screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_EQUALIZER);
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

static void preset_changed_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *dropdown = lv_event_get_target(e);
        uint16_t selected = lv_dropdown_get_selected(dropdown);
        
        if (selected < 5) { // 0-4 are presets, 5 is Custom
            // Apply preset to EQ service
            sx_audio_eq_preset_t preset = (sx_audio_eq_preset_t)selected;
            esp_err_t ret = sx_audio_eq_set_preset(preset);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "EQ preset applied: %d", preset);
            } else {
                ESP_LOGE(TAG, "Failed to apply EQ preset: %s", esp_err_to_name(ret));
            }
            
            // Load preset values to UI sliders
            for (int i = 0; i < 10; i++) {
                if (s_band_sliders[i] != NULL) {
                    // Convert from -12dB to +12dB range to 0-100 slider range
                    int16_t db_value = s_eq_presets[selected][i];
                    int32_t slider_value = 50 + (db_value * 50 / 12); // Center at 50, scale by 12dB
                    if (slider_value < 0) slider_value = 0;
                    if (slider_value > 100) slider_value = 100;
                    lv_slider_set_value(s_band_sliders[i], slider_value, LV_ANIM_ON);
                }
            }
        }
    }
}

static void reverb_slider_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *slider = lv_event_get_target(e);
        int32_t value = lv_slider_get_value(slider);
        ESP_LOGI(TAG, "Reverb changed to %ld%%", value);
        
        // Update reverb level
        esp_err_t ret = sx_audio_reverb_set_level((uint8_t)value);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set reverb level: %s", esp_err_to_name(ret));
        }
    }
}

static void apply_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Applying EQ and effects settings");
        
        // Apply EQ values to audio service
        for (int i = 0; i < 10; i++) {
            if (s_band_sliders[i] != NULL) {
                int32_t value = lv_slider_get_value(s_band_sliders[i]);
                // Convert slider value (0-100, center at 50) to dB gain (-12dB to +12dB in 0.1dB units)
                int16_t db_value = ((int16_t)value - 50) * 120 / 50; // -120 to +120 (0.1dB units)
                esp_err_t ret = sx_audio_eq_set_band(i, db_value);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "Band %d: %.1f dB", i, db_value / 10.0f);
                } else {
                    ESP_LOGE(TAG, "Failed to set EQ band %d: %s", i, esp_err_to_name(ret));
                }
            }
        }
        
        // Apply reverb if slider exists
        if (s_reverb_slider != NULL) {
            int32_t reverb_value = lv_slider_get_value(s_reverb_slider);
            ESP_LOGI(TAG, "Reverb level: %ld%%", reverb_value);
            esp_err_t ret = sx_audio_reverb_set_level((uint8_t)reverb_value);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to set reverb level: %s", esp_err_to_name(ret));
            }
        }
        
        // Mark as custom preset
        sx_audio_eq_set_preset(SX_AUDIO_EQ_PRESET_CUSTOM);
    }
}

static void on_show(void) {
    ESP_LOGI(TAG, "Equalizer screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_EQUALIZER);
    #endif
    
    // Load current EQ settings and update UI
    // Load current preset
    sx_audio_eq_preset_t current_preset = sx_audio_eq_get_preset();
    if (s_preset_selector != NULL) {
        lv_dropdown_set_selected(s_preset_selector, (uint16_t)current_preset);
    }
    
    // Load current band gains
    int16_t current_gains[SX_AUDIO_EQ_NUM_BANDS];
    if (sx_audio_eq_get_bands(current_gains) == ESP_OK) {
        for (int i = 0; i < 10; i++) {
            if (s_band_sliders[i] != NULL) {
                // Convert from dB gain (-120 to +120 in 0.1dB units) to slider value (0-100)
                int32_t slider_value = 50 + (current_gains[i] * 50 / 120);
                if (slider_value < 0) slider_value = 0;
                if (slider_value > 100) slider_value = 100;
                lv_slider_set_value(s_band_sliders[i], slider_value, LV_ANIM_OFF);
            }
        }
    }
    
    // Load current reverb level from reverb service
    if (s_reverb_slider != NULL) {
        uint8_t current_reverb = sx_audio_reverb_get_level();
        lv_slider_set_value(s_reverb_slider, current_reverb, LV_ANIM_OFF);
    }
    
    // Add event handlers
    if (s_preset_selector != NULL && s_apply_btn != NULL) {
        lv_obj_add_event_cb(s_preset_selector, preset_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(s_apply_btn, apply_btn_cb, LV_EVENT_CLICKED, NULL);
    }
}

static void on_update(const sx_state_t *state) {
    // Update EQ display from state if needed
}

// Register this screen
void screen_equalizer_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
        .on_update = on_update,
    };
    ui_screen_registry_register(SCREEN_ID_EQUALIZER, &callbacks);
}
