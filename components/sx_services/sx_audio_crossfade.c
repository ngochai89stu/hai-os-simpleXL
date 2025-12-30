#include "sx_audio_crossfade.h"

#include <esp_log.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "sx_crossfade";

// Crossfade state
static bool s_initialized = false;
static bool s_enabled = true;
static sx_audio_crossfade_state_t s_state = SX_CROSSFADE_IDLE;
static uint32_t s_fade_duration_ms = 500; // Default 500ms
static uint32_t s_samples_processed = 0;
static uint32_t s_total_fade_samples = 0;
static uint32_t s_sample_rate = 16000; // Default sample rate
static SemaphoreHandle_t s_crossfade_mutex = NULL;

esp_err_t sx_audio_crossfade_init(const sx_audio_crossfade_config_t *config) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    if (config != NULL) {
        s_fade_duration_ms = config->fade_duration_ms;
        s_enabled = config->enabled;
        
        // Validate fade duration
        if (s_fade_duration_ms < 10) s_fade_duration_ms = 10;
        if (s_fade_duration_ms > 2000) s_fade_duration_ms = 2000;
    }
    
    s_crossfade_mutex = xSemaphoreCreateMutex();
    if (s_crossfade_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create crossfade mutex");
        return ESP_ERR_NO_MEM;
    }
    
    s_initialized = true;
    s_state = SX_CROSSFADE_IDLE;
    s_samples_processed = 0;
    s_total_fade_samples = 0;
    
    ESP_LOGI(TAG, "Crossfade engine initialized (fade_duration=%lu ms, enabled=%d)",
             (unsigned long)s_fade_duration_ms, s_enabled);
    return ESP_OK;
}

esp_err_t sx_audio_crossfade_start(const int16_t *old_pcm, const int16_t *new_pcm, size_t sample_count) {
    if (!s_initialized || !s_enabled) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (old_pcm == NULL || new_pcm == NULL || sample_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_crossfade_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Calculate total fade samples based on sample rate
    s_total_fade_samples = (s_sample_rate * s_fade_duration_ms) / 1000;
    if (s_total_fade_samples > sample_count) {
        s_total_fade_samples = sample_count; // Limit to available samples
    }
    
    s_samples_processed = 0;
    s_state = SX_CROSSFADE_FADING_OUT;
    
    xSemaphoreGive(s_crossfade_mutex);
    
    ESP_LOGD(TAG, "Crossfade started (%zu samples, %lu fade samples)",
             sample_count, (unsigned long)s_total_fade_samples);
    
    return ESP_OK;
}

bool sx_audio_crossfade_process(int16_t *pcm, size_t sample_count) {
    if (!s_initialized || !s_enabled || s_state == SX_CROSSFADE_IDLE) {
        return false;
    }
    
    if (pcm == NULL || sample_count == 0) {
        return false;
    }
    
    if (xSemaphoreTake(s_crossfade_mutex, 0) != pdTRUE) {
        // If mutex is busy, skip crossfade for this chunk
        return (s_state != SX_CROSSFADE_IDLE);
    }
    
    bool still_active = false;
    
    if (s_state == SX_CROSSFADE_FADING_OUT || s_state == SX_CROSSFADE_FADING_IN) {
        // Calculate fade factor (0.0 to 1.0)
        float fade_progress = (float)s_samples_processed / (float)s_total_fade_samples;
        if (fade_progress > 1.0f) fade_progress = 1.0f;
        
        float old_gain;
        float new_gain __attribute__((unused)); // Reserved for future full crossfade implementation
        
        if (s_state == SX_CROSSFADE_FADING_OUT) {
            // Fade out: old_gain decreases from 1.0 to 0.0
            old_gain = 1.0f - fade_progress;
            new_gain = fade_progress;
            
            // Switch to fading in when halfway
            if (fade_progress >= 0.5f && s_state == SX_CROSSFADE_FADING_OUT) {
                s_state = SX_CROSSFADE_FADING_IN;
            }
        } else {
            // Fade in: new_gain increases from 0.5 to 1.0
            old_gain = 1.0f - fade_progress;
            new_gain = fade_progress;
        }
        
        // Apply crossfade to PCM samples
        for (size_t i = 0; i < sample_count && s_samples_processed < s_total_fade_samples; i++) {
            // For now, we only have one PCM buffer, so we apply fade out
            // In a full implementation, we would mix old_pcm and new_pcm
            pcm[i] = (int16_t)(pcm[i] * old_gain);
            s_samples_processed++;
            
            // Update fade progress
            fade_progress = (float)s_samples_processed / (float)s_total_fade_samples;
            if (fade_progress > 1.0f) fade_progress = 1.0f;
            
            if (s_state == SX_CROSSFADE_FADING_OUT && fade_progress >= 0.5f) {
                s_state = SX_CROSSFADE_FADING_IN;
                old_gain = 1.0f - fade_progress;
                new_gain = fade_progress;
            } else if (s_state == SX_CROSSFADE_FADING_IN) {
                old_gain = 1.0f - fade_progress;
                new_gain = fade_progress;
            }
        }
        
        // Check if crossfade is complete
        if (s_samples_processed >= s_total_fade_samples) {
            s_state = SX_CROSSFADE_COMPLETE;
            still_active = false;
            ESP_LOGD(TAG, "Crossfade complete");
        } else {
            still_active = true;
        }
    } else if (s_state == SX_CROSSFADE_COMPLETE) {
        // Crossfade complete, reset state
        s_state = SX_CROSSFADE_IDLE;
        s_samples_processed = 0;
        s_total_fade_samples = 0;
        still_active = false;
    }
    
    xSemaphoreGive(s_crossfade_mutex);
    return still_active;
}

bool sx_audio_crossfade_is_active(void) {
    return (s_state != SX_CROSSFADE_IDLE && s_state != SX_CROSSFADE_COMPLETE);
}

sx_audio_crossfade_state_t sx_audio_crossfade_get_state(void) {
    return s_state;
}

esp_err_t sx_audio_crossfade_stop(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_crossfade_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    s_state = SX_CROSSFADE_IDLE;
    s_samples_processed = 0;
    s_total_fade_samples = 0;
    
    xSemaphoreGive(s_crossfade_mutex);
    
    ESP_LOGD(TAG, "Crossfade stopped");
    return ESP_OK;
}

esp_err_t sx_audio_crossfade_set_enabled(bool enabled) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    s_enabled = enabled;
    ESP_LOGI(TAG, "Crossfade %s", enabled ? "enabled" : "disabled");
    return ESP_OK;
}

bool sx_audio_crossfade_is_enabled(void) {
    return s_enabled;
}

// Set sample rate (called when audio sample rate changes)
void sx_audio_crossfade_set_sample_rate(uint32_t sample_rate) {
    s_sample_rate = sample_rate;
    ESP_LOGD(TAG, "Crossfade sample rate set to %lu Hz", (unsigned long)sample_rate);
}



