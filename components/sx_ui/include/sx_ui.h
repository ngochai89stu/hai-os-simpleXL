#pragma once

#include <esp_err.h>
#include "sx_platform.h" // For sx_display_handles_t, sx_touch_handles_t

#ifdef __cplusplus
extern "C" {
#endif

// Phase 2: Start the UI owner task with a given display panel handle.
// Phase 3: Optional touch handles (can be NULL if touch not available)
esp_err_t sx_ui_start(const sx_display_handles_t *handles, const sx_touch_handles_t *touch_handles);

#ifdef __cplusplus
}
#endif
