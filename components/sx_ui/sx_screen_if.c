#include "sx_screen_if.h"
#include "ui_screen_registry.h"  // For SCREEN_ID_MAX

#include <esp_log.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static const char *TAG = "sx_screen_if";

// P1.4: Screen registry (Section 6.2 SIMPLEXL_ARCH v1.3)
#define MAX_SCREENS 64
typedef struct {
    uint32_t screen_id;
    const sx_screen_if_t *iface;
    bool registered;
} screen_entry_t;

static screen_entry_t s_screen_registry[MAX_SCREENS];
static uint32_t s_screen_count = 0;
static StaticSemaphore_t s_registry_mutex_buf;
static SemaphoreHandle_t s_registry_mutex = NULL;

static void init_mutex(void) {
    if (s_registry_mutex == NULL) {
        s_registry_mutex = xSemaphoreCreateMutexStatic(&s_registry_mutex_buf);
    }
}

esp_err_t sx_screen_register(uint32_t screen_id, const sx_screen_if_t *iface) {
    if (!iface || screen_id >= SCREEN_ID_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    init_mutex();
    if (s_registry_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    
    // Check if already registered
    for (uint32_t i = 0; i < s_screen_count; i++) {
        if (s_screen_registry[i].registered && s_screen_registry[i].screen_id == screen_id) {
            xSemaphoreGive(s_registry_mutex);
            ESP_LOGW(TAG, "Screen %lu already registered", (unsigned long)screen_id);
            return ESP_ERR_INVALID_STATE;
        }
    }
    
    // Add new screen
    if (s_screen_count >= MAX_SCREENS) {
        xSemaphoreGive(s_registry_mutex);
        ESP_LOGE(TAG, "Screen registry full (max %d)", MAX_SCREENS);
        return ESP_ERR_NO_MEM;
    }
    
    screen_entry_t *entry = &s_screen_registry[s_screen_count++];
    entry->screen_id = screen_id;
    entry->iface = iface;
    entry->registered = true;
    
    xSemaphoreGive(s_registry_mutex);
    ESP_LOGI(TAG, "Screen %lu registered", (unsigned long)screen_id);
    return ESP_OK;
}

esp_err_t sx_screen_unregister(uint32_t screen_id) {
    init_mutex();
    if (s_registry_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    
    for (uint32_t i = 0; i < s_screen_count; i++) {
        if (s_screen_registry[i].registered && s_screen_registry[i].screen_id == screen_id) {
            s_screen_registry[i].registered = false;
            xSemaphoreGive(s_registry_mutex);
            ESP_LOGI(TAG, "Screen %lu unregistered", (unsigned long)screen_id);
            return ESP_OK;
        }
    }
    
    xSemaphoreGive(s_registry_mutex);
    return ESP_ERR_NOT_FOUND;
}

const sx_screen_if_t* sx_screen_get_interface(uint32_t screen_id) {
    init_mutex();
    if (s_registry_mutex == NULL) {
        return NULL;
    }
    
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    
    for (uint32_t i = 0; i < s_screen_count; i++) {
        if (s_screen_registry[i].registered && s_screen_registry[i].screen_id == screen_id) {
            const sx_screen_if_t *iface = s_screen_registry[i].iface;
            xSemaphoreGive(s_registry_mutex);
            return iface;
        }
    }
    
    xSemaphoreGive(s_registry_mutex);
    return NULL;
}

