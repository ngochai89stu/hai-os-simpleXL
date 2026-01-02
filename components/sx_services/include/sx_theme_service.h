#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
// P0.1: Removed lvgl.h include - UI task handles LVGL operations

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: Theme System - Light/Dark themes với persistence

// Theme types
typedef enum {
    SX_THEME_DARK = 0,
    SX_THEME_LIGHT = 1,
    SX_THEME_AUTO = 2,  // Auto-detect based on time or system preference
} sx_theme_type_t;

// Theme color structure
typedef struct {
    uint32_t bg_primary;      // Primary background color (e.g., 0x1a1a1a for dark)
    uint32_t bg_secondary;    // Secondary background color (e.g., 0x2a2a2a for dark)
    uint32_t bg_tertiary;     // Tertiary background color (e.g., 0x3a3a3a for dark)
    uint32_t text_primary;    // Primary text color (e.g., 0xFFFFFF for dark)
    uint32_t text_secondary;  // Secondary text color (e.g., 0x888888 for dark)
    uint32_t accent;          // Accent color (e.g., 0x5b7fff)
    uint32_t error;           // Error color (e.g., 0xFF0000)
    uint32_t success;         // Success color (e.g., 0x00FF00)
} sx_theme_colors_t;

// Initialize theme service
esp_err_t sx_theme_service_init(void);

// Set theme type
esp_err_t sx_theme_set_type(sx_theme_type_t theme_type);

// Get current theme type
sx_theme_type_t sx_theme_get_type(void);

// Get theme colors for current theme
const sx_theme_colors_t* sx_theme_get_colors(void);

// P0.1: sx_theme_apply_to_object() removed - UI task should apply theme directly via LVGL
// UI components receive SX_EVT_THEME_CHANGED event and apply colors via LVGL APIs

// Register theme change callback
typedef void (*sx_theme_change_cb_t)(sx_theme_type_t new_theme);
esp_err_t sx_theme_register_change_callback(sx_theme_change_cb_t callback);

#ifdef __cplusplus
}
#endif



