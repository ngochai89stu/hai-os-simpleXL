# SIMPLEXL_ARCH (hai-os-simplexl)

This document defines the **non-negotiable** architecture rules for the new repo.

## 1) Goals
- No legacy UI, no legacy build graph.
- Single UI owner task for **all LVGL calls**.
- Services never include UI headers.
- UI ↔ services communication happens only via **events** and **state snapshots**.

## 2) Component boundaries

### `sx_core`
- Owns: `sx_event`, `sx_state`, `sx_dispatcher`, `sx_bootstrap`, `sx_orchestrator`.
- **Single writer** for `sx_state_t`.
- Consumes events from queue.

### `sx_ui`
- Owns: UI task (LVGL init + render loop in Phase 2).
- Reads `sx_state_t` snapshots.
- Emits `SX_EVT_UI_INPUT` events only.
- Forbidden: include service headers.

### `sx_platform` (Phase 3)
- Owns: LCD/touch/backlight/SD drivers.
- Forbidden: LVGL calls unless explicitly inside UI task integration points.

### `sx_services` (Phase 4)
- Owns: audio/wifi/ir/mcp/chatbot.
- Emits events and exposes internal service APIs only to `sx_core` (not UI).
- Forbidden: include `sx_ui/*`.

## 3) Dispatch model

- **Event queue**: multi-producer, single-consumer.
  - Producers: UI + services
  - Consumer: orchestrator

- **State snapshot**: single-writer, multi-reader.
  - Writer: orchestrator
  - Readers: UI (and optionally services)

## 4) Ownership rules
- Only UI task may call LVGL APIs.
- Orchestrator is the single source of truth for state.

## 5) Phase status
- Phase 0: PASS (build clean, boots).
- Phase 1: event/state/dispatcher wired; UI task exists and reads state.
