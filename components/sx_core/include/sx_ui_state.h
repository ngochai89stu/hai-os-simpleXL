#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Input events that UI can emit.
typedef enum {
    SX_UI_INPUT_NONE = 0,
    SX_UI_INPUT_BUTTON_A,
    SX_UI_INPUT_BUTTON_B,
    SX_UI_INPUT_TOUCH_TAP,
} sx_ui_input_type_t;

typedef struct {
    sx_ui_input_type_t type;
    int16_t x;
    int16_t y;
} sx_ui_input_event_t;

#ifdef __cplusplus
}
#endif
