#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// LVGL lock guard (RAII-style for C)
typedef struct {
    bool locked;
} sx_lvgl_lock_guard_t;

/**
 * @brief Acquire LVGL lock (returns guard, check .locked)
 * 
 * @return sx_lvgl_lock_guard_t Guard with locked flag set if lock acquired
 */
sx_lvgl_lock_guard_t sx_lvgl_lock_acquire(void);

/**
 * @brief Release LVGL lock (must match acquire)
 * 
 * @param guard Guard returned from sx_lvgl_lock_acquire
 */
void sx_lvgl_lock_release(sx_lvgl_lock_guard_t *guard);

// Macro for automatic lock/unlock
#define SX_LVGL_LOCK() \
    for (sx_lvgl_lock_guard_t _guard = sx_lvgl_lock_acquire(); \
         _guard.locked; \
         sx_lvgl_lock_release(&_guard), _guard.locked = false)

#ifdef __cplusplus
}
#endif









