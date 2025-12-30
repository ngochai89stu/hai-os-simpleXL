#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: Audio Recovery Manager - Buffer underrun detection and recovery

// Recovery configuration
typedef struct {
    uint32_t buffer_underrun_threshold_ms;  // Threshold for buffer underrun detection (default: 100ms)
    uint32_t recovery_buffer_target_ms;      // Target buffer fill for recovery (default: 500ms)
    uint32_t max_recovery_attempts;          // Max recovery attempts before giving up (default: 3)
} sx_audio_recovery_config_t;

// Initialize audio recovery manager
esp_err_t sx_audio_recovery_init(const sx_audio_recovery_config_t *config);

// Check buffer status and trigger recovery if needed
// current_buffer_ms: Current buffer fill in milliseconds
// Returns: true if recovery was triggered
bool sx_audio_recovery_check(uint32_t current_buffer_ms);

// Force recovery (pause, refill, resume)
esp_err_t sx_audio_recovery_trigger(void);

// Check if recovery is in progress
bool sx_audio_recovery_is_active(void);

// Get recovery statistics
typedef struct {
    uint32_t total_recoveries;
    uint32_t successful_recoveries;
    uint32_t failed_recoveries;
} sx_audio_recovery_stats_t;

void sx_audio_recovery_get_stats(sx_audio_recovery_stats_t *stats);

#ifdef __cplusplus
}
#endif





