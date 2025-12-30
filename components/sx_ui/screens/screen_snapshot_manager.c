#include "screen_snapshot_manager.h"

#include <esp_log.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "sx_image_service.h"
#include "sx_settings_service.h"
#include "sx_state_manager.h"

static const char *TAG = "screen_snapshot_manager";

#define MAX_SNAPSHOTS 20
#define SNAPSHOT_DIR "/sdcard/snapshots"
#define SNAPSHOT_PREVIEW_SIZE 40
#define SNAPSHOT_METADATA_EXT ".json"
#define SNAPSHOT_IMAGE_EXT ".png"

typedef struct {
    char file_path[256];
    char file_name[64];
    lv_obj_t *list_item;
    lv_obj_t *preview_img;
    bool image_loaded;
    sx_lvgl_image_t *lvgl_image; // LVGL image descriptor for preview
} snapshot_item_t;

static snapshot_item_t s_snapshots[MAX_SNAPSHOTS];
static size_t s_snapshot_count = 0;

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_snapshot_list = NULL;
static lv_obj_t *s_save_btn = NULL;
static lv_obj_t *s_load_btn = NULL;
static lv_obj_t *s_container = NULL;

// Forward declarations
static void scan_snapshot_files(void);
static void refresh_snapshot_list(void);
static void load_snapshot_image(snapshot_item_t *item);
static void save_btn_cb(lv_event_t *e);
static void load_btn_cb(lv_event_t *e);
static void snapshot_item_click_cb(lv_event_t *e);
static esp_err_t save_snapshot(const char *name);
static esp_err_t load_snapshot(const char *file_path);
static void generate_snapshot_filename(char *filename, size_t max_len);

static void on_create(void) {
    ESP_LOGI(TAG, "Snapshot Manager screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Set background
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "Snapshot Manager");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Action buttons
    lv_obj_t *btn_container = lv_obj_create(s_content);
    lv_obj_set_size(btn_container, LV_PCT(100), 50);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn_container, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_container, 10, LV_PART_MAIN);
    
    s_save_btn = lv_btn_create(btn_container);
    lv_obj_set_size(s_save_btn, LV_PCT(48), 50);
    lv_obj_set_style_bg_color(s_save_btn, lv_color_hex(0x5b7fff), LV_PART_MAIN);
    lv_obj_set_style_radius(s_save_btn, 5, LV_PART_MAIN);
    lv_obj_t *save_label = lv_label_create(s_save_btn);
    lv_label_set_text(save_label, "Save");
    lv_obj_set_style_text_font(save_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(save_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(save_label);
    
    s_load_btn = lv_btn_create(btn_container);
    lv_obj_set_size(s_load_btn, LV_PCT(48), 50);
    lv_obj_set_style_bg_color(s_load_btn, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
    lv_obj_set_style_radius(s_load_btn, 5, LV_PART_MAIN);
    lv_obj_t *load_label = lv_label_create(s_load_btn);
    lv_label_set_text(load_label, "Load");
    lv_obj_set_style_text_font(load_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(load_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(load_label);
    
    // Snapshot list (scrollable)
    s_snapshot_list = lv_obj_create(s_content);
    lv_obj_set_size(s_snapshot_list, LV_PCT(100), LV_PCT(100) - 70);
    lv_obj_set_style_bg_opa(s_snapshot_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_snapshot_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_snapshot_list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_snapshot_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_snapshot_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Add event handlers for buttons
    lv_obj_add_event_cb(s_save_btn, save_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_load_btn, load_btn_cb, LV_EVENT_CLICKED, NULL);
    
    // Scan and refresh snapshot list
    scan_snapshot_files();
    refresh_snapshot_list();
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_SNAPSHOT_MANAGER, "Snapshot Manager", container, s_snapshot_list);
    #endif
}

static void on_show(void) {
    ESP_LOGI(TAG, "Snapshot Manager screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_SNAPSHOT_MANAGER);
    #endif
    
    // Refresh snapshot list when showing
    scan_snapshot_files();
    refresh_snapshot_list();
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Snapshot Manager screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_SNAPSHOT_MANAGER);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Snapshot Manager screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_SNAPSHOT_MANAGER);
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

// Scan snapshot directory for image files
static void scan_snapshot_files(void) {
    s_snapshot_count = 0;
    memset(s_snapshots, 0, sizeof(s_snapshots));
    
    DIR *dir = opendir(SNAPSHOT_DIR);
    if (dir == NULL) {
        ESP_LOGW(TAG, "Snapshot directory not found: %s", SNAPSHOT_DIR);
        return;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && s_snapshot_count < MAX_SNAPSHOTS) {
        // Skip . and ..
        if (entry->d_name[0] == '.') {
            continue;
        }
        
        // Check if file is an image
        char file_path[256];
        int path_len = snprintf(file_path, sizeof(file_path), "%s/%s", SNAPSHOT_DIR, entry->d_name);
        if (path_len < 0 || path_len >= (int)sizeof(file_path)) {
            ESP_LOGW(TAG, "File path too long, skipping: %s", entry->d_name);
            continue;
        }
        
        sx_image_type_t img_type = sx_image_detect_type(file_path);
        if (img_type == SX_IMAGE_TYPE_UNKNOWN) {
            continue; // Skip non-image files
        }
        
        // Add to snapshot list
        snapshot_item_t *item = &s_snapshots[s_snapshot_count];
        strncpy(item->file_path, file_path, sizeof(item->file_path) - 1);
        strncpy(item->file_name, entry->d_name, sizeof(item->file_name) - 1);
        item->list_item = NULL;
        item->preview_img = NULL;
        item->image_loaded = false;
        item->lvgl_image = NULL;
        s_snapshot_count++;
    }
    
    closedir(dir);
    ESP_LOGI(TAG, "Scanned %zu snapshot files", s_snapshot_count);
}

// Refresh snapshot list UI
static void refresh_snapshot_list(void) {
    if (s_snapshot_list == NULL) {
        return;
    }
    
    // Clear existing items
    lv_obj_clean(s_snapshot_list);
    
    // Create list items for each snapshot
    for (size_t i = 0; i < s_snapshot_count; i++) {
        snapshot_item_t *item = &s_snapshots[i];
        
        // Create list item container
        item->list_item = lv_obj_create(s_snapshot_list);
        lv_obj_set_size(item->list_item, LV_PCT(100), 60);
        lv_obj_set_style_bg_color(item->list_item, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        lv_obj_set_style_bg_color(item->list_item, lv_color_hex(0x3a3a3a), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_border_width(item->list_item, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(item->list_item, 10, LV_PART_MAIN);
        lv_obj_set_style_radius(item->list_item, 5, LV_PART_MAIN);
        lv_obj_set_style_pad_row(item->list_item, 5, LV_PART_MAIN);
        
        // Add click event
        lv_obj_add_event_cb(item->list_item, snapshot_item_click_cb, LV_EVENT_CLICKED, item);
        
        // Create preview image (placeholder, will load on demand)
        item->preview_img = lv_img_create(item->list_item);
        lv_obj_set_size(item->preview_img, SNAPSHOT_PREVIEW_SIZE, SNAPSHOT_PREVIEW_SIZE);
        lv_obj_set_style_bg_color(item->preview_img, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(item->preview_img, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_align(item->preview_img, LV_ALIGN_LEFT_MID, 0, 0);
        
        // Create label with file name
        lv_obj_t *label = lv_label_create(item->list_item);
        lv_label_set_text(label, item->file_name);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, SNAPSHOT_PREVIEW_SIZE + 10, 0);
        
        // Load image preview asynchronously (simplified: load immediately)
        load_snapshot_image(item);
    }
    
    if (s_snapshot_count == 0) {
        // Show empty state
        lv_obj_t *empty_label = lv_label_create(s_snapshot_list);
        lv_label_set_text(empty_label, "No snapshots found");
        lv_obj_set_style_text_font(empty_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(empty_label, lv_color_hex(0x888888), 0);
        lv_obj_center(empty_label);
    }
}

// Load snapshot image and display as preview
static void load_snapshot_image(snapshot_item_t *item) {
    if (item == NULL || item->preview_img == NULL || item->image_loaded) {
        return;
    }
    
    sx_image_info_t info;
    uint8_t *image_data = NULL;
    
    esp_err_t ret = sx_image_load_from_file(item->file_path, &info, &image_data);
    if (ret != ESP_OK || image_data == NULL || info.width == 0 || info.height == 0) {
        ESP_LOGW(TAG, "Failed to load image: %s", item->file_path);
        if (image_data != NULL) {
            sx_image_free(image_data);
        }
        return;
    }
    
    // Check if image is already decoded to RGB565
    bool is_rgb565 = (info.data_size == (size_t)(info.width * info.height * 2));
    
    if (is_rgb565) {
        // Create LVGL image descriptor from RGB565 data
        sx_lvgl_image_t *lvgl_img = sx_image_create_lvgl_rgb565(image_data, info.width, info.height);
        
        if (lvgl_img != NULL && lvgl_img->img_dsc != NULL) {
            // Set image source
            if (lvgl_port_lock(0)) {
                lv_img_set_src(item->preview_img, lvgl_img->img_dsc);
                lvgl_port_unlock();
            }
            
            // Store LVGL image for cleanup
            item->lvgl_image = lvgl_img;
            item->image_loaded = true;
            
            ESP_LOGI(TAG, "Loaded preview for: %s (%dx%d)", item->file_name, info.width, info.height);
        } else {
            ESP_LOGE(TAG, "Failed to create LVGL image descriptor for: %s", item->file_name);
            sx_image_free(image_data);
        }
    } else {
        // Image not decoded yet, show placeholder
        ESP_LOGW(TAG, "Image not decoded to RGB565 for: %s, showing placeholder", item->file_name);
        sx_image_free(image_data);
        item->image_loaded = false; // Mark as not loaded so we can retry later
    }
}

// Generate unique snapshot filename with timestamp
static void generate_snapshot_filename(char *filename, size_t max_len) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Format: snapshot_YYYYMMDD_HHMMSS
    snprintf(filename, max_len, "snapshot_%04d%02d%02d_%02d%02d%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

// Save current state as snapshot
static esp_err_t save_snapshot(const char *name) {
    char snapshot_name[64];
    
    if (name == NULL || strlen(name) == 0) {
        generate_snapshot_filename(snapshot_name, sizeof(snapshot_name));
    } else {
        strncpy(snapshot_name, name, sizeof(snapshot_name) - 1);
        snapshot_name[sizeof(snapshot_name) - 1] = '\0';
    }
    
    ESP_LOGI(TAG, "Saving snapshot: %s", snapshot_name);
    
    // Create snapshot directory if it doesn't exist
    struct stat st = {0};
    if (stat(SNAPSHOT_DIR, &st) == -1) {
        // Directory doesn't exist, try to create (requires mkdir implementation)
        ESP_LOGW(TAG, "Snapshot directory does not exist: %s", SNAPSHOT_DIR);
        // Note: mkdir may not be available, this is a limitation
    }
    
    // Build file paths
    char metadata_path[256];
    char image_path[256];
    snprintf(metadata_path, sizeof(metadata_path), "%s/%s%s", SNAPSHOT_DIR, snapshot_name, SNAPSHOT_METADATA_EXT);
    snprintf(image_path, sizeof(image_path), "%s/%s%s", SNAPSHOT_DIR, snapshot_name, SNAPSHOT_IMAGE_EXT);
    
    // Save metadata (current screen, device state, etc.)
    FILE *meta_file = fopen(metadata_path, "w");
    if (meta_file != NULL) {
        ui_screen_id_t current_screen = ui_router_get_current_screen();
        sx_device_state_t device_state = sx_state_manager_get_state();
        
        // Simple JSON-like format
        fprintf(meta_file, "{\n");
        fprintf(meta_file, "  \"name\": \"%s\",\n", snapshot_name);
        fprintf(meta_file, "  \"screen_id\": %d,\n", current_screen);
        fprintf(meta_file, "  \"device_state\": %d,\n", device_state);
        time_t timestamp = time(NULL);
        fprintf(meta_file, "  \"timestamp\": %lld\n", (long long)timestamp);
        fprintf(meta_file, "}\n");
        
        fclose(meta_file);
        ESP_LOGI(TAG, "Saved snapshot metadata: %s", metadata_path);
    } else {
        ESP_LOGE(TAG, "Failed to create metadata file: %s", metadata_path);
        return ESP_ERR_NOT_FOUND;
    }
    
    // TODO: Capture screen and save as image
    // This requires LVGL screen capture API or canvas rendering
    // For now, we'll just save metadata
    ESP_LOGI(TAG, "Snapshot saved: %s (metadata only, screen capture not implemented)", snapshot_name);
    
    return ESP_OK;
}

// Load snapshot from file
static esp_err_t load_snapshot(const char *file_path) {
    if (file_path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Loading snapshot: %s", file_path);
    
    // Check if metadata file exists
    char metadata_path[256];
    strncpy(metadata_path, file_path, sizeof(metadata_path) - 1);
    metadata_path[sizeof(metadata_path) - 1] = '\0';
    
    // Replace image extension with metadata extension
    char *ext = strrchr(metadata_path, '.');
    if (ext != NULL) {
        strncpy(ext, SNAPSHOT_METADATA_EXT, sizeof(metadata_path) - (ext - metadata_path) - 1);
    } else {
        strncat(metadata_path, SNAPSHOT_METADATA_EXT, sizeof(metadata_path) - strlen(metadata_path) - 1);
    }
    
    // Read metadata
    FILE *meta_file = fopen(metadata_path, "r");
    if (meta_file != NULL) {
        char line[256];
        int screen_id = -1;
        
        while (fgets(line, sizeof(line), meta_file) != NULL) {
            if (strstr(line, "screen_id") != NULL) {
                sscanf(line, "  \"screen_id\": %d,", &screen_id);
            }
        }
        
        fclose(meta_file);
        
        if (screen_id >= 0) {
            // Navigate to saved screen
            if (lvgl_port_lock(0)) {
                ui_router_navigate_to((ui_screen_id_t)screen_id);
                lvgl_port_unlock();
            }
            ESP_LOGI(TAG, "Restored screen: %d", screen_id);
        }
    } else {
        ESP_LOGW(TAG, "Metadata file not found: %s, trying to load as image only", metadata_path);
    }
    
    // TODO: Load and display snapshot image if available
    // This would require loading the image file and displaying it
    
    return ESP_OK;
}

// Button callbacks
static void save_btn_cb(lv_event_t *e) {
    (void)e;
    ESP_LOGI(TAG, "Save button clicked");
    
    // Save current snapshot
    esp_err_t ret = save_snapshot(NULL); // Auto-generate name
    if (ret == ESP_OK) {
        // Refresh list
        scan_snapshot_files();
        if (lvgl_port_lock(0)) {
            refresh_snapshot_list();
            lvgl_port_unlock();
        }
    } else {
        ESP_LOGE(TAG, "Failed to save snapshot: %s", esp_err_to_name(ret));
    }
}

static void load_btn_cb(lv_event_t *e) {
    (void)e;
    ESP_LOGI(TAG, "Load button clicked");
    
    // Load first snapshot if available
    if (s_snapshot_count > 0) {
        load_snapshot(s_snapshots[0].file_path);
    } else {
        ESP_LOGW(TAG, "No snapshots available to load");
    }
}

static void snapshot_item_click_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) {
        return;
    }
    
    snapshot_item_t *item = (snapshot_item_t *)lv_event_get_user_data(e);
    if (item == NULL) {
        return;
    }
    
    ESP_LOGI(TAG, "Snapshot item clicked: %s", item->file_name);
    
    // Load selected snapshot
    load_snapshot(item->file_path);
}

// Register this screen
void screen_snapshot_manager_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
    };
    ui_screen_registry_register(SCREEN_ID_SNAPSHOT_MANAGER, &callbacks);
}
