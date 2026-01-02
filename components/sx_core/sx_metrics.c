#include "sx_metrics.h"

#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_heap_caps.h>

static const char *TAG = "sx_metrics";

// P2.6: Metrics storage (Section 9.1 SIMPLEXL_ARCH v1.3)
static sx_metrics_t s_metrics;
static StaticSemaphore_t s_metrics_mutex_buf;
static SemaphoreHandle_t s_metrics_mutex = NULL;

static void init_mutex(void) {
    if (s_metrics_mutex == NULL) {
        s_metrics_mutex = xSemaphoreCreateMutexStatic(&s_metrics_mutex_buf);
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






