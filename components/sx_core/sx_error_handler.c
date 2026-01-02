#include "sx_error_handler.h"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_event_string_pool.h"

#include <esp_log.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sx_error_handler";

// Phase 4: Centralized error handling system

// Error storage (one per category)
static sx_error_info_t s_errors[SX_ERROR_CATEGORY_COUNT] = {0};
static bool s_initialized = false;

// Category names for logging
static const char *s_category_names[] = {
    "PROTOCOL",
    "AUDIO",
    "NETWORK",
    "SYSTEM",
    "UI"
};

esp_err_t sx_error_handler_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    // Clear all errors
    memset(s_errors, 0, sizeof(s_errors));
    
    s_initialized = true;
    ESP_LOGI(TAG, "Error handler initialized");
    return ESP_OK;
}

esp_err_t sx_error_handler_set_error(sx_error_category_t category, 
                                      esp_err_t error, 
                                      const char *message,
                                      sx_error_severity_t severity) {
    if (!s_initialized) {
        sx_error_handler_init();
    }
    
    if (category >= SX_ERROR_CATEGORY_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    sx_error_info_t *err = &s_errors[category];
    err->code = error;
    err->severity = severity;
    err->timestamp = xTaskGetTickCount();
    
    if (message) {
        strncpy(err->message, message, sizeof(err->message) - 1);
        err->message[sizeof(err->message) - 1] = '\0';
    } else {
        err->message[0] = '\0';
    }
    
    // Log based on severity
    const char *category_name = s_category_names[category];
    const char *severity_name = "INFO";
    switch (severity) {
        case SX_ERROR_SEVERITY_WARNING:
            severity_name = "WARNING";
            ESP_LOGW(TAG, "[%s] %s: %s (0x%x)", category_name, severity_name, 
                    err->message[0] ? err->message : esp_err_to_name(error), error);
            break;
        case SX_ERROR_SEVERITY_ERROR:
            severity_name = "ERROR";
            ESP_LOGE(TAG, "[%s] %s: %s (0x%x)", category_name, severity_name, 
                    err->message[0] ? err->message : esp_err_to_name(error), error);
            break;
        case SX_ERROR_SEVERITY_CRITICAL:
            severity_name = "CRITICAL";
            ESP_LOGE(TAG, "[%s] %s: %s (0x%x)", category_name, severity_name, 
                    err->message[0] ? err->message : esp_err_to_name(error), error);
            break;
        default:
            ESP_LOGI(TAG, "[%s] %s: %s (0x%x)", category_name, severity_name, 
                    err->message[0] ? err->message : esp_err_to_name(error), error);
            break;
    }
    
    // Emit error event for critical/error severity
    if (severity >= SX_ERROR_SEVERITY_ERROR) {
        char *error_str = sx_event_alloc_string(err->message[0] ? err->message : esp_err_to_name(error));
        if (error_str) {
            sx_event_t evt = {
                .type = SX_EVT_PROTOCOL_ERROR,  // Reuse protocol error event for now
                .arg0 = (uint32_t)category,
                .ptr = error_str,
            };
            sx_dispatcher_post_event(&evt);
        }
    }
    
    return ESP_OK;
}

bool sx_error_handler_get_error(sx_error_category_t category, sx_error_info_t *out_info) {
    if (!s_initialized || category >= SX_ERROR_CATEGORY_COUNT) {
        return false;
    }
    
    sx_error_info_t *err = &s_errors[category];
    
    // Check if error exists (code != ESP_OK)
    if (err->code == ESP_OK) {
        return false;
    }
    
    if (out_info) {
        *out_info = *err;
    }
    
    return true;
}

void sx_error_handler_clear_error(sx_error_category_t category) {
    if (!s_initialized || category >= SX_ERROR_CATEGORY_COUNT) {
        return;
    }
    
    sx_error_info_t *err = &s_errors[category];
    memset(err, 0, sizeof(sx_error_info_t));
}

void sx_error_handler_clear_all(void) {
    if (!s_initialized) {
        return;
    }
    
    memset(s_errors, 0, sizeof(s_errors));
    ESP_LOGI(TAG, "All errors cleared");
}

bool sx_error_handler_has_any_error(void) {
    if (!s_initialized) {
        return false;
    }
    
    for (int i = 0; i < SX_ERROR_CATEGORY_COUNT; i++) {
        if (s_errors[i].code != ESP_OK) {
            return true;
        }
    }
    
    return false;
}

esp_err_t sx_error_handler_get_error_message(sx_error_category_t category, 
                                              char *out_message, 
                                              size_t max_len) {
    if (!s_initialized || category >= SX_ERROR_CATEGORY_COUNT || !out_message || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    sx_error_info_t *err = &s_errors[category];
    
    if (err->code == ESP_OK) {
        out_message[0] = '\0';
        return ESP_OK;
    }
    
    if (err->message[0] != '\0') {
        strncpy(out_message, err->message, max_len - 1);
        out_message[max_len - 1] = '\0';
    } else {
        const char *err_str = esp_err_to_name(err->code);
        strncpy(out_message, err_str, max_len - 1);
        out_message[max_len - 1] = '\0';
    }
    
    return ESP_OK;
}






