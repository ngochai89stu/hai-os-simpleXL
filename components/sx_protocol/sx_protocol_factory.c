#include "sx_protocol_factory.h"
#include "sx_protocol_ws.h"
#include "sx_protocol_mqtt.h"

#include <esp_log.h>
#include <string.h>

static const char *TAG = "sx_protocol_factory";

// Current protocol type
static sx_protocol_type_t s_current_type = SX_PROTOCOL_TYPE_AUTO;
static bool s_initialized = false;

esp_err_t sx_protocol_factory_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    s_current_type = SX_PROTOCOL_TYPE_AUTO;
    s_initialized = true;
    
    ESP_LOGI(TAG, "Protocol factory initialized");
    return ESP_OK;
}

sx_protocol_base_t* sx_protocol_factory_get_current(void) {
    if (!s_initialized) {
        sx_protocol_factory_init();
    }
    
    // If AUTO, detect from available protocols
    if (s_current_type == SX_PROTOCOL_TYPE_AUTO) {
        // Try WebSocket first
        if (sx_protocol_ws_is_connected()) {
            sx_protocol_base_t *base = sx_protocol_ws_get_base();
            if (base && base->ops) {
                return base;
            }
        }
        
        // Try MQTT
        if (sx_protocol_mqtt_is_connected()) {
            sx_protocol_base_t *base = sx_protocol_mqtt_get_base();
            if (base && base->ops) {
                return base;
            }
        }
        
        // If not connected, return first available (for initialization)
        sx_protocol_base_t *ws_base = sx_protocol_ws_get_base();
        if (ws_base && ws_base->ops) {
            return ws_base;
        }
        
        sx_protocol_base_t *mqtt_base = sx_protocol_mqtt_get_base();
        if (mqtt_base && mqtt_base->ops) {
            return mqtt_base;
        }
        
        return NULL;
    }
    
    // Return explicitly set protocol
    switch (s_current_type) {
        case SX_PROTOCOL_TYPE_WEBSOCKET: {
            sx_protocol_base_t *base = sx_protocol_ws_get_base();
            if (base && base->ops) {
                return base;
            }
            break;
        }
        case SX_PROTOCOL_TYPE_MQTT: {
            sx_protocol_base_t *base = sx_protocol_mqtt_get_base();
            if (base && base->ops) {
                return base;
            }
            break;
        }
        default:
            break;
    }
    
    return NULL;
}

esp_err_t sx_protocol_factory_set_type(sx_protocol_type_t type) {
    if (type >= SX_PROTOCOL_TYPE_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    s_current_type = type;
    ESP_LOGI(TAG, "Protocol type set to: %d", (int)type);
    return ESP_OK;
}

sx_protocol_type_t sx_protocol_factory_get_type(void) {
    return s_current_type;
}

bool sx_protocol_factory_is_available(void) {
    sx_protocol_base_t *protocol = sx_protocol_factory_get_current();
    return (protocol != NULL && protocol->ops != NULL);
}

bool sx_protocol_factory_is_connected(void) {
    sx_protocol_base_t *protocol = sx_protocol_factory_get_current();
    if (!protocol || !protocol->ops || !protocol->ops->is_connected) {
        return false;
    }
    return protocol->ops->is_connected(protocol);
}






