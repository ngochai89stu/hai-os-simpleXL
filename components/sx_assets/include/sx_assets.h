#pragma once

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>
// KHÔNG include lvgl.h - tuân theo SIMPLEXL_ARCH v1.3 Section 7.5
// Chỉ sx_ui được phép include LVGL

#ifdef __cplusplus
extern "C" {
#endif

// Phase 3: RGB565 asset loader from SD card
// Assets are stored as raw RGB565 binary files (.bin) on SD card

// Asset handle (opaque pointer to loaded asset data)
typedef struct sx_asset* sx_asset_handle_t;

// Asset information
typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t size_bytes;  // Size in bytes (width * height * 2 for RGB565)
} sx_asset_info_t;

// Embedded image raw data structure (không phụ thuộc LVGL)
typedef struct {
    const uint8_t *data;      // Raw pixel data (RGB565)
    uint16_t width;
    uint16_t height;
    uint32_t data_size;       // Size in bytes
    uint8_t color_format;    // Format code (tương ứng LV_COLOR_FORMAT_RGB565 = 0x0B)
} sx_embedded_img_data_t;

// Initialize asset loader (mount SD card, etc.)
esp_err_t sx_assets_init(void);

// Load RGB565 asset from SD card
// path: relative path from SD root (e.g., "assets/boot_image.bin")
// Returns asset handle on success, NULL on failure
sx_asset_handle_t sx_assets_load_rgb565(const char *path, sx_asset_info_t *info);

// Get asset data pointer (RGB565 buffer)
// Returns NULL if handle is invalid
const uint16_t* sx_assets_get_data(sx_asset_handle_t handle);

// Free asset (unload from memory)
void sx_assets_free(sx_asset_handle_t handle);

// Check if SD card is mounted and ready
bool sx_assets_sd_ready(void);

// Phase 4: set SD ready state (called by sx_sd_service)
void sx_assets_set_sd_ready(bool ready);

// Boot screen image raw data (embedded RGB565)
// Returns raw data structure - sx_ui sẽ wrap thành lv_img_dsc_t
const sx_embedded_img_data_t* sx_assets_get_bootscreen_data(void);

// Flash screen background image raw data (embedded RGB565)
// Returns raw data structure - sx_ui sẽ wrap thành lv_img_dsc_t
const sx_embedded_img_data_t* sx_assets_get_flashscreen_data(void);

#ifdef __cplusplus
}
#endif

