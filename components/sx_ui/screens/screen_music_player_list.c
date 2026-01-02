/**
 * @file screen_music_player_list.c
 * @brief Playlist view for music player screen
 * Phase 2: Core Features - Playlist View
 */

#include "screen_music_player_list.h"
#include "sx_playlist_manager.h"
#include "sx_audio_service.h"
#include "../assets/sx_ui_assets.h"
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)
#include <esp_log.h>
#include <stdio.h>

static const char *TAG = "music_playlist";

// Static variables
static lv_obj_t *s_list = NULL;
static const lv_font_t *s_font_small = NULL;
static const lv_font_t *s_font_medium = NULL;
static lv_style_t s_style_scrollbar;
static lv_style_t s_style_btn;
static lv_style_t s_style_button_pr;
static lv_style_t s_style_button_chk;
static lv_style_t s_style_button_dis;
static lv_style_t s_style_title;
static lv_style_t s_style_artist;
static lv_style_t s_style_time;

// Helper function to get fonts
static const lv_font_t *get_font_small(void) {
#if LV_FONT_MONTSERRAT_12
    return &lv_font_montserrat_12;
#else
    return &lv_font_montserrat_14;
#endif
}

static const lv_font_t *get_font_medium(void) {
#if LV_FONT_MONTSERRAT_16
    return &lv_font_montserrat_16;
#else
    return &lv_font_montserrat_14;
#endif
}

// Static function prototypes
static lv_obj_t *add_list_button(lv_obj_t *parent, size_t track_index);
static void btn_click_event_cb(lv_event_t *e);
static void list_delete_event_cb(lv_event_t *e);

// Create playlist view
lv_obj_t *create_playlist_view(lv_obj_t *parent) {
    s_font_small = get_font_small();
    s_font_medium = get_font_medium();

    // Initialize scrollbar style
    lv_style_init(&s_style_scrollbar);
    lv_style_set_width(&s_style_scrollbar, 4);
    lv_style_set_bg_opa(&s_style_scrollbar, LV_OPA_COVER);
    lv_style_set_bg_color(&s_style_scrollbar, lv_color_hex3(0xeee));
    lv_style_set_radius(&s_style_scrollbar, LV_RADIUS_CIRCLE);
    lv_style_set_pad_right(&s_style_scrollbar, 4);

    // Grid layout for buttons
    static const int32_t grid_cols[] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    static const int32_t grid_rows[] = {22, 17, LV_GRID_TEMPLATE_LAST};

    // Button style
    lv_style_init(&s_style_btn);
    lv_style_set_bg_opa(&s_style_btn, LV_OPA_TRANSP);
    lv_style_set_grid_column_dsc_array(&s_style_btn, grid_cols);
    lv_style_set_grid_row_dsc_array(&s_style_btn, grid_rows);
    lv_style_set_grid_row_align(&s_style_btn, LV_GRID_ALIGN_CENTER);
    lv_style_set_layout(&s_style_btn, LV_LAYOUT_GRID);
    lv_style_set_pad_right(&s_style_btn, 20);

    // Button pressed style
    lv_style_init(&s_style_button_pr);
    lv_style_set_bg_opa(&s_style_button_pr, LV_OPA_COVER);
    lv_style_set_bg_color(&s_style_button_pr, lv_color_hex(0x4c4965));

    // Button checked style
    lv_style_init(&s_style_button_chk);
    lv_style_set_bg_opa(&s_style_button_chk, LV_OPA_COVER);
    lv_style_set_bg_color(&s_style_button_chk, lv_color_hex(0x4c4965));

    // Button disabled style
    lv_style_init(&s_style_button_dis);
    lv_style_set_text_opa(&s_style_button_dis, LV_OPA_40);
    lv_style_set_image_opa(&s_style_button_dis, LV_OPA_40);

    // Title style
    lv_style_init(&s_style_title);
    lv_style_set_text_font(&s_style_title, s_font_medium);
    lv_style_set_text_color(&s_style_title, lv_color_hex(0xffffff));

    // Artist style
    lv_style_init(&s_style_artist);
    lv_style_set_text_font(&s_style_artist, s_font_small);
    lv_style_set_text_color(&s_style_artist, lv_color_hex(0xb1b0be));

    // Time style
    lv_style_init(&s_style_time);
    lv_style_set_text_font(&s_style_time, s_font_medium);
    lv_style_set_text_color(&s_style_time, lv_color_hex(0xffffff));

    // Create scrollable list container
    s_list = lv_obj_create(parent);
    lv_obj_add_event_cb(s_list, list_delete_event_cb, LV_EVENT_DELETE, NULL);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_size(s_list, LV_PCT(100), LV_PCT(100));
    lv_obj_add_style(s_list, &s_style_scrollbar, LV_PART_SCROLLBAR);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);

    // Add playlist items
    size_t count = sx_playlist_get_count();
    for (size_t i = 0; i < count; i++) {
        add_list_button(s_list, i);
    }

    // Check current track
    int current_index = sx_playlist_get_current_index();
    if (current_index >= 0) {
        playlist_button_check((size_t)current_index, sx_audio_is_playing());
    }

    return s_list;
}

// Update button state (play/pause icon)
void playlist_button_check(size_t track_index, bool is_playing) {
    if (s_list == NULL) return;

    lv_obj_t *btn = lv_obj_get_child(s_list, track_index);
    if (btn == NULL) return;

    lv_obj_t *icon = lv_obj_get_child(btn, 0);
    if (icon == NULL) return;

    if (is_playing) {
        lv_obj_add_state(btn, LV_STATE_CHECKED);
        // lv_image_set_src(icon, &img_lv_demo_music_btn_list_pause);
        if (icon != NULL) {
            lv_label_set_text(icon, LV_SYMBOL_PAUSE);
        }
        lv_obj_scroll_to_view(btn, LV_ANIM_ON);
    } else {
        lv_obj_remove_state(btn, LV_STATE_CHECKED);
        // lv_image_set_src(icon, &img_lv_demo_music_btn_list_play);
        if (icon != NULL) {
            lv_label_set_text(icon, LV_SYMBOL_PLAY);
        }
    }
}

// Add list button for a track
static lv_obj_t *add_list_button(lv_obj_t *parent, size_t track_index) {
    uint32_t duration = sx_playlist_get_duration(track_index);
    char time_str[32];
    snprintf(time_str, sizeof(time_str), "%lu:%02lu", (unsigned long)(duration / 60), (unsigned long)(duration % 60));

    const char *title = sx_playlist_get_title(track_index);
    const char *artist = sx_playlist_get_artist(track_index);

    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, LV_PCT(100), 60);

    lv_obj_add_style(btn, &s_style_btn, 0);
    lv_obj_add_style(btn, &s_style_button_pr, LV_STATE_PRESSED);
    lv_obj_add_style(btn, &s_style_button_chk, LV_STATE_CHECKED);
    lv_obj_add_style(btn, &s_style_button_dis, LV_STATE_DISABLED);
    lv_obj_add_event_cb(btn, btn_click_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(btn, (void *)track_index);

    // Play/Pause icon (temporary symbol label, until image assets available)
    lv_obj_t *icon = lv_label_create(btn);
    lv_label_set_text(icon, LV_SYMBOL_PLAY);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    // Title and artist container
    lv_obj_t *text_cont = lv_obj_create(btn);
    lv_obj_remove_style_all(text_cont);
    lv_obj_set_size(text_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(text_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(text_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_grid_cell(text_cont, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, 0, 2);

    // Title label
    lv_obj_t *title_label = lv_label_create(text_cont);
    lv_label_set_text(title_label, title ? title : "Unknown Title");
    lv_obj_add_style(title_label, &s_style_title, 0);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(title_label, LV_PCT(100));

    // Artist label
    lv_obj_t *artist_label = lv_label_create(text_cont);
    lv_label_set_text(artist_label, artist ? artist : "Unknown Artist");
    lv_obj_add_style(artist_label, &s_style_artist, 0);
    lv_label_set_long_mode(artist_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(artist_label, LV_PCT(100));

    // Time label
    lv_obj_t *time_label = lv_label_create(btn);
    lv_label_set_text(time_label, time_str);
    lv_obj_add_style(time_label, &s_style_time, 0);
    lv_obj_set_grid_cell(time_label, LV_GRID_ALIGN_END, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    return btn;
}

// Button click event handler
static void btn_click_event_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    size_t track_index = (size_t)lv_obj_get_user_data(btn);

    ESP_LOGI(TAG, "Playlist item clicked: track %zu", track_index);

    esp_err_t ret = sx_playlist_play_index(track_index);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to play track %zu: %s", track_index, esp_err_to_name(ret));
    } else {
        // Update button states
        size_t count = sx_playlist_get_count();
        for (size_t i = 0; i < count; i++) {
            bool is_current = (i == track_index);
            playlist_button_check(i, is_current && sx_audio_is_playing());
        }
    }
}

// List delete event handler
static void list_delete_event_cb(lv_event_t *e) {
    // Clean up styles
    lv_style_reset(&s_style_scrollbar);
    lv_style_reset(&s_style_btn);
    lv_style_reset(&s_style_button_pr);
    lv_style_reset(&s_style_button_chk);
    lv_style_reset(&s_style_button_dis);
    lv_style_reset(&s_style_title);
    lv_style_reset(&s_style_artist);
    lv_style_reset(&s_style_time);
    
    s_list = NULL;
}



