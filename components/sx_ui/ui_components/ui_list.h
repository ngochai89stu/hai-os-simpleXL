#pragma once

#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a scrollable list container
 * 
 * Creates a scrollable list with custom scrollbar style.
 * 
 * @param parent Parent object
 * @return Created list object, or NULL on error
 */
lv_obj_t *ui_scrollable_list_create(lv_obj_t *parent);

/**
 * @brief Create a two-line list item
 * 
 * Creates a list item with icon (optional), title, subtitle (optional), and extra text (optional).
 * Uses grid layout internally.
 * 
 * @param parent Parent list object
 * @param icon_src Icon source (can be NULL)
 * @param title Title text (required)
 * @param subtitle Subtitle text (can be NULL)
 * @param extra_text Extra text (can be NULL, typically shown on the right)
 * @param cb Event callback function (can be NULL)
 * @param user_data User data to pass to callback
 * @return Created list item object, or NULL on error
 */
lv_obj_t *ui_list_item_two_line_create(
    lv_obj_t *parent,
    const void *icon_src,
    const char *title,
    const char *subtitle,
    const char *extra_text,
    lv_event_cb_t cb,
    void *user_data
);

#ifdef __cplusplus
}
#endif

