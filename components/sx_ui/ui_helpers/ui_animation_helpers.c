#include "ui_animation_helpers.h"
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)
#include <stddef.h>

void ui_helper_fade_in_staggered(lv_obj_t **objs, size_t count, uint32_t base_delay_ms, uint32_t step_ms, uint32_t duration_ms) {
    if (objs == NULL || count == 0) {
        return;
    }
    
    // First, set all objects to transparent
    for (size_t i = 0; i < count; i++) {
        if (objs[i] != NULL) {
            lv_obj_set_style_opa(objs[i], LV_OPA_TRANSP, 0);
        }
    }
    
    // Then apply staggered fade-in animations
    for (size_t i = 0; i < count; i++) {
        if (objs[i] != NULL) {
            uint32_t delay = base_delay_ms + (i * step_ms);
            lv_obj_fade_in(objs[i], duration_ms, delay);
        }
    }
}

