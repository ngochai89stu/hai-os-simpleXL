#include "screen_music_player.h"

#include <esp_log.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "sx_state.h"
#include "sx_audio_service.h"
#include "sx_playlist_manager.h"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "ui_icons.h"

static const char *TAG = "screen_music_player";

// Custom UI mode: full state
static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_album_art = NULL;
static lv_obj_t *s_track_title = NULL;
static lv_obj_t *s_track_artist = NULL;
static lv_obj_t *s_progress_bar = NULL;
static lv_obj_t *s_play_btn = NULL;
static lv_obj_t *s_prev_btn = NULL;
static lv_obj_t *s_next_btn = NULL;
static lv_obj_t *s_volume_slider = NULL;
static lv_obj_t *s_container = NULL;
static lv_obj_t *s_play_label = NULL;  // Label inside play button
static bool s_last_playing_state = false;
static uint8_t s_last_volume = 50;

static void play_pause_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (sx_audio_is_playing()) {
            sx_audio_pause();
        } else {
            sx_audio_resume();
        }
    }
}

static void volume_slider_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        int32_t value = lv_slider_get_value(s_volume_slider);
        sx_audio_set_volume((uint8_t)value);
    }
}

static void prev_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Previous track button clicked");
        esp_err_t ret = sx_playlist_previous();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to play previous track: %s", esp_err_to_name(ret));
        }
    }
}

static void next_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Next track button clicked");
        esp_err_t ret = sx_playlist_next();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to play next track: %s", esp_err_to_name(ret));
        }
    }
}

static void on_create(void) {
    ESP_LOGI(TAG, "Music Player screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Custom UI mode - Web demo style
    // Set background
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    
    // Create top bar with back button (matching web demo)
    s_top_bar = screen_common_create_top_bar_with_back(container, "Music Player");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 20, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Album art (large, centered) - Web demo style with shadow effect
    s_album_art = lv_obj_create(s_content);
    lv_obj_set_size(s_album_art, 220, 220);  // Slightly larger
    lv_obj_set_style_bg_color(s_album_art, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_album_art, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_album_art, 16, LV_PART_MAIN);  // More rounded
    
    // Placeholder icon in album art
    lv_obj_t *album_icon = ui_icon_create(s_album_art, UI_ICON_MUSIC_PLAYER, 48);
    if (album_icon != NULL) {
        lv_obj_set_style_text_color(album_icon, lv_color_hex(0x888888), 0);  // Gray icon
        lv_obj_center(album_icon);
    }
    
    // Track info (title, artist) - Web demo style
    s_track_title = lv_label_create(s_content);
    lv_label_set_text(s_track_title, "No track");
    lv_obj_set_style_text_font(s_track_title, &lv_font_montserrat_14, 0);  // Larger font
    lv_obj_set_style_text_color(s_track_title, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_long_mode(s_track_title, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(s_track_title, LV_PCT(90));
    
    s_track_artist = lv_label_create(s_content);
    lv_label_set_text(s_track_artist, "Unknown artist");
    lv_obj_set_style_text_font(s_track_artist, &lv_font_montserrat_14, 0);  // Larger font
    lv_obj_set_style_text_color(s_track_artist, lv_color_hex(0x888888), 0);
    lv_label_set_long_mode(s_track_artist, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(s_track_artist, LV_PCT(90));
    
    // Progress bar - Web demo style
    s_progress_bar = lv_bar_create(s_content);
    lv_obj_set_size(s_progress_bar, LV_PCT(90), 6);  // Slightly thicker
    lv_obj_set_style_bg_color(s_progress_bar, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_progress_bar, lv_color_hex(0x5b7fff), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_progress_bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(s_progress_bar, 3, LV_PART_INDICATOR);
    lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);
    
    // Control buttons container
    lv_obj_t *controls = lv_obj_create(s_content);
    lv_obj_set_size(controls, LV_PCT(90), 60);
    lv_obj_set_style_bg_opa(controls, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(controls, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(controls, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(controls, 20, LV_PART_MAIN);
    
    // Previous button
    s_prev_btn = lv_btn_create(controls);
    lv_obj_set_size(s_prev_btn, 50, 50);
    lv_obj_set_style_bg_color(s_prev_btn, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_prev_btn, lv_color_hex(0x3a3a3a), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(s_prev_btn, 25, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_prev_btn, 0, LV_PART_MAIN);
    lv_obj_t *prev_icon = ui_icon_create(s_prev_btn, UI_ICON_PREV, 20);
    if (prev_icon != NULL) {
        lv_obj_center(prev_icon);
    }
    lv_obj_add_event_cb(s_prev_btn, prev_btn_cb, LV_EVENT_CLICKED, NULL);
    
    // Play/Pause button
    s_play_btn = lv_btn_create(controls);
    lv_obj_set_size(s_play_btn, 60, 60);
    lv_obj_set_style_bg_color(s_play_btn, lv_color_hex(0x5b7fff), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_play_btn, lv_color_hex(0x4a6fff), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(s_play_btn, 30, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_play_btn, 0, LV_PART_MAIN);
    s_play_label = ui_icon_create(s_play_btn, UI_ICON_PLAY, 24);
    if (s_play_label != NULL) {
        lv_obj_set_style_text_color(s_play_label, lv_color_hex(0xFFFFFF), 0);  // White icon on blue button
        lv_obj_center(s_play_label);
    }
    // Add event handler for play/pause button
    lv_obj_add_event_cb(s_play_btn, play_pause_btn_cb, LV_EVENT_CLICKED, NULL);
    
    // Next button
    s_next_btn = lv_btn_create(controls);
    lv_obj_set_size(s_next_btn, 50, 50);
    lv_obj_set_style_bg_color(s_next_btn, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_next_btn, lv_color_hex(0x3a3a3a), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(s_next_btn, 25, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_next_btn, 0, LV_PART_MAIN);
    lv_obj_t *next_icon = ui_icon_create(s_next_btn, UI_ICON_NEXT, 20);
    if (next_icon != NULL) {
        lv_obj_center(next_icon);
    }
    lv_obj_add_event_cb(s_next_btn, next_btn_cb, LV_EVENT_CLICKED, NULL);
    
    // Volume control - Web demo style
    lv_obj_t *volume_container = lv_obj_create(s_content);
    lv_obj_set_size(volume_container, LV_PCT(90), 60);
    lv_obj_set_style_bg_opa(volume_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(volume_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(volume_container, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(volume_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(volume_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t *volume_label = lv_label_create(volume_container);
    lv_label_set_text(volume_label, "Volume");
    lv_obj_set_style_text_font(volume_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(volume_label, lv_color_hex(0x888888), 0);
    
    s_volume_slider = lv_slider_create(volume_container);
    lv_obj_set_size(s_volume_slider, LV_PCT(100), 24);  // Slightly thicker
    lv_obj_set_style_bg_color(s_volume_slider, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_volume_slider, lv_color_hex(0x5b7fff), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_volume_slider, lv_color_hex(0x5b7fff), LV_PART_KNOB);
    lv_obj_set_style_radius(s_volume_slider, 12, LV_PART_MAIN);
    lv_obj_set_style_radius(s_volume_slider, 12, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_volume_slider, 12, LV_PART_KNOB);
    uint8_t current_volume = sx_audio_get_volume();
    lv_slider_set_value(s_volume_slider, current_volume, LV_ANIM_OFF);
    lv_slider_set_range(s_volume_slider, 0, 100);
    s_last_volume = current_volume;
    
    // Add event handler for volume slider
    lv_obj_add_event_cb(s_volume_slider, volume_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_MUSIC_PLAYER, "Music Player", container, s_content);
    #endif
}

static void on_show(void) {
    ESP_LOGI(TAG, "Music Player screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_MUSIC_PLAYER);
    #endif
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Music Player screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_MUSIC_PLAYER);
    #endif
}

static void on_update(const sx_state_t *state) {
    // Update play/pause button icon
    bool is_playing = sx_audio_is_playing();
    if (is_playing != s_last_playing_state && s_play_label != NULL) {
        const char *symbol = is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY;
        lv_label_set_text(s_play_label, symbol);
        s_last_playing_state = is_playing;
    }
    
    // Update volume slider
    uint8_t current_volume = sx_audio_get_volume();
    if (current_volume != s_last_volume && s_volume_slider != NULL) {
        lv_slider_set_value(s_volume_slider, current_volume, LV_ANIM_OFF);
        s_last_volume = current_volume;
    }
    
    // Update audio state from state snapshot
    if (state != NULL) {
        // Update track info if available in state (future enhancement)
        // For now, we rely on events for track updates
    }
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Music Player screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_MUSIC_PLAYER);
    #endif
    
    if (lvgl_port_lock(0)) {
        // Custom UI mode: clean up UI objects
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

// Register this screen
void screen_music_player_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
        .on_update = on_update,
    };
    ui_screen_registry_register(SCREEN_ID_MUSIC_PLAYER, &callbacks);
}
