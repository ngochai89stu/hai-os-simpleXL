#include "sx_metrics.h"

#include <string.h>
#include <stdio.h>  // for FILE operations
#include <sys/param.h>
#include <unistd.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>  // Phase 2: xTaskCreate, xTaskGetTickCount, vTaskDelayUntil
#include <freertos/semphr.h>
#include <esp_heap_caps.h>

static const char *TAG = "sx_metrics";

// P2.6: Metrics storage (Section 9.1 SIMPLEXL_ARCH v1.3)
static sx_metrics_t s_metrics;  // Global metrics

// Forward declaration for internal helper
static void metrics_write_kv(FILE *f, const char *key, uint32_t value);
static StaticSemaphore_t s_metrics_mutex_buf;
static SemaphoreHandle_t s_metrics_mutex = NULL;

static void init_mutex(void) {
    if (s_metrics_mutex == NULL) {
        s_metrics_mutex = xSemaphoreCreateMutexStatic(&s_metrics_mutex_buf);
    }
}

// Phase 2: Periodic metrics update task
static void sx_metrics_update_task(void *arg) {
    (void)arg;
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(5000);  // Update every 5 seconds
    
    ESP_LOGI(TAG, "Metrics update task started");
    
    for (;;) {
        // Update heap metrics
        size_t heap_free = esp_get_free_heap_size();
        size_t heap_min = esp_get_minimum_free_heap_size();
        sx_metrics_update_heap(heap_free, heap_min);
        
        #ifdef CONFIG_SPIRAM_SUPPORT
        // Update PSRAM metrics
        size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        // Note: ESP-IDF doesn't have get_minimum_free_psram, so we track current as min
        // In practice, we can use largest_free_block as approximation
        size_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
        sx_metrics_update_psram(psram_free, psram_free - psram_largest);  // Approximate min
        #endif
        
        vTaskDelayUntil(&last_wake_time, interval);
    }
}

bool sx_metrics_init(void) {
    init_mutex();
    if (s_metrics_mutex == NULL) {
        return false;
    }
    
    memset(&s_metrics, 0, sizeof(s_metrics));
    
    // Initialize heap/PSRAM metrics
    size_t heap_free = esp_get_free_heap_size();
    s_metrics.heap_free_current = heap_free;
    s_metrics.heap_free_min = heap_free;
    
    #ifdef CONFIG_SPIRAM_SUPPORT
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    s_metrics.psram_free_current = psram_free;
    s_metrics.psram_free_min = psram_free;
    #else
    s_metrics.psram_free_current = 0;
    s_metrics.psram_free_min = 0;
    #endif
    
    // Phase 2: Start periodic update task (low priority, runs every 5 seconds)
    BaseType_t ret = xTaskCreate(sx_metrics_update_task, "metrics_upd", 2048, NULL, 1, NULL);
    if (ret != pdPASS) {
        ESP_LOGW(TAG, "Failed to create metrics update task (non-critical)");
    } else {
        ESP_LOGI(TAG, "Metrics update task created");
    }
    
    ESP_LOGI(TAG, "Metrics system initialized");
    return true;
}

void sx_metrics_get(sx_metrics_t *out_metrics) {
    if (!out_metrics || s_metrics_mutex == NULL) {
        return;
    }
    
    xSemaphoreTake(s_metrics_mutex, portMAX_DELAY);
    *out_metrics = s_metrics;
    xSemaphoreGive(s_metrics_mutex);
}

void sx_metrics_reset(void) {
    if (s_metrics_mutex == NULL) {
        return;
    }
    
    xSemaphoreTake(s_metrics_mutex, portMAX_DELAY);
    memset(&s_metrics, 0, sizeof(s_metrics));
    
    // Reset heap/PSRAM min values to current
    size_t heap_free = esp_get_free_heap_size();
    s_metrics.heap_free_current = heap_free;
    s_metrics.heap_free_min = heap_free;
    
    #ifdef CONFIG_SPIRAM_SUPPORT
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    s_metrics.psram_free_current = psram_free;
    s_metrics.psram_free_min = psram_free;
    #endif
    
    xSemaphoreGive(s_metrics_mutex);
    ESP_LOGI(TAG, "Metrics reset");
}

void sx_metrics_inc_evt_posted(uint32_t priority) {
    if (priority >= 4 || s_metrics_mutex == NULL) {
        return;
    }
    
    xSemaphoreTake(s_metrics_mutex, portMAX_DELAY);
    s_metrics.evt_posted_total[priority]++;
    xSemaphoreGive(s_metrics_mutex);
}

void sx_metrics_inc_evt_dropped(uint32_t priority) {
    if (priority >= 4 || s_metrics_mutex == NULL) {
        return;
    }
    
    xSemaphoreTake(s_metrics_mutex, portMAX_DELAY);
    s_metrics.evt_dropped_total[priority]++;
    xSemaphoreGive(s_metrics_mutex);
}

void sx_metrics_inc_evt_coalesced(uint32_t priority) {
    if (priority >= 4 || s_metrics_mutex == NULL) {
        return;
    }
    
    xSemaphoreTake(s_metrics_mutex, portMAX_DELAY);
    s_metrics.evt_coalesced_total[priority]++;
    xSemaphoreGive(s_metrics_mutex);
}

void sx_metrics_set_queue_depth(uint32_t priority, uint32_t depth) {
    if (priority >= 4 || s_metrics_mutex == NULL) {
        return;
    }
    
    xSemaphoreTake(s_metrics_mutex, portMAX_DELAY);
    s_metrics.queue_depth[priority] = depth;
    xSemaphoreGive(s_metrics_mutex);
}

void sx_metrics_set_state_version(uint32_t version) {
    if (s_metrics_mutex == NULL) {
        return;
    }
    
    xSemaphoreTake(s_metrics_mutex, portMAX_DELAY);
    s_metrics.state_version = version;
    xSemaphoreGive(s_metrics_mutex);
}

void sx_metrics_inc_state_updates(void) {
    if (s_metrics_mutex == NULL) {
        return;
    }
    
    xSemaphoreTake(s_metrics_mutex, portMAX_DELAY);
    s_metrics.state_updates_total++;
    xSemaphoreGive(s_metrics_mutex);
}

void sx_metrics_update_ui_render(uint32_t render_ms) {
    if (s_metrics_mutex == NULL) {
        return;
    }
    
    xSemaphoreTake(s_metrics_mutex, portMAX_DELAY);
    s_metrics.ui_render_ms_last = render_ms;
    
    // Update average (simple moving average)
    if (s_metrics.ui_frames_total == 0) {
        s_metrics.ui_render_ms_avg = render_ms;
    } else {
        s_metrics.ui_render_ms_avg = (s_metrics.ui_render_ms_avg + render_ms) / 2;
    }
    
    // Update max
    if (render_ms > s_metrics.ui_render_ms_max) {
        s_metrics.ui_render_ms_max = render_ms;
    }
    
    s_metrics.ui_frames_total++;
    xSemaphoreGive(s_metrics_mutex);
}

void sx_metrics_update_heap(uint32_t free_current, uint32_t free_min) {
    if (s_metrics_mutex == NULL) {
        return;
    }
    
    xSemaphoreTake(s_metrics_mutex, portMAX_DELAY);
    s_metrics.heap_free_current = free_current;
    if (free_min < s_metrics.heap_free_min) {
        s_metrics.heap_free_min = free_min;
    }
    xSemaphoreGive(s_metrics_mutex);
}

void sx_metrics_update_psram(uint32_t free_current, uint32_t free_min) {
    if (s_metrics_mutex == NULL) {
        return;
    }
    
    xSemaphoreTake(s_metrics_mutex, portMAX_DELAY);
    s_metrics.psram_free_current = free_current;
    if (free_min < s_metrics.psram_free_min) {
        s_metrics.psram_free_min = free_min;
    }
    xSemaphoreGive(s_metrics_mutex);
}

// Phase-3: Audio helpers implementation
void sx_metrics_inc_audio_underrun(void) {
    if (s_metrics_mutex == NULL) return;
    xSemaphoreTake(s_metrics_mutex, portMAX_DELAY);
    s_metrics.audio_underrun_total++;
    xSemaphoreGive(s_metrics_mutex);
}

void sx_metrics_inc_audio_recovery(void) {
    if (s_metrics_mutex == NULL) return;
    xSemaphoreTake(s_metrics_mutex, portMAX_DELAY);
    s_metrics.audio_recovery_total++;
    xSemaphoreGive(s_metrics_mutex);
}

// Phase-3: Wi-Fi helpers implementation
void sx_metrics_update_wifi_reconnect(uint32_t reconnect_ms) {
    if (s_metrics_mutex == NULL) return;
    xSemaphoreTake(s_metrics_mutex, portMAX_DELAY);
    s_metrics.wifi_reconnect_ms_last = reconnect_ms;
    if (reconnect_ms > s_metrics.wifi_reconnect_ms_max) {
        s_metrics.wifi_reconnect_ms_max = reconnect_ms;
    }
    xSemaphoreGive(s_metrics_mutex);
}

// Phase-3: Prometheus exporter
esp_err_t sx_metrics_export_prom(const char *file_path) {
    if (file_path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    FILE *f = fopen(file_path, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s for write", file_path);
        return ESP_FAIL;
    }
    sx_metrics_t m;
    sx_metrics_get(&m);

    // Phase 4: Prometheus format with proper metric types and labels
    // Format: metric_name{label="value"} value timestamp
    
    #define WRITE_GAUGE(name, val) fprintf(f, "%s %u\n", (name), (unsigned)(val))
    #define WRITE_COUNTER(name, val) fprintf(f, "%s_total %u\n", (name), (unsigned)(val))

    // UI metrics (gauges)
    WRITE_GAUGE("ui_render_ms_last", m.ui_render_ms_last);
    WRITE_GAUGE("ui_render_ms_avg", m.ui_render_ms_avg);
    WRITE_GAUGE("ui_render_ms_max", m.ui_render_ms_max);
    WRITE_COUNTER("ui_frames_total", m.ui_frames_total);

    // Audio metrics (counters)
    WRITE_COUNTER("audio_underrun_total", m.audio_underrun_total);
    WRITE_COUNTER("audio_recovery_total", m.audio_recovery_total);

    // WiFi metrics (gauges)
    WRITE_GAUGE("wifi_reconnect_ms_last", m.wifi_reconnect_ms_last);
    WRITE_GAUGE("wifi_reconnect_ms_max", m.wifi_reconnect_ms_max);

    // Memory metrics (gauges)
    WRITE_GAUGE("heap_free_min_bytes", m.heap_free_min);
    WRITE_GAUGE("heap_free_current_bytes", m.heap_free_current);
    WRITE_GAUGE("psram_free_min_bytes", m.psram_free_min);
    WRITE_GAUGE("psram_free_current_bytes", m.psram_free_current);

    // State metrics
    WRITE_GAUGE("state_version", m.state_version);
    WRITE_COUNTER("state_updates_total", m.state_updates_total);

    // Event metrics with priority labels
    for (int p = 0; p < 4; p++) {
        const char *priority_name = (p == 0) ? "low" : 
                                    (p == 1) ? "normal" : 
                                    (p == 2) ? "high" : "critical";
        fprintf(f, "evt_posted_total{priority=\"%s\"} %u\n", priority_name, (unsigned)m.evt_posted_total[p]);
        fprintf(f, "evt_dropped_total{priority=\"%s\"} %u\n", priority_name, (unsigned)m.evt_dropped_total[p]);
        fprintf(f, "evt_coalesced_total{priority=\"%s\"} %u\n", priority_name, (unsigned)m.evt_coalesced_total[p]);
        fprintf(f, "queue_depth{priority=\"%s\"} %u\n", priority_name, (unsigned)m.queue_depth[p]);
    }
    
    WRITE_COUNTER("evt_processed_total", m.evt_processed_total);

    #undef WRITE_GAUGE
    #undef WRITE_COUNTER

    fclose(f);
    ESP_LOGI(TAG, "Metrics exported to %s (Prometheus format)", file_path);
    return ESP_OK;
}

#undef WRITE_KV

void sx_metrics_dump(void) {
    if (s_metrics_mutex == NULL) {
        return;
    }
    
    sx_metrics_t metrics;
    sx_metrics_get(&metrics);
    
    ESP_LOGI(TAG, "=== Metrics Dump ===");
    ESP_LOGI(TAG, "Events Posted: LOW=%lu NORMAL=%lu HIGH=%lu CRITICAL=%lu",
             (unsigned long)metrics.evt_posted_total[0],
             (unsigned long)metrics.evt_posted_total[1],
             (unsigned long)metrics.evt_posted_total[2],
             (unsigned long)metrics.evt_posted_total[3]);
    ESP_LOGI(TAG, "Events Dropped: LOW=%lu NORMAL=%lu HIGH=%lu CRITICAL=%lu",
             (unsigned long)metrics.evt_dropped_total[0],
             (unsigned long)metrics.evt_dropped_total[1],
             (unsigned long)metrics.evt_dropped_total[2],
             (unsigned long)metrics.evt_dropped_total[3]);
    ESP_LOGI(TAG, "Events Coalesced: LOW=%lu NORMAL=%lu HIGH=%lu CRITICAL=%lu",
             (unsigned long)metrics.evt_coalesced_total[0],
             (unsigned long)metrics.evt_coalesced_total[1],
             (unsigned long)metrics.evt_coalesced_total[2],
             (unsigned long)metrics.evt_coalesced_total[3]);
    ESP_LOGI(TAG, "Queue Depth: LOW=%lu NORMAL=%lu HIGH=%lu CRITICAL=%lu",
             (unsigned long)metrics.queue_depth[0],
             (unsigned long)metrics.queue_depth[1],
             (unsigned long)metrics.queue_depth[2],
             (unsigned long)metrics.queue_depth[3]);
    ESP_LOGI(TAG, "State: version=%lu updates=%lu",
             (unsigned long)metrics.state_version,
             (unsigned long)metrics.state_updates_total);
    ESP_LOGI(TAG, "UI Render: last=%lu ms avg=%lu ms max=%lu ms frames=%lu",
             (unsigned long)metrics.ui_render_ms_last,
             (unsigned long)metrics.ui_render_ms_avg,
             (unsigned long)metrics.ui_render_ms_max,
             (unsigned long)metrics.ui_frames_total);
    ESP_LOGI(TAG, "Heap: free_current=%lu bytes free_min=%lu bytes",
             (unsigned long)metrics.heap_free_current,
             (unsigned long)metrics.heap_free_min);
    if (metrics.psram_free_current > 0) {
        ESP_LOGI(TAG, "PSRAM: free_current=%lu bytes free_min=%lu bytes",
                 (unsigned long)metrics.psram_free_current,
                 (unsigned long)metrics.psram_free_min);
    }
    ESP_LOGI(TAG, "===================");
}








