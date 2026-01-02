#include "sx_spi_bus_manager.h"

#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "sx_spi_bus_mgr";

static SemaphoreHandle_t s_spi_bus_mutex = NULL;
static bool s_initialized = false;

esp_err_t sx_spi_bus_manager_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }

    s_spi_bus_mutex = xSemaphoreCreateMutex();
    if (s_spi_bus_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create SPI bus mutex");
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "SPI bus manager initialized");
    return ESP_OK;
}

void sx_spi_bus_lock(void) {
    if (!s_initialized) {
        ESP_LOGW(TAG, "SPI bus manager not initialized, initializing now...");
        if (sx_spi_bus_manager_init() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize SPI bus manager");
            return;
        }
    }

    if (xSemaphoreTake(s_spi_bus_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take SPI bus mutex");
    }
}

void sx_spi_bus_unlock(void) {
    if (!s_initialized || s_spi_bus_mutex == NULL) {
        ESP_LOGW(TAG, "SPI bus manager not initialized, cannot unlock");
        return;
    }

    if (xSemaphoreGive(s_spi_bus_mutex) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to give SPI bus mutex");
    }
}


















