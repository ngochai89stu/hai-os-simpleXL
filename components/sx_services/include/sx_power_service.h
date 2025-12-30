#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: Power Management Service

// Power save mode
typedef enum {
    SX_POWER_MODE_NORMAL = 0,
    SX_POWER_MODE_LOW_POWER,
    SX_POWER_MODE_DEEP_SLEEP,
} sx_power_mode_t;

// Battery status
typedef struct {
    uint8_t level;          // Battery level (0-100)
    bool charging;         // Is charging
    bool plugged;          // Is plugged in
} sx_battery_status_t;

// Power configuration
typedef struct {
    bool enable_auto_power_save;  // Auto power save when idle
    uint32_t idle_timeout_ms;     // Idle timeout before power save (default: 5 minutes)
    bool enable_battery_monitoring; // Enable battery monitoring
    int battery_adc_channel;      // ADC channel for battery monitoring (-1 if not used)
} sx_power_config_t;

// Initialize power service
esp_err_t sx_power_service_init(const sx_power_config_t *config);

// Set power save mode
esp_err_t sx_power_set_mode(sx_power_mode_t mode);

// Get current power mode
sx_power_mode_t sx_power_get_mode(void);

// Get battery status
esp_err_t sx_power_get_battery_status(sx_battery_status_t *status);

// Set sleep timer (auto sleep after specified time)
esp_err_t sx_power_set_sleep_timer(uint32_t timeout_ms);

// Cancel sleep timer
esp_err_t sx_power_cancel_sleep_timer(void);

// Enter deep sleep
esp_err_t sx_power_enter_deep_sleep(uint32_t sleep_time_ms);

#ifdef __cplusplus
}
#endif












