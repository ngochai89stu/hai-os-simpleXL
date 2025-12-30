#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Diagnostics Service
// System health monitoring and diagnostics

// System health status
typedef enum {
    SX_DIAG_STATUS_HEALTHY = 0,
    SX_DIAG_STATUS_WARNING,
    SX_DIAG_STATUS_CRITICAL,
    SX_DIAG_STATUS_UNKNOWN,
} sx_diagnostics_status_t;

// System metrics
typedef struct {
    uint32_t free_heap_bytes;
    uint32_t min_free_heap_bytes;
    uint32_t largest_free_block_bytes;
    uint32_t cpu_usage_percent;
    uint32_t task_count;
    uint32_t uptime_seconds;
    bool wifi_connected;
    bool audio_active;
    bool memory_warning;
    bool cpu_warning;
} sx_diagnostics_metrics_t;

// Service health info
typedef struct {
    char service_name[32];
    bool is_initialized;
    bool is_running;
    sx_diagnostics_status_t status;
    char status_message[32];
} sx_service_health_t;

// Initialize diagnostics service
esp_err_t sx_diagnostics_service_init(void);

// Deinitialize diagnostics service
void sx_diagnostics_service_deinit(void);

// Get system metrics
esp_err_t sx_diagnostics_get_metrics(sx_diagnostics_metrics_t *metrics);

// Get overall system health status
sx_diagnostics_status_t sx_diagnostics_get_system_status(void);

// Register service health check
// service_name: Name of the service
// health_callback: Callback to check service health
typedef sx_diagnostics_status_t (*sx_service_health_cb_t)(void);

esp_err_t sx_diagnostics_register_service(const char *service_name,
                                          sx_service_health_cb_t health_callback);

// Unregister service health check
esp_err_t sx_diagnostics_unregister_service(const char *service_name);

// Get all registered services health
esp_err_t sx_diagnostics_get_services_health(sx_service_health_t *services,
                                             size_t max_services, size_t *service_count);

// Check service health
sx_diagnostics_status_t sx_diagnostics_check_service(const char *service_name);

// Print diagnostics report to log
void sx_diagnostics_print_report(void);

#ifdef __cplusplus
}
#endif


