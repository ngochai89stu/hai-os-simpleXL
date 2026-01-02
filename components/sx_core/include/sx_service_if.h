#pragma once

#include "sx_event.h"
#include <esp_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file sx_service_if.h
 * @brief Service lifecycle interface (Section 6.1 SIMPLEXL_ARCH v1.3)
 * 
 * This file defines the mandatory service interface vtable.
 * All services MUST implement this interface for lifecycle management.
 */

// Service lifecycle interface vtable
typedef struct {
    /**
     * @brief Initialize service (allocate resources, setup)
     * Called once during bootstrap, before start()
     * @return ESP_OK on success
     */
    esp_err_t (*init)(void);
    
    /**
     * @brief Start service (begin operation)
     * Called after init(), service should start processing
     * @return ESP_OK on success
     */
    esp_err_t (*start)(void);
    
    /**
     * @brief Stop service (pause operation)
     * Service should stop processing but keep resources
     * @return ESP_OK on success
     */
    esp_err_t (*stop)(void);
    
    /**
     * @brief Deinitialize service (free resources)
     * Called during shutdown, after stop()
     * @return ESP_OK on success
     */
    esp_err_t (*deinit)(void);
    
    /**
     * @brief Handle event (optional)
     * Service can subscribe to events and handle them here
     * @param evt Event to handle
     * @return ESP_OK if handled, ESP_ERR_NOT_SUPPORTED if not interested
     */
    esp_err_t (*on_event)(const sx_event_t *evt);
} sx_service_if_t;

/**
 * @brief Register service with lifecycle interface
 * 
 * @param name Service name (for logging/debugging)
 * @param iface Service interface vtable
 * @return ESP_OK on success
 */
esp_err_t sx_service_register(const char *name, const sx_service_if_t *iface);

/**
 * @brief Unregister service
 * 
 * @param name Service name
 * @return ESP_OK on success
 */
esp_err_t sx_service_unregister(const char *name);

/**
 * @brief Initialize all registered services
 * Calls init() on all registered services
 * @return ESP_OK on success
 */
esp_err_t sx_service_init_all(void);

/**
 * @brief Start all registered services
 * Calls start() on all registered services
 * @return ESP_OK on success
 */
esp_err_t sx_service_start_all(void);

/**
 * @brief Stop all registered services
 * Calls stop() on all registered services
 * @return ESP_OK on success
 */
esp_err_t sx_service_stop_all(void);

/**
 * @brief Deinitialize all registered services
 * Calls deinit() on all registered services
 * @return ESP_OK on success
 */
esp_err_t sx_service_deinit_all(void);

#ifdef __cplusplus
}
#endif






