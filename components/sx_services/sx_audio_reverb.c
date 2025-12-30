#include "sx_audio_reverb.h"
#include "sx_settings_service.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define REVERB_SETTINGS_KEY_LEVEL "reverb_level"
#define REVERB_SETTINGS_KEY_ENABLED "reverb_enabled"

static const char *TAG = "sx_audio_reverb";

// Reverb state
static bool s_initialized = false;
static bool s_enabled = true;
static uint32_t s_sample_rate = 16000;
static uint8_t s_reverb_level = 0;  // 0-100

// Simple delay-based reverb implementation
// Uses multiple delay lines with feedback for reverb effect
#define REVERB_DELAY_LINES 4
#define REVERB_MAX_DELAY_SAMPLES 4800  // Max delay ~300ms at 16kHz

typedef struct {
    int16_t *delay_buffer;
    size_t delay_samples;
    size_t write_pos;
    float feedback;
    float mix;
} reverb_delay_line_t;

static reverb_delay_line_t s_delay_lines[REVERB_DELAY_LINES] = {0};

// Initialize delay line
static esp_err_t init_delay_line(reverb_delay_line_t *line, size_t delay_samples, float feedback, float mix) {
    if (line->delay_buffer != NULL) {
        free(line->delay_buffer);
    }
    
    line->delay_buffer = (int16_t *)malloc(delay_samples * sizeof(int16_t));
    if (line->delay_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate delay buffer");
        return ESP_ERR_NO_MEM;
    }
    
    memset(line->delay_buffer, 0, delay_samples * sizeof(int16_t));
    line->delay_samples = delay_samples;
    line->write_pos = 0;
    line->feedback = feedback;
    line->mix = mix;
    
    return ESP_OK;
}

// Process sample through delay line
static int16_t process_delay_line(reverb_delay_line_t *line, int16_t input) {
    // Read from delay buffer (read position is delay_samples behind write position)
    size_t read_pos = (line->write_pos >= line->delay_samples) ? 
                      (line->write_pos - line->delay_samples) : 
                      (line->write_pos + line->delay_samples - line->delay_samples);
    if (read_pos >= line->delay_samples) {
        read_pos = 0; // Safety check
    }
    int16_t delayed = line->delay_buffer[read_pos];
    
    // Mix input with delayed signal
    float output = (float)input + (float)delayed * line->mix;
    
    // Write to delay buffer with feedback
    float feedback_sample = (float)input + (float)delayed * line->feedback;
    line->delay_buffer[line->write_pos] = (int16_t)feedback_sample;
    
    // Update write position
    line->write_pos = (line->write_pos + 1) % line->delay_samples;
    
    // Clamp output
    if (output > 32767.0f) output = 32767.0f;
    if (output < -32768.0f) output = -32768.0f;
    
    return (int16_t)output;
}

// Update delay lines based on reverb level
static void update_delay_lines(void) {
    if (!s_initialized) {
        return;
    }
    
    // Calculate delay times (in samples) - different for each line
    // Use milliseconds converted to samples
    size_t delays[REVERB_DELAY_LINES] = {
        (size_t)(s_sample_rate * 0.030f),  // 30ms
        (size_t)(s_sample_rate * 0.050f),  // 50ms
        (size_t)(s_sample_rate * 0.070f),   // 70ms
        (size_t)(s_sample_rate * 0.090f)   // 90ms
    };
    
    // Calculate feedback and mix based on reverb level (0-100)
    float level_factor = (float)s_reverb_level / 100.0f;
    float feedback = 0.3f * level_factor;  // Max 30% feedback
    float mix = 0.2f * level_factor;       // Max 20% mix
    
    for (int i = 0; i < REVERB_DELAY_LINES; i++) {
        size_t delay_samples = delays[i];
        if (delay_samples > REVERB_MAX_DELAY_SAMPLES) {
            delay_samples = REVERB_MAX_DELAY_SAMPLES;
        }
        
        // Slightly different feedback for each line
        float line_feedback = feedback * (0.8f + 0.4f * (float)i / REVERB_DELAY_LINES);
        float line_mix = mix * (0.7f + 0.6f * (float)i / REVERB_DELAY_LINES);
        
        init_delay_line(&s_delay_lines[i], delay_samples, line_feedback, line_mix);
    }
}

esp_err_t sx_audio_reverb_init(uint32_t sample_rate_hz) {
    if (s_initialized) {
        ESP_LOGW(TAG, "Reverb already initialized, updating sample rate");
        return sx_audio_reverb_set_sample_rate(sample_rate_hz);
    }
    
    ESP_LOGI(TAG, "Initializing audio reverb (sample rate: %u Hz)", sample_rate_hz);
    
    s_sample_rate = sample_rate_hz;
    
    // Load settings from NVS
    int32_t level_int = 0;
    sx_settings_get_int_default(REVERB_SETTINGS_KEY_LEVEL, &level_int, 0);
    if (level_int >= 0 && level_int <= 100) {
        s_reverb_level = (uint8_t)level_int;
    } else {
        s_reverb_level = 0;
    }
    
    bool enabled = true;
    sx_settings_get_bool_default(REVERB_SETTINGS_KEY_ENABLED, &enabled, true);
    s_enabled = enabled;
    
    // Initialize delay lines
    update_delay_lines();
    
    s_initialized = true;
    ESP_LOGI(TAG, "Audio reverb initialized: level=%d%%, enabled=%d", s_reverb_level, s_enabled);
    
    return ESP_OK;
}

void sx_audio_reverb_deinit(void) {
    if (!s_initialized) {
        return;
    }
    
    // Free delay buffers
    for (int i = 0; i < REVERB_DELAY_LINES; i++) {
        if (s_delay_lines[i].delay_buffer != NULL) {
            free(s_delay_lines[i].delay_buffer);
            s_delay_lines[i].delay_buffer = NULL;
        }
    }
    
    s_initialized = false;
    ESP_LOGI(TAG, "Audio reverb deinitialized");
}

esp_err_t sx_audio_reverb_set_level(uint8_t level) {
    if (level > 100) {
        return ESP_ERR_INVALID_ARG;
    }
    
    s_reverb_level = level;
    
    // Update delay lines with new level
    if (s_initialized) {
        update_delay_lines();
    }
    
    // Save to settings
    sx_settings_set_int(REVERB_SETTINGS_KEY_LEVEL, (int32_t)level);
    sx_settings_commit();
    
    ESP_LOGI(TAG, "Reverb level set to %d%%", level);
    return ESP_OK;
}

uint8_t sx_audio_reverb_get_level(void) {
    return s_reverb_level;
}

esp_err_t sx_audio_reverb_enable(bool enable) {
    s_enabled = enable;
    
    // Save to settings
    sx_settings_set_bool(REVERB_SETTINGS_KEY_ENABLED, enable);
    sx_settings_commit();
    
    ESP_LOGI(TAG, "Reverb %s", enable ? "enabled" : "disabled");
    return ESP_OK;
}

bool sx_audio_reverb_is_enabled(void) {
    return s_initialized && s_enabled;
}

esp_err_t sx_audio_reverb_process(int16_t *samples, size_t sample_count) {
    if (!s_initialized || !s_enabled || samples == NULL || sample_count == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_reverb_level == 0) {
        // No reverb, just pass through
        return ESP_OK;
    }
    
    // Process stereo interleaved samples
    for (size_t i = 0; i < sample_count; i += 2) {
        if (i + 1 >= sample_count) {
            break; // Need stereo pair
        }
        
        int16_t left = samples[i];
        int16_t right = samples[i + 1];
        
        // Process through delay lines (simple: process left channel through all lines)
        int16_t reverb_left = left;
        int16_t reverb_right = right;
        
        for (int j = 0; j < REVERB_DELAY_LINES; j++) {
            // Process left channel
            reverb_left = process_delay_line(&s_delay_lines[j], reverb_left);
            
            // For stereo, process right channel through same lines with slight variation
            if (j < REVERB_DELAY_LINES / 2) {
                reverb_right = process_delay_line(&s_delay_lines[j + REVERB_DELAY_LINES / 2], reverb_right);
            }
        }
        
        // Mix original with reverb
        float level_factor = (float)s_reverb_level / 100.0f;
        float dry = 1.0f - (level_factor * 0.3f);  // Keep 70-100% of original
        float wet = level_factor * 0.3f;           // Add 0-30% reverb
        
        float out_left = (float)left * dry + (float)reverb_left * wet;
        float out_right = (float)right * dry + (float)reverb_right * wet;
        
        // Clamp
        if (out_left > 32767.0f) out_left = 32767.0f;
        if (out_left < -32768.0f) out_left = -32768.0f;
        if (out_right > 32767.0f) out_right = 32767.0f;
        if (out_right < -32768.0f) out_right = -32768.0f;
        
        samples[i] = (int16_t)out_left;
        samples[i + 1] = (int16_t)out_right;
    }
    
    return ESP_OK;
}

esp_err_t sx_audio_reverb_set_sample_rate(uint32_t sample_rate_hz) {
    if (sample_rate_hz == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    s_sample_rate = sample_rate_hz;
    
    // Re-initialize delay lines with new sample rate
    if (s_initialized) {
        update_delay_lines();
    }
    
    ESP_LOGI(TAG, "Reverb sample rate updated to %u Hz", sample_rate_hz);
    return ESP_OK;
}

