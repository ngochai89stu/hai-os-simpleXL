#include "screen_ota.h"

#include <esp_log.h>
#include <string.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "sx_state.h"
#include "sx_ota_service.h"
#include "sx_settings_service.h"

static const char *TAG = "screen_ota";

// Forward declarations
static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_progress_bar = NULL;

// Progress callback for OTA
static void ota_progress_callback(int percent) {
    if (s_progress_bar != NULL && lvgl_port_lock(0)) {
        lv_bar_set_value(s_progress_bar, percent, LV_ANIM_ON);
        if (s_status_label != NULL) {
            char status[64];
            snprintf(status, sizeof(status), "Downloading: %d%%", percent);
            lv_label_set_text(s_status_label, status);
        }
        lvgl_port_unlock();
    }
}
static lv_obj_t *s_check_btn = NULL;
static lv_obj_t *s_update_btn = NULL;
static lv_obj_t *s_container = NULL;

static void on_create(void) {
    ESP_LOGI(TAG, "OTA Update screen onCreate");
    
    if (!lvgl_port_lock(0)) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock");
        return;
    }
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        lvgl_port_unlock();
        return;
    }
    
    s_container = container;
    
    // Set background
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "OTA Update");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 20, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Update status (matching web demo)
    s_status_label = lv_label_create(s_content);
    lv_label_set_text(s_status_label, "Ready to check for updates");
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_status_label, LV_PCT(90));
    
    // Progress bar (matching web demo)
    s_progress_bar = lv_bar_create(s_content);
    lv_obj_set_size(s_progress_bar, LV_PCT(90), 20);
    lv_obj_set_style_bg_color(s_progress_bar, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_progress_bar, lv_color_hex(0x5b7fff), LV_PART_INDICATOR);
    lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);
    
    // Check button (matching web demo)
    s_check_btn = lv_btn_create(s_content);
    lv_obj_set_size(s_check_btn, LV_PCT(90), 50);
    lv_obj_set_style_bg_color(s_check_btn, lv_color_hex(0x5b7fff), LV_PART_MAIN);
    lv_obj_set_style_radius(s_check_btn, 5, LV_PART_MAIN);
    lv_obj_t *check_label = lv_label_create(s_check_btn);
    lv_label_set_text(check_label, "Check for Updates");
    lv_obj_set_style_text_font(check_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(check_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(check_label);
    
    // Update button (hidden initially, shown when update available)
    s_update_btn = lv_btn_create(s_content);
    lv_obj_set_size(s_update_btn, LV_PCT(90), 50);
    lv_obj_set_style_bg_color(s_update_btn, lv_color_hex(0x00ff00), LV_PART_MAIN);
    lv_obj_set_style_radius(s_update_btn, 5, LV_PART_MAIN);
    lv_obj_add_flag(s_update_btn, LV_OBJ_FLAG_HIDDEN); // Hidden by default
    lv_obj_t *update_label = lv_label_create(s_update_btn);
    lv_label_set_text(update_label, "Start Update");
    lv_obj_set_style_text_font(update_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(update_label, lv_color_hex(0x000000), 0);
    lv_obj_center(update_label);
    
    lvgl_port_unlock();
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_OTA, "OTA Update", container, s_content);
    #endif
}

static void check_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Check for updates button clicked");
        
        if (s_status_label != NULL && lvgl_port_lock(0)) {
            lv_label_set_text(s_status_label, "Checking for updates...");
            lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);
            lvgl_port_unlock();
        }
        
        // Get OTA URL from settings (or use default)
        char ota_url[256] = {0};
        size_t url_len = sizeof(ota_url);
        if (sx_settings_get_string("ota_update_url", ota_url, url_len) != ESP_OK) {
            // Use default URL if not configured
            strncpy(ota_url, "https://example.com/firmware.bin", sizeof(ota_url) - 1);
        }
        
        // For now, assume update is available if URL is configured
        // In a real implementation, you would check version from server
        if (strlen(ota_url) > 0 && lvgl_port_lock(0)) {
            lv_label_set_text(s_status_label, "Update available. Click 'Start Update' to install.");
            lv_obj_clear_flag(s_update_btn, LV_OBJ_FLAG_HIDDEN);
            lvgl_port_unlock();
        } else {
            if (lvgl_port_lock(0)) {
                lv_label_set_text(s_status_label, "No update URL configured. Check settings.");
                lvgl_port_unlock();
            }
        }
    }
}

static void update_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Start update button clicked");
        
        if (sx_ota_is_updating()) {
            ESP_LOGW(TAG, "OTA update already in progress");
            return;
        }
        
        if (s_status_label != NULL && lvgl_port_lock(0)) {
            lv_label_set_text(s_status_label, "Starting update...");
            lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);
            lv_obj_add_flag(s_check_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_update_btn, LV_OBJ_FLAG_HIDDEN);
            lvgl_port_unlock();
        }
        
        // Get OTA URL from settings
        char ota_url[256] = {0};
        size_t url_len = sizeof(ota_url);
        if (sx_settings_get_string("ota_update_url", ota_url, url_len) != ESP_OK) {
            ESP_LOGE(TAG, "OTA URL not configured");
            if (s_status_label != NULL && lvgl_port_lock(0)) {
                lv_label_set_text(s_status_label, "Error: OTA URL not configured");
                lv_obj_clear_flag(s_check_btn, LV_OBJ_FLAG_HIDDEN);
                lvgl_port_unlock();
            }
            return;
        }
        
        // Configure OTA
        sx_ota_config_t ota_cfg = {
            .url = ota_url,
            .cert_pem = NULL,  // TODO: Add certificate if using HTTPS
            .timeout_ms = 30000,
        };
        
        // Start OTA update
        esp_err_t ret = sx_ota_start_update(&ota_cfg, ota_progress_callback);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start OTA update: %s", esp_err_to_name(ret));
            const char *error_msg = sx_ota_get_last_error();
            if (s_status_label != NULL && lvgl_port_lock(0)) {
                char status[128];
                if (error_msg != NULL) {
                    snprintf(status, sizeof(status), "Error: %s", error_msg);
                } else {
                    snprintf(status, sizeof(status), "Error: %s", esp_err_to_name(ret));
                }
                lv_label_set_text(s_status_label, status);
                lv_obj_clear_flag(s_check_btn, LV_OBJ_FLAG_HIDDEN);
                lvgl_port_unlock();
            }
        } else {
            if (s_status_label != NULL && lvgl_port_lock(0)) {
                lv_label_set_text(s_status_label, "Downloading update...");
                lvgl_port_unlock();
            }
        }
    }
}

static void on_show(void) {
    ESP_LOGI(TAG, "OTA Update screen onShow");
    
    // Add event handlers
    if (s_check_btn != NULL && s_update_btn != NULL) {
        if (lvgl_port_lock(0)) {
            lv_obj_add_event_cb(s_check_btn, check_btn_cb, LV_EVENT_CLICKED, NULL);
            lv_obj_add_event_cb(s_update_btn, update_btn_cb, LV_EVENT_CLICKED, NULL);
            lvgl_port_unlock();
        }
    }
}

static void on_hide(void) {
    ESP_LOGI(TAG, "OTA Update screen onHide");
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "OTA Update screen onDestroy");
    
    if (lvgl_port_lock(0)) {
        if (s_top_bar != NULL) {
            lv_obj_del(s_top_bar);
            s_top_bar = NULL;
        }
        if (s_content != NULL) {
            lv_obj_del(s_content);
            s_content = NULL;
        }
        lvgl_port_unlock();
    }
}

static void on_update(const sx_state_t *state) {
    // Update UI based on OTA state
    if (sx_ota_is_updating()) {
        // Update is in progress - progress callback will handle UI updates
        // But we can also check for completion here
        if (s_status_label != NULL && lvgl_port_lock(0)) {
            // Check if update completed (progress should be 100%)
            int32_t progress = lv_bar_get_value(s_progress_bar);
            if (progress >= 100) {
                lv_label_set_text(s_status_label, "Update complete. Rebooting...");
            }
            lvgl_port_unlock();
        }
    } else {
        // Update not in progress - check for errors
        const char *error_msg = sx_ota_get_last_error();
        if (error_msg != NULL && strlen(error_msg) > 0) {
            if (s_status_label != NULL && lvgl_port_lock(0)) {
                char status[128];
                snprintf(status, sizeof(status), "Error: %s", error_msg);
                lv_label_set_text(s_status_label, status);
                lv_obj_clear_flag(s_check_btn, LV_OBJ_FLAG_HIDDEN);
                lvgl_port_unlock();
            }
        }
    }
}

// Register this screen
void screen_ota_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
        .on_update = on_update,
    };
    ui_screen_registry_register(SCREEN_ID_OTA, &callbacks);
}
