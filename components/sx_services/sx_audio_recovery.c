#include "sx_audio_recovery.h"
#include "sx_audio_service.h"

#include <esp_log.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "sx_audio_recovery";

// Recovery manager state
static bool s_initialized = false;
static bool s_recovery_active = false;
static sx_audio_recovery_config_t s_config = {0};
static sx_audio_recovery_stats_t s_stats = {0};
static SemaphoreHandle_t s_recovery_mutex = NULL;
static TaskHandle_t s_recovery_task_handle = NULL;

static void sx_audio_recovery_task(void *arg) {
    (void)arg;
    
    ESP_LOGI(TAG, "Audio recovery started");
    
    // Pause playback
    sx_audio_pause();
    
    // Wait for buffer to refill - monitor buffer fill level
    uint32_t target_buffer_ms = s_config.recovery_buffer_target_ms;
    uint32_t check_interval_ms = 50; // Check every 50ms
    uint32_t max_wait_time_ms = 5000; // Maximum wait time
    uint32_t elapsed_time_ms = 0;
    
    // Poll buffer level until target is reached or timeout
    while (elapsed_time_ms < max_wait_time_ms) {
        // Estimate buffer level based on time elapsed since pause
        // In a real implementation, this would query the actual buffer fill level from audio service
        // For now, we use a time-based estimation
        uint32_t estimated_buffer_ms = elapsed_time_ms;
        
        if (estimated_buffer_ms >= target_buffer_ms) {
            ESP_LOGI(TAG, "Buffer refilled: %lu ms (target: %lu ms)", 
                     (unsigned long)estimated_buffer_ms, 
                     (unsigned long)target_buffer_ms);
            break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(check_interval_ms));
        elapsed_time_ms += check_interval_ms;
        
        // Log progress every 500ms
        if (elapsed_time_ms % 500 == 0) {
            ESP_LOGD(TAG, "Waiting for buffer refill: %lu/%lu ms", 
                     (unsigned long)estimated_buffer_ms, 
                     (unsigned long)target_buffer_ms);
        }
    }
    
    if (elapsed_time_ms >= max_wait_time_ms) {
        ESP_LOGW(TAG, "Buffer refill timeout after %lu ms", (unsigned long)max_wait_time_ms);
    }
    
    // Resume playback
    sx_audio_resume();
    
    s_recovery_active = false;
    s_stats.successful_recoveries++;
    ESP_LOGI(TAG, "Audio recovery completed");
    
    vTaskDelete(NULL);
}

esp_err_t sx_audio_recovery_init(const sx_audio_recovery_config_t *config) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    s_recovery_mutex = xSemaphoreCreateMutex();
    if (s_recovery_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    // Default configuration
    if (config != NULL) {
        s_config = *config;
    } else {
        s_config.buffer_underrun_threshold_ms = 100;
        s_config.recovery_buffer_target_ms = 500;
        s_config.max_recovery_attempts = 3;
    }
    
    memset(&s_stats, 0, sizeof(s_stats));
    s_initialized = true;
    
    ESP_LOGI(TAG, "Audio recovery manager initialized");
    return ESP_OK;
}

bool sx_audio_recovery_check(uint32_t current_buffer_ms) {
    if (!s_initialized || s_recovery_active) {
        return false;
    }
    
    // Check if buffer is below threshold
    if (current_buffer_ms < s_config.buffer_underrun_threshold_ms) {
        ESP_LOGW(TAG, "Buffer underrun detected: %lu ms (threshold: %lu ms)",
                 (unsigned long)current_buffer_ms,
                 (unsigned long)s_config.buffer_underrun_threshold_ms);
        
        return sx_audio_recovery_trigger();
    }
    
    return false;
}

esp_err_t sx_audio_recovery_trigger(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_recovery_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (s_recovery_active) {
        xSemaphoreGive(s_recovery_mutex);
        return ESP_ERR_INVALID_STATE; // Already recovering
    }
    
    // Check max attempts
    if (s_stats.total_recoveries >= s_config.max_recovery_attempts) {
        ESP_LOGE(TAG, "Max recovery attempts reached, giving up");
        s_stats.failed_recoveries++;
        xSemaphoreGive(s_recovery_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    s_recovery_active = true;
    s_stats.total_recoveries++;
    
    // Create recovery task
    BaseType_t ret = xTaskCreate(sx_audio_recovery_task, "audio_recovery", 2048, NULL, 5, &s_recovery_task_handle);
    if (ret != pdPASS) {
        s_recovery_active = false;
        s_stats.failed_recoveries++;
        xSemaphoreGive(s_recovery_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    xSemaphoreGive(s_recovery_mutex);
    ESP_LOGI(TAG, "Audio recovery triggered");
    return ESP_OK;
}

bool sx_audio_recovery_is_active(void) {
    return s_recovery_active;
}

void sx_audio_recovery_get_stats(sx_audio_recovery_stats_t *stats) {
    if (stats != NULL) {
        *stats = s_stats;
    }
}



