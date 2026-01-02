#pragma once

#include "ui_screen_registry.h"
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#ifdef __cplusplus
extern "C" {
#endif

// Initializes the UI router and creates the main screen container.
// Must be called from UI task after LVGL is initialized.
void ui_router_init(void);

// Navigates to the specified screen.
// Must be called from UI task (or queued via dispatcher).
void ui_router_navigate_to(ui_screen_id_t screen_id);

// Get current active screen ID
ui_screen_id_t ui_router_get_current_screen(void);

// Get the screen container object (for screen implementations)
lv_obj_t* ui_router_get_container(void);

#ifdef __cplusplus
}
#endif


