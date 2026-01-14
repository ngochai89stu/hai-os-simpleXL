#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file sx_metrics.h
 * @brief Metrics collection system (Section 9.1 SIMPLEXL_ARCH v1.3)
 * Phase-3 KPI extension
 */

// P2.6: Metrics types
typedef struct {
    // Event metrics (Section 9.2)
    uint32_t evt_posted_total[4];      // Per priority: LOW, NORMAL, HIGH, CRITICAL
    uint32_t evt_dropped_total[4];     // Per priority
    uint32_t evt_coalesced_total[4];   // Per priority
    uint32_t evt_processed_total;      // Total processed by orchestrator

    // Queue depth gauges (Section 9.2)
    uint32_t queue_depth[4];           // Per priority: LOW, NORMAL, HIGH, CRITICAL

    // State metrics (Section 9.2)
    uint32_t state_version;            // Current state version
    uint32_t state_updates_total;      // Total state updates

    // UI metrics (Section 9.2)
    uint32_t ui_render_ms_last;        // Last render time in ms
    uint32_t ui_render_ms_avg;         // Average render time in ms
    uint32_t ui_render_ms_max;         // Max render time in ms
    uint32_t ui_frames_total;          // Total frames rendered

    // Audio metrics (Phase-3)
    uint32_t audio_underrun_total;     // Total buffer underruns detected
    uint32_t audio_recovery_total;     // Total recovery attempts executed

    // Wi-Fi metrics (Phase-3)
    uint32_t wifi_reconnect_ms_last;   // Last reconnect time (ms)
    uint32_t wifi_reconnect_ms_max;    // Max reconnect time (ms)

    // Memory metrics (Section 9.2)
    uint32_t heap_free_min;            // Minimum free heap (bytes)
    uint32_t heap_free_current;        // Current free heap (bytes)
    uint32_t psram_free_min;           // Minimum free PSRAM (bytes)
    uint32_t psram_free_current;       // Current free PSRAM (bytes)
} sx_metrics_t;

bool  sx_metrics_init(void);
void  sx_metrics_get(sx_metrics_t *out_metrics);
void  sx_metrics_reset(void);

// Event helper APIs
void  sx_metrics_inc_evt_posted(uint32_t priority);
void  sx_metrics_inc_evt_dropped(uint32_t priority);
void  sx_metrics_inc_evt_coalesced(uint32_t priority);
void  sx_metrics_set_queue_depth(uint32_t priority, uint32_t depth);

// State helpers
void  sx_metrics_set_state_version(uint32_t version);
void  sx_metrics_inc_state_updates(void);

// UI helpers
void  sx_metrics_update_ui_render(uint32_t render_ms);

// Heap / PSRAM helpers
void  sx_metrics_update_heap(uint32_t free_current, uint32_t free_min);
void  sx_metrics_update_psram(uint32_t free_current, uint32_t free_min);

// Phase-3: Audio helpers
void  sx_metrics_inc_audio_underrun(void);
void  sx_metrics_inc_audio_recovery(void);

// Phase-3: Wi-Fi helpers
void  sx_metrics_update_wifi_reconnect(uint32_t reconnect_ms);

// Export helpers
esp_err_t sx_metrics_export_prom(const char *file_path);

#ifdef __cplusplus
}
#endif
