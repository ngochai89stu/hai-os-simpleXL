#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file sx_metrics.h
 * @brief Metrics collection system (Section 9.1 SIMPLEXL_ARCH v1.3)
 * P2.6: Complete metrics system
 */

// P2.6: Metrics types
typedef struct {
    // Event metrics (Section 9.2)
    uint32_t evt_posted_total[4];      // Per priority: LOW, NORMAL, HIGH, CRITICAL
    uint32_t evt_dropped_total[4];      // Per priority
    uint32_t evt_coalesced_total[4];   // Per priority
    uint32_t evt_processed_total;      // Total processed by orchestrator
    
    // Queue depth gauges (Section 9.2)
    uint32_t queue_depth[4];           // Per priority: LOW, NORMAL, HIGH, CRITICAL
    
    // State metrics (Section 9.2)
    uint32_t state_version;             // Current state version
    uint32_t state_updates_total;       // Total state updates
    
    // UI metrics (Section 9.2)
    uint32_t ui_render_ms_last;         // Last render time in ms
    uint32_t ui_render_ms_avg;          // Average render time in ms
    uint32_t ui_render_ms_max;          // Max render time in ms
    uint32_t ui_frames_total;           // Total frames rendered
    
    // Memory metrics (Section 9.2)
    uint32_t heap_free_min;             // Minimum free heap (in bytes)
    uint32_t heap_free_current;         // Current free heap (in bytes)
    uint32_t psram_free_min;            // Minimum free PSRAM (in bytes, 0 if not available)
    uint32_t psram_free_current;        // Current free PSRAM (in bytes, 0 if not available)
} sx_metrics_t;

/**
 * @brief Initialize metrics system
 * @return true on success
 */
bool sx_metrics_init(void);

/**
 * @brief Get current metrics snapshot
 * @param out_metrics Output metrics structure
 */
void sx_metrics_get(sx_metrics_t *out_metrics);

/**
 * @brief Reset all metrics counters
 */
void sx_metrics_reset(void);

/**
 * @brief Increment event posted counter for priority
 * @param priority Event priority (0=LOW, 1=NORMAL, 2=HIGH, 3=CRITICAL)
 */
void sx_metrics_inc_evt_posted(uint32_t priority);

/**
 * @brief Increment event dropped counter for priority
 * @param priority Event priority
 */
void sx_metrics_inc_evt_dropped(uint32_t priority);

/**
 * @brief Increment event coalesced counter for priority
 * @param priority Event priority
 */
void sx_metrics_inc_evt_coalesced(uint32_t priority);

/**
 * @brief Update queue depth gauge for priority
 * @param priority Event priority
 * @param depth Current queue depth
 */
void sx_metrics_set_queue_depth(uint32_t priority, uint32_t depth);

/**
 * @brief Update state version gauge
 * @param version Current state version
 */
void sx_metrics_set_state_version(uint32_t version);

/**
 * @brief Increment state updates counter
 */
void sx_metrics_inc_state_updates(void);

/**
 * @brief Update UI render time
 * @param render_ms Render time in milliseconds
 */
void sx_metrics_update_ui_render(uint32_t render_ms);

/**
 * @brief Update heap metrics
 * @param free_current Current free heap in bytes
 * @param free_min Minimum free heap in bytes
 */
void sx_metrics_update_heap(uint32_t free_current, uint32_t free_min);

/**
 * @brief Update PSRAM metrics
 * @param free_current Current free PSRAM in bytes
 * @param free_min Minimum free PSRAM in bytes
 */
void sx_metrics_update_psram(uint32_t free_current, uint32_t free_min);

/**
 * @brief Dump metrics to log (for debugging)
 */
void sx_metrics_dump(void);

#ifdef __cplusplus
}
#endif






