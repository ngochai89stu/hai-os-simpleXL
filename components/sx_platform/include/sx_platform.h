#pragma once

#include <esp_err.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

// Holds all required handles for a display, allowing platform to hide implementation.
typedef struct {
    esp_lcd_panel_io_handle_t io_handle;
    esp_lcd_panel_handle_t panel_handle;
} sx_display_handles_t;

// Phase 3: Touch handle
typedef struct {
    esp_lcd_touch_handle_t touch_handle;
} sx_touch_handles_t;

// Phase 2: Platform initialization for display hardware.
sx_display_handles_t sx_platform_display_init(void);

// Phase 3: Platform initialization for touch hardware.
esp_err_t sx_platform_touch_init(sx_touch_handles_t *touch_handles);

// Phase 5: Backlight brightness control (PWM)
// Set display brightness (0-100 percent)
esp_err_t sx_platform_set_brightness(uint8_t percent);

// Get current brightness (0-100 percent)
uint8_t sx_platform_get_brightness(void);

// Get screen dimensions (width and height in pixels)
// Returns ESP_OK on success
esp_err_t sx_platform_get_screen_size(uint16_t *width, uint16_t *height);

#ifdef __cplusplus
}
#endif
