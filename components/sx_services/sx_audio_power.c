#include "sx_audio_power.h"
#include "sx_audio_service.h"
#include <esp_log.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_timer.h"

static const char *TAG = "sx_audio_power";

// Power management state
static bool s_initialized = false;
static bool s_enabled = false;
static bool s_in_power_save = false;
static sx_audio_power_config_t s_config = {0};
static int64_t s_last_activity_time = 0;
static TimerHandle_t s_power_timer = NULL;

// Power timer callback
static void sx_audio_power_timer_callback(TimerHandle_t xTimer);

esp_err_t sx_audio_power_init(const sx_audio_power_config_t *config) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    // Apply configuration
    if (config) {
        s_config = *config;
    } else {
        // Default configuration
        s_config.timeout_ms = 15000;  // 15 seconds
        s_config.check_interval_ms = 1000;  // 1 second
        s_config.enable_auto_power_save = true;
    }
    
    s_enabled = s_config.enable_auto_power_save;
    s_in_power_save = false;
    s_last_activity_time = esp_timer_get_time();
    
    // Create power check timer
    s_power_timer = xTimerCreate("sx_audio_power",
                                 pdMS_TO_TICKS(s_config.check_interval_ms),
                                 pdTRUE,  // Auto-reload
                                 NULL,
                                 sx_audio_power_timer_callback);
    if (!s_power_timer) {
        return ESP_ERR_NO_MEM;
    }
    
    // Start timer
    if (xTimerStart(s_power_timer, 0) != pdPASS) {
        xTimerDelete(s_power_timer, portMAX_DELAY);
        s_power_timer = NULL;
        return ESP_FAIL;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "Audio power management initialized (timeout: %lu ms, check: %lu ms)",
             (unsigned long)s_config.timeout_ms, (unsigned long)s_config.check_interval_ms);
    return ESP_OK;
}

void sx_audio_power_deinit(void) {
    if (!s_initialized) {
        return;
    }
    
    // Stop and delete timer
    if (s_power_timer) {
        xTimerStop(s_power_timer, portMAX_DELAY);
        xTimerDelete(s_power_timer, portMAX_DELAY);
        s_power_timer = NULL;
    }
    
    // Exit power save if active
    if (s_in_power_save) {
        // Restore normal power state
        // Note: In production, you may want to restore I2S, WiFi, etc.
        s_in_power_save = false;
    }
    
    s_initialized = false;
    ESP_LOGI(TAG, "Audio power management deinitialized");
}

static void sx_audio_power_timer_callback(TimerHandle_t xTimer) {
    (void)xTimer;
    sx_audio_power_check_and_update();
}

void sx_audio_power_check_and_update(void) {
    if (!s_initialized || !s_enabled) {
        return;
    }
    
    int64_t current_time = esp_timer_get_time();
    int64_t idle_time = current_time - s_last_activity_time;
    uint32_t idle_time_ms = (uint32_t)(idle_time / 1000);
    
    // Check if audio is currently active
    extern bool sx_audio_is_playing(void);
    extern bool sx_audio_is_recording(void);
    bool audio_active = sx_audio_is_playing() || sx_audio_is_recording();
    
    if (audio_active) {
        // Audio is active, ensure not in power save
        if (s_in_power_save) {
            ESP_LOGI(TAG, "Exiting power save mode (audio active)");
            s_in_power_save = false;
            // Restore normal power state
            // Note: In production, you may want to restore I2S, WiFi, etc.
        }
        s_last_activity_time = current_time;
    } else {
        // Audio is idle
        if (idle_time_ms >= s_config.timeout_ms) {
            // Enter power save mode
            if (!s_in_power_save) {
                ESP_LOGI(TAG, "Entering power save mode (idle for %lu ms)", (unsigned long)idle_time_ms);
                s_in_power_save = true;
                // Reduce power consumption
                // Note: In production, you may want to:
                // - Reduce CPU frequency
                // - Disable I2S when not needed
                // - Reduce WiFi power
                // - Enter light sleep mode
            }
        }
    }
}

void sx_audio_power_notify_activity(void) {
    if (!s_initialized) {
        return;
    }
    
    s_last_activity_time = esp_timer_get_time();
    
    // If in power save, exit immediately
    if (s_in_power_save) {
        sx_audio_power_check_and_update();
    }
}

void sx_audio_power_set_enabled(bool enabled) {
    if (!s_initialized) {
        return;
    }
    
    s_enabled = enabled;
    
    if (enabled) {
        s_last_activity_time = esp_timer_get_time();
        if (s_power_timer) {
            xTimerStart(s_power_timer, 0);
        }
    } else {
        if (s_power_timer) {
            xTimerStop(s_power_timer, 0);
        }
        if (s_in_power_save) {
            s_in_power_save = false;
        }
    }
    
    ESP_LOGI(TAG, "Audio power management %s", enabled ? "enabled" : "disabled");
}

bool sx_audio_power_is_enabled(void) {
    return s_initialized && s_enabled;
}

bool sx_audio_power_is_in_power_save(void) {
    return s_initialized && s_in_power_save;
}


