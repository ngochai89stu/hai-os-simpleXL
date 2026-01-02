#include "screen_music_player.h"

#include <esp_log.h>
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "sx_state.h"
#include "sx_audio_service.h"
#include "sx_playlist_manager.h"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "ui_icons.h"
#include "screen_music_player_spectrum.h"
#include "screen_music_player_list.h"
#include "../assets/sx_ui_assets.h"
#include "../ui_helpers/ui_animation_helpers.h"
#include "ui_theme_tokens.h"
#include "ui_slider.h"

static const char *TAG = "screen_music_player";

// Global album art reference for spectrum (Phase 2)
lv_obj_t *g_album_art_obj = NULL;

// Custom UI mode: full state
static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_album_art = NULL;
static lv_obj_t *s_track_title = NULL;
static lv_obj_t *s_track_artist = NULL;
static lv_obj_t *s_track_genre = NULL;  // Phase 3: Genre label
static lv_obj_t *s_progress_slider = NULL;  // Phase 2: Interactive slider
static lv_obj_t *s_play_btn = NULL;
static lv_obj_t *s_prev_btn = NULL;
static lv_obj_t *s_next_btn = NULL;
static lv_obj_t *s_volume_slider = NULL;
static lv_obj_t *s_container = NULL;
static lv_obj_t *s_play_label = NULL;  // Label inside play button
static bool s_last_playing_state = false;
static uint8_t s_last_volume = 50;

// Phase 2: Spectrum and Time Display
static lv_obj_t *s_spectrum_obj = NULL;
static lv_obj_t *s_time_current = NULL;
static lv_obj_t *s_time_total = NULL;
static lv_timer_t *s_time_timer = NULL;
static bool s_spectrum_anim_running = false;

// Phase 2: Playlist View
static lv_obj_t *s_playlist_list = NULL;
static lv_obj_t *s_list_btn = NULL;
static bool s_playlist_visible = false;

// Phase 3: Intro and Animation state
#define INTRO_TIME 2000

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

// Phase 4: Add wave decorations
// TODO: Implement wave decorations when assets are available
static void add_wave_decorations(lv_obj_t *parent) {
    // LV_IMAGE_DECLARE(img_lv_demo_music_wave_top);
    // LV_IMAGE_DECLARE(img_lv_demo_music_wave_bottom);
    // lv_obj_t *wave_top = lv_img_create(parent);
    // lv_img_set_src(wave_top, &img_lv_demo_music_wave_top);
    // lv_obj_set_width(wave_top, LV_PCT(100));
    // lv_obj_align(wave_top, LV_ALIGN_TOP_MID, 0, 0);
    // lv_obj_add_flag(wave_top, LV_OBJ_FLAG_IGNORE_LAYOUT);
    // 
    // lv_obj_t *wave_bottom = lv_img_create(parent);
    // lv_img_set_src(wave_bottom, &img_lv_demo_music_wave_bottom);
    // lv_obj_set_width(wave_bottom, LV_PCT(100));
    // lv_obj_align(wave_bottom, LV_ALIGN_BOTTOM_MID, 0, 0);
    // lv_obj_add_flag(wave_bottom, LV_OBJ_FLAG_IGNORE_LAYOUT);
    (void)parent; // Suppress unused parameter warning
}

// Phase 2: Progress slider seek handler
static void progress_slider_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        // Phase 5: Check if seek is supported
        sx_audio_caps_t caps = sx_audio_get_caps();
        if (!caps.seek_supported) {
            // Seek not supported, don't try to seek
            return;
        }
        
        int32_t value = lv_slider_get_value(s_progress_slider);
        uint32_t total = sx_audio_get_duration();
        if (total > 0) {
            uint32_t seek_pos = (value * total) / 100;
            esp_err_t ret = sx_audio_seek(seek_pos);
            if (ret != ESP_OK && ret != ESP_ERR_NOT_SUPPORTED) {
                ESP_LOGW(TAG, "Seek failed: %s", esp_err_to_name(ret));
            }
        }
    }
}

// Phase 2: Toggle playlist view
static void toggle_list_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        s_playlist_visible = !s_playlist_visible;
        
        if (s_playlist_visible) {
            // Show playlist, hide main content
            if (s_playlist_list != NULL) {
                lv_obj_clear_flag(s_playlist_list, LV_OBJ_FLAG_HIDDEN);
            }
            if (s_spectrum_obj != NULL) {
                lv_obj_add_flag(s_spectrum_obj, LV_OBJ_FLAG_HIDDEN);
            }
            if (s_album_art != NULL) {
                lv_obj_add_flag(s_album_art, LV_OBJ_FLAG_HIDDEN);
            }
            if (s_track_title != NULL) {
                lv_obj_add_flag(s_track_title, LV_OBJ_FLAG_HIDDEN);
            }
            if (s_track_artist != NULL) {
                lv_obj_add_flag(s_track_artist, LV_OBJ_FLAG_HIDDEN);
            }
            if (s_progress_slider != NULL) {
                lv_obj_add_flag(s_progress_slider, LV_OBJ_FLAG_HIDDEN);
            }
            if (s_time_current != NULL) {
                lv_obj_add_flag(s_time_current, LV_OBJ_FLAG_HIDDEN);
            }
            if (s_time_total != NULL) {
                lv_obj_add_flag(s_time_total, LV_OBJ_FLAG_HIDDEN);
            }
            // Update button icon
            if (s_list_btn != NULL) {
                // lv_img_set_src(s_list_btn, &img_lv_demo_music_btn_list_pause);
                lv_label_set_text(lv_obj_get_child(s_list_btn, 0), LV_SYMBOL_LIST);
            }
        } else {
            // Hide playlist, show main content
            if (s_playlist_list != NULL) {
                lv_obj_add_flag(s_playlist_list, LV_OBJ_FLAG_HIDDEN);
            }
            if (s_spectrum_obj != NULL) {
                lv_obj_clear_flag(s_spectrum_obj, LV_OBJ_FLAG_HIDDEN);
            }
            if (s_album_art != NULL) {
                lv_obj_clear_flag(s_album_art, LV_OBJ_FLAG_HIDDEN);
            }
            if (s_track_title != NULL) {
                lv_obj_clear_flag(s_track_title, LV_OBJ_FLAG_HIDDEN);
            }
            if (s_track_artist != NULL) {
                lv_obj_clear_flag(s_track_artist, LV_OBJ_FLAG_HIDDEN);
            }
            if (s_progress_slider != NULL) {
                lv_obj_clear_flag(s_progress_slider, LV_OBJ_FLAG_HIDDEN);
            }
            if (s_time_current != NULL) {
                lv_obj_clear_flag(s_time_current, LV_OBJ_FLAG_HIDDEN);
            }
            if (s_time_total != NULL) {
                lv_obj_clear_flag(s_time_total, LV_OBJ_FLAG_HIDDEN);
            }
            // Update button icon
            if (s_list_btn != NULL) {
                // lv_img_set_src(s_list_btn, &img_lv_demo_music_btn_list_play);
                lv_label_set_text(lv_obj_get_child(s_list_btn, 0), LV_SYMBOL_LIST);
            }
        }
    }
}

// Phase 2: Time timer callback
static void time_timer_cb(lv_timer_t *t) {
    uint32_t current = sx_audio_get_position();
    uint32_t total = sx_audio_get_duration();
    
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu:%02lu", (unsigned long)(current / 60), (unsigned long)(current % 60));
    if (s_time_current != NULL) {
        lv_label_set_text(s_time_current, buf);
    }
    
    snprintf(buf, sizeof(buf), "/ %lu:%02lu", (unsigned long)(total / 60), (unsigned long)(total % 60));
    if (s_time_total != NULL) {
        lv_label_set_text(s_time_total, buf);
    }
    
    // Update progress slider
    if (total > 0 && s_progress_slider != NULL) {
        // Only update if user is not dragging
        if (!lv_obj_has_state(s_progress_slider, LV_STATE_PRESSED)) {
            lv_slider_set_value(s_progress_slider, (current * 100) / total, LV_ANIM_ON);
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
    // Set background using token
    lv_obj_set_style_bg_color(container, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // Create top bar with back button (matching web demo)
    s_top_bar = screen_common_create_top_bar_with_back(container, "Music Player");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, UI_SPACE_XL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_content, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // Phase 4: Grid Layout (replaces flex)
    static const int32_t grid_cols[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t grid_rows[] = {
        LV_GRID_FR(1),        // Spacer top
        LV_GRID_CONTENT,      // Spectrum
        LV_GRID_CONTENT,      // Album art
        LV_GRID_FR(1),        // Spacer
        LV_GRID_CONTENT,      // Title
        LV_GRID_CONTENT,      // Artist
        LV_GRID_CONTENT,      // Genre
        LV_GRID_FR(1),        // Spacer
        LV_GRID_CONTENT,      // Progress slider
        LV_GRID_CONTENT,      // Time labels
        LV_GRID_CONTENT,      // Control buttons
        LV_GRID_FR(1),        // Spacer
        LV_GRID_CONTENT,      // Volume slider
        LV_GRID_TEMPLATE_LAST
    };
    lv_obj_set_grid_dsc_array(s_content, grid_cols, grid_rows);
    
    // Phase 4: Add wave decorations
    add_wave_decorations(s_content);
    
    // Phase 2: Spectrum visualization (before album art)
    s_spectrum_obj = lv_obj_create(s_content);
    lv_obj_remove_style_all(s_spectrum_obj);
    lv_obj_set_size(s_spectrum_obj, LV_PCT(100), 250);
    lv_obj_remove_flag(s_spectrum_obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_spectrum_obj, spectrum_draw_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_refresh_ext_draw_size(s_spectrum_obj);
    
    // Album art (large, centered) - Web demo style with shadow effect
    s_album_art = lv_obj_create(s_content);
    lv_obj_set_size(s_album_art, 220, 220);  // Slightly larger
    lv_obj_set_style_bg_color(s_album_art, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_album_art, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_album_art, 16, LV_PART_MAIN);  // More rounded
    
    // Set global reference for spectrum
    g_album_art_obj = s_album_art;
    
    // Placeholder icon in album art
    lv_obj_t *album_icon = ui_icon_create(s_album_art, UI_ICON_MUSIC_PLAYER, 48);
    if (album_icon != NULL) {
        lv_obj_set_style_text_color(album_icon, UI_COLOR_TEXT_SECONDARY, 0);  // Gray icon
        lv_obj_center(album_icon);
    }
    
    // Phase 3: Typography Hierarchy - Track info (title, artist, genre)
    s_track_title = lv_label_create(s_content);
    lv_label_set_text(s_track_title, "No track");
#if LV_FONT_MONTSERRAT_22
    lv_obj_set_style_text_font(s_track_title, UI_FONT_XLARGE, 0);  // Large font
#else
    lv_obj_set_style_text_font(s_track_title, UI_FONT_LARGE, 0);
#endif
    lv_obj_set_style_text_color(s_track_title, UI_COLOR_TEXT_PRIMARY, 0);
    lv_label_set_long_mode(s_track_title, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(s_track_title, LV_PCT(90));
    
    s_track_artist = lv_label_create(s_content);
    lv_label_set_text(s_track_artist, "Unknown artist");
#if LV_FONT_MONTSERRAT_16
    lv_obj_set_style_text_font(s_track_artist, UI_FONT_LARGE, 0);  // Medium font
#else
    lv_obj_set_style_text_font(s_track_artist, UI_FONT_MEDIUM, 0);
#endif
    lv_obj_set_style_text_color(s_track_artist, UI_COLOR_TEXT_SECONDARY, 0);
    lv_label_set_long_mode(s_track_artist, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(s_track_artist, LV_PCT(90));
    
    // Phase 3: Add genre label
    s_track_genre = lv_label_create(s_content);
    lv_label_set_text(s_track_genre, "Unknown genre");
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(s_track_genre, UI_FONT_SMALL, 0);  // Small font
#else
    lv_obj_set_style_text_font(s_track_genre, UI_FONT_MEDIUM, 0);
#endif
    lv_obj_set_style_text_color(s_track_genre, lv_color_hex(0x8a86b8), 0);  // Keep custom color for genre
    lv_label_set_long_mode(s_track_genre, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(s_track_genre, LV_PCT(90));
    
    // Phase 2: Time display labels
    lv_obj_t *time_container = lv_obj_create(s_content);
    lv_obj_set_size(time_container, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(time_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(time_container, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(time_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    s_time_current = lv_label_create(time_container);
    lv_label_set_text(s_time_current, "0:00");
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(s_time_current, UI_FONT_SMALL, 0);
#else
    lv_obj_set_style_text_font(s_time_current, UI_FONT_MEDIUM, 0);
#endif
    lv_obj_set_style_text_color(s_time_current, UI_COLOR_TEXT_PRIMARY, 0);
    
    s_time_total = lv_label_create(time_container);
    lv_label_set_text(s_time_total, "/ 0:00");
#if LV_FONT_MONTSERRAT_12
    lv_obj_set_style_text_font(s_time_total, UI_FONT_SMALL, 0);
#else
    lv_obj_set_style_text_font(s_time_total, UI_FONT_MEDIUM, 0);
#endif
    lv_obj_set_style_text_color(s_time_total, UI_COLOR_TEXT_SECONDARY, 0);
    
    // Phase 2: Interactive Progress Slider (replaces progress bar) - using shared component
    sx_audio_caps_t caps = sx_audio_get_caps();
    s_progress_slider = ui_gradient_slider_create(s_content, progress_slider_cb, NULL);
    lv_obj_set_size(s_progress_slider, LV_PCT(90), UI_SIZE_SLIDER_HEIGHT);  // Thin slider for progress
    
    // Phase 5: Disable slider if seek not supported
    if (!caps.seek_supported) {
        lv_obj_add_state(s_progress_slider, LV_STATE_DISABLED);
    } else {
        lv_obj_add_flag(s_progress_slider, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_set_style_anim_duration(s_progress_slider, 100, 0);
    
    lv_slider_set_range(s_progress_slider, 0, 100);
    lv_slider_set_value(s_progress_slider, 0, LV_ANIM_OFF);
    
    // Add seek event handler
    lv_obj_add_event_cb(s_progress_slider, progress_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Control buttons container
    lv_obj_t *controls = lv_obj_create(s_content);
    lv_obj_set_size(controls, LV_PCT(90), 60);
    lv_obj_set_style_bg_opa(controls, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(controls, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(controls, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(controls, 20, LV_PART_MAIN);
    
    // Phase 3: Previous button - Symbol button (temporary, until image assets available)
    s_prev_btn = lv_btn_create(controls);
    s_play_label = lv_label_create(s_prev_btn);
    lv_label_set_text(s_play_label, LV_SYMBOL_PREV);
    lv_obj_set_size(s_prev_btn, 50, 50);
    lv_obj_set_style_radius(s_prev_btn, 25, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_prev_btn, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
    lv_obj_add_event_cb(s_prev_btn, prev_btn_cb, LV_EVENT_CLICKED, NULL);
    
    // Phase 3: Play/Pause button - Symbol button (temporary, until image assets available)
    s_play_btn = lv_btn_create(controls);
    s_play_label = lv_label_create(s_play_btn);
    lv_label_set_text(s_play_label, LV_SYMBOL_PLAY);
    lv_obj_add_flag(s_play_btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_size(s_play_btn, 60, 60);
    lv_obj_set_style_radius(s_play_btn, 30, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_play_btn, UI_COLOR_PRIMARY, LV_PART_MAIN);
    lv_obj_add_event_cb(s_play_btn, play_pause_btn_cb, LV_EVENT_CLICKED, NULL);
    
    // Phase 3: Next button - Symbol button (temporary, until image assets available)
    s_next_btn = lv_btn_create(controls);
    lv_obj_t *next_label = lv_label_create(s_next_btn);
    lv_label_set_text(next_label, LV_SYMBOL_NEXT);
    lv_obj_set_size(s_next_btn, 50, 50);
    lv_obj_set_style_radius(s_next_btn, 25, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_next_btn, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
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
    lv_obj_set_style_text_font(volume_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(volume_label, UI_COLOR_TEXT_SECONDARY, 0);
    
    // Volume slider using shared component
    s_volume_slider = ui_gradient_slider_create(volume_container, volume_slider_cb, NULL);
    lv_obj_set_size(s_volume_slider, LV_PCT(100), UI_SIZE_SLIDER_HEIGHT_THICK);
    uint8_t current_volume = sx_audio_get_volume();
    lv_slider_set_value(s_volume_slider, current_volume, LV_ANIM_OFF);
    lv_slider_set_range(s_volume_slider, 0, 100);
    s_last_volume = current_volume;
    
    // Phase 2: Create playlist view (initially hidden)
    s_playlist_list = create_playlist_view(s_content);
    lv_obj_set_size(s_playlist_list, LV_PCT(100), LV_PCT(100) - 200);
    lv_obj_add_flag(s_playlist_list, LV_OBJ_FLAG_HIDDEN);
    
    // Phase 2: Add playlist toggle button (temporary symbol button, until image assets available)
    s_list_btn = lv_btn_create(s_content);
    lv_obj_t *list_label = lv_label_create(s_list_btn);
    lv_label_set_text(list_label, LV_SYMBOL_LIST);
    lv_obj_add_flag(s_list_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(s_list_btn, 40, 40);
    lv_obj_add_event_cb(s_list_btn, toggle_list_cb, LV_EVENT_CLICKED, NULL);
    
    // Phase 2: Create time update timer
    s_time_timer = lv_timer_create(time_timer_cb, 1000, NULL);
    lv_timer_pause(s_time_timer);
    
    // Phase 4: Set grid cells for all elements
    // Row 0: Spacer (empty)
    // Row 1: Spectrum
    if (s_spectrum_obj != NULL) {
        lv_obj_set_grid_cell(s_spectrum_obj, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    }
    // Row 2: Album art
    if (s_album_art != NULL) {
        lv_obj_set_grid_cell(s_album_art, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    }
    // Row 3: Spacer (empty)
    // Row 4: Title
    if (s_track_title != NULL) {
        lv_obj_set_grid_cell(s_track_title, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    }
    // Row 5: Artist
    if (s_track_artist != NULL) {
        lv_obj_set_grid_cell(s_track_artist, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 5, 1);
    }
    // Row 6: Genre
    if (s_track_genre != NULL) {
        lv_obj_set_grid_cell(s_track_genre, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 6, 1);
    }
    // Row 7: Spacer (empty)
    // Row 8: Progress slider
    if (s_progress_slider != NULL) {
        lv_obj_set_grid_cell(s_progress_slider, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 8, 1);
    }
    // Row 9: Time labels
    if (time_container != NULL) {
        lv_obj_set_grid_cell(time_container, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 9, 1);
    }
    // Row 10: Control buttons
    if (controls != NULL) {
        lv_obj_set_grid_cell(controls, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 10, 1);
    }
    // Row 11: Spacer (empty)
    // Row 12: Volume slider
    if (volume_container != NULL) {
        lv_obj_set_grid_cell(volume_container, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 12, 1);
    }
    
    // Phase 4: Add wave decorations (after all content is created)
    add_wave_decorations(s_content);
    
    // Phase 3: Intro animations - Use shared helper
    lv_obj_t *intro_objs[] = {
        s_album_art,
        s_track_title,
        s_track_artist,
        s_track_genre
    };
    ui_helper_fade_in_staggered(intro_objs, sizeof(intro_objs)/sizeof(intro_objs[0]), INTRO_TIME + 500, 200, 1000);
    
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
    // Update play/pause button state
    bool is_playing = sx_audio_is_playing();
    if (is_playing != s_last_playing_state && s_play_btn != NULL) {
        // Phase 3: Image button - use checked state for pause
        if (is_playing) {
            lv_obj_add_state(s_play_btn, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(s_play_btn, LV_STATE_CHECKED);
        }
        s_last_playing_state = is_playing;
        
        // Phase 2: Start/stop spectrum animation
        if (is_playing && s_spectrum_obj != NULL) {
            if (s_time_timer != NULL) {
                lv_timer_resume(s_time_timer);
            }
            // Phase 4: Only start animation if not already running (performance optimization)
            if (!s_spectrum_anim_running) {
                lv_anim_t a;
                lv_anim_init(&a);
                lv_anim_set_var(&a, s_spectrum_obj);
                lv_anim_set_values(&a, 0, 1000);
                lv_anim_set_exec_cb(&a, spectrum_anim_cb);
                lv_anim_set_duration(&a, 10000);
                lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
                lv_anim_start(&a);
                s_spectrum_anim_running = true;
            }
        } else {
            if (s_time_timer != NULL) {
                lv_timer_pause(s_time_timer);
            }
            if (s_spectrum_obj != NULL && s_spectrum_anim_running) {
                lv_anim_delete(s_spectrum_obj, spectrum_anim_cb);
                s_spectrum_anim_running = false;
            }
        }
    }
    
    // Update volume slider
    uint8_t current_volume = sx_audio_get_volume();
    if (current_volume != s_last_volume && s_volume_slider != NULL) {
        lv_slider_set_value(s_volume_slider, current_volume, LV_ANIM_OFF);
        s_last_volume = current_volume;
    }
    
    // Phase 2: Update track info from playlist
    int current_index = sx_playlist_get_current_index();
    if (current_index >= 0) {
        size_t track_index = (size_t)current_index;
        const char *title = sx_playlist_get_title(track_index);
        const char *artist = sx_playlist_get_artist(track_index);
        
        if (title && s_track_title != NULL) {
            lv_label_set_text(s_track_title, title);
        }
        if (artist && s_track_artist != NULL) {
            lv_label_set_text(s_track_artist, artist);
        }
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
    
    // Phase 2: Clean up timer and animation
    if (s_time_timer != NULL) {
        lv_timer_delete(s_time_timer);
        s_time_timer = NULL;
    }
    if (s_spectrum_obj != NULL && s_spectrum_anim_running) {
        lv_anim_delete(s_spectrum_obj, spectrum_anim_cb);
        s_spectrum_anim_running = false;
    }
    spectrum_reset();
    g_album_art_obj = NULL;
    
    // Custom UI mode: clean up UI objects
    if (s_top_bar != NULL) {
        lv_obj_del(s_top_bar);
        s_top_bar = NULL;
    }
    if (s_content != NULL) {
        lv_obj_del(s_content);
        s_content = NULL;
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
