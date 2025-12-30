#include "screen_radio.h"

#include <esp_log.h>
#include <string.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "sx_ui_verify.h"
#include "screen_common.h"
#include "sx_state.h"
#include "sx_radio_service.h"
#include "ui_icons.h"
// Note: sx_radio_station_table.h is in sx_services, not sx_ui
// #include "sx_radio_station_table.h"
#include "sx_dispatcher.h"
#include "sx_event.h"

static const char *TAG = "screen_radio";

// Use VOV stations from station table
static const char* s_station_keys[] = {
    "VOV1",
    "VOV2",
    "VOV3",
    "VOV5",
};

#define STATION_COUNT (sizeof(s_station_keys) / sizeof(s_station_keys[0]))

static const char* get_station_name(int idx) {
    if (idx < 0 || idx >= STATION_COUNT) return "Unknown";
    // Note: sx_radio_lookup_station is in sx_radio_station_table.h (sx_services)
    // For now, return the station key as name
    return s_station_keys[idx];
}

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_station_title = NULL;
static lv_obj_t *s_station_info = NULL;
static lv_obj_t *s_error_label = NULL;
static lv_obj_t *s_retry_btn = NULL;
static lv_obj_t *s_play_btn = NULL;
static lv_obj_t *s_play_label = NULL;
static lv_obj_t *s_station_list = NULL;
static lv_obj_t *s_container = NULL;
static bool s_last_playing_state = false;
static char s_last_error[256] = {0};

static void on_create(void) {
    ESP_LOGI(TAG, "Radio screen onCreate");
    
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
    s_top_bar = screen_common_create_top_bar_with_back(container, "Radio");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Current station info (matching web demo)
    lv_obj_t *current_station = lv_obj_create(s_content);
    lv_obj_set_size(current_station, LV_PCT(100), 80);
    lv_obj_set_style_bg_color(current_station, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_border_width(current_station, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(current_station, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(current_station, 15, LV_PART_MAIN);
    
    s_station_title = lv_label_create(current_station);
    lv_label_set_text(s_station_title, "No station");
    lv_obj_set_style_text_font(s_station_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_station_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_station_title, LV_ALIGN_TOP_LEFT, 0, 0);
    
    s_station_info = lv_label_create(current_station);
    lv_label_set_text(s_station_info, "Tap a station to play");
    lv_obj_set_style_text_font(s_station_info, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_station_info, lv_color_hex(0x888888), 0);
    lv_obj_align(s_station_info, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    
    // Error label (hidden by default)
    s_error_label = lv_label_create(current_station);
    lv_label_set_text(s_error_label, "");
    lv_obj_set_style_text_font(s_error_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_error_label, lv_color_hex(0xFF0000), 0);
    lv_label_set_long_mode(s_error_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_error_label, LV_PCT(70));
    lv_obj_align(s_error_label, LV_ALIGN_BOTTOM_LEFT, 0, -25);
    lv_obj_add_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);
    
    // Retry button (hidden by default)
    s_retry_btn = lv_btn_create(current_station);
    lv_obj_set_size(s_retry_btn, 80, 30);
    lv_obj_set_style_bg_color(s_retry_btn, lv_color_hex(0xFF4444), LV_PART_MAIN);
    lv_obj_set_style_radius(s_retry_btn, 5, LV_PART_MAIN);
    lv_obj_align(s_retry_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_add_flag(s_retry_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *retry_label = lv_label_create(s_retry_btn);
    lv_label_set_text(retry_label, "Retry");
    lv_obj_set_style_text_font(retry_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(retry_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(retry_label);
    
    // Play/Pause control
    s_play_btn = lv_btn_create(current_station);
    lv_obj_set_size(s_play_btn, 50, 50);
    lv_obj_set_style_bg_color(s_play_btn, lv_color_hex(0x5b7fff), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_play_btn, lv_color_hex(0x4a6fff), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(s_play_btn, 25, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_play_btn, 0, LV_PART_MAIN);
    lv_obj_align(s_play_btn, LV_ALIGN_RIGHT_MID, -10, 0);
    s_play_label = ui_icon_create(s_play_btn, UI_ICON_PLAY, 20);
    if (s_play_label != NULL) {
        lv_obj_set_style_text_color(s_play_label, lv_color_hex(0xFFFFFF), 0);  // White icon on blue button
        lv_obj_center(s_play_label);
    }
    
    // Station list (scrollable) - matching web demo
    s_station_list = lv_obj_create(s_content);
    lv_obj_set_size(s_station_list, LV_PCT(100), LV_PCT(100) - 100);
    lv_obj_set_style_bg_opa(s_station_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_station_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_station_list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_station_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_station_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Create station list items
    for (int i = 0; i < STATION_COUNT; i++) {
        lv_obj_t *station_item = lv_obj_create(s_station_list);
        lv_obj_set_size(station_item, LV_PCT(100), 50);
        lv_obj_set_style_bg_color(station_item, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        lv_obj_set_style_bg_color(station_item, lv_color_hex(0x3a3a3a), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_border_width(station_item, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(station_item, 10, LV_PART_MAIN);
        lv_obj_set_style_radius(station_item, 5, LV_PART_MAIN);
        
        // Store station index in user data
        lv_obj_set_user_data(station_item, (void *)(intptr_t)i);
        
        lv_obj_t *label = lv_label_create(station_item);
        lv_label_set_text(label, get_station_name(i));
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
        
        // Add click handler
        // Will be added after callback function is defined
    }
    
    #if SX_UI_VERIFY_MODE
    // Add touch probe for verification
    screen_common_add_touch_probe(container, "Radio Screen", SCREEN_ID_RADIO);
    #endif
    
    lvgl_port_unlock();
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_RADIO, "Radio", container, s_content);
    #endif
}

static void play_pause_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (sx_radio_is_playing()) {
            if (sx_radio_is_paused()) {
                sx_radio_resume();
            } else {
                sx_radio_pause();
            }
        } else {
            // If not playing, try to resume or play last station
            const char *url = sx_radio_get_current_url();
            if (url != NULL) {
                sx_radio_resume();
            }
        }
    }
}

static void retry_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Retry button clicked");
        // Hide error UI
        if (s_error_label != NULL && lvgl_port_lock(0)) {
            lv_obj_add_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);
            lvgl_port_unlock();
        }
        if (s_retry_btn != NULL && lvgl_port_lock(0)) {
            lv_obj_add_flag(s_retry_btn, LV_OBJ_FLAG_HIDDEN);
            lvgl_port_unlock();
        }
        
        // Retry playing last station
        const char *url = sx_radio_get_current_url();
        if (url != NULL && strlen(url) > 0) {
            esp_err_t ret = sx_radio_play_url(url);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Retry failed: %s", esp_err_to_name(ret));
            }
        }
    }
}

static void station_item_click_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_obj_t *item = lv_event_get_target(e);
        int station_idx = (int)(intptr_t)lv_obj_get_user_data(item);
        
        if (station_idx >= 0 && station_idx < STATION_COUNT) {
            const char *key = s_station_keys[station_idx];
            ESP_LOGI(TAG, "Playing station: %s", key);
            
            // Hide error UI when starting new station
            if (s_error_label != NULL && lvgl_port_lock(0)) {
                lv_obj_add_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);
                lvgl_port_unlock();
            }
            if (s_retry_btn != NULL && lvgl_port_lock(0)) {
                lv_obj_add_flag(s_retry_btn, LV_OBJ_FLAG_HIDDEN);
                lvgl_port_unlock();
            }
            
            esp_err_t ret = sx_radio_play_station(key);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to play station: %s", esp_err_to_name(ret));
            }
        }
    }
}

static void on_show(void) {
    ESP_LOGI(TAG, "Radio screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_RADIO);
    #endif
    
    // Add event handlers if not already added
    if (s_play_btn != NULL && s_station_list != NULL) {
        if (lvgl_port_lock(0)) {
            lv_obj_add_event_cb(s_play_btn, play_pause_btn_cb, LV_EVENT_CLICKED, NULL);
            if (s_retry_btn != NULL) {
                lv_obj_add_event_cb(s_retry_btn, retry_btn_cb, LV_EVENT_CLICKED, NULL);
            }
            
            // Add click handlers for station items
            uint32_t child_cnt = lv_obj_get_child_cnt(s_station_list);
            for (uint32_t i = 0; i < child_cnt; i++) {
                lv_obj_t *child = lv_obj_get_child(s_station_list, i);
                lv_obj_add_event_cb(child, station_item_click_cb, LV_EVENT_CLICKED, NULL);
            }
            lvgl_port_unlock();
        }
    }
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Radio screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_RADIO);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Radio screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_RADIO);
    #endif
    
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
    if (!lvgl_port_lock(0)) {
        return;
    }
    
    // Check for radio error events
    sx_event_t evt;
    while (sx_dispatcher_poll_event(&evt)) {
        if (evt.type == SX_EVT_RADIO_ERROR && evt.ptr != NULL) {
            const char *error_msg = (const char *)evt.ptr;
            strncpy(s_last_error, error_msg, sizeof(s_last_error) - 1);
            s_last_error[sizeof(s_last_error) - 1] = '\0';
            
            // Show error UI
            if (s_error_label != NULL) {
                lv_label_set_text(s_error_label, s_last_error);
                lv_obj_clear_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);
            }
            if (s_retry_btn != NULL) {
                lv_obj_clear_flag(s_retry_btn, LV_OBJ_FLAG_HIDDEN);
            }
            
            // Free error message
            free((void *)evt.ptr);
        }
    }
    
    // Update play/pause button icon
    bool is_playing = sx_radio_is_playing() && !sx_radio_is_paused();
    if (is_playing != s_last_playing_state && s_play_label != NULL) {
        const char *symbol = is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY;
        lv_label_set_text(s_play_label, symbol);
        s_last_playing_state = is_playing;
        
        // Hide error UI when playing successfully
        if (is_playing && s_error_label != NULL) {
            lv_obj_add_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);
        }
        if (is_playing && s_retry_btn != NULL) {
            lv_obj_add_flag(s_retry_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
    // Update station info from metadata
    const sx_radio_metadata_t *metadata = sx_radio_get_metadata();
    if (metadata != NULL && s_station_title != NULL) {
        if (strlen(metadata->stream_title) > 0) {
            lv_label_set_text(s_station_title, metadata->stream_title);
        }
        
        char info[128];
        if (metadata->bitrate > 0) {
            snprintf(info, sizeof(info), "%d kbps", metadata->bitrate);
            if (s_station_info != NULL) {
                lv_label_set_text(s_station_info, info);
            }
        }
    } else {
        // Update from current URL - try to match station
        const char *url = sx_radio_get_current_url();
        if (url != NULL && s_station_title != NULL) {
            // Note: sx_radio_lookup_station is not available in sx_ui
            // For now, just show the URL
            lv_label_set_text(s_station_title, url);
        }
    }
    
    lvgl_port_unlock();
}

// Register this screen
void screen_radio_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
        .on_update = on_update,
    };
    ui_screen_registry_register(SCREEN_ID_RADIO, &callbacks);
}
