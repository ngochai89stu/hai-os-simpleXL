#include "sx_theme_service.h"
#include "sx_settings_service.h"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_event_payloads.h"

#include <esp_log.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "sx_theme";

// Theme state
static bool s_initialized = false;
static sx_theme_type_t s_current_theme = SX_THEME_DARK;
static sx_theme_change_cb_t s_change_callback = NULL;
static SemaphoreHandle_t s_theme_mutex = NULL;

// Dark theme colors
static const sx_theme_colors_t s_dark_theme = {
    .bg_primary = 0x1a1a1a,
    .bg_secondary = 0x2a2a2a,
    .bg_tertiary = 0x3a3a3a,
    .text_primary = 0xFFFFFF,
    .text_secondary = 0x888888,
    .accent = 0x5b7fff,
    .error = 0xFF0000,
    .success = 0x00FF00,
};

// Light theme colors
static const sx_theme_colors_t s_light_theme = {
    .bg_primary = 0xFFFFFF,
    .bg_secondary = 0xF5F5F5,
    .bg_tertiary = 0xE0E0E0,
    .text_primary = 0x000000,
    .text_secondary = 0x666666,
    .accent = 0x5b7fff,
    .error = 0xFF0000,
    .success = 0x00AA00,
};

esp_err_t sx_theme_service_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    s_theme_mutex = xSemaphoreCreateMutex();
    if (s_theme_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    // Load theme from settings
    int32_t saved_theme = SX_THEME_DARK;
    sx_settings_get_int_default("ui_theme", &saved_theme, SX_THEME_DARK);
    s_current_theme = (sx_theme_type_t)saved_theme;
    
    // Validate theme
    if (s_current_theme > SX_THEME_AUTO) {
        s_current_theme = SX_THEME_DARK;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "Theme service initialized (theme: %d)", s_current_theme);
    return ESP_OK;
}

esp_err_t sx_theme_set_type(sx_theme_type_t theme_type) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (theme_type > SX_THEME_AUTO) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_theme_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    sx_theme_type_t old_theme = s_current_theme;
    s_current_theme = theme_type;
    
    // Save to settings
    sx_settings_set_int("ui_theme", (int32_t)theme_type);
    sx_settings_commit();
    
    xSemaphoreGive(s_theme_mutex);
    
    // Notify callback if theme changed
    if (old_theme != theme_type && s_change_callback != NULL) {
        s_change_callback(theme_type);
    }
    
    // P0.1: Notify UI task via event (remove LVGL from service)
    if (old_theme != theme_type) {
        const sx_theme_colors_t *colors = sx_theme_get_colors();
        sx_theme_data_t *theme_data = (sx_theme_data_t *)malloc(sizeof(sx_theme_data_t));
        if (theme_data) {
            theme_data->theme_type = theme_type;
            theme_data->bg_primary = colors->bg_primary;
            theme_data->bg_secondary = colors->bg_secondary;
            theme_data->bg_tertiary = colors->bg_tertiary;
            theme_data->text_primary = colors->text_primary;
            theme_data->text_secondary = colors->text_secondary;
            theme_data->accent = colors->accent;
            theme_data->error = colors->error;
            theme_data->success = colors->success;
            
            sx_event_t evt = {
                .type = SX_EVT_THEME_CHANGED,
                .priority = SX_EVT_PRIORITY_NORMAL,
                .arg0 = 0,
                .arg1 = 0,
                .ptr = theme_data  // UI will handle and free this
            };
            
            sx_dispatcher_post_event(&evt);
        }
    }
    
    ESP_LOGI(TAG, "Theme changed to: %d", theme_type);
    return ESP_OK;
}

sx_theme_type_t sx_theme_get_type(void) {
    return s_current_theme;
}

const sx_theme_colors_t* sx_theme_get_colors(void) {
    if (!s_initialized) {
        return &s_dark_theme; // Default
    }
    
    // For AUTO theme, detect based on time of day
    sx_theme_type_t effective_theme = s_current_theme;
    if (effective_theme == SX_THEME_AUTO) {
        // Get current time
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        // Use dark theme from 18:00 (6 PM) to 6:00 (6 AM), light theme otherwise
        int hour = timeinfo.tm_hour;
        if (hour >= 18 || hour < 6) {
            effective_theme = SX_THEME_DARK;
        } else {
            effective_theme = SX_THEME_LIGHT;
        }
    }
    
    return (effective_theme == SX_THEME_LIGHT) ? &s_light_theme : &s_dark_theme;
}

// P0.1: This function has been removed - UI task should apply theme directly via LVGL
// Theme colors are available via sx_theme_get_colors() and UI receives SX_EVT_THEME_CHANGED event
// UI components should call LVGL theme functions directly

esp_err_t sx_theme_register_change_callback(sx_theme_change_cb_t callback) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    s_change_callback = callback;
    ESP_LOGI(TAG, "Theme change callback %s", callback ? "registered" : "unregistered");
    return ESP_OK;
}



