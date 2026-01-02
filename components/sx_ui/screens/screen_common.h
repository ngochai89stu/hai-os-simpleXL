#pragma once

#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)
#include "ui_screen_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

// Common screen utilities
lv_obj_t* screen_common_create_top_bar(lv_obj_t *parent, const char *title);
lv_obj_t* screen_common_create_top_bar_with_back(lv_obj_t *parent, const char *title);

// List item creator (for scrollable lists)
lv_obj_t* screen_common_create_list_item(lv_obj_t *parent, const char *title, ui_screen_id_t target_screen);

#if SX_UI_VERIFY_MODE
// Touch probe for verification (only in verify mode)
lv_obj_t* screen_common_add_touch_probe(lv_obj_t *parent, const char *label, ui_screen_id_t screen_id);
#endif

#ifdef __cplusplus
}
#endif


