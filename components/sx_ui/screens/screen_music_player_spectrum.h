#pragma once

#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#ifdef __cplusplus
extern "C" {
#endif

// Spectrum draw event callback
void spectrum_draw_event_cb(lv_event_t *e);

// Spectrum animation callback
void spectrum_anim_cb(void *a, int32_t v);

// Reset spectrum state
void spectrum_reset(void);

#ifdef __cplusplus
}
#endif










