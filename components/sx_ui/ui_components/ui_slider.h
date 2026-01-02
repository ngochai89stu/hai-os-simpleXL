#pragma once

#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a gradient slider
 * 
 * Creates a slider with gradient indicator (from primary to accent color).
 * Suitable for progress bars, volume controls, etc.
 * 
 * @param parent Parent object
 * @param cb Event callback function (can be NULL)
 * @param user_data User data to pass to callback
 * @return Created slider object, or NULL on error
 */
lv_obj_t *ui_gradient_slider_create(lv_obj_t *parent, lv_event_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif

