#pragma once

#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)
#include "sx_qr_code_service.h"  // For sx_qr_code_generate
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file ui_qr_code_helpers.h
 * @brief UI helper functions for QR code widget creation
 * 
 * These functions are UI-layer helpers that create LVGL QR code widgets.
 * They replace the removed service-layer functions to comply with SIMPLEXL_ARCH v1.3.
 * 
 * Section 8: Known violations - sx_qr_code_service functions moved to UI layer
 */

/**
 * @brief Create LVGL QR code widget from text
 * 
 * This function generates QR code data using the service, then creates
 * an LVGL widget to display it. The widget is created as a child of parent.
 * 
 * @param parent Parent LVGL object
 * @param text Text to encode in QR code
 * @param size Widget size in pixels (width and height)
 * @return lv_obj_t* QR code widget object, or NULL on error
 */
lv_obj_t* sx_ui_qr_code_create_widget(lv_obj_t *parent, const char *text, uint16_t size);

#ifdef __cplusplus
}
#endif






