#include "screen_settings.h"

#include <esp_log.h>
#include <string.h>
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)
#include "ui_router.h"
#include "sx_ui_verify.h"
#include "screen_common.h"
#include "sx_state.h"
#include "ui_theme_tokens.h"
#include "ui_icons.h"

static const char *TAG = "screen_settings";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_grid = NULL;
static lv_obj_t *s_container = NULL;


// Settings menu items with icons (like phone app)
typedef struct {
    const char *title;
    ui_icon_type_t icon_type;
    ui_screen_id_t target;
} settings_menu_item_t;

static const settings_menu_item_t s_settings_items[] = {
    {"Display", UI_ICON_DISPLAY, SCREEN_ID_DISPLAY_SETTING},
    {"Bluetooth", UI_ICON_BLUETOOTH, SCREEN_ID_BLUETOOTH_SETTING},
    {"Screensaver", UI_ICON_SCREENSAVER, SCREEN_ID_SCREENSAVER_SETTING},
    {"Equalizer", UI_ICON_EQUALIZER, SCREEN_ID_EQUALIZER},
    {"Wi-Fi", UI_ICON_WIFI, SCREEN_ID_WIFI_SETUP},
    {"Voice", UI_ICON_VOICE, SCREEN_ID_VOICE_SETTINGS},
    {"About", UI_ICON_ABOUT, SCREEN_ID_ABOUT},
};

// Navigation callback for icon items
static void settings_item_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    ui_screen_id_t target_screen = (ui_screen_id_t)(intptr_t)lv_event_get_user_data(e);
    if (target_screen > 0) {  // Valid screen ID
        if (lvgl_port_lock(0)) {
            ui_router_navigate_to(target_screen);
            lvgl_port_unlock();
        }
    }
}

// Create settings icon card (like phone app)
static lv_obj_t* create_settings_icon_card(lv_obj_t *parent, const settings_menu_item_t *item) {
    // Create card container
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, LV_PCT(48), 100);  // 2 columns, compact size
    lv_obj_set_style_bg_color(card, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, UI_COLOR_BG_PRESSED, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, UI_SPACE_LG, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
    lv_obj_set_user_data(card, (void *)(intptr_t)item->target);
    lv_obj_add_event_cb(card, settings_item_cb, LV_EVENT_CLICKED, NULL);
    
    // Icon - larger and centered
    lv_obj_t *icon = ui_icon_create(card, item->icon_type, 36);
    if (icon != NULL) {
        lv_obj_set_style_text_color(icon, UI_COLOR_PRIMARY, 0);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 8);
    }
    
    // Title label below icon
    lv_obj_t *label = lv_label_create(card);
    lv_label_set_text(label, item->title);
    lv_obj_set_style_text_font(label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(label, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(label, LV_PCT(90));
    
    return card;
}

static void on_create(void) {
    ESP_LOGI(TAG, "Settings screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Set background using token
    lv_obj_set_style_bg_color(container, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "Settings");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, UI_SPACE_LG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_content, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // Create grid container for icon layout (2 columns, like phone app)
    s_grid = lv_obj_create(s_content);
    lv_obj_set_size(s_grid, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_grid, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_grid, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_grid, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(s_grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(s_grid, UI_SPACE_LG, LV_PART_MAIN);
    lv_obj_set_style_pad_column(s_grid, UI_SPACE_LG, LV_PART_MAIN);
    
    // Create settings icon cards (2 columns grid)
    for (int i = 0; i < sizeof(s_settings_items) / sizeof(s_settings_items[0]); i++) {
        create_settings_icon_card(s_grid, &s_settings_items[i]);
    }
    
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_SETTINGS, "Settings", container, s_content);
    #endif
}

static void on_show(void) {
    ESP_LOGI(TAG, "Settings screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_SETTINGS);
    #endif
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Settings screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_SETTINGS);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Settings screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_SETTINGS);
    #endif
    
    if (s_top_bar != NULL) {
        lv_obj_del(s_top_bar);
        s_top_bar = NULL;
    }
    if (s_grid != NULL) {
        lv_obj_del(s_grid);
        s_grid = NULL;
    }
    if (s_content != NULL) {
        lv_obj_del(s_content);
        s_content = NULL;
    }
}

static void on_update(const sx_state_t *state) {
    // Settings list doesn't need state updates
    (void)state;
}

// Register this screen
void screen_settings_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
        .on_update = on_update,
    };
    ui_screen_registry_register(SCREEN_ID_SETTINGS, &callbacks);
}
