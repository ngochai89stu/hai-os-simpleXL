#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sx_event.h"
#include "sx_state.h"

#ifdef __cplusplus
extern "C" {
#endif

// Dispatcher is the core cross-module boundary.
// - Services: emit events only.
// - UI: reads state snapshots only and emits UI input events.
// - Orchestrator (sx_core): owns the state and processes events.

// P1.3: Backpressure policy (Section 4.3 SIMPLEXL_ARCH v1.3)
typedef enum {
    SX_BP_DROP,        // Drop if queue full (default for NORMAL/LOW)
    SX_BP_COALESCE,    // Keep only latest event (by type or key) - for duplicate events
    SX_BP_BLOCK,       // Block with timeout (only for CRITICAL priority)
} sx_backpressure_policy_t;

bool sx_dispatcher_init(void);

// Event queue: multi-producer, single-consumer (orchestrator).
// P1.3: Default policy is DROP (backward compatible)
bool sx_dispatcher_post_event(const sx_event_t *evt);

// P1.3: Post event with explicit backpressure policy
// coalesce_key: For COALESCE policy, events with same key will replace older ones
//               Use event type as key if coalesce_key = 0
bool sx_dispatcher_post_event_with_policy(
    const sx_event_t *evt,
    sx_backpressure_policy_t policy,
    uint32_t coalesce_key
);

bool sx_dispatcher_poll_event(sx_event_t *out_evt);

// State snapshot: single-writer (orchestrator), multi-reader.
// Copy-out pattern avoids sharing mutable pointers.
void sx_dispatcher_set_state(const sx_state_t *state);
void sx_dispatcher_get_state(sx_state_t *out_state);

// P1.3: Backpressure metrics (Section 4.3 SIMPLEXL_ARCH v1.3)
// Get dropped event count per priority [LOW, NORMAL, HIGH, CRITICAL]
void sx_dispatcher_get_drop_count(uint32_t drop_count[4]);

// Get coalesced event count per priority [LOW, NORMAL, HIGH, CRITICAL]
void sx_dispatcher_get_coalesce_count(uint32_t coalesce_count[4]);

#ifdef __cplusplus
}
#endif
