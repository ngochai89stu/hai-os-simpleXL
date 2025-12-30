#pragma once

#include <esp_err.h>
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// A screen in the SimpleXL architecture is a function that creates the UI.
typedef lv_obj_t* (*sx_screen_create_func_t)(lv_obj_t *parent);

// Screen IDs will be defined in a shared header later.
typedef enum {
    SX_SCREEN_ID_BOOT = 0,
    SX_SCREEN_ID_HOME,
    // ... other screens
    SX_SCREEN_ID_MAX,
} sx_screen_id_t;

void sx_ui_router_init(void);
void sx_ui_router_register_screen(sx_screen_id_t id, sx_screen_create_func_t create_func);
void sx_ui_router_navigate_to(sx_screen_id_t id);

#ifdef __cplusplus
}
#endif

