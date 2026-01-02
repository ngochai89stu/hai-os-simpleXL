#pragma once

#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file ui_image_helpers.h
 * @brief UI helper functions for LVGL image operations
 * 
 * These functions are UI-layer helpers that work with LVGL image descriptors.
 * They replace the removed service-layer functions to comply with SIMPLEXL_ARCH v1.3.
 * 
 * Section 8: Known violations - sx_image_service functions moved to UI layer
 */

// LVGL image wrapper structure
// This wraps lv_img_dsc_t to manage lifetime of dynamically created image descriptors
typedef struct {
    lv_img_dsc_t *img_dsc;  // LVGL image descriptor
    uint8_t *data;          // RGB565 data (owned by this structure)
    uint16_t width;
    uint16_t height;
} sx_lvgl_image_t;

/**
 * @brief Create LVGL image descriptor from RGB565 data
 * 
 * This function creates an LVGL image descriptor from RGB565 pixel data.
 * The returned structure must be freed with sx_ui_image_free_lvgl().
 * 
 * @param rgb565_data RGB565 pixel data (width * height * 2 bytes)
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @return sx_lvgl_image_t* Pointer to image structure, or NULL on error
 */
sx_lvgl_image_t* sx_ui_image_create_lvgl_rgb565(const uint8_t *rgb565_data, uint16_t width, uint16_t height);

/**
 * @brief Free LVGL image structure
 * 
 * Frees the image descriptor and associated RGB565 data.
 * 
 * @param img Image structure to free (can be NULL)
 */
void sx_ui_image_free_lvgl(sx_lvgl_image_t *img);

#ifdef __cplusplus
}
#endif






