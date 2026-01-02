#include "screen_music_online_list.h"

#include <esp_log.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "sx_music_online_service.h"
#include "sx_audio_service.h"
#include "ui_theme_tokens.h"
#include "ui_list.h"

static const char *TAG = "screen_music_online_list";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_search_bar = NULL;
static lv_obj_t *s_search_btn = NULL;
static lv_obj_t *s_track_list = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_container = NULL;
// Removed unused variable: s_search_text

// Forward declarations
static void search_btn_cb(lv_event_t *e);
static void update_track_info(void);

static void search_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (s_search_bar == NULL) {
            return;
        }
        
        const char *search_text = lv_textarea_get_text(s_search_bar);
        if (search_text == NULL || strlen(search_text) == 0) {
            ESP_LOGW(TAG, "Search text is empty");
            return;
        }
        
        ESP_LOGI(TAG, "Search button clicked: %s", search_text);
        
        // Update status
        if (s_status_label != NULL) {
            lv_label_set_text(s_status_label, "Playing...");
        }
        
        // Play song using music online service
        // Note: sx_music_online_play_song expects song_name and optional artist_name
        // For now, we'll use the search text as song name
        esp_err_t ret = sx_music_online_play_song(search_text, NULL);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Song play request sent: %s", search_text);
            // Update track info after a short delay (service will update track info)
            vTaskDelay(pdMS_TO_TICKS(500));
            update_track_info();
        } else {
            ESP_LOGE(TAG, "Failed to play song: %s", esp_err_to_name(ret));
            if (s_status_label != NULL) {
                lv_label_set_text(s_status_label, "Play failed");
            }
        }
    }
}

static void on_create(void) {
    ESP_LOGI(TAG, "Online Music screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Set background using token
    lv_obj_set_style_bg_color(container, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "Online Music");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, UI_SPACE_XL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_content, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Search bar with button
    lv_obj_t *search_container = lv_obj_create(s_content);
    lv_obj_set_size(search_container, LV_PCT(100), UI_SIZE_BUTTON_HEIGHT);
    lv_obj_set_style_bg_opa(search_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(search_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(search_container, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(search_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(search_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    s_search_bar = lv_textarea_create(search_container);
    lv_obj_set_size(s_search_bar, LV_PCT(80), UI_SIZE_BUTTON_HEIGHT - 10);
    lv_textarea_set_placeholder_text(s_search_bar, "Search song name...");
    lv_obj_set_style_bg_color(s_search_bar, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_search_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_search_bar, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_search_bar, UI_SPACE_XL, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_search_bar, UI_FONT_MEDIUM, 0);
    
    s_search_btn = lv_btn_create(search_container);
    lv_obj_set_size(s_search_btn, LV_PCT(18), UI_SIZE_BUTTON_HEIGHT - 10);
    lv_obj_set_style_bg_color(s_search_btn, UI_COLOR_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_radius(s_search_btn, 5, LV_PART_MAIN);
    lv_obj_t *search_btn_label = lv_label_create(s_search_btn);
    lv_label_set_text(search_btn_label, "Play");
    lv_obj_set_style_text_font(search_btn_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(search_btn_label, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(search_btn_label);
    
    // Status label
    s_status_label = lv_label_create(s_content);
    lv_label_set_text(s_status_label, "Enter song name and click Play");
    lv_obj_set_style_text_font(s_status_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(s_status_label, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_width(s_status_label, LV_PCT(100));
    
    // Track list (scrollable) - using shared component
    s_track_list = ui_scrollable_list_create(s_content);
    lv_obj_set_size(s_track_list, LV_PCT(100), LV_PCT(100) - 110);
    
    // Add search button event handler
    lv_obj_add_event_cb(s_search_btn, search_btn_cb, LV_EVENT_CLICKED, NULL);
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_MUSIC_ONLINE_LIST, "Music Online List", container, s_track_list);
    #endif
}

// Forward declaration
static void update_track_info(void);

static void update_track_info(void) {
    // Clear existing track list
    if (s_track_list != NULL) {
        lv_obj_clean(s_track_list);
    }
    
    // Get current track from service
    const sx_music_online_track_t *track = sx_music_online_get_current_track();
    if (track != NULL && track->title[0] != '\0') {
        // Create track info using shared component
        char subtitle[128];
        if (track->artist[0] != '\0') {
            snprintf(subtitle, sizeof(subtitle), "%s", track->artist);
        } else {
            subtitle[0] = '\0';
        }
        
        ui_list_item_two_line_create(
            s_track_list,
            NULL,  // No icon
            track->title,
            subtitle[0] ? subtitle : NULL,
            NULL,  // No extra text
            NULL,  // No callback (read-only info)
            NULL
        );
        
        // Skip old manual creation code
        return;
        
        // Old code (kept for reference)
        lv_obj_t *track_card = lv_obj_create(s_track_list);
        lv_obj_set_size(track_card, LV_PCT(100), 120);
        lv_obj_set_style_bg_color(track_card, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
        lv_obj_set_style_border_width(track_card, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(track_card, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_all(track_card, UI_SPACE_LG, LV_PART_MAIN);
        
        // Title
        lv_obj_t *title_label = lv_label_create(track_card);
        lv_label_set_text(title_label, track->title);
        lv_obj_set_style_text_font(title_label, UI_FONT_MEDIUM, 0);
        lv_obj_set_style_text_color(title_label, UI_COLOR_TEXT_PRIMARY, 0);
        lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_label_set_long_mode(title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(title_label, LV_PCT(100));
        
        // Artist
        if (track->artist[0] != '\0') {
            lv_obj_t *artist_label = lv_label_create(track_card);
            lv_label_set_text(artist_label, track->artist);
            lv_obj_set_style_text_font(artist_label, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(artist_label, lv_color_hex(0x888888), 0);
            lv_obj_align(artist_label, LV_ALIGN_LEFT_MID, 0, 0);
        }
        
        // Album
        if (track->album[0] != '\0') {
            lv_obj_t *album_label = lv_label_create(track_card);
            lv_label_set_text(album_label, track->album);
            lv_obj_set_style_text_font(album_label, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(album_label, lv_color_hex(0x888888), 0);
            lv_obj_align(album_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        }
        
        // Update status
        if (s_status_label != NULL) {
            lv_label_set_text(s_status_label, "Now playing");
        }
    } else {
        // No track info available
        if (s_status_label != NULL) {
            lv_label_set_text(s_status_label, "Enter song name and click Play");
        }
    }
}

static void on_show(void) {
    ESP_LOGI(TAG, "Online Music screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_MUSIC_ONLINE_LIST);
    #endif
    
    // Update track info when screen is shown
    update_track_info();
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Online Music screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_MUSIC_ONLINE_LIST);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Online Music screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_MUSIC_ONLINE_LIST);
    #endif
    
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
void screen_music_online_list_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
    };
    ui_screen_registry_register(SCREEN_ID_MUSIC_ONLINE_LIST, &callbacks);
}
