#include "sx_diagnostics_service.h"
#include "sx_wifi_service.h"
#include "sx_audio_service.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"

static const char *TAG = "sx_diagnostics";

// Diagnostics service state
static bool s_initialized = false;
static int64_t s_start_time = 0;

// Service health registry
#define MAX_REGISTERED_SERVICES 32
typedef struct {
    char name[32];
    sx_service_health_cb_t callback;
    bool registered;
} sx_service_registry_t;

static sx_service_registry_t s_service_registry[MAX_REGISTERED_SERVICES] = {0};

// Thresholds for warnings
#define MEMORY_WARNING_THRESHOLD_KB 50  // 50KB free heap
#define CPU_WARNING_THRESHOLD_PERCENT 80  // 80% CPU usage

esp_err_t sx_diagnostics_service_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    memset(s_service_registry, 0, sizeof(s_service_registry));
    s_start_time = esp_timer_get_time();
    
    s_initialized = true;
    ESP_LOGI(TAG, "Diagnostics service initialized");
    return ESP_OK;
}

void sx_diagnostics_service_deinit(void) {
    if (!s_initialized) {
        return;
    }
    
    memset(s_service_registry, 0, sizeof(s_service_registry));
    s_initialized = false;
    
    ESP_LOGI(TAG, "Diagnostics service deinitialized");
}

esp_err_t sx_diagnostics_get_metrics(sx_diagnostics_metrics_t *metrics) {
    if (!s_initialized || !metrics) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(metrics, 0, sizeof(*metrics));
    
    // Heap information
    metrics->free_heap_bytes = esp_get_free_heap_size();
    metrics->min_free_heap_bytes = esp_get_minimum_free_heap_size();
    metrics->largest_free_block_bytes = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    
    // Memory warning
    metrics->memory_warning = (metrics->free_heap_bytes < (MEMORY_WARNING_THRESHOLD_KB * 1024));
    
    // CPU usage (simplified - would need more sophisticated tracking)
    // For now, use task count as proxy
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    metrics->task_count = task_count;
    metrics->cpu_usage_percent = 0; // Would need CPU usage tracking
    metrics->cpu_warning = false; // Would need CPU usage tracking
    
    // Uptime
    int64_t current_time = esp_timer_get_time();
    metrics->uptime_seconds = (uint32_t)((current_time - s_start_time) / 1000000);
    
    // WiFi status
    extern bool sx_wifi_is_connected(void);
    metrics->wifi_connected = sx_wifi_is_connected();
    
    // Audio status
    extern bool sx_audio_is_playing(void);
    extern bool sx_audio_is_recording(void);
    metrics->audio_active = sx_audio_is_playing() || sx_audio_is_recording();
    
    return ESP_OK;
}

sx_diagnostics_status_t sx_diagnostics_get_system_status(void) {
    if (!s_initialized) {
        return SX_DIAG_STATUS_UNKNOWN;
    }
    
    sx_diagnostics_metrics_t metrics;
    if (sx_diagnostics_get_metrics(&metrics) != ESP_OK) {
        return SX_DIAG_STATUS_UNKNOWN;
    }
    
    // Check critical conditions
    if (metrics.memory_warning) {
        return SX_DIAG_STATUS_CRITICAL;
    }
    
    if (metrics.cpu_warning) {
        return SX_DIAG_STATUS_WARNING;
    }
    
    // Check service health
    for (int i = 0; i < MAX_REGISTERED_SERVICES; i++) {
        if (s_service_registry[i].registered) {
            sx_diagnostics_status_t service_status = s_service_registry[i].callback();
            if (service_status == SX_DIAG_STATUS_CRITICAL) {
                return SX_DIAG_STATUS_CRITICAL;
            }
            if (service_status == SX_DIAG_STATUS_WARNING) {
                return SX_DIAG_STATUS_WARNING;
            }
        }
    }
    
    return SX_DIAG_STATUS_HEALTHY;
}

esp_err_t sx_diagnostics_register_service(const char *service_name,
                                          sx_service_health_cb_t health_callback) {
    if (!s_initialized || !service_name || !health_callback) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Find free slot
    for (int i = 0; i < MAX_REGISTERED_SERVICES; i++) {
        if (!s_service_registry[i].registered) {
            strncpy(s_service_registry[i].name, service_name, sizeof(s_service_registry[i].name) - 1);
            s_service_registry[i].name[sizeof(s_service_registry[i].name) - 1] = '\0';
            s_service_registry[i].callback = health_callback;
            s_service_registry[i].registered = true;
            ESP_LOGI(TAG, "Registered service health check: %s", service_name);
            return ESP_OK;
        }
    }
    
    ESP_LOGE(TAG, "Service registry full, cannot register: %s", service_name);
    return ESP_ERR_NO_MEM;
}

esp_err_t sx_diagnostics_unregister_service(const char *service_name) {
    if (!s_initialized || !service_name) {
        return ESP_ERR_INVALID_ARG;
    }
    
    for (int i = 0; i < MAX_REGISTERED_SERVICES; i++) {
        if (s_service_registry[i].registered &&
            strcmp(s_service_registry[i].name, service_name) == 0) {
            s_service_registry[i].registered = false;
            memset(&s_service_registry[i], 0, sizeof(s_service_registry[i]));
            ESP_LOGI(TAG, "Unregistered service health check: %s", service_name);
            return ESP_OK;
        }
    }
    
    return ESP_ERR_NOT_FOUND;
}

esp_err_t sx_diagnostics_get_services_health(sx_service_health_t *services,
                                             size_t max_services, size_t *service_count) {
    if (!s_initialized || !services || !service_count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *service_count = 0;
    
    for (int i = 0; i < MAX_REGISTERED_SERVICES && *service_count < max_services; i++) {
        if (s_service_registry[i].registered) {
            sx_service_health_t *health = &services[*service_count];
            strncpy(health->service_name, s_service_registry[i].name, 32);
            health->service_name[31] = '\0';
            health->is_initialized = true; // Assume initialized if registered
            health->is_running = true; // Assume running if registered
            health->status = s_service_registry[i].callback();
            
            switch (health->status) {
                case SX_DIAG_STATUS_HEALTHY:
                    strncpy(health->status_message, "Healthy", 32);
                    break;
                case SX_DIAG_STATUS_WARNING:
                    strncpy(health->status_message, "Warning", 32);
                    break;
                case SX_DIAG_STATUS_CRITICAL:
                    strncpy(health->status_message, "Critical", 32);
                    break;
                default:
                    strncpy(health->status_message, "Unknown", 32);
                    break;
            }
            health->status_message[31] = '\0';
            
            (*service_count)++;
        }
    }
    
    return ESP_OK;
}

sx_diagnostics_status_t sx_diagnostics_check_service(const char *service_name) {
    if (!s_initialized || !service_name) {
        return SX_DIAG_STATUS_UNKNOWN;
    }
    
    for (int i = 0; i < MAX_REGISTERED_SERVICES; i++) {
        if (s_service_registry[i].registered &&
            strcmp(s_service_registry[i].name, service_name) == 0) {
            return s_service_registry[i].callback();
        }
    }
    
    return SX_DIAG_STATUS_UNKNOWN;
}

void sx_diagnostics_print_report(void) {
    if (!s_initialized) {
        ESP_LOGW(TAG, "Diagnostics service not initialized");
        return;
    }
    
    sx_diagnostics_metrics_t metrics;
    if (sx_diagnostics_get_metrics(&metrics) != ESP_OK) {
        return;
    }
    
    sx_diagnostics_status_t system_status = sx_diagnostics_get_system_status();
    
    ESP_LOGI(TAG, "=== System Diagnostics Report ===");
    ESP_LOGI(TAG, "System Status: %s", 
             system_status == SX_DIAG_STATUS_HEALTHY ? "HEALTHY" :
             system_status == SX_DIAG_STATUS_WARNING ? "WARNING" :
             system_status == SX_DIAG_STATUS_CRITICAL ? "CRITICAL" : "UNKNOWN");
    ESP_LOGI(TAG, "Uptime: %lu seconds", (unsigned long)metrics.uptime_seconds);
    ESP_LOGI(TAG, "Free Heap: %lu bytes (min: %lu bytes)", 
             (unsigned long)metrics.free_heap_bytes,
             (unsigned long)metrics.min_free_heap_bytes);
    ESP_LOGI(TAG, "Largest Free Block: %lu bytes", 
             (unsigned long)metrics.largest_free_block_bytes);
    ESP_LOGI(TAG, "Task Count: %lu", (unsigned long)metrics.task_count);
    ESP_LOGI(TAG, "WiFi: %s", metrics.wifi_connected ? "Connected" : "Disconnected");
    ESP_LOGI(TAG, "Audio: %s", metrics.audio_active ? "Active" : "Idle");
    ESP_LOGI(TAG, "Memory Warning: %s", metrics.memory_warning ? "YES" : "NO");
    ESP_LOGI(TAG, "CPU Warning: %s", metrics.cpu_warning ? "YES" : "NO");
    
    // Print service health
    sx_service_health_t services[32];
    size_t service_count = 0;
    if (sx_diagnostics_get_services_health(services, 32, &service_count) == ESP_OK && service_count > 0) {
        ESP_LOGI(TAG, "--- Service Health ---");
        for (size_t i = 0; i < service_count; i++) {
            ESP_LOGI(TAG, "  %s: %s (%s)", 
                     services[i].service_name,
                     services[i].status_message,
                     services[i].is_running ? "Running" : "Stopped");
        }
    }
    
    ESP_LOGI(TAG, "================================");
}


