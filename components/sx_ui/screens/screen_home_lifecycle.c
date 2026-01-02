/**
 * @file screen_home_lifecycle.c
 * @brief Home screen lifecycle interface implementation (OPT-3)
 * 
 * This file provides an example migration of screen_home to use
 * the lifecycle interface (sx_screen_if.h) defined in P1.4.
 */

#include "screen_home.h"
#include "sx_screen_if.h"
#include "sx_state.h"
#include "sx_lvgl.h"
#include <esp_log.h>

static const char *TAG = "screen_home_lifecycle";

// Forward declarations from screen_home.c
extern lv_obj_t* screen_home_create(lv_obj_t *parent);
extern void screen_home_destroy(void);
extern void screen_home_on_enter(void);
extern void screen_home_on_exit(void);
extern void screen_home_on_state_change(const sx_state_t *state);

// Lifecycle interface implementation
static lv_obj_t* home_screen_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Home screen create (lifecycle)");
    return screen_home_create(parent);
}

static void home_screen_destroy(lv_obj_t *screen) {
    ESP_LOGI(TAG, "Home screen destroy (lifecycle)");
    (void)screen;  // screen_home_destroy doesn't take parameter
    screen_home_destroy();
}

static void home_screen_on_enter(lv_obj_t *screen) {
    ESP_LOGI(TAG, "Home screen on_enter (lifecycle)");
    (void)screen;
    screen_home_on_enter();
}

static void home_screen_on_exit(lv_obj_t *screen) {
    ESP_LOGI(TAG, "Home screen on_exit (lifecycle)");
    (void)screen;
    screen_home_on_exit();
}

static void home_screen_on_state_change(lv_obj_t *screen, uint32_t dirty_mask, const sx_state_t *state) {
    ESP_LOGD(TAG, "Home screen on_state_change (lifecycle), dirty_mask=0x%lx", (unsigned long)dirty_mask);
    (void)screen;
    (void)dirty_mask;  // screen_home_on_state_change doesn't use dirty_mask yet
    screen_home_on_state_change(state);
}

// Screen interface vtable
static const sx_screen_if_t s_home_screen_if = {
    .create = home_screen_create,
    .destroy = home_screen_destroy,
    .on_enter = home_screen_on_enter,
    .on_exit = home_screen_on_exit,
    .on_state_change = home_screen_on_state_change,
};

// Registration function (call this during UI initialization)
esp_err_t screen_home_register_lifecycle(void) {
    return sx_screen_register(SCREEN_ID_HOME, &s_home_screen_if);
}






