#include "sx_lvgl_guard.h"

// UI task handle storage (Section 7.3 SIMPLEXL_ARCH v1.3)
TaskHandle_t g_sx_ui_task_handle = NULL;

void sx_ui_set_ui_task_handle(TaskHandle_t handle) {
    g_sx_ui_task_handle = handle;
    ESP_LOGI("SX_LVGL_GUARD", "UI task handle registered: %p", (void*)handle);
}






