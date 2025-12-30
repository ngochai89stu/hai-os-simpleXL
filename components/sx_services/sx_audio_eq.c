#include "sx_audio_eq.h"
#include "sx_settings_service.h"
#include <esp_log.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define EQ_SETTINGS_KEY_PRESET "eq_preset"
#define EQ_SETTINGS_KEY_ENABLED "eq_enabled"
#define EQ_SETTINGS_KEY_BANDS "eq_bands"

static const char *TAG = "sx_audio_eq";

// EQ preset values (in 0.1dB units, so 20 = 2.0dB)
static const int16_t s_eq_presets[SX_AUDIO_EQ_PRESET_MAX][SX_AUDIO_EQ_NUM_BANDS] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           // Flat
    {20, 40, 60, 40, 20, 0, -20, -40, -20, 0},        // Pop
    {40, 20, -20, -40, -20, 20, 40, 60, 40, 20},       // Rock
    {0, 20, 40, 20, 0, -20, -40, -20, 0, 20},       // Jazz
    {-20, -40, -20, 0, 20, 40, 60, 40, 20, 0},       // Classical
};

// EQ state
static bool s_initialized = false;
static bool s_enabled = true;
static uint32_t s_sample_rate = 16000;
static int16_t s_band_gains[SX_AUDIO_EQ_NUM_BANDS] = {0};
static sx_audio_eq_preset_t s_current_preset = SX_AUDIO_EQ_PRESET_FLAT;

// Simple biquad filter structure for each band
typedef struct {
    float b0, b1, b2;  // Numerator coefficients
    float a1, a2;      // Denominator coefficients (a0 = 1.0)
    // Separate history for left and right channels
    float x1_l, x2_l;  // Left channel input history
    float y1_l, y2_l;  // Left channel output history
    float x1_r, x2_r;  // Right channel input history
    float y1_r, y2_r;  // Right channel output history
} biquad_filter_t;

// EQ filter bank (one biquad per band)
static biquad_filter_t s_filters[SX_AUDIO_EQ_NUM_BANDS] = {0};

// Band center frequencies (Hz)
static const float s_band_frequencies[SX_AUDIO_EQ_NUM_BANDS] = {
    31.0f, 62.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
};

// Calculate biquad coefficients for a peaking EQ filter
static void calculate_biquad_coefficients(int band, float gain_db, float q, biquad_filter_t *filter) {
    float freq = s_band_frequencies[band];
    float w = 2.0f * M_PI * freq / s_sample_rate;
    float cos_w = cosf(w);
    float sin_w = sinf(w);
    
    float A = powf(10.0f, gain_db / 40.0f);  // Amplitude
    float alpha = sin_w / (2.0f * q);
    float b0 = 1.0f + alpha * A;
    float b1 = -2.0f * cos_w;
    float b2 = 1.0f - alpha * A;
    float a0 = 1.0f + alpha / A;
    float a1 = -2.0f * cos_w;
    float a2 = 1.0f - alpha / A;
    
    // Normalize coefficients (divide by a0)
    filter->b0 = b0 / a0;
    filter->b1 = b1 / a0;
    filter->b2 = b2 / a0;
    filter->a1 = a1 / a0;
    filter->a2 = a2 / a0;
}

// Process one sample through a biquad filter (left channel)
static float process_biquad_left(biquad_filter_t *filter, float input) {
    float output = filter->b0 * input + filter->b1 * filter->x1_l + filter->b2 * filter->x2_l
                   - filter->a1 * filter->y1_l - filter->a2 * filter->y2_l;
    
    // Update history
    filter->x2_l = filter->x1_l;
    filter->x1_l = input;
    filter->y2_l = filter->y1_l;
    filter->y1_l = output;
    
    return output;
}

// Process one sample through a biquad filter (right channel)
static float process_biquad_right(biquad_filter_t *filter, float input) {
    float output = filter->b0 * input + filter->b1 * filter->x1_r + filter->b2 * filter->x2_r
                   - filter->a1 * filter->y1_r - filter->a2 * filter->y2_r;
    
    // Update history
    filter->x2_r = filter->x1_r;
    filter->x1_r = input;
    filter->y2_r = filter->y1_r;
    filter->y1_r = output;
    
    return output;
}

// Update all filter coefficients based on current gains
static void update_filters(void) {
    for (int i = 0; i < SX_AUDIO_EQ_NUM_BANDS; i++) {
        float gain_db = s_band_gains[i] / 10.0f;  // Convert from 0.1dB units to dB
        float q = 1.0f;  // Quality factor (bandwidth)
        calculate_biquad_coefficients(i, gain_db, q, &s_filters[i]);
    }
}

esp_err_t sx_audio_eq_init(uint32_t sample_rate_hz) {
    if (s_initialized) {
        ESP_LOGW(TAG, "EQ already initialized, updating sample rate");
        return sx_audio_eq_set_sample_rate(sample_rate_hz);
    }
    
    ESP_LOGI(TAG, "Initializing audio EQ (sample rate: %u Hz)", sample_rate_hz);
    
    s_sample_rate = sample_rate_hz;
    
    // Load settings from NVS
    int32_t preset_int = SX_AUDIO_EQ_PRESET_FLAT;
    sx_settings_get_int_default(EQ_SETTINGS_KEY_PRESET, &preset_int, SX_AUDIO_EQ_PRESET_FLAT);
    if (preset_int >= 0 && preset_int < SX_AUDIO_EQ_PRESET_MAX) {
        s_current_preset = (sx_audio_eq_preset_t)preset_int;
    } else {
        s_current_preset = SX_AUDIO_EQ_PRESET_FLAT;
    }
    
    bool enabled = true;
    sx_settings_get_bool_default(EQ_SETTINGS_KEY_ENABLED, &enabled, true);
    s_enabled = enabled;
    
    // Load band gains from settings
    size_t blob_size = 0;
    if (sx_settings_get_blob_size(EQ_SETTINGS_KEY_BANDS, &blob_size) == ESP_OK && 
        blob_size == sizeof(s_band_gains)) {
        if (sx_settings_get_blob(EQ_SETTINGS_KEY_BANDS, s_band_gains, &blob_size) == ESP_OK) {
            ESP_LOGI(TAG, "Loaded EQ band gains from settings");
        } else {
            // Failed to load, use default (flat)
            memset(s_band_gains, 0, sizeof(s_band_gains));
        }
    } else {
        // No saved settings, use preset or default (flat)
        if (s_current_preset != SX_AUDIO_EQ_PRESET_CUSTOM) {
            memcpy(s_band_gains, s_eq_presets[s_current_preset], sizeof(s_band_gains));
        } else {
            memset(s_band_gains, 0, sizeof(s_band_gains));
        }
    }
    
    // Initialize filters
    memset(s_filters, 0, sizeof(s_filters));
    update_filters();
    
    s_initialized = true;
    ESP_LOGI(TAG, "Audio EQ initialized (preset: %d, enabled: %d)", s_current_preset, s_enabled);
    return ESP_OK;
}

void sx_audio_eq_deinit(void) {
    if (!s_initialized) {
        return;
    }
    
    s_initialized = false;
    s_enabled = false;
    memset(s_filters, 0, sizeof(s_filters));
    ESP_LOGI(TAG, "Audio EQ deinitialized");
}

esp_err_t sx_audio_eq_set_band(int band, int16_t gain_db) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (band < 0 || band >= SX_AUDIO_EQ_NUM_BANDS) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (gain_db < SX_AUDIO_EQ_GAIN_MIN || gain_db > SX_AUDIO_EQ_GAIN_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    s_band_gains[band] = gain_db;
    s_current_preset = SX_AUDIO_EQ_PRESET_CUSTOM;
    
    // Update filter coefficients for this band
    float gain_db_float = gain_db / 10.0f;
    float q = 1.0f;
    calculate_biquad_coefficients(band, gain_db_float, q, &s_filters[band]);
    
    // Save to settings
    sx_settings_set_blob(EQ_SETTINGS_KEY_BANDS, s_band_gains, sizeof(s_band_gains));
    sx_settings_set_int(EQ_SETTINGS_KEY_PRESET, (int32_t)s_current_preset);
    sx_settings_commit();
    
    ESP_LOGD(TAG, "Set EQ band %d to %.1f dB", band, gain_db / 10.0f);
    return ESP_OK;
}

int16_t sx_audio_eq_get_band(int band) {
    if (!s_initialized || band < 0 || band >= SX_AUDIO_EQ_NUM_BANDS) {
        return 0;
    }
    return s_band_gains[band];
}

esp_err_t sx_audio_eq_set_bands(const int16_t gains[SX_AUDIO_EQ_NUM_BANDS]) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (gains == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    for (int i = 0; i < SX_AUDIO_EQ_NUM_BANDS; i++) {
        if (gains[i] < SX_AUDIO_EQ_GAIN_MIN || gains[i] > SX_AUDIO_EQ_GAIN_MAX) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    
    memcpy(s_band_gains, gains, sizeof(s_band_gains));
    s_current_preset = SX_AUDIO_EQ_PRESET_CUSTOM;
    update_filters();
    
    // Save to settings
    sx_settings_set_blob(EQ_SETTINGS_KEY_BANDS, s_band_gains, sizeof(s_band_gains));
    sx_settings_set_int(EQ_SETTINGS_KEY_PRESET, (int32_t)s_current_preset);
    sx_settings_commit();
    
    ESP_LOGD(TAG, "Set all EQ bands");
    return ESP_OK;
}

esp_err_t sx_audio_eq_get_bands(int16_t gains[SX_AUDIO_EQ_NUM_BANDS]) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (gains == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(gains, s_band_gains, sizeof(s_band_gains));
    return ESP_OK;
}

esp_err_t sx_audio_eq_set_preset(sx_audio_eq_preset_t preset) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (preset < 0 || preset >= SX_AUDIO_EQ_PRESET_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (preset == SX_AUDIO_EQ_PRESET_CUSTOM) {
        // Keep current custom settings
        s_current_preset = preset;
        // Still save preset to settings
        sx_settings_set_int(EQ_SETTINGS_KEY_PRESET, (int32_t)preset);
        sx_settings_commit();
        return ESP_OK;
    }
    
    memcpy(s_band_gains, s_eq_presets[preset], sizeof(s_band_gains));
    s_current_preset = preset;
    update_filters();
    
    // Save to settings
    sx_settings_set_blob(EQ_SETTINGS_KEY_BANDS, s_band_gains, sizeof(s_band_gains));
    sx_settings_set_int(EQ_SETTINGS_KEY_PRESET, (int32_t)preset);
    sx_settings_commit();
    
    ESP_LOGI(TAG, "Set EQ preset: %d", preset);
    return ESP_OK;
}

sx_audio_eq_preset_t sx_audio_eq_get_preset(void) {
    return s_current_preset;
}

esp_err_t sx_audio_eq_enable(bool enable) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    s_enabled = enable;
    
    // Save to settings
    sx_settings_set_bool(EQ_SETTINGS_KEY_ENABLED, enable);
    sx_settings_commit();
    
    ESP_LOGI(TAG, "EQ %s", enable ? "enabled" : "disabled");
    return ESP_OK;
}

bool sx_audio_eq_is_enabled(void) {
    return s_initialized && s_enabled;
}

esp_err_t sx_audio_eq_process(int16_t *samples, size_t sample_count) {
    if (!s_initialized || !s_enabled || samples == NULL || sample_count == 0) {
        return ESP_OK;  // No processing needed
    }
    
    // Process each sample through all EQ bands
    // For stereo interleaved: process left and right channels separately with separate history
    for (size_t i = 0; i < sample_count; i += 2) {
        float left = (float)samples[i];
        float right = (float)samples[i + 1];
        
        // Process left channel through all bands (cascade)
        for (int band = 0; band < SX_AUDIO_EQ_NUM_BANDS; band++) {
            left = process_biquad_left(&s_filters[band], left);
        }
        
        // Process right channel through all bands with separate history
        for (int band = 0; band < SX_AUDIO_EQ_NUM_BANDS; band++) {
            right = process_biquad_right(&s_filters[band], right);
        }
        
        // Clamp and convert back to int16_t
        if (left > 32767.0f) left = 32767.0f;
        if (left < -32768.0f) left = -32768.0f;
        if (right > 32767.0f) right = 32767.0f;
        if (right < -32768.0f) right = -32768.0f;
        
        samples[i] = (int16_t)left;
        samples[i + 1] = (int16_t)right;
    }
    
    return ESP_OK;
}

esp_err_t sx_audio_eq_set_sample_rate(uint32_t sample_rate_hz) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (sample_rate_hz == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    s_sample_rate = sample_rate_hz;
    
    // Reset filter history (both left and right channels)
    memset(s_filters, 0, sizeof(s_filters));
    
    // Recalculate all filter coefficients
    update_filters();
    
    ESP_LOGI(TAG, "EQ sample rate updated to %u Hz", sample_rate_hz);
    return ESP_OK;
}

