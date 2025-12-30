#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Icon cache service for navigation icons
// Stores icons in SPIFFS to avoid re-downloading

#define SX_NAV_ICON_CACHE_PATH "/spiffs/nav_icons"
#define SX_NAV_ICON_CACHE_MAX_ICONS 50

// Initialize icon cache service
esp_err_t sx_nav_icon_cache_init(void);

// Deinitialize icon cache service
void sx_nav_icon_cache_deinit(void);

// Check if icon exists in cache
bool sx_nav_icon_cache_exists(const char *icon_hash);

// Save icon to cache
// icon_hash: 16-byte hash string
// bitmap: Icon bitmap data (RGB565, 8192 bytes)
// Returns: ESP_OK on success
esp_err_t sx_nav_icon_cache_save(const char *icon_hash, const uint8_t *bitmap, size_t size);

// Load icon from cache
// icon_hash: 16-byte hash string
// bitmap: Output buffer (must be at least 8192 bytes)
// size: Size of output buffer
// Returns: ESP_OK on success, ESP_ERR_NOT_FOUND if not cached
esp_err_t sx_nav_icon_cache_load(const char *icon_hash, uint8_t *bitmap, size_t size);

// Get list of cached icon hashes
// hashes: Output array of hash strings
// max_count: Maximum number of hashes to return
// Returns: Number of cached icons
size_t sx_nav_icon_cache_list(char hashes[][17], size_t max_count);

// Clear all cached icons
esp_err_t sx_nav_icon_cache_clear_all(void);

#ifdef __cplusplus
}
#endif










