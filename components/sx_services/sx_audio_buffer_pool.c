#include "sx_audio_buffer_pool.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"

static const char *TAG = "sx_buffer_pool";

// Buffer pool structure
struct sx_audio_buffer_pool {
    size_t buffer_size;
    size_t pool_size;
    bool use_psram;
    char name[32];
    
    // Buffer management
    void **buffers;           // Array of buffer pointers
    bool *in_use;             // Array of in-use flags
    SemaphoreHandle_t mutex;  // Thread safety mutex
    
    // Statistics
    size_t allocated_count;
};

esp_err_t sx_audio_buffer_pool_create(const sx_audio_buffer_pool_config_t *config,
                                      sx_audio_buffer_pool_t **pool) {
    if (!config || !pool || config->buffer_size == 0 || config->pool_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Allocate pool structure
    sx_audio_buffer_pool_t *new_pool = (sx_audio_buffer_pool_t *)malloc(sizeof(sx_audio_buffer_pool_t));
    if (!new_pool) {
        return ESP_ERR_NO_MEM;
    }
    
    memset(new_pool, 0, sizeof(*new_pool));
    new_pool->buffer_size = config->buffer_size;
    new_pool->pool_size = config->pool_size;
    new_pool->use_psram = config->use_psram && sx_audio_buffer_pool_psram_available();
    
    if (config->name) {
        strncpy(new_pool->name, config->name, sizeof(new_pool->name) - 1);
        new_pool->name[sizeof(new_pool->name) - 1] = '\0';
    } else {
        snprintf(new_pool->name, sizeof(new_pool->name), "pool_%p", new_pool);
    }
    
    // Create mutex
    new_pool->mutex = xSemaphoreCreateMutex();
    if (!new_pool->mutex) {
        free(new_pool);
        return ESP_ERR_NO_MEM;
    }
    
    // Allocate buffer pointer array
    new_pool->buffers = (void **)malloc(config->pool_size * sizeof(void *));
    if (!new_pool->buffers) {
        vSemaphoreDelete(new_pool->mutex);
        free(new_pool);
        return ESP_ERR_NO_MEM;
    }
    
    // Allocate in-use flags array
    new_pool->in_use = (bool *)malloc(config->pool_size * sizeof(bool));
    if (!new_pool->in_use) {
        free(new_pool->buffers);
        vSemaphoreDelete(new_pool->mutex);
        free(new_pool);
        return ESP_ERR_NO_MEM;
    }
    
    // Allocate buffers
    bool all_allocated = true;
    for (size_t i = 0; i < config->pool_size; i++) {
        new_pool->buffers[i] = sx_audio_buffer_alloc_heap(config->buffer_size, new_pool->use_psram);
        if (!new_pool->buffers[i]) {
            ESP_LOGE(TAG, "[%s] Failed to allocate buffer %zu", new_pool->name, i);
            all_allocated = false;
            // Free already allocated buffers
            for (size_t j = 0; j < i; j++) {
                sx_audio_buffer_free_heap(new_pool->buffers[j]);
            }
            break;
        }
        new_pool->in_use[i] = false;
    }
    
    if (!all_allocated) {
        free(new_pool->in_use);
        free(new_pool->buffers);
        vSemaphoreDelete(new_pool->mutex);
        free(new_pool);
        return ESP_ERR_NO_MEM;
    }
    
    new_pool->allocated_count = 0;
    
    *pool = new_pool;
    ESP_LOGI(TAG, "[%s] Created buffer pool: %zu buffers x %zu bytes (%s)",
             new_pool->name, config->pool_size, config->buffer_size,
             new_pool->use_psram ? "PSRAM" : "SRAM");
    
    return ESP_OK;
}

void sx_audio_buffer_pool_destroy(sx_audio_buffer_pool_t *pool) {
    if (!pool) {
        return;
    }
    
    // Free all buffers
    if (pool->buffers) {
        for (size_t i = 0; i < pool->pool_size; i++) {
            if (pool->buffers[i]) {
                sx_audio_buffer_free_heap(pool->buffers[i]);
            }
        }
        free(pool->buffers);
    }
    
    if (pool->in_use) {
        free(pool->in_use);
    }
    
    if (pool->mutex) {
        vSemaphoreDelete(pool->mutex);
    }
    
    ESP_LOGI(TAG, "[%s] Destroyed buffer pool", pool->name);
    free(pool);
}

esp_err_t sx_audio_buffer_pool_alloc(sx_audio_buffer_pool_t *pool, void **buffer) {
    if (!pool || !buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(pool->mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Find free buffer
    bool found = false;
    for (size_t i = 0; i < pool->pool_size; i++) {
        if (!pool->in_use[i]) {
            pool->in_use[i] = true;
            *buffer = pool->buffers[i];
            pool->allocated_count++;
            found = true;
            break;
        }
    }
    
    xSemaphoreGive(pool->mutex);
    
    if (!found) {
        ESP_LOGW(TAG, "[%s] Pool exhausted, no free buffers", pool->name);
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

esp_err_t sx_audio_buffer_pool_free(sx_audio_buffer_pool_t *pool, void *buffer) {
    if (!pool || !buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(pool->mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Find buffer in pool
    bool found = false;
    for (size_t i = 0; i < pool->pool_size; i++) {
        if (pool->buffers[i] == buffer) {
            if (pool->in_use[i]) {
                pool->in_use[i] = false;
                pool->allocated_count--;
                found = true;
            }
            break;
        }
    }
    
    xSemaphoreGive(pool->mutex);
    
    if (!found) {
        ESP_LOGW(TAG, "[%s] Buffer %p not found in pool or already free", pool->name, buffer);
        return ESP_ERR_INVALID_ARG;
    }
    
    return ESP_OK;
}

esp_err_t sx_audio_buffer_pool_get_stats(sx_audio_buffer_pool_t *pool,
                                         sx_audio_buffer_pool_stats_t *stats) {
    if (!pool || !stats) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(pool->mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    stats->total_buffers = pool->pool_size;
    stats->allocated_buffers = pool->allocated_count;
    stats->free_buffers = pool->pool_size - pool->allocated_count;
    stats->buffer_size = pool->buffer_size;
    stats->use_psram = pool->use_psram;
    
    xSemaphoreGive(pool->mutex);
    
    return ESP_OK;
}

bool sx_audio_buffer_pool_psram_available(void) {
    #ifdef CONFIG_SPIRAM_SUPPORT
    return heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0;
    #else
    return false;
    #endif
}

void *sx_audio_buffer_alloc_heap(size_t size, bool use_psram) {
    if (size == 0) {
        return NULL;
    }
    
    void *buffer = NULL;
    
    if (use_psram && sx_audio_buffer_pool_psram_available()) {
        // Try PSRAM first
        buffer = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (buffer) {
            return buffer;
        }
        // Fallback to SRAM if PSRAM allocation failed
        ESP_LOGW(TAG, "PSRAM allocation failed, falling back to SRAM");
    }
    
    // Allocate from SRAM
    buffer = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    
    return buffer;
}

void sx_audio_buffer_free_heap(void *buffer) {
    if (buffer) {
        heap_caps_free(buffer);
    }
}











