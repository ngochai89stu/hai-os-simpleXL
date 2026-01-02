#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 4: Centralized error handling system

// Error categories
typedef enum {
    SX_ERROR_CATEGORY_PROTOCOL = 0,
    SX_ERROR_CATEGORY_AUDIO,
    SX_ERROR_CATEGORY_NETWORK,
    SX_ERROR_CATEGORY_SYSTEM,
    SX_ERROR_CATEGORY_UI,
    SX_ERROR_CATEGORY_COUNT
} sx_error_category_t;

// Error severity levels
typedef enum {
    SX_ERROR_SEVERITY_INFO = 0,
    SX_ERROR_SEVERITY_WARNING,
    SX_ERROR_SEVERITY_ERROR,
    SX_ERROR_SEVERITY_CRITICAL
} sx_error_severity_t;

// Error information structure
typedef struct {
    esp_err_t code;
    sx_error_severity_t severity;
    uint32_t timestamp;
    char message[128];
} sx_error_info_t;

/**
 * @brief Initialize error handler system
 * @return ESP_OK on success
 */
esp_err_t sx_error_handler_init(void);

/**
 * @brief Set error for a category
 * @param category Error category
 * @param error Error code
 * @param message Error message (can be NULL)
 * @param severity Error severity
 * @return ESP_OK on success
 */
esp_err_t sx_error_handler_set_error(sx_error_category_t category, 
                                      esp_err_t error, 
                                      const char *message,
                                      sx_error_severity_t severity);

/**
 * @brief Get error for a category
 * @param category Error category
 * @param out_info Output error info (can be NULL to just check)
 * @return true if error exists, false otherwise
 */
bool sx_error_handler_get_error(sx_error_category_t category, sx_error_info_t *out_info);

/**
 * @brief Clear error for a category
 * @param category Error category
 */
void sx_error_handler_clear_error(sx_error_category_t category);

/**
 * @brief Clear all errors
 */
void sx_error_handler_clear_all(void);

/**
 * @brief Check if any error exists
 * @return true if any error exists
 */
bool sx_error_handler_has_any_error(void);

/**
 * @brief Get error message string (for logging)
 * @param category Error category
 * @param out_message Output buffer
 * @param max_len Maximum length of output buffer
 * @return ESP_OK on success
 */
esp_err_t sx_error_handler_get_error_message(sx_error_category_t category, 
                                              char *out_message, 
                                              size_t max_len);

#ifdef __cplusplus
}
#endif






