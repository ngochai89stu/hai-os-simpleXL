#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// P0.1: Removed LVGL include - UI task handles LVGL operations

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

// P0.1: LVGL image descriptor functions removed - moved to UI helper
// UI task should create LVGL image descriptors directly using sx_lvgl.h
// Service only provides decoded RGB565 data, UI handles LVGL operations

#ifdef __cplusplus
}
#endif



