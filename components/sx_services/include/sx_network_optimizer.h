#pragma once

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: Network Optimizer - Adaptive retry and connection optimization

typedef struct {
    uint32_t initial_delay_ms;      // Initial retry delay
    uint32_t max_delay_ms;          // Maximum retry delay
    uint32_t max_attempts;          // Maximum retry attempts (0 = unlimited)
    bool exponential_backoff;       // Use exponential backoff
    float backoff_multiplier;       // Backoff multiplier (default 1.5)
} sx_retry_config_t;

// Initialize network optimizer
esp_err_t sx_network_optimizer_init(void);

// Get retry delay based on attempt number and config
uint32_t sx_network_optimizer_get_retry_delay(uint32_t attempt_num, const sx_retry_config_t *config);

// Record connection success/failure for statistics
void sx_network_optimizer_record_connection(bool success);

// Record reconnect attempt
void sx_network_optimizer_record_reconnect(void);

// Get connection success rate (0.0 to 1.0)
float sx_network_optimizer_get_success_rate(void);

// Reset statistics
void sx_network_optimizer_reset_stats(void);

#ifdef __cplusplus
}
#endif






















