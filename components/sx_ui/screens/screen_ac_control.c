#include "screen_ac_control.h"

#include <esp_log.h>
#include <string.h>
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "sx_state.h"
#include "ui_theme_tokens.h"
#include "ui_slider.h"
#include "ui_buttons.h"

static const char *TAG = "screen_ac_control";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_temp_slider = NULL;
static lv_obj_t *s_temp_label = NULL;
static lv_obj_t *s_power_btn = NULL;
static lv_obj_t *s_mode_btns[3] = {NULL};
static int32_t s_current_temp = 24;
static bool s_power_on = false;
static int s_current_mode = 0;  // 0: Cool, 1: Fan, 2: Heat

// Forward declarations
static void temp_slider_cb(lv_event_t *e);
static void power_btn_cb(lv_event_t *e);
static void mode_btn_cb(lv_event_t *e);

static void temp_slider_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        int32_t value = lv_slider_get_value(s_temp_slider);
        s_current_temp = value;
        
        // Update temperature label
        if (s_temp_label != NULL) {
            char temp_str[16];
            snprintf(temp_str, sizeof(temp_str), "%ld°C", (long)s_current_temp);
            lv_label_set_text(s_temp_label, temp_str);
        }
        
        // Set AC temperature (if service available)
        // sx_ac_set_temperature(s_current_temp);
        ESP_LOGI(TAG, "Temperature set to %ld°C", (long)s_current_temp);
    }
}

static void power_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        s_power_on = !s_power_on;
        
        // Update button checked state
        if (s_power_btn != NULL) {
            if (s_power_on) {
                lv_obj_add_state(s_power_btn, LV_STATE_CHECKED);
            } else {
                lv_obj_clear_state(s_power_btn, LV_STATE_CHECKED);
            }
        }
        
        // Set AC power (if service available)
        // sx_ac_set_power(s_power_on);
        ESP_LOGI(TAG, "AC power: %s", s_power_on ? "ON" : "OFF");
    }
}

static void mode_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_obj_t *btn = lv_event_get_target(e);
        int mode = (int)(intptr_t)lv_event_get_user_data(e);
        s_current_mode = mode;
        
        // Update button states
        for (int i = 0; i < 3; i++) {
            if (s_mode_btns[i] != NULL) {
                lv_obj_clear_state(s_mode_btns[i], LV_STATE_CHECKED);
            }
        }
        lv_obj_add_state(btn, LV_STATE_CHECKED);
        
        // Set AC mode (if service available)
        // sx_ac_set_mode(mode);
        ESP_LOGI(TAG, "AC mode: %d", mode);
    }
}

static void on_create(void) {
    ESP_LOGI(TAG, "AC Control screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    // Set background using token
    lv_obj_set_style_bg_color(container, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "AC Control");
    
    // Create content area (Grid layout)
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, UI_SPACE_XL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_content, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // Grid layout
    static const int32_t grid_cols[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t grid_rows[] = {
        LV_GRID_CONTENT,  // Power button
        LV_GRID_FR(1),    // Spacer
        LV_GRID_CONTENT,  // Temperature slider
        LV_GRID_CONTENT,  // Temperature label
        LV_GRID_FR(1),    // Spacer
        LV_GRID_CONTENT,  // Mode buttons
        LV_GRID_TEMPLATE_LAST
    };
    lv_obj_set_grid_dsc_array(s_content, grid_cols, grid_rows);
    
    // Power button (checkable)
    // Note: Using regular button with checked state since we don't have AC icons yet
    s_power_btn = lv_btn_create(s_content);
    lv_obj_set_size(s_power_btn, 80, 80);
    lv_obj_set_style_bg_color(s_power_btn, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_power_btn, UI_COLOR_PRIMARY, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_radius(s_power_btn, 40, LV_PART_MAIN);
    lv_obj_set_grid_cell(s_power_btn, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_add_event_cb(s_power_btn, power_btn_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *power_label = lv_label_create(s_power_btn);
    lv_label_set_text(power_label, "POWER");
    lv_obj_set_style_text_font(power_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(power_label, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(power_label);
    
    // Temperature slider
    s_temp_slider = ui_gradient_slider_create(s_content, temp_slider_cb, NULL);
    lv_obj_set_size(s_temp_slider, LV_PCT(90), UI_SIZE_SLIDER_HEIGHT_THICK);
    lv_slider_set_range(s_temp_slider, 16, 30);
    lv_slider_set_value(s_temp_slider, s_current_temp, LV_ANIM_OFF);
    lv_obj_set_grid_cell(s_temp_slider, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    
    // Temperature label
    s_temp_label = lv_label_create(s_content);
    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%ld°C", (long)s_current_temp);
    lv_label_set_text(s_temp_label, temp_str);
#if LV_FONT_MONTSERRAT_22
    lv_obj_set_style_text_font(s_temp_label, UI_FONT_XLARGE, 0);
#else
    lv_obj_set_style_text_font(s_temp_label, UI_FONT_LARGE, 0);
#endif
    lv_obj_set_style_text_color(s_temp_label, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_grid_cell(s_temp_label, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 3, 1);
    
    // Mode buttons container
    lv_obj_t *mode_container = lv_obj_create(s_content);
    lv_obj_set_size(mode_container, LV_PCT(90), 60);
    lv_obj_set_style_bg_opa(mode_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(mode_container, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(mode_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mode_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_grid_cell(mode_container, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 5, 1);
    
    // Mode buttons
    const char *mode_labels[] = {"Cool", "Fan", "Heat"};
    for (int i = 0; i < 3; i++) {
        s_mode_btns[i] = lv_btn_create(mode_container);
        lv_obj_set_size(s_mode_btns[i], 60, 60);
        lv_obj_set_style_bg_color(s_mode_btns[i], UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_mode_btns[i], UI_COLOR_PRIMARY, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_radius(s_mode_btns[i], 30, LV_PART_MAIN);
        lv_obj_add_event_cb(s_mode_btns[i], mode_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        
        lv_obj_t *mode_label = lv_label_create(s_mode_btns[i]);
        lv_label_set_text(mode_label, mode_labels[i]);
        lv_obj_set_style_text_font(mode_label, UI_FONT_SMALL, 0);
        lv_obj_set_style_text_color(mode_label, UI_COLOR_TEXT_PRIMARY, 0);
        lv_obj_center(mode_label);
    }
    
    // Set initial mode
    lv_obj_add_state(s_mode_btns[0], LV_STATE_CHECKED);
    
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_AC_CONTROL, "AC Control", container, s_content);
    #endif
}

static void on_show(void) {
    ESP_LOGI(TAG, "AC Control screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_AC_CONTROL);
    #endif
}

static void on_hide(void) {
    ESP_LOGI(TAG, "AC Control screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_AC_CONTROL);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "AC Control screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_AC_CONTROL);
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
    // Update UI from AC state if needed
    (void)state;
}

// Register this screen
void screen_ac_control_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
        .on_update = on_update,
    };
    ui_screen_registry_register(SCREEN_ID_AC_CONTROL, &callbacks);
}

