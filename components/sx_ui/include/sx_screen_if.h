#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>
#include "sx_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file sx_screen_if.h
 * @brief Screen lifecycle interface (Section 6.2 SIMPLEXL_ARCH v1.3)
 * 
 * This file defines the mandatory screen interface vtable.
 * All screens MUST implement this interface for lifecycle management.
 */

// Forward declaration
typedef struct _lv_obj_t lv_obj_t;

// Screen lifecycle interface vtable
typedef struct {
    /**
     * @brief Create screen UI (allocate and create LVGL objects)
     * Called once when screen is first created
     * @param parent Parent LVGL object (screen container)
     * @return Screen object (lv_obj_t*) or NULL on error
     */
    lv_obj_t* (*create)(lv_obj_t *parent);
    
    /**
     * @brief Destroy screen UI (free LVGL objects)
     * Called when screen is being destroyed
     * @param screen Screen object returned from create()
     */
    void (*destroy)(lv_obj_t *screen);
    
    /**
     * @brief Called when screen becomes active (entered)
     * Screen should prepare for display, start animations, etc.
     * @param screen Screen object
     */
    void (*on_enter)(lv_obj_t *screen);
    
    /**
     * @brief Called when screen becomes inactive (exited)
     * Screen should cleanup, stop animations, etc.
     * @param screen Screen object
     */
    void (*on_exit)(lv_obj_t *screen);
    
    /**
     * @brief Called when state changes (with dirty_mask)
     * Screen should update UI based on changed state domains
     * @param screen Screen object
     * @param dirty_mask Bitmask of changed domains (SX_STATE_DIRTY_*)
     * @param state Current state snapshot
     */
    void (*on_state_change)(lv_obj_t *screen, uint32_t dirty_mask, const sx_state_t *state);
} sx_screen_if_t;

/**
 * @brief Register screen with lifecycle interface
 * 
 * @param screen_id Screen ID (from ui_screen_id_t)
 * @param iface Screen interface vtable
 * @return ESP_OK on success
 */
esp_err_t sx_screen_register(uint32_t screen_id, const sx_screen_if_t *iface);

/**
 * @brief Unregister screen
 * 
 * @param screen_id Screen ID
 * @return ESP_OK on success
 */
esp_err_t sx_screen_unregister(uint32_t screen_id);

/**
 * @brief Get screen interface
 * 
 * @param screen_id Screen ID
 * @return Screen interface or NULL if not registered
 */
const sx_screen_if_t* sx_screen_get_interface(uint32_t screen_id);

#ifdef __cplusplus
}
#endif

