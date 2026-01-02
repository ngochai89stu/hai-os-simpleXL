#pragma once

#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Apply staggered fade-in animation to multiple objects
 * 
 * This function applies fade-in animations to an array of objects with staggered delays,
 * creating a smooth sequential appearance effect.
 * 
 * @param objs Array of lv_obj_t pointers to animate
 * @param count Number of objects in the array
 * @param base_delay_ms Base delay in milliseconds before first animation starts
 * @param step_ms Delay step in milliseconds between each object animation
 * @param duration_ms Duration of each fade-in animation in milliseconds
 */
void ui_helper_fade_in_staggered(lv_obj_t **objs, size_t count, uint32_t base_delay_ms, uint32_t step_ms, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif

