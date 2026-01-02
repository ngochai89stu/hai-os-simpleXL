/**
 * @file screen_display_helper.c
 * @brief UI helper to handle display service events (P0.1)
 * 
 * This file handles events from display service and performs LVGL operations.
 * This is the ONLY place where display service events trigger LVGL calls.
 */

#include "screen_display_helper.h"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_event_payloads.h"
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)
#include "sx_lvgl_lock.h"
#include "sx_image_service.h"  // For image service types

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/timers.h"

static const char *TAG = "ui_display_helper";

// Display helper state
static lv_obj_t *s_preview_image_obj = NULL;
static TimerHandle_t s_preview_timer = NULL;

// Preview timer callback
static void preview_timer_callback(TimerHandle_t xTimer) {
    (void)xTimer;
    // Request hide via event (to keep service interface clean)
    sx_event_t evt = {
        .type = SX_EVT_DISPLAY_HIDE_REQUEST,
        .priority = SX_EVT_PRIORITY_NORMAL,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = NULL
    };
    sx_dispatcher_post_event(&evt);
}

// Handle display capture request
static void handle_display_capture_request(const sx_event_t *evt) {
    uint16_t width = (uint16_t)evt->arg0;
    uint16_t height = (uint16_t)evt->arg1;
    uint8_t *buffer = (uint8_t *)evt->ptr;  // Service-provided buffer
    
    if (!buffer) {
        ESP_LOGE(TAG, "Capture request: buffer is NULL");
        return;
    }
    
    SX_LVGL_LOCK() {
        // Get default display
        lv_display_t *disp = lv_display_get_default();
        if (!disp) {
            ESP_LOGE(TAG, "No default display available");
            return;
        }
        
        // Get display dimensions
        int32_t disp_width = lv_display_get_horizontal_resolution(disp);
        int32_t disp_height = lv_display_get_vertical_resolution(disp);
        
        // Validate dimensions
        if (width != disp_width || height != disp_height) {
            ESP_LOGW(TAG, "Requested dimensions (%dx%d) don't match display (%dx%d)", 
                     width, height, disp_width, disp_height);
            width = disp_width;
            height = disp_height;
        }
        
        // Create a canvas to capture screen
        lv_obj_t *canvas = lv_canvas_create(lv_scr_act());
        if (!canvas) {
            ESP_LOGE(TAG, "Failed to create canvas");
            return;
        }
        
        // Set canvas size
        lv_canvas_set_buffer(canvas, buffer, width, height, LV_COLOR_FORMAT_RGB565);
        
        // Copy screen content to canvas
        lv_obj_t *screen = lv_scr_act();
        lv_obj_invalidate(screen);
        lv_refr_now(disp);
        
        // Note: Actual screen capture requires display buffer access
        // This is a placeholder - fill with test pattern
        ESP_LOGW(TAG, "Screen capture placeholder - fill with test pattern");
        uint16_t *rgb565_ptr = (uint16_t *)buffer;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                rgb565_ptr[y * width + x] = 0x0000; // Black
            }
        }
        
        // Cleanup
        lv_obj_del(canvas);
        
        // Send capture done event back to service
        sx_display_capture_payload_t *payload = (sx_display_capture_payload_t *)malloc(sizeof(sx_display_capture_payload_t));
        if (payload) {
            payload->buffer = buffer;
            payload->width = width;
            payload->height = height;
            payload->buffer_size = width * height * 2;
            
            sx_event_t done_evt = {
                .type = SX_EVT_DISPLAY_CAPTURE_DONE,
                .priority = SX_EVT_PRIORITY_NORMAL,
                .arg0 = 0,
                .arg1 = 0,
                .ptr = payload  // Service will handle and free
            };
            sx_dispatcher_post_event(&done_evt);
        }
    }
    
    ESP_LOGI(TAG, "Screen captured: %dx%d", width, height);
}

// Handle display preview request
static void handle_display_preview_request(const sx_event_t *evt) {
    sx_display_preview_payload_t *payload = (sx_display_preview_payload_t *)evt->ptr;
    if (!payload || !payload->image_data) {
        ESP_LOGE(TAG, "Preview request: invalid payload");
        return;
    }
    
    SX_LVGL_LOCK() {
        // Remove existing preview if any
        if (s_preview_image_obj) {
            lv_obj_del(s_preview_image_obj);
            s_preview_image_obj = NULL;
        }
        
        // Stop existing timer if any
        if (s_preview_timer) {
            xTimerStop(s_preview_timer, 0);
            xTimerDelete(s_preview_timer, 0);
            s_preview_timer = NULL;
        }
        
        // Copy image data (UI will own it)
        size_t data_size = payload->width * payload->height * 2; // RGB565 = 2 bytes per pixel
        uint8_t *image_copy = (uint8_t *)malloc(data_size);
        if (!image_copy) {
            ESP_LOGE(TAG, "Failed to allocate image copy");
            free(payload->image_data);  // Free service copy
            free(payload);
            return;
        }
        memcpy(image_copy, payload->image_data, data_size);
        free(payload->image_data);  // Free service copy
        
        // Lưu các giá trị cần thiết trước khi free payload (fix use-after-free)
        uint16_t img_width = payload->width;
        uint16_t img_height = payload->height;
        uint32_t timeout_ms = payload->timeout_ms;
        free(payload);
        
        // Create LVGL image descriptor from RGB565 data
        // Note: UI helper creates LVGL image descriptor directly
        lv_image_dsc_t *img_dsc = (lv_image_dsc_t *)malloc(sizeof(lv_image_dsc_t));
        if (!img_dsc) {
            free(image_copy);
            ESP_LOGE(TAG, "Failed to allocate image descriptor");
            return;
        }
        
        img_dsc->header.w = img_width;
        img_dsc->header.h = img_height;
        img_dsc->header.cf = LV_COLOR_FORMAT_RGB565;
        img_dsc->data_size = data_size;
        img_dsc->data = image_copy;
        
        // Create image object
        lv_obj_t *screen = lv_scr_act();
        s_preview_image_obj = lv_img_create(screen);
        if (!s_preview_image_obj) {
            free(img_dsc);
            free(image_copy);
            ESP_LOGE(TAG, "Failed to create image object");
            return;
        }
        
        // Set image source
        lv_img_set_src(s_preview_image_obj, img_dsc);
        
        // Center image
        lv_obj_align(s_preview_image_obj, LV_ALIGN_CENTER, 0, 0);
        
        // Set size
        lv_obj_set_size(s_preview_image_obj, img_width, img_height);
        
        // Bring to front
        lv_obj_move_foreground(s_preview_image_obj);
        
        // Create timer if timeout specified
        if (timeout_ms > 0) {
            s_preview_timer = xTimerCreate("preview_timer", pdMS_TO_TICKS(timeout_ms), 
                                          pdFALSE, NULL, preview_timer_callback);
            if (s_preview_timer) {
                xTimerStart(s_preview_timer, 0);
            }
        }
        
        // Send preview done event back to service
        sx_event_t done_evt = {
            .type = SX_EVT_DISPLAY_PREVIEW_DONE,
            .priority = SX_EVT_PRIORITY_NORMAL,
            .arg0 = 0,
            .arg1 = 0,
            .ptr = NULL
        };
        sx_dispatcher_post_event(&done_evt);
    }
    
    ESP_LOGI(TAG, "Image preview displayed: %dx%d (timeout: %lu ms)", 
             payload->width, payload->height, payload->timeout_ms);
}

// Handle display hide request
static void handle_display_hide_request(const sx_event_t *evt) {
    (void)evt;
    
    SX_LVGL_LOCK() {
        // Remove image object
        if (s_preview_image_obj) {
            // Get image descriptor to free it
            const lv_image_dsc_t *img_dsc = lv_img_get_src(s_preview_image_obj);
            if (img_dsc && img_dsc->data) {
                // Free the image data
                free((void *)img_dsc->data);
            }
            // Free the descriptor structure
            if (img_dsc) {
                free((void *)img_dsc);
            }
            lv_obj_del(s_preview_image_obj);
            s_preview_image_obj = NULL;
        }
        
        // Stop timer
        if (s_preview_timer) {
            xTimerStop(s_preview_timer, 0);
            xTimerDelete(s_preview_timer, 0);
            s_preview_timer = NULL;
        }
    }
    
    ESP_LOGI(TAG, "Preview hidden");
}

// Public API: Handle display service events
void screen_display_helper_handle_event(const sx_event_t *evt) {
    if (!evt) {
        return;
    }
    
    switch (evt->type) {
        case SX_EVT_DISPLAY_CAPTURE_REQUEST:
            handle_display_capture_request(evt);
            break;
        case SX_EVT_DISPLAY_PREVIEW_REQUEST:
            handle_display_preview_request(evt);
            break;
        case SX_EVT_DISPLAY_HIDE_REQUEST:
            handle_display_hide_request(evt);
            break;
        default:
            // Not a display event
            break;
    }
}

