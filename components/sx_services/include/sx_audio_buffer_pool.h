#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Audio Buffer Pool Service
// Thread-safe buffer pool with PSRAM support for efficient buffer management

// Buffer pool configuration
typedef struct {
    size_t buffer_size;        // Size of each buffer in bytes
    size_t pool_size;         // Number of buffers in pool
    bool use_psram;           // Use PSRAM if available
    const char *name;          // Pool name for logging
} sx_audio_buffer_pool_config_t;

// Buffer pool handle (opaque)
typedef struct sx_audio_buffer_pool sx_audio_buffer_pool_t;

// Create a new buffer pool
// config: Pool configuration
// pool: Output pool handle
// Returns: ESP_OK on success
esp_err_t sx_audio_buffer_pool_create(const sx_audio_buffer_pool_config_t *config,
                                      sx_audio_buffer_pool_t **pool);

// Destroy buffer pool
// pool: Pool handle (can be NULL)
void sx_audio_buffer_pool_destroy(sx_audio_buffer_pool_t *pool);

// Allocate a buffer from pool
// pool: Pool handle
// buffer: Output buffer pointer
// Returns: ESP_OK on success, ESP_ERR_NO_MEM if pool exhausted
esp_err_t sx_audio_buffer_pool_alloc(sx_audio_buffer_pool_t *pool, void **buffer);

// Release a buffer back to pool
// pool: Pool handle
// buffer: Buffer to release (must be from this pool)
// Returns: ESP_OK on success, ESP_ERR_INVALID_ARG if buffer not from pool
esp_err_t sx_audio_buffer_pool_free(sx_audio_buffer_pool_t *pool, void *buffer);

// Get pool statistics
typedef struct {
    size_t total_buffers;
    size_t allocated_buffers;
    size_t free_buffers;
    size_t buffer_size;
    bool use_psram;
} sx_audio_buffer_pool_stats_t;

esp_err_t sx_audio_buffer_pool_get_stats(sx_audio_buffer_pool_t *pool,
                                         sx_audio_buffer_pool_stats_t *stats);

// Check if PSRAM is available
bool sx_audio_buffer_pool_psram_available(void);

// Allocate buffer from heap (with PSRAM preference)
// size: Buffer size in bytes
// use_psram: Prefer PSRAM if available
// Returns: Allocated buffer or NULL on failure
void *sx_audio_buffer_alloc_heap(size_t size, bool use_psram);

// Free buffer allocated from heap
// buffer: Buffer to free
void sx_audio_buffer_free_heap(void *buffer);

#ifdef __cplusplus
}
#endif











