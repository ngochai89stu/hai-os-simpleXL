#pragma once

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

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

// Boot screen image (embedded RGB565)
// Returns pointer to LVGL image descriptor
const lv_img_dsc_t* sx_assets_get_bootscreen_img(void);

// Flash screen background image (embedded RGB565)
// Returns pointer to LVGL image descriptor
const lv_img_dsc_t* sx_assets_get_flashscreen_img(void);

#ifdef __cplusplus
}
#endif

