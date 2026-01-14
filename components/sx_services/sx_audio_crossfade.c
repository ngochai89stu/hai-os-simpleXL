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

// Phase 2: Full crossfade implementation - buffers for old and new PCM
#define CROSSFADE_BUFFER_MAX_SAMPLES 4096  // Max samples to buffer (enough for ~250ms at 16kHz)
static int16_t *s_old_pcm_buffer = NULL;  // Old track buffer (fading out)
static int16_t *s_new_pcm_buffer = NULL;  // New track buffer (fading in)
static size_t s_crossfade_buffer_samples = 0;  // Size of buffers in samples
static size_t s_new_pcm_samples_available = 0;  // How many new PCM samples we have
static size_t s_new_pcm_samples_consumed = 0;   // How many new PCM samples we've used

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
    
    // Phase 2: Allocate buffers for full crossfade
    s_crossfade_buffer_samples = CROSSFADE_BUFFER_MAX_SAMPLES;
    s_old_pcm_buffer = (int16_t *)malloc(s_crossfade_buffer_samples * sizeof(int16_t));
    s_new_pcm_buffer = (int16_t *)malloc(s_crossfade_buffer_samples * sizeof(int16_t));
    
    if (s_old_pcm_buffer == NULL || s_new_pcm_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate crossfade buffers");
        if (s_old_pcm_buffer) free(s_old_pcm_buffer);
        if (s_new_pcm_buffer) free(s_new_pcm_buffer);
        s_old_pcm_buffer = NULL;
        s_new_pcm_buffer = NULL;
        vSemaphoreDelete(s_crossfade_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    s_initialized = true;
    s_state = SX_CROSSFADE_IDLE;
    s_samples_processed = 0;
    s_total_fade_samples = 0;
    s_new_pcm_samples_available = 0;
    s_new_pcm_samples_consumed = 0;
    
    ESP_LOGI(TAG, "Crossfade engine initialized (fade_duration=%lu ms, enabled=%d, buffer=%zu samples)",
             (unsigned long)s_fade_duration_ms, s_enabled, s_crossfade_buffer_samples);
    return ESP_OK;
}

esp_err_t sx_audio_crossfade_start(const int16_t *old_pcm, const int16_t *new_pcm, size_t sample_count) {
    if (!s_initialized || !s_enabled) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (old_pcm == NULL || new_pcm == NULL || sample_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (s_old_pcm_buffer == NULL || s_new_pcm_buffer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_crossfade_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Phase 2: Copy old and new PCM buffers
    size_t samples_to_copy = (sample_count > s_crossfade_buffer_samples) ? s_crossfade_buffer_samples : sample_count;
    memcpy(s_old_pcm_buffer, old_pcm, samples_to_copy * sizeof(int16_t));
    memcpy(s_new_pcm_buffer, new_pcm, samples_to_copy * sizeof(int16_t));
    
    // Calculate total fade samples based on sample rate
    s_total_fade_samples = (s_sample_rate * s_fade_duration_ms) / 1000;
    if (s_total_fade_samples > samples_to_copy) {
        s_total_fade_samples = samples_to_copy; // Limit to available samples
    }
    if (s_total_fade_samples == 0) {
        s_total_fade_samples = samples_to_copy; // At least fade the available samples
    }
    
    s_samples_processed = 0;
    s_new_pcm_samples_available = samples_to_copy;
    s_new_pcm_samples_consumed = 0;
    s_state = SX_CROSSFADE_FADING_OUT;
    
    xSemaphoreGive(s_crossfade_mutex);
    
    ESP_LOGD(TAG, "Crossfade started (%zu samples, %lu fade samples)",
             samples_to_copy, (unsigned long)s_total_fade_samples);
    
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
        // Phase 2: Full crossfade - mix old_pcm (fading out) and new_pcm (fading in)
        size_t samples_to_process = sample_count;
        if (samples_to_process > (s_total_fade_samples - s_samples_processed)) {
            samples_to_process = s_total_fade_samples - s_samples_processed;
        }
        if (samples_to_process > (s_new_pcm_samples_available - s_new_pcm_samples_consumed)) {
            samples_to_process = s_new_pcm_samples_available - s_new_pcm_samples_consumed;
        }
        
        for (size_t i = 0; i < samples_to_process && s_samples_processed < s_total_fade_samples; i++) {
            // Calculate gains for this sample
            fade_progress = (float)s_samples_processed / (float)s_total_fade_samples;
            if (fade_progress > 1.0f) fade_progress = 1.0f;
            
            // Use smooth curve (sine or linear) for better audio quality
            // Linear fade: old_gain = 1.0 - fade_progress, new_gain = fade_progress
            // Sine fade (smoother): old_gain = cos(fade_progress * PI/2), new_gain = sin(fade_progress * PI/2)
            // Using linear for now (can be optimized later)
            old_gain = 1.0f - fade_progress;
            new_gain = fade_progress;
            
            // Get samples from buffers
            int16_t old_sample = (s_samples_processed < s_crossfade_buffer_samples) ? 
                                 s_old_pcm_buffer[s_samples_processed] : 0;
            int16_t new_sample = (s_new_pcm_samples_consumed < s_new_pcm_samples_available) ?
                                 s_new_pcm_buffer[s_new_pcm_samples_consumed] : 0;
            
            // Mix: output = old * old_gain + new * new_gain
            float mixed = (float)old_sample * old_gain + (float)new_sample * new_gain;
            
            // Clamp to int16_t range
            if (mixed > 32767.0f) mixed = 32767.0f;
            if (mixed < -32768.0f) mixed = -32768.0f;
            
            pcm[i] = (int16_t)mixed;
            
            s_samples_processed++;
            s_new_pcm_samples_consumed++;
            
            // Switch to fading in when halfway (optional, for state tracking)
            if (fade_progress >= 0.5f && s_state == SX_CROSSFADE_FADING_OUT) {
                s_state = SX_CROSSFADE_FADING_IN;
            }
        }
        
        // If we didn't process all samples, fill remaining with new track (if available)
        if (samples_to_process < sample_count && s_new_pcm_samples_consumed < s_new_pcm_samples_available) {
            size_t remaining = sample_count - samples_to_process;
            size_t new_remaining = s_new_pcm_samples_available - s_new_pcm_samples_consumed;
            if (remaining > new_remaining) remaining = new_remaining;
            
            for (size_t i = samples_to_process; i < samples_to_process + remaining; i++) {
                pcm[i] = s_new_pcm_buffer[s_new_pcm_samples_consumed++];
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
    s_new_pcm_samples_available = 0;
    s_new_pcm_samples_consumed = 0;
    
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



