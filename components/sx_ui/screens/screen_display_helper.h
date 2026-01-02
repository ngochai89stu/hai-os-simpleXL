#pragma once

#include "sx_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handle display service events in UI task
 * 
 * This function should be called from UI task's event loop to handle
 * display service events (capture, preview, hide).
 * 
 * @param evt Event to handle
 */
void screen_display_helper_handle_event(const sx_event_t *evt);

#ifdef __cplusplus
}
#endif






