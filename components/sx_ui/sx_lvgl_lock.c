#include "sx_lvgl_lock.h"
#include "esp_lvgl_port.h"
#include <esp_log.h>

static const char *TAG = "sx_lvgl_lock";
static bool s_lock_acquired = false;  // Track lock state for debugging

sx_lvgl_lock_guard_t sx_lvgl_lock_acquire(void) {
    sx_lvgl_lock_guard_t guard = {.locked = false};
    
    // Prevent nested locks
    if (s_lock_acquired) {
        ESP_LOGE(TAG, "Nested LVGL lock detected! This may cause deadlock.");
        return guard;
    }
    
    guard.locked = lvgl_port_lock(0);
    if (guard.locked) {
        s_lock_acquired = true;
    }
    
    return guard;
}

void sx_lvgl_lock_release(sx_lvgl_lock_guard_t *guard) {
    if (guard == NULL || !guard->locked) {
        return;
    }
    
    lvgl_port_unlock();
    s_lock_acquired = false;
    guard->locked = false;
}









