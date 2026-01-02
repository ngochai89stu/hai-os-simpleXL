#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Audio Power Management Service
// Auto power save khi idle

// Power management configuration
typedef struct {
    uint32_t timeout_ms;        // Timeout before entering power save (default: 15000ms)
    uint32_t check_interval_ms;  // Check interval (default: 1000ms)
    bool enable_auto_power_save; // Enable auto power save
} sx_audio_power_config_t;

// Initialize audio power management
esp_err_t sx_audio_power_init(const sx_audio_power_config_t *config);

// Deinitialize audio power management
void sx_audio_power_deinit(void);

// Check and update audio power state
// Should be called periodically or when audio activity changes
void sx_audio_power_check_and_update(void);

// Notify audio activity (call when audio is playing/recording)
void sx_audio_power_notify_activity(void);

// Enable/disable power save mode
void sx_audio_power_set_enabled(bool enabled);

// Check if power save is enabled
bool sx_audio_power_is_enabled(void);

// Get current power state
bool sx_audio_power_is_in_power_save(void);

#ifdef __cplusplus
}
#endif



















