#pragma once

#include "sx_event.h"
#include "sx_state.h"
#include <esp_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Event handler function type
 * 
 * @param evt Event to handle
 * @param state Current state (can be modified)
 * @return dirty_mask indicating which domains changed (0 = no update needed)
 *         Phase 3: Handler returns dirty_mask instead of bool to reduce coupling
 */
typedef uint32_t (*sx_event_handler_t)(const sx_event_t *evt, sx_state_t *state);

/**
 * @brief Register event handler for specific event type
 * 
 * @param event_type Event type to handle
 * @param handler Handler function
 * @return ESP_OK on success
 */
esp_err_t sx_event_handler_register(sx_event_type_t event_type, sx_event_handler_t handler);

/**
 * @brief Unregister event handler
 * 
 * @param event_type Event type to unregister
 * @return ESP_OK on success
 */
esp_err_t sx_event_handler_unregister(sx_event_type_t event_type);

/**
 * @brief Process event using registered handlers
 * 
 * @param evt Event to process
 * @param state State to update
 * @return dirty_mask indicating which domains changed (0 = not handled or no update needed)
 *         Phase 3: Returns dirty_mask instead of bool
 */
uint32_t sx_event_handler_process(const sx_event_t *evt, sx_state_t *state);

/**
 * @brief Initialize event handler system
 */
esp_err_t sx_event_handler_init(void);

#ifdef __cplusplus
}
#endif











