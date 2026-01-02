#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <assert.h>
#include <esp_log.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file sx_lvgl_guard.h
 * @brief Runtime guard for LVGL calls (Section 7.3 SIMPLEXL_ARCH v1.3)
 * 
 * This file provides runtime assertions to ensure LVGL calls are only made
 * from the UI task. This is a MUST requirement in Section 7.3.
 */

// UI task handle (set by sx_ui_task)
extern TaskHandle_t g_sx_ui_task_handle;

/**
 * @brief Set UI task handle (call from sx_ui_task)
 * 
 * This must be called once from sx_ui_task() to register the UI task handle.
 * 
 * @param handle Task handle of the UI task
 */
void sx_ui_set_ui_task_handle(TaskHandle_t handle);

/**
 * @brief Runtime assert macro to verify we're in UI task
 * 
 * This macro checks if the current task is the UI task. If not, it logs an error
 * and asserts (if enabled).
 * 
 * Usage:
 *   SX_ASSERT_UI_THREAD();
 *   lv_obj_create(...);
 */
#ifdef CONFIG_SX_ENABLE_UI_THREAD_ASSERT
#define SX_ASSERT_UI_THREAD() \
    do { \
        if (g_sx_ui_task_handle != NULL) { \
            TaskHandle_t current_task = xTaskGetCurrentTaskHandle(); \
            if (current_task != g_sx_ui_task_handle) { \
                ESP_LOGE("SX_LVGL_GUARD", \
                         "LVGL call from non-UI task! Current: %p, UI: %p, Task: %s", \
                         (void*)current_task, \
                         (void*)g_sx_ui_task_handle, \
                         pcTaskGetName(current_task)); \
                assert(0 && "LVGL call must be in UI task"); \
            } \
        } \
    } while(0)
#else
#define SX_ASSERT_UI_THREAD() ((void)0)
#endif

/**
 * @brief Wrapper macro for LVGL calls with thread verification
 * 
 * This macro wraps LVGL calls with runtime thread assertion.
 * 
 * Usage:
 *   SX_LVGL_CALL(lv_obj_create(parent));
 *   SX_LVGL_CALL(lv_obj_set_size(obj, 100, 100));
 */
#define SX_LVGL_CALL(stmt) \
    do { \
        SX_ASSERT_UI_THREAD(); \
        stmt; \
    } while(0)

#ifdef __cplusplus
}
#endif






