#include "sx_audio_profiler.h"
#include <esp_log.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

static const char *TAG = "sx_profiler";

// Profiler state
static bool s_initialized = false;
static bool s_enabled = false;
static sx_profiler_stats_t s_stats = {0};
static SemaphoreHandle_t s_mutex = NULL;

// Stage names
static const char *s_stage_names[SX_PROFILER_STAGE_COUNT] = {
    "DECODE",
    "EQ",
    "CROSSFADE",
    "VOLUME",
    "I2S",
    "HTTP_READ",
    "BUFFER",
};

esp_err_t sx_audio_profiler_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        return ESP_ERR_NO_MEM;
    }
    
    memset(&s_stats, 0, sizeof(s_stats));
    s_enabled = false;
    s_initialized = true;
    
    ESP_LOGI(TAG, "Audio profiler initialized");
    return ESP_OK;
}

void sx_audio_profiler_deinit(void) {
    if (!s_initialized) {
        return;
    }
    
    if (s_mutex) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }
    
    s_initialized = false;
    ESP_LOGI(TAG, "Audio profiler deinitialized");
}

void sx_audio_profiler_enable(bool enable) {
    if (!s_initialized) {
        return;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    
    s_enabled = enable;
    
    if (enable) {
        sx_audio_profiler_reset_stats();
    }
    
    xSemaphoreGive(s_mutex);
    
    ESP_LOGI(TAG, "Audio profiler %s", enable ? "enabled" : "disabled");
}

bool sx_audio_profiler_is_enabled(void) {
    if (!s_initialized) {
        return false;
    }
    
    return s_enabled;
}

uint32_t sx_audio_profiler_start_timing(sx_profiler_stage_t stage) {
    if (!s_initialized || !s_enabled || stage >= SX_PROFILER_STAGE_COUNT) {
        return 0;
    }
    
    return (uint32_t)esp_timer_get_time();
}

void sx_audio_profiler_end_timing(sx_profiler_stage_t stage, uint32_t handle) {
    if (!s_initialized || !s_enabled || stage >= SX_PROFILER_STAGE_COUNT || handle == 0) {
        return;
    }
    
    int64_t start_time = (int64_t)handle;
    int64_t end_time = esp_timer_get_time();
    uint64_t duration_us = (uint64_t)(end_time - start_time);
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    
    sx_profiler_metrics_t *metrics = &s_stats.stages[stage];
    metrics->total_time_us += duration_us;
    metrics->call_count++;
    metrics->last_time_us = duration_us;
    
    if (metrics->call_count == 1) {
        metrics->max_time_us = duration_us;
        metrics->min_time_us = duration_us;
    } else {
        if (duration_us > metrics->max_time_us) {
            metrics->max_time_us = duration_us;
        }
        if (duration_us < metrics->min_time_us) {
            metrics->min_time_us = duration_us;
        }
    }
    
    s_stats.total_processing_time_us += duration_us;
    
    xSemaphoreGive(s_mutex);
}

void sx_audio_profiler_record_samples(uint32_t samples) {
    if (!s_initialized || !s_enabled) {
        return;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    
    s_stats.total_audio_samples += samples;
    
    xSemaphoreGive(s_mutex);
}

esp_err_t sx_audio_profiler_get_stats(sx_profiler_stats_t *stats) {
    if (!s_initialized || !stats) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    *stats = s_stats;
    stats->enabled = s_enabled;
    
    xSemaphoreGive(s_mutex);
    
    return ESP_OK;
}

void sx_audio_profiler_reset_stats(void) {
    if (!s_initialized) {
        return;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    
    memset(&s_stats, 0, sizeof(s_stats));
    s_stats.enabled = s_enabled;
    
    xSemaphoreGive(s_mutex);
    
    ESP_LOGI(TAG, "Profiler statistics reset");
}

const char *sx_audio_profiler_stage_name(sx_profiler_stage_t stage) {
    if (stage >= SX_PROFILER_STAGE_COUNT) {
        return "UNKNOWN";
    }
    return s_stage_names[stage];
}

void sx_audio_profiler_print_stats(void) {
    if (!s_initialized || !s_enabled) {
        ESP_LOGW(TAG, "Profiler not enabled");
        return;
    }
    
    sx_profiler_stats_t stats;
    if (sx_audio_profiler_get_stats(&stats) != ESP_OK) {
        return;
    }
    
    ESP_LOGI(TAG, "=== Audio Pipeline Profiler Statistics ===");
    ESP_LOGI(TAG, "Total samples: %llu", (unsigned long long)stats.total_audio_samples);
    ESP_LOGI(TAG, "Total processing time: %llu us", (unsigned long long)stats.total_processing_time_us);
    
    if (stats.total_processing_time_us > 0 && stats.total_audio_samples > 0) {
        uint64_t avg_time_per_sample = stats.total_processing_time_us / stats.total_audio_samples;
        ESP_LOGI(TAG, "Average time per sample: %llu us", (unsigned long long)avg_time_per_sample);
    }
    
    ESP_LOGI(TAG, "--- Stage Metrics ---");
    for (int i = 0; i < SX_PROFILER_STAGE_COUNT; i++) {
        sx_profiler_metrics_t *m = &stats.stages[i];
        if (m->call_count > 0) {
            uint64_t avg_time = m->total_time_us / m->call_count;
            float percentage = 0.0f;
            if (stats.total_processing_time_us > 0) {
                percentage = (float)m->total_time_us * 100.0f / (float)stats.total_processing_time_us;
            }
            
            ESP_LOGI(TAG, "  %s:", s_stage_names[i]);
            ESP_LOGI(TAG, "    Calls: %llu", (unsigned long long)m->call_count);
            ESP_LOGI(TAG, "    Total: %llu us (%.2f%%)", (unsigned long long)m->total_time_us, percentage);
            ESP_LOGI(TAG, "    Avg: %llu us", (unsigned long long)avg_time);
            ESP_LOGI(TAG, "    Min: %llu us, Max: %llu us", 
                    (unsigned long long)m->min_time_us, (unsigned long long)m->max_time_us);
        }
    }
    ESP_LOGI(TAG, "===========================================");
}

uint32_t sx_audio_profiler_detect_bottlenecks(uint8_t threshold_percent) {
    if (!s_initialized || !s_enabled || threshold_percent > 100) {
        return 0;
    }
    
    sx_profiler_stats_t stats;
    if (sx_audio_profiler_get_stats(&stats) != ESP_OK) {
        return 0;
    }
    
    if (stats.total_processing_time_us == 0) {
        return 0;
    }
    
    uint32_t bottlenecks = 0;
    
    for (int i = 0; i < SX_PROFILER_STAGE_COUNT; i++) {
        sx_profiler_metrics_t *m = &stats.stages[i];
        if (m->call_count > 0) {
            float percentage = (float)m->total_time_us * 100.0f / (float)stats.total_processing_time_us;
            if (percentage >= threshold_percent) {
                bottlenecks |= (1U << i);
                ESP_LOGW(TAG, "Bottleneck detected: %s (%.2f%% of total time)",
                         s_stage_names[i], percentage);
            }
        }
    }
    
    return bottlenecks;
}




