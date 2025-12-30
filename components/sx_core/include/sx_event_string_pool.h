#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Event string pool for reducing malloc/free overhead
 * 
 * This pool provides pre-allocated string buffers for event payloads,
 * reducing memory fragmentation and allocation overhead.
 */

// Pool configuration
#define SX_EVENT_STRING_POOL_SIZE 8
#define SX_EVENT_STRING_MAX_LEN 128

/**
 * @brief Allocate a string from the pool
 * 
 * @param src Source string to copy
 * @return Pointer to allocated string, or NULL if pool is full
 *         Note: Falls back to malloc if pool is full
 */
char* sx_event_alloc_string(const char *src);

/**
 * @brief Free a string allocated from the pool
 * 
 * @param str String pointer to free
 *         If from pool, marks as unused. If malloc'd, calls free().
 */
void sx_event_free_string(char *str);

/**
 * @brief Initialize the event string pool
 */
void sx_event_string_pool_init(void);

/**
 * @brief Get pool statistics
 * 
 * @param used_count Output: number of used slots
 * @param total_count Output: total number of slots
 */
void sx_event_string_pool_stats(size_t *used_count, size_t *total_count);

#ifdef __cplusplus
}
#endif



