#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include "sx_protocol_base.h"

#ifdef __cplusplus
extern "C" {
#endif

// Protocol types
typedef enum {
    SX_PROTOCOL_TYPE_WEBSOCKET = 0,
    SX_PROTOCOL_TYPE_MQTT,
    SX_PROTOCOL_TYPE_AUTO,  // Auto-detect from config (default)
    SX_PROTOCOL_TYPE_COUNT
} sx_protocol_type_t;

// Initialize protocol factory
esp_err_t sx_protocol_factory_init(void);

// Get current protocol instance (auto-detect if not set)
// Returns NULL if no protocol is available
sx_protocol_base_t* sx_protocol_factory_get_current(void);

// Set protocol type explicitly
esp_err_t sx_protocol_factory_set_type(sx_protocol_type_t type);

// Get current protocol type
sx_protocol_type_t sx_protocol_factory_get_type(void);

// Check if protocol is available and connected
bool sx_protocol_factory_is_available(void);
bool sx_protocol_factory_is_connected(void);

#ifdef __cplusplus
}
#endif






