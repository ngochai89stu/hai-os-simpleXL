#pragma once

#include "sx_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Helper function to update state with version and dirty_mask (P0.4)
 * 
 * This function increments version and sets dirty_mask based on which domains changed.
 * Use this helper when updating state in orchestrator.
 * 
 * @param state State to update
 * @param dirty_mask Bitmask of changed domains (SX_STATE_DIRTY_*)
 */
static inline void sx_state_update_version_and_dirty(sx_state_t *state, uint32_t dirty_mask) {
    if (state) {
        state->version++;  // Increment version on every state change
        state->dirty_mask = dirty_mask;  // Set dirty mask for changed domains
        state->seq++;  // Keep seq for backward compatibility
    }
}

#ifdef __cplusplus
}
#endif






