#include "sx_state_manager.h"
#include "sx_settings_service.h"

#include <esp_log.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "sx_state_mgr";

// State manager state
static bool s_initialized = false;
static sx_device_state_t s_current_state = SX_DEV_UNKNOWN;
static sx_state_change_cb_t s_callback = NULL;
static void *s_callback_user_data = NULL;
static SemaphoreHandle_t s_state_mutex = NULL;

// State names for logging (mapped to sx_device_state_t)
static const char* state_names[] = {
    "UNKNOWN",  // SX_DEV_UNKNOWN
    "BOOTING",  // SX_DEV_BOOTING
    "IDLE",     // SX_DEV_IDLE
    "BUSY",     // SX_DEV_BUSY
    "ERROR",    // SX_DEV_ERROR
};

esp_err_t sx_state_manager_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    s_state_mutex = xSemaphoreCreateMutex();
    if (s_state_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    s_current_state = SX_DEV_UNKNOWN;
    s_initialized = true;
    
    // Load persisted state
    sx_state_manager_load();
    
    const char* state_name = (s_current_state < sizeof(state_names)/sizeof(state_names[0])) 
                             ? state_names[s_current_state] : "INVALID";
    ESP_LOGI(TAG, "State manager initialized (current state: %s)", state_name);
    return ESP_OK;
}

esp_err_t sx_state_manager_set_state(sx_device_state_t state) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_state_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    sx_device_state_t old_state = s_current_state;
    s_current_state = state;
    
    xSemaphoreGive(s_state_mutex);
    
    const char* old_name = (old_state < sizeof(state_names)/sizeof(state_names[0])) 
                           ? state_names[old_state] : "INVALID";
    const char* new_name = (state < sizeof(state_names)/sizeof(state_names[0])) 
                           ? state_names[state] : "INVALID";
    ESP_LOGI(TAG, "State changed: %s -> %s", old_name, new_name);
    
    // Call callback if registered
    if (s_callback != NULL) {
        s_callback(old_state, state, s_callback_user_data);
    }
    
    // Persist state
    sx_state_manager_persist();
    
    return ESP_OK;
}

sx_device_state_t sx_state_manager_get_state(void) {
    return s_current_state;
}

esp_err_t sx_state_manager_register_callback(sx_state_change_cb_t callback, void *user_data) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    s_callback = callback;
    s_callback_user_data = user_data;
    
    ESP_LOGI(TAG, "State change callback %s", callback ? "registered" : "unregistered");
    return ESP_OK;
}

esp_err_t sx_state_manager_persist(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    int32_t state_int = (int32_t)s_current_state;
    esp_err_t ret = sx_settings_set_int("device_state", state_int);
    if (ret == ESP_OK) {
        sx_settings_commit();
    }
    
    return ret;
}

esp_err_t sx_state_manager_load(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    int32_t state_int = 0;
    if (sx_settings_get_int_default("device_state", &state_int, (int32_t)SX_DEV_UNKNOWN) == ESP_OK) {
        if (state_int >= 0 && state_int <= SX_DEV_ERROR) {
            s_current_state = (sx_device_state_t)state_int;
            const char* state_name = (s_current_state < sizeof(state_names)/sizeof(state_names[0])) 
                                     ? state_names[s_current_state] : "INVALID";
            ESP_LOGI(TAG, "Loaded persisted state: %s", state_name);
        }
    }
    
    return ESP_OK;
}



