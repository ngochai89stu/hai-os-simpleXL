#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Include LVGL for image descriptor
#include "lvgl.h"

// LVGL v9 uses lv_image_dsc_t, v8 uses lv_img_dsc_t
// Use lv_image_dsc_t for compatibility
#ifndef lv_img_dsc_t
#define lv_img_dsc_t lv_image_dsc_t
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: Image Service - GIF, JPEG, CBin support

// Image types
typedef enum {
    SX_IMAGE_TYPE_UNKNOWN = 0,
    SX_IMAGE_TYPE_GIF,
    SX_IMAGE_TYPE_JPEG,
    SX_IMAGE_TYPE_PNG,
    SX_IMAGE_TYPE_CBIN,  // Compressed binary format
} sx_image_type_t;

// Image info
typedef struct {
    sx_image_type_t type;
    uint16_t width;
    uint16_t height;
    bool has_alpha;
    size_t data_size;
} sx_image_info_t;

// Initialize image service
esp_err_t sx_image_service_init(void);

// Load image from file path
// Returns image info and data pointer (caller must free data)
esp_err_t sx_image_load_from_file(const char *file_path, sx_image_info_t *info, uint8_t **data);

// Load image from memory
esp_err_t sx_image_load_from_memory(const uint8_t *data, size_t data_len, sx_image_info_t *info, uint8_t **out_data);

// Detect image type from file path
sx_image_type_t sx_image_detect_type(const char *file_path);

// Detect image type from memory
sx_image_type_t sx_image_detect_type_from_memory(const uint8_t *data, size_t data_len);

// Free image data
void sx_image_free(uint8_t *data);

// Create LVGL image descriptor from decoded RGB565 data
// Note: The returned lv_image_dsc_t structure and its data must be kept in memory
//       until the image is no longer used by LVGL
// Returns NULL on error
typedef struct {
    lv_image_dsc_t *img_dsc;
    uint8_t *data;  // Pointer to RGB565 data (owned by caller, must be freed)
} sx_lvgl_image_t;

// Create LVGL image descriptor from RGB565 data
// data: RGB565 pixel data (width * height * 2 bytes)
// width, height: Image dimensions
// Returns sx_lvgl_image_t with img_dsc and data pointer
// Caller must free both img_dsc and data when done
sx_lvgl_image_t* sx_image_create_lvgl_rgb565(uint8_t *data, uint16_t width, uint16_t height);

// Free LVGL image descriptor and data
void sx_image_free_lvgl(sx_lvgl_image_t *img);

#ifdef __cplusplus
}
#endif



