#pragma once

#include <esp_err.h>
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: UI Animations and Transitions

// Animation types
typedef enum {
    UI_ANIM_FADE_IN = 0,
    UI_ANIM_FADE_OUT,
    UI_ANIM_SLIDE_IN_LEFT,
    UI_ANIM_SLIDE_IN_RIGHT,
    UI_ANIM_SLIDE_IN_TOP,
    UI_ANIM_SLIDE_IN_BOTTOM,
    UI_ANIM_SLIDE_OUT_LEFT,
    UI_ANIM_SLIDE_OUT_RIGHT,
    UI_ANIM_SLIDE_OUT_TOP,
    UI_ANIM_SLIDE_OUT_BOTTOM,
    UI_ANIM_SCALE_IN,
    UI_ANIM_SCALE_OUT,
} ui_anim_type_t;

// Animation configuration
typedef struct {
    ui_anim_type_t type;
    uint32_t duration_ms;      // Animation duration in milliseconds
    uint32_t delay_ms;         // Delay before animation starts
    lv_anim_path_cb_t path_cb; // Animation path (e.g., lv_anim_path_ease_in_out)
} ui_anim_config_t;

// Apply animation to object
esp_err_t ui_anim_apply(lv_obj_t *obj, const ui_anim_config_t *config);

// Fade in animation (helper)
esp_err_t ui_anim_fade_in(lv_obj_t *obj, uint32_t duration_ms);

// Fade out animation (helper)
esp_err_t ui_anim_fade_out(lv_obj_t *obj, uint32_t duration_ms);

// Slide in from left (helper)
esp_err_t ui_anim_slide_in_left(lv_obj_t *obj, uint32_t duration_ms);

// Slide in from right (helper)
esp_err_t ui_anim_slide_in_right(lv_obj_t *obj, uint32_t duration_ms);

// Scale in animation (helper)
esp_err_t ui_anim_scale_in(lv_obj_t *obj, uint32_t duration_ms);

// Screen transition animation
esp_err_t ui_anim_screen_transition(lv_obj_t *old_screen, lv_obj_t *new_screen, ui_anim_type_t transition_type);

#ifdef __cplusplus
}
#endif















