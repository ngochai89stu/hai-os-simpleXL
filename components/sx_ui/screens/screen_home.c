#include "screen_home.h"

#include <esp_log.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "ui_screen_registry.h"
#include "sx_dispatcher.h"
#include "sx_state.h"
#include "sx_ui_verify.h"
#include "screen_common.h"
#include "ui_icons.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

static const char *TAG = "screen_home";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_grid = NULL;
static lv_obj_t *s_container = NULL;
static lv_timer_t *s_idle_timer = NULL;
static const uint32_t IDLE_TIMEOUT_MS = 30000;  // 30 seconds

// Forward declarations
static void idle_timer_cb(lv_timer_t *timer);
static void home_touch_event_cb(lv_event_t *e);
static void reset_idle_timer(void);

// Home menu items (matching web demo: 2x3 grid + Chatbot)
typedef struct {
    const char *title;
    ui_icon_type_t icon_type;
    ui_screen_id_t screen_id;
} home_menu_item_t;

static const home_menu_item_t s_home_menu_items[] = {
    {"Music Player", UI_ICON_MUSIC_PLAYER, SCREEN_ID_MUSIC_PLAYER},
    {"Online Music", UI_ICON_MUSIC_ONLINE, SCREEN_ID_MUSIC_ONLINE_LIST},
    {"Radio", UI_ICON_RADIO, SCREEN_ID_RADIO},
    {"SD Card", UI_ICON_SD_CARD, SCREEN_ID_SD_CARD_MUSIC},
    {"IR Control", UI_ICON_IR_CONTROL, SCREEN_ID_IR_CONTROL},
    {"Settings", UI_ICON_SETTINGS, SCREEN_ID_SETTINGS},
    {"Chatbot", UI_ICON_CHAT, SCREEN_ID_CHAT},  // Chatbot as 7th item
};

static void home_menu_item_click_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *item = lv_event_get_target(e);
        ui_screen_id_t screen_id = (ui_screen_id_t)(intptr_t)lv_obj_get_user_data(item);
        ESP_LOGI(TAG, "Home menu item clicked, navigating to screen: %d", screen_id);
        
        ui_router_navigate_to(screen_id);
    }
}

static lv_obj_t* create_home_menu_item(lv_obj_t *parent, const home_menu_item_t *menu_item) {
    // Create grid item (card) - Web demo style
    lv_obj_t *item = lv_obj_create(parent);
    lv_obj_set_size(item, LV_PCT(48), 110);  // 2 columns, slightly taller
    lv_obj_set_style_bg_color(item, lv_color_hex(0x2a2a2a), LV_PART_MAIN);  // Darker card background
    lv_obj_set_style_bg_color(item, lv_color_hex(0x3a3a3a), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(item, 0, LV_PART_MAIN);  // No border for cleaner look
    lv_obj_set_style_pad_all(item, 15, LV_PART_MAIN);
    lv_obj_set_style_radius(item, 12, LV_PART_MAIN);  // Rounded corners
    lv_obj_set_user_data(item, (void *)(intptr_t)menu_item->screen_id);
    lv_obj_add_event_cb(item, home_menu_item_click_cb, LV_EVENT_CLICKED, NULL);
    
    // Icon using LVGL symbols - larger and centered
    lv_obj_t *icon = ui_icon_create(item, menu_item->icon_type, 32);
    if (icon != NULL) {
        lv_obj_set_style_text_color(icon, lv_color_hex(0x5b7fff), 0);  // Primary blue
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 10);
    }
    
    // Title - larger font
    lv_obj_t *label = lv_label_create(item);
    lv_label_set_text(label, menu_item->title);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(label, LV_PCT(90));
    
    return item;
}

static void on_create(void) {
    ESP_LOGI(TAG, "Home screen onCreate (Launcher)");
    
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
    
    // Set dark background (web demo style)
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    
    // Create grid container (2x3 + 1 chatbot = 7 items) - full screen
    s_grid = lv_obj_create(container);
    lv_obj_set_size(s_grid, LV_PCT(100), LV_PCT(100));  // Full screen
    lv_obj_align(s_grid, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(s_grid, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_grid, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_grid, 15, LV_PART_MAIN);  // More padding
    lv_obj_set_flex_flow(s_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(s_grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(s_grid, 15, LV_PART_MAIN);  // More spacing
    lv_obj_set_style_pad_column(s_grid, 15, LV_PART_MAIN);
    
    // Add home menu items (2x3 grid + chatbot)
    for (int i = 0; i < sizeof(s_home_menu_items) / sizeof(s_home_menu_items[0]); i++) {
        create_home_menu_item(s_grid, &s_home_menu_items[i]);
    }
    
    // Add touch event handler to reset idle timer
    lv_obj_add_event_cb(container, home_touch_event_cb, LV_EVENT_ALL, NULL);
    
    lvgl_port_unlock();
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_HOME, "Home", container, s_grid);
    #endif
}

static void idle_timer_cb(lv_timer_t *timer) {
    (void)timer;
    ESP_LOGI(TAG, "Home screen idle timeout, returning to screensaver");
    
    // Navigate to flash screen (screensaver)
    if (lvgl_port_lock(0)) {
        ui_router_navigate_to(SCREEN_ID_FLASH);
        lvgl_port_unlock();
    }
}

static void home_touch_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    // Reset idle timer on any touch event
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_CLICKED || code == LV_EVENT_LONG_PRESSED) {
        reset_idle_timer();
    }
}

static void reset_idle_timer(void) {
    if (s_idle_timer != NULL) {
        lv_timer_reset(s_idle_timer);
    } else {
        // Create timer if it doesn't exist
        s_idle_timer = lv_timer_create(idle_timer_cb, IDLE_TIMEOUT_MS, NULL);
        lv_timer_set_repeat_count(s_idle_timer, 1);  // Run once
    }
}

static void on_show(void) {
    ESP_LOGI(TAG, "Home screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_HOME);
    #endif
    
    // Start/reset idle timer
    reset_idle_timer();
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Home screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_HOME);
    #endif
    
    // Stop idle timer
    if (s_idle_timer != NULL) {
        lv_timer_del(s_idle_timer);
        s_idle_timer = NULL;
    }
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Home screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_HOME);
    #endif
    
    // Delete idle timer
    if (s_idle_timer != NULL) {
        lv_timer_del(s_idle_timer);
        s_idle_timer = NULL;
    }
    
    if (lvgl_port_lock(0)) {
        if (s_top_bar != NULL) {
            lv_obj_del(s_top_bar);
            s_top_bar = NULL;
        }
        if (s_grid != NULL) {
            lv_obj_del(s_grid);
            s_grid = NULL;
        }
        lvgl_port_unlock();
    }
}

static void on_update(const sx_state_t *state) {
    // Reset idle timer on any state update (user activity)
    reset_idle_timer();
}

// Register this screen
void screen_home_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
        .on_update = on_update,
    };
    ui_screen_registry_register(SCREEN_ID_HOME, &callbacks);
}

