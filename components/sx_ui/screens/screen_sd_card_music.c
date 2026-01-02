#include "screen_sd_card_music.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "sx_state.h"
#include "sx_sd_service.h"
#include "sx_sd_music_service.h"
#include "sx_audio_service.h"
#include "ui_icons.h"
#include "ui_theme_tokens.h"
#include "ui_list.h"

static const char *TAG = "screen_sd_card_music";

typedef struct {
    char path[512];
    bool is_dir;
} file_item_data_t;

// Forward declarations
static void load_sd_files(void);

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_file_list = NULL;
static lv_obj_t *s_refresh_btn = NULL;
static lv_obj_t *s_container = NULL;
static bool s_files_loaded = false;
static char s_current_path[256] = "/";

static void on_create(void) {
    ESP_LOGI(TAG, "SD Card Music screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Set background using token
    lv_obj_set_style_bg_color(container, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "SD Card Music");
    
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
    
    // Refresh button
    s_refresh_btn = lv_btn_create(s_content);
    lv_obj_set_size(s_refresh_btn, LV_PCT(100), UI_SIZE_BUTTON_HEIGHT);
    lv_obj_set_style_bg_color(s_refresh_btn, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_radius(s_refresh_btn, 5, LV_PART_MAIN);
    lv_obj_t *refresh_label = lv_label_create(s_refresh_btn);
    lv_label_set_text(refresh_label, "🔄 Refresh");
    lv_obj_set_style_text_font(refresh_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(refresh_label, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(refresh_label);
    
    // File browser list (scrollable) - using shared component
    s_file_list = ui_scrollable_list_create(s_content);
    lv_obj_set_size(s_file_list, LV_PCT(100), LV_PCT(100) - 50);
    
    // Load files from SD card if mounted
    s_files_loaded = false;
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_SD_CARD_MUSIC, "SD Card Music", container, s_file_list);
    #endif
}

static void refresh_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Refresh button clicked");
        s_files_loaded = false;
        load_sd_files();
    }
}

static void on_show(void) {
    ESP_LOGI(TAG, "SD Card Music screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_SD_CARD_MUSIC);
    #endif
    
    // Add refresh button handler
    if (s_refresh_btn != NULL) {
        lv_obj_add_event_cb(s_refresh_btn, refresh_btn_cb, LV_EVENT_CLICKED, NULL);
    }
}

static void on_hide(void) {
    ESP_LOGI(TAG, "SD Card Music screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_SD_CARD_MUSIC);
    #endif
}

static void navigate_to_directory(const char *path) {
    if (path == NULL) {
        return;
    }
    
    strncpy(s_current_path, path, sizeof(s_current_path) - 1);
    s_current_path[sizeof(s_current_path) - 1] = '\0';
    s_files_loaded = false;
    load_sd_files();
}

static void file_item_click_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_obj_t *item = lv_event_get_target(e);
        void *user_data = lv_obj_get_user_data(item);
        
        // Check if it's parent directory marker
        if (user_data == (void *)-1) {
            // Navigate to parent directory
            char *last_slash = strrchr(s_current_path, '/');
            if (last_slash != NULL && last_slash != s_current_path) {
                *last_slash = '\0';
            } else {
                strcpy(s_current_path, "/");
            }
            s_files_loaded = false;
            load_sd_files();
            return;
        }
        
        file_item_data_t *data = (file_item_data_t *)user_data;
        if (data == NULL) {
            return;
        }
        
        if (data->is_dir) {
            // Navigate into directory
            navigate_to_directory(data->path);
        } else {
            // Play audio file
            ESP_LOGI(TAG, "Playing file: %s", data->path);
            sx_audio_play_file(data->path);
        }
    }
}

static void load_sd_files(void) {
    if (s_file_list == NULL) {
        return;
    }
    
    if (!sx_sd_is_mounted()) {
        // Show "SD card not mounted" message
        if (lvgl_port_lock(0)) {
            lv_obj_clean(s_file_list);
            lv_obj_t *msg = lv_label_create(s_file_list);
            lv_label_set_text(msg, "SD card not mounted");
            lv_obj_set_style_text_font(msg, UI_FONT_MEDIUM, 0);
            lv_obj_set_style_text_color(msg, UI_COLOR_TEXT_SECONDARY, 0);
            lv_obj_align(msg, LV_ALIGN_CENTER, 0, 0);
            lvgl_port_unlock();
        }
        s_files_loaded = false;
        return;
    }
    
    if (!lvgl_port_lock(0)) {
        return;
    }
    
    // Clear existing items
    lv_obj_clean(s_file_list);
    
    // List music files from current directory (with metadata)
    sx_sd_music_entry_t entries[50];
    size_t count = 0;
    esp_err_t ret = sx_sd_music_list_files(s_current_path, entries, 50, &count);
    
    if (ret == ESP_OK && count > 0) {
        // Add ".." for parent directory if not at root
        if (strcmp(s_current_path, "/") != 0) {
            ui_list_item_two_line_create(
                s_file_list,
                NULL,  // No icon (or use UI_ICON_SD_CARD if needed)
                "..",
                "Parent directory",
                NULL,
                file_item_click_cb,
                (void *)-1  // Special marker for parent directory
            );
        }
        
        // Add files using shared component
        for (size_t i = 0; i < count; i++) {
            
            // Build full path
            char full_path[512];
            if (strcmp(s_current_path, "/") == 0) {
                snprintf(full_path, sizeof(full_path), "/%s", entries[i].name);
            } else {
                snprintf(full_path, sizeof(full_path), "%s/%s", s_current_path, entries[i].name);
            }
            
            // Store file path and is_dir in user data
            file_item_data_t *data = malloc(sizeof(file_item_data_t));
            if (data != NULL) {
                strncpy(data->path, entries[i].path, sizeof(data->path) - 1);
                data->path[sizeof(data->path) - 1] = '\0';
                data->is_dir = entries[i].is_dir;
            }
            
            // Build display text
            char display_text[256];
            char subtitle[256] = {0};
            if (!entries[i].is_dir && entries[i].metadata.has_metadata) {
                if (strlen(entries[i].metadata.title) > 0 && strlen(entries[i].metadata.artist) > 0) {
                    snprintf(display_text, sizeof(display_text), "%s", entries[i].metadata.title);
                    snprintf(subtitle, sizeof(subtitle), "%s", entries[i].metadata.artist);
                } else if (strlen(entries[i].metadata.title) > 0) {
                    snprintf(display_text, sizeof(display_text), "%s", entries[i].metadata.title);
                } else {
                    snprintf(display_text, sizeof(display_text), "%s", entries[i].name);
                }
            } else {
                snprintf(display_text, sizeof(display_text), "%s", entries[i].name);
                if (entries[i].is_dir) {
                    snprintf(subtitle, sizeof(subtitle), "Directory");
                }
            }
            
            ui_list_item_two_line_create(
                s_file_list,
                NULL,  // No icon for now
                display_text,
                subtitle[0] ? subtitle : NULL,
                NULL,  // No extra text
                file_item_click_cb,
                data
            );
        }
        s_files_loaded = true;
    } else {
        // Show "No files" message
        lv_obj_t *msg = lv_label_create(s_file_list);
        lv_label_set_text(msg, "No files found");
        lv_obj_set_style_text_font(msg, UI_FONT_MEDIUM, 0);
        lv_obj_set_style_text_color(msg, UI_COLOR_TEXT_SECONDARY, 0);
        lv_obj_align(msg, LV_ALIGN_CENTER, 0, 0);
        s_files_loaded = false;
    }
    
    lvgl_port_unlock();
}

static void on_update(const sx_state_t *state) {
    // Load SD files when screen is shown and SD is mounted
    if (!s_files_loaded) {
        load_sd_files();
    }
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "SD Card Music screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_SD_CARD_MUSIC);
    #endif
    
    // Free user data for all file items
    if (s_file_list != NULL) {
        uint32_t child_cnt = lv_obj_get_child_cnt(s_file_list);
        for (uint32_t i = 0; i < child_cnt; i++) {
            lv_obj_t *child = lv_obj_get_child(s_file_list, i);
            void *user_data = lv_obj_get_user_data(child);
            if (user_data != NULL && user_data != (void *)-1) {
                free(user_data);
            }
        }
    }
    
    if (s_top_bar != NULL) {
        lv_obj_del(s_top_bar);
        s_top_bar = NULL;
    }
    if (s_content != NULL) {
        lv_obj_del(s_content);
        s_content = NULL;
    }
    
    s_files_loaded = false;
    strcpy(s_current_path, "/");
}

// Register this screen
void screen_sd_card_music_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
        .on_update = on_update,
    };
    ui_screen_registry_register(SCREEN_ID_SD_CARD_MUSIC, &callbacks);
}
