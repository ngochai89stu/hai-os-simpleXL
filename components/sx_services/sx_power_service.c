#include "sx_power_service.h"

#include <esp_log.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_sleep.h"
// Note: esp_adc_cal is deprecated in ESP-IDF v5.x, use esp_adc component instead
// TODO: Migrate to esp_adc/adc_oneshot.h and esp_adc/adc_cali.h when implementing battery monitoring
// #include "driver/adc.h"
// #include "esp_adc_cal.h"
// #include "esp_adc/adc_oneshot.h"
// #include "esp_adc/adc_cali.h"
// #include "esp_adc/adc_cali_scheme.h"
#include "driver/rtc_io.h"
// Note: RTC clock API changed in ESP-IDF v5.x
// TODO: Use esp_pm component or new RTC clock API for frequency control
// #include "soc/rtc.h"
// #include "esp_pm.h"
#include "soc/rtc.h"

static const char *TAG = "sx_power";

// Power service state
static bool s_initialized = false;
static sx_power_config_t s_config = {0};
static sx_power_mode_t s_current_mode = SX_POWER_MODE_NORMAL;
static TimerHandle_t s_sleep_timer = NULL;

// Battery monitoring (if enabled)
static bool s_battery_monitoring = false;
static int s_battery_adc_channel = -1;
// Note: ADC calibration code needs to be migrated to esp_adc component (ESP-IDF v5.x)
// static adc1_channel_t s_adc1_channel = ADC1_CHANNEL_MAX;
// static esp_adc_cal_characteristics_t s_adc1_chars;
// static bool s_adc_calibrated = false;

// Battery voltage calculation
// Typical voltage divider: V_batt = ADC_reading * (R1 + R2) / R2 * Vref / 4095
// For ESP32, Vref is typically 1100mV, ADC is 12-bit (0-4095)
// Common divider: R1=100k, R2=100k -> ratio = 2.0
// Note: These variables are reserved for future battery monitoring implementation
// static float s_voltage_divider_ratio = 2.0f; // Default 2:1 divider
// static float s_battery_full_voltage = 4.2f; // Full charge voltage (V)
// static float s_battery_empty_voltage = 3.0f; // Empty voltage (V)

static void sx_power_sleep_timer_callback(TimerHandle_t xTimer) {
    (void)xTimer;
    
    ESP_LOGI(TAG, "Sleep timer expired, entering deep sleep");
    sx_power_enter_deep_sleep(0); // Sleep indefinitely
}

static void sx_power_idle_monitor_task(void *arg) {
    (void)arg;
    
    ESP_LOGI(TAG, "Power idle monitor task started");
    
    uint32_t last_activity_time = xTaskGetTickCount();
    
    while (s_config.enable_auto_power_save) {
        uint32_t current_time = xTaskGetTickCount();
        uint32_t idle_time = (current_time - last_activity_time) * portTICK_PERIOD_MS;
        
        if (idle_time >= s_config.idle_timeout_ms) {
            ESP_LOGI(TAG, "Idle timeout reached (%lu ms), entering power save mode", (unsigned long)idle_time);
            sx_power_set_mode(SX_POWER_MODE_LOW_POWER);
            last_activity_time = current_time; // Reset timer
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // Check every second
    }
    
    vTaskDelete(NULL);
}

esp_err_t sx_power_service_init(const sx_power_config_t *config) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    if (config != NULL) {
        s_config = *config;
    } else {
        // Default configuration
        s_config.enable_auto_power_save = false;
        s_config.idle_timeout_ms = 5 * 60 * 1000; // 5 minutes
        s_config.enable_battery_monitoring = false;
        s_config.battery_adc_channel = -1;
    }
    
    s_battery_monitoring = s_config.enable_battery_monitoring;
    s_battery_adc_channel = s_config.battery_adc_channel;
    
    // Initialize battery monitoring if enabled
    // Note: ADC code needs to be migrated to esp_adc component (ESP-IDF v5.x)
    // TODO: Migrate to esp_adc/adc_oneshot.h and esp_adc/adc_cali.h
    if (s_battery_monitoring && s_battery_adc_channel >= 0) {
        ESP_LOGW(TAG, "Battery monitoring requested but ADC code needs migration to esp_adc component");
        // Map channel number to ADC1 channel
        // Common mappings: 0->ADC1_CHANNEL_0 (GPIO36), 1->ADC1_CHANNEL_1 (GPIO37), etc.
        // if (s_battery_adc_channel >= 0 && s_battery_adc_channel <= 7) {
        //     s_adc1_channel = (adc1_channel_t)s_battery_adc_channel;
        //     
        //     // Configure ADC1
        //     adc1_config_width(ADC_WIDTH_BIT_12);
        //     adc1_config_channel_atten(s_adc1_channel, ADC_ATTEN_DB_11); // 0-3.3V range
        //     
        //     // Characterize ADC
        //     esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
        //         ADC_UNIT_1,
        //         ADC_ATTEN_DB_11,
        //         ADC_WIDTH_BIT_12,
        //         1100, // Default Vref (mV)
        //         &s_adc1_chars
        //     );
        //     
        //     if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        //         ESP_LOGI(TAG, "ADC calibrated using Two Point Value");
        //     } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        //         ESP_LOGI(TAG, "ADC calibrated using eFuse Vref");
        //     } else {
        //         ESP_LOGI(TAG, "ADC calibrated using Default Vref");
        //     }
        //     
        //     s_adc_calibrated = true;
        //     ESP_LOGI(TAG, "Battery monitoring initialized: ADC1 channel %d (GPIO%d)",
        //              s_battery_adc_channel, 36 + s_battery_adc_channel);
        // } else {
        //     ESP_LOGE(TAG, "Invalid ADC channel: %d (must be 0-7)", s_battery_adc_channel);
        //     s_battery_monitoring = false;
        // }
    }
    
    // Start idle monitor task if auto power save is enabled
    if (s_config.enable_auto_power_save) {
        xTaskCreate(sx_power_idle_monitor_task, "power_idle", 2048, NULL, 3, NULL);
    }
    
    s_initialized = true;
    s_current_mode = SX_POWER_MODE_NORMAL;
    
    ESP_LOGI(TAG, "Power service initialized");
    return ESP_OK;
}

esp_err_t sx_power_set_mode(sx_power_mode_t mode) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    s_current_mode = mode;
    
    switch (mode) {
        case SX_POWER_MODE_NORMAL:
            // Normal operation - no changes needed
            ESP_LOGI(TAG, "Power mode: Normal");
            break;
            
        case SX_POWER_MODE_LOW_POWER:
            // Low power mode - reduce CPU frequency, disable peripherals
            // Note: CPU frequency control API changed in ESP-IDF v5.x
            // TODO: Use esp_pm component or new RTC clock API for frequency control
            // For now, use power management component if available
            // rtc_cpu_freq_config_t cpu_config;
            // rtc_clk_cpu_freq_get_config(&cpu_config);
            // 
            // // Try to set to 80MHz (if supported)
            // if (rtc_clk_cpu_freq_mhz() > 80) {
            //     rtc_clk_cpu_freq_set(RTC_CPU_FREQ_80M);
            //     ESP_LOGI(TAG, "CPU frequency reduced to 80MHz for low power mode");
            // }
            
            // Disable WiFi if not needed (optional, requires careful handling)
            // esp_wifi_stop(); // Only if WiFi is not needed
            
            ESP_LOGI(TAG, "Power mode: Low Power (CPU frequency control needs migration to ESP-IDF v5.x API)");
            break;
            
        case SX_POWER_MODE_DEEP_SLEEP:
            // Deep sleep - handled by sx_power_enter_deep_sleep
            ESP_LOGI(TAG, "Power mode: Deep Sleep");
            break;
    }
    
    return ESP_OK;
}

sx_power_mode_t sx_power_get_mode(void) {
    return s_current_mode;
}

esp_err_t sx_power_get_battery_status(sx_battery_status_t *status) {
    if (!s_initialized || status == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!s_battery_monitoring) {
        // Return default values if monitoring is not enabled
        status->level = 100;
        status->charging = false;
        status->plugged = false;
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    // Note: ADC code needs to be migrated to esp_adc component (ESP-IDF v5.x)
    // TODO: Migrate to esp_adc/adc_oneshot.h and esp_adc/adc_cali.h
    // if (!s_adc_calibrated || s_adc1_channel == ADC1_CHANNEL_MAX) {
    //     // ADC not initialized, return default values
    //     status->level = 100;
    //     status->charging = false;
    //     status->plugged = false;
    //     return ESP_ERR_INVALID_STATE;
    // }
    // 
    // // Read ADC value
    // int adc_raw = adc1_get_raw(s_adc1_channel);
    // if (adc_raw < 0) {
    //     ESP_LOGE(TAG, "Failed to read ADC");
    //     return ESP_FAIL;
    // }
    // 
    // // Convert to voltage (mV)
    // uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(adc_raw, &s_adc1_chars);
    
    // Temporary: return default values until ADC code is migrated
    status->level = 100;
    status->charging = false;
    status->plugged = false;
    return ESP_ERR_NOT_SUPPORTED;
    
    // Note: Code below needs to be migrated to esp_adc component (ESP-IDF v5.x)
    // // Apply voltage divider ratio to get actual battery voltage
    // float battery_voltage = (voltage_mv / 1000.0f) * s_voltage_divider_ratio;
    // 
    // // Calculate battery level (0-100%)
    // // Linear interpolation between empty and full voltage
    // float voltage_range = s_battery_full_voltage - s_battery_empty_voltage;
    // float voltage_above_empty = battery_voltage - s_battery_empty_voltage;
    // int level = (int)((voltage_above_empty / voltage_range) * 100.0f);
    // 
    // // Clamp to 0-100
    // if (level < 0) level = 0;
    // if (level > 100) level = 100;
    // 
    // status->level = (uint8_t)level;
    // 
    // // Charging detection: if voltage is above full voltage, likely charging
    // // This is a simple heuristic - actual implementation may need GPIO for charge detection
    // status->charging = (battery_voltage > s_battery_full_voltage);
    // status->plugged = status->charging; // Assume plugged if charging
    // 
    // ESP_LOGD(TAG, "Battery: %d%% (%.2fV, ADC: %d)", level, battery_voltage, adc_raw);
    
    return ESP_OK;
}

esp_err_t sx_power_set_sleep_timer(uint32_t timeout_ms) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Cancel existing timer if any
    if (s_sleep_timer != NULL) {
        xTimerDelete(s_sleep_timer, 0);
        s_sleep_timer = NULL;
    }
    
    // Create new timer
    s_sleep_timer = xTimerCreate("sleep_timer", pdMS_TO_TICKS(timeout_ms), pdFALSE, NULL, sx_power_sleep_timer_callback);
    if (s_sleep_timer == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    xTimerStart(s_sleep_timer, 0);
    ESP_LOGI(TAG, "Sleep timer set for %lu ms", (unsigned long)timeout_ms);
    
    return ESP_OK;
}

esp_err_t sx_power_cancel_sleep_timer(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_sleep_timer != NULL) {
        xTimerStop(s_sleep_timer, 0);
        xTimerDelete(s_sleep_timer, 0);
        s_sleep_timer = NULL;
        ESP_LOGI(TAG, "Sleep timer cancelled");
    }
    
    return ESP_OK;
}

esp_err_t sx_power_enter_deep_sleep(uint32_t sleep_time_ms) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Entering deep sleep for %lu ms", (unsigned long)sleep_time_ms);
    
    // Configure deep sleep
    if (sleep_time_ms > 0) {
        esp_sleep_enable_timer_wakeup(sleep_time_ms * 1000); // Convert to microseconds
    }
    
    // Enter deep sleep
    esp_deep_sleep_start();
    
    // Will not return
    return ESP_OK;
}


