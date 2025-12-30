#include "sx_ui.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "esp_lvgl_port_disp.h"
#include "esp_lvgl_port_touch.h"

#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_state.h"
#include "ui_router.h"
#include "ui_screen_registry.h"
#include "screens/register_all_screens.h"
#include "sx_ui_verify.h"

static const char *TAG = "sx_ui";

#if LVGL_VERSION_MAJOR != 9
#error "LVGL major version mismatch: esp_lvgl_port requires LVGL v9"
#endif

// Phase 3: UI task arguments (display + optional touch)
typedef struct {
    const sx_display_handles_t *display_handles;
    const sx_touch_handles_t *touch_handles;
} sx_ui_task_args_t;

static void sx_ui_task(void *arg) {
    ESP_LOGI(TAG, "LVGL version: %d.%d.%d, Color depth: %d", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH, LV_COLOR_DEPTH);
    const sx_ui_task_args_t *args = (const sx_ui_task_args_t *)arg;
    const sx_display_handles_t *handles = args->display_handles;
    const sx_touch_handles_t *touch_handles = args->touch_handles;
    ESP_LOGI(TAG, "UI task started");

    // Initialize LVGL port (vendored esp_lvgl_port uses lvgl_port_* API)
    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_cfg.task_stack = 6144; // Optimized: Reduced from 8192 to 6144 (sufficient)
    esp_err_t err = lvgl_port_init(&lvgl_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "lvgl_port_init failed with error: 0x%x", err);
        vTaskDelay(portMAX_DELAY);
    }

    // All LVGL API calls must be protected by a mutex
    if (lvgl_port_lock(0)) {
        ESP_LOGI(TAG, "LVGL mutex obtained, adding display...");
        
        // Validate display handles
        if (handles == NULL || handles->io_handle == NULL || handles->panel_handle == NULL) {
            ESP_LOGE(TAG, "Invalid display handles: handles=%p, io_handle=%p, panel_handle=%p", 
                     (void*)handles, 
                     handles ? (void*)handles->io_handle : NULL,
                     handles ? (void*)handles->panel_handle : NULL);
            lvgl_port_unlock();
            return;
        }

        // Register our LCD panel as an LVGL display
        const lvgl_port_display_cfg_t disp_cfg = {
            .io_handle = handles->io_handle,
            .panel_handle = handles->panel_handle,
            .control_handle = NULL,

            // Optimized: Increased buffer size for better scrolling performance
            // Recommended >= 1/10 screen; 320*30 = 9600 (~1/15 of 320*480)
            .buffer_size = 320 * 30,  // Increased from 20 to 30 lines
            .double_buffer = true,     // Enable double buffering for smoother rendering
            .trans_size = 0,

            .hres = 320,
            .vres = 480,

            .monochrome = false,

            .rotation = {
                .swap_xy = false,
                .mirror_x = true,  // Fix: mirror X to correct reversed text
                .mirror_y = false,
            },

            .color_format = LV_COLOR_FORMAT_RGB565,

            .flags = {
                .buff_dma = 0,
                .buff_spiram = 1,
                .sw_rotate = 0,
                .swap_bytes = 1,  // Enable byte swap for BGR mode (matching xiaozhi-esp32-vietnam)
                .full_refresh = 0,
                .direct_mode = 0,
            },
        };
        // STEP 2: Log all buffer config values for verification
        ESP_LOGI(TAG, "disp_cfg: hres=%d, vres=%d, buffer_size=%d, double_buffer=%d, spiram=%d, dma=%d",
                 disp_cfg.hres, disp_cfg.vres, (int)disp_cfg.buffer_size, disp_cfg.double_buffer, disp_cfg.flags.buff_spiram, disp_cfg.flags.buff_dma);

        lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);

        if (disp == NULL) {
            ESP_LOGE(TAG, "lvgl_port_add_disp failed (out of memory or invalid config)");
        } else {
            ESP_LOGI(TAG, "LVGL display added: disp=%p", (void *)disp);

            // Phase 2: Initialize UI router (creates screen container + init registry)
            ui_router_init();

            // Phase 2: Register all screens (29 screens total)
            register_all_screens();
            
            // Verification: Initialize verify system
            #if SX_UI_VERIFY_MODE
            sx_ui_verify_init();
            #endif
            
            // Phase 3: Add touch input device (if available)
            if (touch_handles != NULL && touch_handles->touch_handle != NULL) {
                const lvgl_port_touch_cfg_t touch_cfg = {
                    .disp = disp,
                    .handle = touch_handles->touch_handle,
                };
                lv_indev_t *touch_indev = lvgl_port_add_touch(&touch_cfg);
                if (touch_indev != NULL) {
                    ESP_LOGI(TAG, "Touch input device added");
                    
                    // Verification: Check touch indev
                    #if SX_UI_VERIFY_MODE
                    lv_indev_t *indev = lv_indev_get_next(NULL);
                    bool found_pointer = false;
                    while (indev != NULL) {
                        lv_indev_type_t type = lv_indev_get_type(indev);
                        ESP_LOGI(TAG, "[VERIFY] Found indev type=%d ptr=%p", type, (void *)indev);
                        if (type == LV_INDEV_TYPE_POINTER) {
                            found_pointer = true;
                            ESP_LOGI(TAG, "[VERIFY] ✓ Pointer indev found: %p", (void *)indev);
                        }
                        indev = lv_indev_get_next(indev);
                    }
                    if (!found_pointer) {
                        ESP_LOGE(TAG, "[VERIFY] ERROR: TOUCH_NOT_READY - No pointer indev found!");
                    }
                    #endif
                } else {
                    ESP_LOGW(TAG, "Failed to add touch input device");
                    #if SX_UI_VERIFY_MODE
                    ESP_LOGE(TAG, "[VERIFY] ERROR: TOUCH_NOT_READY - Touch indev registration failed!");
                    #endif
                }
            } else {
                ESP_LOGI(TAG, "Touch not available, skipping touch input device");
            }
            
            // Phase 2: Navigate to boot screen initially
            ui_router_navigate_to(SCREEN_ID_BOOT);
            
            // Post UI ready event
            sx_event_t evt = {
                .type = SX_EVT_UI_READY,
                .arg0 = 0,
                .arg1 = 0,
                .ptr = NULL
            };
            sx_dispatcher_post_event(&evt);
        }

        lvgl_port_unlock();

        if (disp == NULL) {
            vTaskDelay(portMAX_DELAY);
        }

    } else {
        ESP_LOGE(TAG, "Failed to take LVGL mutex");
        vTaskDelay(portMAX_DELAY);
    }

    // Phase 2: Main UI loop - poll state and handle navigation events
    sx_state_t state;
    ui_screen_id_t last_screen = SCREEN_ID_BOOT;
    uint32_t flash_start_time = 0;
    bool flash_shown = false;
    uint32_t last_state_seq = 0;  // Optimized: Track state sequence to detect changes
    
    // Optimized: Use vTaskDelayUntil for consistent frame timing
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t render_interval = pdMS_TO_TICKS(16);  // ~60 FPS
    
    for (;;) {
        // Poll state snapshot (read-only)
        sx_dispatcher_get_state(&state);
        
        // Optimized: Only update UI if state actually changed
        bool state_changed = (state.seq != last_state_seq);
        
        // Phase 2: Simple navigation based on device state (demo)
        // Boot sequence: BOOT (3s) → FLASH (2s) → HOME
        ui_screen_id_t target_screen = SCREEN_ID_BOOT;
        
        if (state.ui.device_state == SX_DEV_BOOTING) {
            target_screen = SCREEN_ID_BOOT;
            flash_shown = false;
        } else if (state.ui.device_state == SX_DEV_IDLE) {
            // Boot sequence: BOOT (3s) → FLASH (screensaver/waiting screen)
            // Navigation is handled by:
            // - BootScreen timer: BOOT → FLASH (after 3s)
            // - FlashScreen: Swipe gesture to navigate to HOME
            // - HomeScreen: 30s idle timeout to return to FLASH
            // - Main loop: Only track current screen, don't force navigation
            
            ui_screen_id_t current_screen = ui_router_get_current_screen();
            
            if (current_screen == SCREEN_ID_BOOT) {
                // Still on boot screen - let timer handle navigation to FLASH
                target_screen = SCREEN_ID_BOOT;
            } else if (current_screen == SCREEN_ID_FLASH) {
                // Stay on flash screen (screensaver) - user must swipe to unlock
                target_screen = SCREEN_ID_FLASH;
            } else {
                // Default to current screen (don't force navigation)
                target_screen = current_screen;
            }
        }
        
        // Navigate if screen changed (but skip if already on target to prevent duplicate)
        if (target_screen != last_screen) {
            ui_screen_id_t current_screen = ui_router_get_current_screen();
            // Only navigate if target is different from current screen
            if (target_screen != current_screen) {
                ESP_LOGI("sx_ui", "Main loop navigate: target=%d current=%d last=%d", 
                         target_screen, current_screen, last_screen);
                if (lvgl_port_lock(0)) {
                    ui_router_navigate_to(target_screen);
                    lvgl_port_unlock();
                }
            }
            last_screen = target_screen;
        }
        
        // Update current screen UI from state (if screen supports it and state changed)
        if (state_changed) {
            last_state_seq = state.seq;
            ui_screen_id_t current_screen = ui_router_get_current_screen();
            if (current_screen != SCREEN_ID_MAX) {
                const ui_screen_callbacks_t *callbacks = ui_screen_registry_get(current_screen);
                if (callbacks && callbacks->on_update) {
                    callbacks->on_update(&state);
                }
            }
        }
        
        // LVGL tick (always needed for smooth rendering)
        if (lvgl_port_lock(0)) {
            lv_timer_handler();
            lvgl_port_unlock();
        }
        
        // Optimized: Fixed interval for consistent frame rate
        vTaskDelayUntil(&last_wake_time, render_interval);
    }
}

esp_err_t sx_ui_start(const sx_display_handles_t *handles, const sx_touch_handles_t *touch_handles) {
    if (handles == NULL || handles->panel_handle == NULL) {
        ESP_LOGE(TAG, "Display handles or panel handle is NULL, cannot start UI task");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Phase 3: Prepare task arguments (display + optional touch)
    static sx_ui_task_args_t task_args;
    task_args.display_handles = handles;
    task_args.touch_handles = touch_handles; // Can be NULL
    
    // Create the UI owner task, passing the task args struct.
    // Optimized: Reduced stack size from 12KB to 8KB (sufficient for UI rendering)
    xTaskCreatePinnedToCore(sx_ui_task, "sx_ui", 1024 * 8, (void *)&task_args, 5, NULL, tskNO_AFFINITY);
    return ESP_OK;
}
