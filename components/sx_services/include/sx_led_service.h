#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: LED Control Service

// LED types
typedef enum {
    SX_LED_TYPE_GPIO = 0,
    SX_LED_TYPE_SINGLE,
    SX_LED_TYPE_WS2812,
    SX_LED_TYPE_STATE_BASED,
} sx_led_type_t;

// LED configuration
typedef struct {
    sx_led_type_t type;
    int gpio_pin;           // GPIO pin for GPIO/Single LED
    uint8_t led_count;      // Number of LEDs for WS2812
    bool inverted;          // Inverted logic (for GPIO)
} sx_led_config_t;

// LED state
typedef struct {
    uint8_t r, g, b;        // RGB values (0-255)
    uint8_t brightness;     // Brightness (0-100)
    bool on;                // On/off state
} sx_led_state_t;

// Initialize LED service
esp_err_t sx_led_service_init(const sx_led_config_t *config);

// Set LED state
esp_err_t sx_led_set_state(const sx_led_state_t *state);

// Set LED color (RGB)
esp_err_t sx_led_set_color(uint8_t r, uint8_t g, uint8_t b);

// Set LED brightness (0-100)
esp_err_t sx_led_set_brightness(uint8_t brightness);

// Turn LED on/off
esp_err_t sx_led_set_on(bool on);

// Get current LED state
esp_err_t sx_led_get_state(sx_led_state_t *state);

// Set state-based LED (automatically changes based on device state)
esp_err_t sx_led_set_state_based(bool enabled);

#ifdef __cplusplus
}
#endif





