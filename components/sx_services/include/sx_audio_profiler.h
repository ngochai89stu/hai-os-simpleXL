#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Audio Pipeline Profiler
// Performance profiling, bottleneck detection, and metrics tracking

// Profiler stage/component
typedef enum {
    SX_PROFILER_STAGE_DECODE = 0,
    SX_PROFILER_STAGE_EQ,
    SX_PROFILER_STAGE_CROSSFADE,
    SX_PROFILER_STAGE_VOLUME,
    SX_PROFILER_STAGE_I2S,
    SX_PROFILER_STAGE_HTTP_READ,
    SX_PROFILER_STAGE_BUFFER,
    SX_PROFILER_STAGE_COUNT,
} sx_profiler_stage_t;

// Profiler metrics for a stage
typedef struct {
    uint64_t total_time_us;      // Total time spent in this stage (microseconds)
    uint64_t call_count;          // Number of calls
    uint64_t max_time_us;         // Maximum time for a single call
    uint64_t min_time_us;         // Minimum time for a single call
    uint64_t last_time_us;        // Last call time
    bool active;                  // Currently active
} sx_profiler_metrics_t;

// Profiler statistics
typedef struct {
    sx_profiler_metrics_t stages[SX_PROFILER_STAGE_COUNT];
    uint64_t total_audio_samples;
    uint64_t total_processing_time_us;
    uint32_t sample_rate;
    bool enabled;
} sx_profiler_stats_t;

// Initialize profiler
esp_err_t sx_audio_profiler_init(void);

// Deinitialize profiler
void sx_audio_profiler_deinit(void);

// Enable/disable profiler
void sx_audio_profiler_enable(bool enable);

// Check if profiler is enabled
bool sx_audio_profiler_is_enabled(void);

// Start timing a stage
// stage: Stage to profile
// Returns: Handle for end_timing (or 0 if disabled)
uint32_t sx_audio_profiler_start_timing(sx_profiler_stage_t stage);

// End timing a stage
// stage: Stage that was profiled
// handle: Handle returned from start_timing
void sx_audio_profiler_end_timing(sx_profiler_stage_t stage, uint32_t handle);

// Record sample count
// samples: Number of audio samples processed
void sx_audio_profiler_record_samples(uint32_t samples);

// Get profiler statistics
// stats: Output statistics
// Returns: ESP_OK on success
esp_err_t sx_audio_profiler_get_stats(sx_profiler_stats_t *stats);

// Reset profiler statistics
void sx_audio_profiler_reset_stats(void);

// Get stage name string
const char *sx_audio_profiler_stage_name(sx_profiler_stage_t stage);

// Print profiler statistics to log
void sx_audio_profiler_print_stats(void);

// Detect bottlenecks (stages taking > threshold percentage of total time)
// threshold_percent: Threshold percentage (0-100)
// Returns: Bitmask of bottleneck stages
uint32_t sx_audio_profiler_detect_bottlenecks(uint8_t threshold_percent);

#ifdef __cplusplus
}
#endif



















