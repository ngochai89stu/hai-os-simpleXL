#include "sx_event_string_pool.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <esp_log.h>

static const char *TAG = "sx_event_pool";

// Static pool storage
static char s_event_string_pool[SX_EVENT_STRING_POOL_SIZE][SX_EVENT_STRING_MAX_LEN];
static bool s_event_string_used[SX_EVENT_STRING_POOL_SIZE] = {0};
static bool s_pool_initialized = false;

// Pool metrics
static sx_event_string_pool_metrics_t s_metrics = {0};

void sx_event_string_pool_init(void) {
    if (s_pool_initialized) {
        return;
    }
    
    memset(s_event_string_pool, 0, sizeof(s_event_string_pool));
    memset(s_event_string_used, 0, sizeof(s_event_string_used));
    s_pool_initialized = true;
    
    ESP_LOGI(TAG, "Event string pool initialized: %d slots, %d bytes each",
             SX_EVENT_STRING_POOL_SIZE, SX_EVENT_STRING_MAX_LEN);
}

char* sx_event_alloc_string(const char *src) {
    if (!s_pool_initialized) {
        sx_event_string_pool_init();
    }
    
    if (src == NULL) {
        return NULL;
    }
    
    // Update metrics
    s_metrics.total_allocations++;
    
    size_t src_len = strlen(src);
    if (src_len >= SX_EVENT_STRING_MAX_LEN) {
        ESP_LOGW(TAG, "String too long (%zu >= %d), using malloc fallback", 
                 src_len, SX_EVENT_STRING_MAX_LEN);
        s_metrics.pool_misses++;
        s_metrics.malloc_fallbacks++;
        return strdup(src);
    }
    
    // Try to find an unused slot
    for (int i = 0; i < SX_EVENT_STRING_POOL_SIZE; i++) {
        if (!s_event_string_used[i]) {
            s_event_string_used[i] = true;
            strncpy(s_event_string_pool[i], src, SX_EVENT_STRING_MAX_LEN - 1);
            s_event_string_pool[i][SX_EVENT_STRING_MAX_LEN - 1] = '\0';
            
            // Update metrics
            s_metrics.pool_hits++;
            s_metrics.current_usage++;
            if (s_metrics.current_usage > s_metrics.peak_usage) {
                s_metrics.peak_usage = s_metrics.current_usage;
            }
            
            return s_event_string_pool[i];
        }
    }
    
    // Pool is full, fallback to malloc
    ESP_LOGW(TAG, "Event string pool full, using malloc fallback");
    s_metrics.pool_misses++;
    s_metrics.malloc_fallbacks++;
    return strdup(src);
}

void sx_event_free_string(char *str) {
    if (str == NULL) {
        return;
    }
    
    // Check if pointer is within pool range
    if (str >= s_event_string_pool[0] && 
        str < s_event_string_pool[SX_EVENT_STRING_POOL_SIZE - 1] + SX_EVENT_STRING_MAX_LEN) {
        // Pointer is in pool, mark as unused
        int idx = (str - s_event_string_pool[0]) / SX_EVENT_STRING_MAX_LEN;
        if (idx >= 0 && idx < SX_EVENT_STRING_POOL_SIZE) {
            s_event_string_used[idx] = false;
            // Clear the string for security
            memset(s_event_string_pool[idx], 0, SX_EVENT_STRING_MAX_LEN);
            
            // Update metrics
            if (s_metrics.current_usage > 0) {
                s_metrics.current_usage--;
            }
        }
    } else {
        // Pointer is malloc'd, free it
        free(str);
    }
}

void sx_event_string_pool_stats(size_t *used_count, size_t *total_count) {
    if (used_count) {
        *used_count = s_metrics.current_usage;
    }
    if (total_count) {
        *total_count = SX_EVENT_STRING_POOL_SIZE;
    }
}

void sx_event_string_pool_get_metrics(sx_event_string_pool_metrics_t *metrics) {
    if (metrics) {
        // Update current_usage before returning
        s_metrics.current_usage = 0;
        for (int i = 0; i < SX_EVENT_STRING_POOL_SIZE; i++) {
            if (s_event_string_used[i]) {
                s_metrics.current_usage++;
            }
        }
        *metrics = s_metrics;
    }
}

void sx_event_string_pool_reset_metrics(void) {
    memset(&s_metrics, 0, sizeof(s_metrics));
}










