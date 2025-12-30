#include "sx_audio_ducking.h"
#include "sx_audio_service.h"

#include <esp_log.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "sx_audio_duck";

// Audio ducking state
static bool s_initialized = false;
static bool s_ducked = false;
static float s_duck_level = 0.3f; // Default: 30% volume when ducked
static uint32_t s_fade_duration_ms = 200; // Default: 200ms fade
static float s_original_volume_factor = 1.0f; // Store original volume before ducking
static SemaphoreHandle_t s_duck_mutex = NULL;

// Forward declaration
static void sx_audio_ducking_fade_task(void *arg);

esp_err_t sx_audio_ducking_init(const sx_audio_ducking_config_t *config) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    if (config != NULL) {
        s_duck_level = config->duck_level;
        s_fade_duration_ms = config->fade_duration_ms;
        
        // Validate duck level
        if (s_duck_level < 0.0f) s_duck_level = 0.0f;
        if (s_duck_level > 1.0f) s_duck_level = 1.0f;
        
        // Validate fade duration
        if (s_fade_duration_ms < 10) s_fade_duration_ms = 10;
        if (s_fade_duration_ms > 1000) s_fade_duration_ms = 1000;
    }
    
    s_duck_mutex = xSemaphoreCreateMutex();
    if (s_duck_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create ducking mutex");
        return ESP_ERR_NO_MEM;
    }
    
    s_initialized = true;
    s_ducked = false;
    s_original_volume_factor = 1.0f;
    
    ESP_LOGI(TAG, "Audio ducking manager initialized (duck_level=%.2f, fade_duration=%lu ms)",
             s_duck_level, (unsigned long)s_fade_duration_ms);
    return ESP_OK;
}

// Fade task for smooth volume transitions
static void sx_audio_ducking_fade_task(void *arg) {
    bool ducking = (bool)(intptr_t)arg;
    
    const uint32_t FADE_STEP_MS = 10;
    const uint32_t FADE_STEPS = s_fade_duration_ms / FADE_STEP_MS;
    
    // Get current volume from audio service
    // Note: We need to access the internal volume factor
    // For now, we'll use a workaround by getting volume percentage and converting
    uint8_t current_volume = sx_audio_get_volume();
    float current_factor = (float)current_volume / 100.0f;
    
    float start_factor = current_factor;
    float target_factor;
    
    if (ducking) {
        // Duck: reduce to duck_level
        target_factor = current_factor * s_duck_level;
        s_original_volume_factor = current_factor; // Store original
    } else {
        // Restore: return to original volume
        target_factor = s_original_volume_factor;
    }
    
    float step = (target_factor - start_factor) / FADE_STEPS;
    
    for (uint32_t i = 0; i <= FADE_STEPS; i++) {
        float new_factor = start_factor + (step * i);
        if (new_factor < 0.0f) new_factor = 0.0f;
        if (new_factor > 1.0f) new_factor = 1.0f;
        
        // Convert factor back to volume percentage
        uint8_t new_volume = (uint8_t)(new_factor * 100.0f);
        if (new_volume > 100) new_volume = 100;
        
        sx_audio_set_volume(new_volume);
        
        vTaskDelay(pdMS_TO_TICKS(FADE_STEP_MS));
    }
    
    // Finalize
    if (ducking) {
        s_ducked = true;
        ESP_LOGD(TAG, "Audio ducked to %.2f%%", target_factor * 100.0f);
    } else {
        s_ducked = false;
        ESP_LOGD(TAG, "Audio restored to %.2f%%", target_factor * 100.0f);
    }
    
    vTaskDelete(NULL);
}

esp_err_t sx_audio_duck(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_ducked) {
        ESP_LOGD(TAG, "Audio already ducked");
        return ESP_OK;
    }
    
    if (xSemaphoreTake(s_duck_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Start fade task to duck audio
    BaseType_t ret = xTaskCreatePinnedToCore(
        sx_audio_ducking_fade_task,
        "sx_audio_duck",
        2048,
        (void *)(intptr_t)true, // ducking = true
        3,
        NULL,
        tskNO_AFFINITY
    );
    
    xSemaphoreGive(s_duck_mutex);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create ducking fade task");
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

esp_err_t sx_audio_restore(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!s_ducked) {
        ESP_LOGD(TAG, "Audio not ducked");
        return ESP_OK;
    }
    
    if (xSemaphoreTake(s_duck_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Start fade task to restore audio
    BaseType_t ret = xTaskCreatePinnedToCore(
        sx_audio_ducking_fade_task,
        "sx_audio_restore",
        2048,
        (void *)(intptr_t)false, // ducking = false
        3,
        NULL,
        tskNO_AFFINITY
    );
    
    xSemaphoreGive(s_duck_mutex);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create restore fade task");
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

bool sx_audio_is_ducked(void) {
    return s_ducked;
}

esp_err_t sx_audio_ducking_set_level(float duck_level) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (duck_level < 0.0f) duck_level = 0.0f;
    if (duck_level > 1.0f) duck_level = 1.0f;
    
    s_duck_level = duck_level;
    ESP_LOGI(TAG, "Duck level set to %.2f", duck_level);
    return ESP_OK;
}

float sx_audio_ducking_get_level(void) {
    return s_duck_level;
}












