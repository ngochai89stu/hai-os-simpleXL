#pragma once

#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#ifdef __cplusplus
extern "C" {
#endif

// Create playlist view
lv_obj_t *create_playlist_view(lv_obj_t *parent);

// Update button state (play/pause icon)
void playlist_button_check(size_t track_index, bool is_playing);

#ifdef __cplusplus
}
#endif










