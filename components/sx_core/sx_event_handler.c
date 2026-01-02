#include "sx_event_handler.h"
#include <esp_log.h>
#include <string.h>

#define MAX_EVENT_TYPES 64

static sx_event_handler_t s_handlers[MAX_EVENT_TYPES] = {0};
static bool s_initialized = false;
static const char *TAG = "sx_event_handler";

esp_err_t sx_event_handler_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    memset(s_handlers, 0, sizeof(s_handlers));
    s_initialized = true;
    ESP_LOGI(TAG, "Event handler system initialized");
    return ESP_OK;
}

esp_err_t sx_event_handler_register(sx_event_type_t event_type, sx_event_handler_t handler) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (event_type >= MAX_EVENT_TYPES || handler == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_handlers[event_type] = handler;
    ESP_LOGI(TAG, "Registered handler for event type %d", event_type);
    return ESP_OK;
}

esp_err_t sx_event_handler_unregister(sx_event_type_t event_type) {
    if (!s_initialized || event_type >= MAX_EVENT_TYPES) {
        return ESP_ERR_INVALID_ARG;
    }
    s_handlers[event_type] = NULL;
    ESP_LOGI(TAG, "Unregistered handler for event type %d", event_type);
    return ESP_OK;
}

bool sx_event_handler_process(const sx_event_t *evt, sx_state_t *state) {
    if (!s_initialized || evt == NULL || state == NULL) {
        return false;
    }
    
    if (evt->type >= MAX_EVENT_TYPES) {
        ESP_LOGW(TAG, "Invalid event type: %d", evt->type);
        return false;
    }
    
    sx_event_handler_t handler = s_handlers[evt->type];
    if (handler == NULL) {
        ESP_LOGD(TAG, "No handler registered for event type %d", evt->type);
        return false;
    }
    
    return handler(evt, state);
}









