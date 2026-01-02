#include "sx_service_if.h"

#include <esp_log.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static const char *TAG = "sx_service_if";

// P1.4: Service registry (Section 6.1 SIMPLEXL_ARCH v1.3)
#define MAX_SERVICES 32
typedef struct {
    char name[32];
    const sx_service_if_t *iface;
    bool registered;
} service_entry_t;

static service_entry_t s_service_registry[MAX_SERVICES];
static uint32_t s_service_count = 0;
static StaticSemaphore_t s_registry_mutex_buf;
static SemaphoreHandle_t s_registry_mutex = NULL;

static void init_mutex(void) {
    if (s_registry_mutex == NULL) {
        s_registry_mutex = xSemaphoreCreateMutexStatic(&s_registry_mutex_buf);
    }
}

esp_err_t sx_service_register(const char *name, const sx_service_if_t *iface) {
    if (!name || !iface) {
        return ESP_ERR_INVALID_ARG;
    }
    
    init_mutex();
    if (s_registry_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    
    // Check if already registered
    for (uint32_t i = 0; i < s_service_count; i++) {
        if (s_service_registry[i].registered && strcmp(s_service_registry[i].name, name) == 0) {
            xSemaphoreGive(s_registry_mutex);
            ESP_LOGW(TAG, "Service '%s' already registered", name);
            return ESP_ERR_INVALID_STATE;
        }
    }
    
    // Add new service
    if (s_service_count >= MAX_SERVICES) {
        xSemaphoreGive(s_registry_mutex);
        ESP_LOGE(TAG, "Service registry full (max %d)", MAX_SERVICES);
        return ESP_ERR_NO_MEM;
    }
    
    service_entry_t *entry = &s_service_registry[s_service_count++];
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->iface = iface;
    entry->registered = true;
    
    xSemaphoreGive(s_registry_mutex);
    ESP_LOGI(TAG, "Service '%s' registered", name);
    return ESP_OK;
}

esp_err_t sx_service_unregister(const char *name) {
    if (!name) {
        return ESP_ERR_INVALID_ARG;
    }
    
    init_mutex();
    if (s_registry_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    
    for (uint32_t i = 0; i < s_service_count; i++) {
        if (s_service_registry[i].registered && strcmp(s_service_registry[i].name, name) == 0) {
            s_service_registry[i].registered = false;
            xSemaphoreGive(s_registry_mutex);
            ESP_LOGI(TAG, "Service '%s' unregistered", name);
            return ESP_OK;
        }
    }
    
    xSemaphoreGive(s_registry_mutex);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t sx_service_init_all(void) {
    init_mutex();
    if (s_registry_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    
    esp_err_t ret = ESP_OK;
    for (uint32_t i = 0; i < s_service_count; i++) {
        if (s_service_registry[i].registered && s_service_registry[i].iface->init) {
            esp_err_t err = s_service_registry[i].iface->init();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Service '%s' init failed: %s", 
                        s_service_registry[i].name, esp_err_to_name(err));
                ret = ESP_FAIL;
            } else {
                ESP_LOGI(TAG, "Service '%s' initialized", s_service_registry[i].name);
            }
        }
    }
    
    xSemaphoreGive(s_registry_mutex);
    return ret;
}

esp_err_t sx_service_start_all(void) {
    init_mutex();
    if (s_registry_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    
    esp_err_t ret = ESP_OK;
    for (uint32_t i = 0; i < s_service_count; i++) {
        if (s_service_registry[i].registered && s_service_registry[i].iface->start) {
            esp_err_t err = s_service_registry[i].iface->start();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Service '%s' start failed: %s", 
                        s_service_registry[i].name, esp_err_to_name(err));
                ret = ESP_FAIL;
            } else {
                ESP_LOGI(TAG, "Service '%s' started", s_service_registry[i].name);
            }
        }
    }
    
    xSemaphoreGive(s_registry_mutex);
    return ret;
}

esp_err_t sx_service_stop_all(void) {
    init_mutex();
    if (s_registry_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    
    esp_err_t ret = ESP_OK;
    // Stop in reverse order
    for (int32_t i = (int32_t)s_service_count - 1; i >= 0; i--) {
        if (s_service_registry[i].registered && s_service_registry[i].iface->stop) {
            esp_err_t err = s_service_registry[i].iface->stop();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Service '%s' stop failed: %s", 
                        s_service_registry[i].name, esp_err_to_name(err));
                ret = ESP_FAIL;
            } else {
                ESP_LOGI(TAG, "Service '%s' stopped", s_service_registry[i].name);
            }
        }
    }
    
    xSemaphoreGive(s_registry_mutex);
    return ret;
}

esp_err_t sx_service_deinit_all(void) {
    init_mutex();
    if (s_registry_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    
    esp_err_t ret = ESP_OK;
    // Deinit in reverse order
    for (int32_t i = (int32_t)s_service_count - 1; i >= 0; i--) {
        if (s_service_registry[i].registered && s_service_registry[i].iface->deinit) {
            esp_err_t err = s_service_registry[i].iface->deinit();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Service '%s' deinit failed: %s", 
                        s_service_registry[i].name, esp_err_to_name(err));
                ret = ESP_FAIL;
            } else {
                ESP_LOGI(TAG, "Service '%s' deinitialized", s_service_registry[i].name);
            }
        }
    }
    
    xSemaphoreGive(s_registry_mutex);
    return ret;
}






