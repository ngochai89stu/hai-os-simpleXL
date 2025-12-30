#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ui_screen_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

// Enable verification mode (compile-time flag)
// Set to 1 to enable verification instrumentation
// DISABLED for commercial firmware
#ifndef SX_UI_VERIFY_MODE
#define SX_UI_VERIFY_MODE 0  // Disabled for commercial release
#endif

#if SX_UI_VERIFY_MODE

// Screen creation evidence structure
typedef struct {
    ui_screen_id_t screen_id;
    const char *screen_name;
    void *container_ptr;      // lv_obj_t* container
    void *root_ptr;           // lv_obj_t* root object
    uint32_t create_count;
    uint32_t show_count;
    uint32_t hide_count;
    uint32_t destroy_count;
    uint64_t last_create_ts;  // Timestamp in ms
    uint64_t last_show_ts;
    bool touch_ok;            // Touch event received
    uint32_t touch_count;
} sx_ui_verify_screen_evidence_t;

// Verification API
void sx_ui_verify_init(void);
void sx_ui_verify_on_create(ui_screen_id_t screen_id, const char *screen_name, 
                            void *container, void *root);
void sx_ui_verify_on_show(ui_screen_id_t screen_id);
void sx_ui_verify_on_hide(ui_screen_id_t screen_id);
void sx_ui_verify_on_destroy(ui_screen_id_t screen_id);
void sx_ui_verify_mark_touch_ok(ui_screen_id_t screen_id);

// Reporting
void sx_ui_verify_dump_report(void);
const char* sx_ui_verify_get_report_string(void);
const sx_ui_verify_screen_evidence_t* sx_ui_verify_get_evidence(ui_screen_id_t screen_id);

// Screen walk mode
void sx_ui_verify_start_screen_walk(void);
bool sx_ui_verify_is_walking(void);

#endif // SX_UI_VERIFY_MODE

#ifdef __cplusplus
}
#endif





