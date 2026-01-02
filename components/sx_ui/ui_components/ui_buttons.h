#pragma once

#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an image button
 * 
 * Creates a clickable image button with the specified image source and event callback.
 * The button is created as an image object with clickable flag enabled.
 * 
 * @param parent Parent object
 * @param img_src Image source (can be LV_SYMBOL_*, image path, or image data)
 * @param cb Event callback function
 * @param user_data User data to pass to callback
 * @return Created button object, or NULL on error
 */
lv_obj_t *ui_image_button_create(lv_obj_t *parent, const void *img_src, lv_event_cb_t cb, void *user_data);

/**
 * @brief Set image source for an image button
 * 
 * @param btn Button object
 * @param img_src Image source
 */
void ui_image_button_set_src(lv_obj_t *btn, const void *img_src);

/**
 * @brief Create a checkable image button
 * 
 * Creates a checkable image button that can have different images for released and checked states.
 * Uses lv_imagebutton internally.
 * 
 * @param parent Parent object
 * @param released_src Image source for released state
 * @param checked_src Image source for checked state
 * @param cb Event callback function
 * @param user_data User data to pass to callback
 * @return Created button object, or NULL on error
 */
lv_obj_t *ui_checkable_image_button_create(lv_obj_t *parent, const void *released_src, const void *checked_src, lv_event_cb_t cb, void *user_data);

/**
 * @brief Set checked state of a checkable image button
 * 
 * @param btn Button object
 * @param checked true to set checked, false to uncheck
 */
void ui_checkable_image_button_set_checked(lv_obj_t *btn, bool checked);

#ifdef __cplusplus
}
#endif

