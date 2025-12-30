#pragma once

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize SPI bus manager
 * 
 * This function must be called before using any SPI bus lock/unlock functions.
 * It initializes the mutex used for SPI bus access synchronization.
 * 
 * @return ESP_OK on success
 */
esp_err_t sx_spi_bus_manager_init(void);

/**
 * @brief Lock SPI bus for exclusive access
 * 
 * This function blocks until the SPI bus is available.
 * Always call sx_spi_bus_unlock() after finishing SPI operations.
 * 
 * @note This is a blocking call that will wait indefinitely for the bus to be available.
 */
void sx_spi_bus_lock(void);

/**
 * @brief Unlock SPI bus
 * 
 * Releases the SPI bus lock acquired by sx_spi_bus_lock().
 * Must be called after every sx_spi_bus_lock() call.
 */
void sx_spi_bus_unlock(void);

#ifdef __cplusplus
}
#endif



