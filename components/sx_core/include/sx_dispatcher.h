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

bool sx_dispatcher_init(void);

// Event queue: multi-producer, single-consumer (orchestrator).
bool sx_dispatcher_post_event(const sx_event_t *evt);
bool sx_dispatcher_poll_event(sx_event_t *out_evt);

// State snapshot: single-writer (orchestrator), multi-reader.
// Copy-out pattern avoids sharing mutable pointers.
void sx_dispatcher_set_state(const sx_state_t *state);
void sx_dispatcher_get_state(sx_state_t *out_state);

#ifdef __cplusplus
}
#endif
