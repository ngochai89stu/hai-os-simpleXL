#include "sx_network_optimizer.h"

#include <esp_log.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sx_netopt";

// Statistics
static uint32_t s_total_connections = 0;
static uint32_t s_successful_connections = 0;
static uint32_t s_failed_connections = 0;
static uint32_t s_reconnect_attempts = 0;

static bool s_initialized = false;

esp_err_t sx_network_optimizer_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    s_total_connections = 0;
    s_successful_connections = 0;
    s_failed_connections = 0;
    s_reconnect_attempts = 0;
    
    s_initialized = true;
    ESP_LOGI(TAG, "Network optimizer initialized");
    return ESP_OK;
}

uint32_t sx_network_optimizer_get_retry_delay(uint32_t attempt_num, const sx_retry_config_t *config) {
    // Default config
    sx_retry_config_t default_config = {
        .initial_delay_ms = 1000,
        .max_delay_ms = 30000,
        .max_attempts = 10,
        .exponential_backoff = true,
        .backoff_multiplier = 1.5f,
    };
    
    const sx_retry_config_t *cfg = (config != NULL) ? config : &default_config;
    
    if (attempt_num == 0) {
        return cfg->initial_delay_ms;
    }
    
    if (!cfg->exponential_backoff) {
        return cfg->initial_delay_ms;
    }
    
    // Exponential backoff: delay = initial * (multiplier ^ attempt_num)
    float delay = (float)cfg->initial_delay_ms;
    for (uint32_t i = 0; i < attempt_num; i++) {
        delay *= cfg->backoff_multiplier;
        if (delay >= (float)cfg->max_delay_ms) {
            delay = (float)cfg->max_delay_ms;
            break;
        }
    }
    
    return (uint32_t)delay;
}

void sx_network_optimizer_record_connection(bool success) {
    if (!s_initialized) {
        return;
    }
    
    s_total_connections++;
    if (success) {
        s_successful_connections++;
    } else {
        s_failed_connections++;
    }
    
    ESP_LOGD(TAG, "Connection recorded: success=%d, total=%lu, success=%lu, failed=%lu",
             success, (unsigned long)s_total_connections,
             (unsigned long)s_successful_connections,
             (unsigned long)s_failed_connections);
}

void sx_network_optimizer_record_reconnect(void) {
    if (!s_initialized) {
        return;
    }
    
    s_reconnect_attempts++;
    ESP_LOGD(TAG, "Reconnect recorded: attempts=%lu", (unsigned long)s_reconnect_attempts);
}

float sx_network_optimizer_get_success_rate(void) {
    if (!s_initialized || s_total_connections == 0) {
        return 0.0f;
    }
    
    return (float)s_successful_connections / (float)s_total_connections;
}

void sx_network_optimizer_reset_stats(void) {
    s_total_connections = 0;
    s_successful_connections = 0;
    s_failed_connections = 0;
    s_reconnect_attempts = 0;
    ESP_LOGI(TAG, "Network optimizer statistics reset");
}

