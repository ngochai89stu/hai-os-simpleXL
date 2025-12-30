#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include "sx_state.h"

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: Enhanced State Manager
// Note: Uses sx_device_state_t from sx_state.h to avoid type conflicts

// Extended state values for state manager (mapped to sx_device_state_t)
// These are internal to state manager and map to sx_device_state_t values
#define SX_STATE_MGR_UNKNOWN      SX_DEV_UNKNOWN
#define SX_STATE_MGR_STARTING      SX_DEV_BOOTING
#define SX_STATE_MGR_CONFIGURING  SX_DEV_BUSY
#define SX_STATE_MGR_IDLE          SX_DEV_IDLE
#define SX_STATE_MGR_CONNECTING    SX_DEV_BUSY
#define SX_STATE_MGR_LISTENING     SX_DEV_BUSY
#define SX_STATE_MGR_SPEAKING      SX_DEV_BUSY
#define SX_STATE_MGR_UPGRADING     SX_DEV_BUSY
#define SX_STATE_MGR_ACTIVATING    SX_DEV_BUSY
#define SX_STATE_MGR_AUDIO_TESTING SX_DEV_BUSY
#define SX_STATE_MGR_FATAL_ERROR   SX_DEV_ERROR

// State change callback
typedef void (*sx_state_change_cb_t)(sx_device_state_t old_state, sx_device_state_t new_state, void *user_data);

// Initialize state manager
esp_err_t sx_state_manager_init(void);

// Set device state
esp_err_t sx_state_manager_set_state(sx_device_state_t state);

// Get current device state
sx_device_state_t sx_state_manager_get_state(void);

// Register state change callback
esp_err_t sx_state_manager_register_callback(sx_state_change_cb_t callback, void *user_data);

// Persist current state to NVS
esp_err_t sx_state_manager_persist(void);

// Load persisted state from NVS
esp_err_t sx_state_manager_load(void);

#ifdef __cplusplus
}
#endif



