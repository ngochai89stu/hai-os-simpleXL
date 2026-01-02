#pragma once

#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)
#include "sx_theme_service.h"  // For sx_theme_colors_t
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file ui_theme_helpers.h
 * @brief UI helper functions for theme application
 * 
 * These functions are UI-layer helpers that apply theme colors to LVGL objects.
 * They replace the removed service-layer functions to comply with SIMPLEXL_ARCH v1.3.
 * 
 * Section 8: Known violations - sx_theme_service functions moved to UI layer
 */

/**
 * @brief Apply current theme colors to an LVGL object and its children
 * 
 * This function applies the current theme colors to the specified object.
 * If recursive is true, it also applies theme to all child objects.
 * 
 * @param obj LVGL object to apply theme to
 * @param recursive If true, apply theme to children recursively
 */
void sx_ui_theme_apply_to_object(lv_obj_t *obj, bool recursive);

#ifdef __cplusplus
}
#endif






