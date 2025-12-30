#include "screen_google_navigation.h"

#include <esp_log.h>
#include <inttypes.h>
#include <string.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "sx_navigation_service.h"
#include "sx_navigation_ble.h"
#include "sx_settings_service.h"
#include "sx_tts_service.h"

static const char *TAG = "screen_google_navigation";

// Screen dimensions: 320x480
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 480
#define TOP_BAR_HEIGHT 40
#define CONTENT_HEIGHT (SCREEN_HEIGHT - TOP_BAR_HEIGHT)

// UI Components
static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_speed_card = NULL;
static lv_obj_t *s_speed_value_label = NULL;
static lv_obj_t *s_speed_unit_label = NULL;
static lv_obj_t *s_map_card = NULL;
static lv_obj_t *s_icon_image = NULL;
static lv_obj_t *s_instruction_card = NULL;
static lv_obj_t *s_next_road_label = NULL;
static lv_obj_t *s_next_road_desc_label = NULL;
static lv_obj_t *s_instruction_label = NULL;
static lv_obj_t *s_info_card = NULL;
static lv_obj_t *s_distance_label = NULL;
static lv_obj_t *s_time_label = NULL;
static lv_obj_t *s_ete_label = NULL;
static lv_obj_t *s_total_dist_label = NULL;
static lv_obj_t *s_connection_badge = NULL;
static lv_obj_t *s_container = NULL;
static lv_timer_t *s_nav_timer = NULL;
static lv_img_dsc_t s_icon_img_desc = {0};
static uint8_t s_icon_bitmap_buffer[SX_NAV_ICON_SIZE] = {0};
static bool s_overspeed_active = false;
static lv_timer_t *s_overspeed_timer = NULL;
static bool s_last_overspeed_state = false;

// Forward declarations
static void trigger_overspeed_alert(void);
static void overspeed_flash_cb(lv_timer_t *timer);
static void update_connection_status(void);
static lv_obj_t* create_card(lv_obj_t *parent, int32_t width, int32_t height);
static void apply_card_style(lv_obj_t *card);

// Color scheme (modern app-like)
#define COLOR_BG_DARK       0x121212
#define COLOR_CARD_DARK     0x1E1E1E
#define COLOR_CARD_LIGHT    0x2A2A2A
#define COLOR_PRIMARY       0x4285F4  // Google Blue
#define COLOR_ACCENT        0x34A853  // Google Green
#define COLOR_WARNING        0xFBBC04  // Google Yellow
#define COLOR_ERROR          0xEA4335  // Google Red
#define COLOR_TEXT_PRIMARY   0xFFFFFF
#define COLOR_TEXT_SECONDARY 0xB0B0B0
#define COLOR_TEXT_TERTIARY  0x808080

// Optimized spacing for 320x480 screen
#define PADDING_SMALL  8
#define PADDING_MEDIUM 12
#define PADDING_LARGE  16
#define GAP_SMALL      6
#define GAP_MEDIUM     10
#define GAP_LARGE      12

// Apply modern card style
static void apply_card_style(lv_obj_t *card) {
    lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_CARD_DARK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 12, LV_PART_MAIN);  // Smaller radius for small screen
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, PADDING_MEDIUM, LV_PART_MAIN);
    // Subtle shadow for small screen
    lv_obj_set_style_shadow_width(card, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_offset_x(card, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_offset_y(card, 2, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(card, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), LV_PART_MAIN);
}

// Create a modern card
static lv_obj_t* create_card(lv_obj_t *parent, int32_t width, int32_t height) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, width, height);
    apply_card_style(card);
    return card;
}

// Periodic UI updater for navigation data
static void nav_timer_cb(lv_timer_t *timer)
{
    (void) timer;

    sx_navigation_state_t state = sx_navigation_get_state();

    if (state != SX_NAV_STATE_NAVIGATING && state != SX_NAV_STATE_ARRIVED) {
        // No active navigation
        if (s_instruction_label) {
            lv_label_set_text(s_instruction_label, "Chưa có điều hướng");
        }
        if (s_next_road_label) {
            lv_label_set_text(s_next_road_label, "");
        }
        if (s_next_road_desc_label) {
            lv_label_set_text(s_next_road_desc_label, "");
        }
        if (s_distance_label) {
            lv_label_set_text(s_distance_label, "--");
        }
        if (s_time_label) {
            lv_label_set_text(s_time_label, "--");
        }
        if (s_ete_label) {
            lv_label_set_text(s_ete_label, "--");
        }
        if (s_total_dist_label) {
            lv_label_set_text(s_total_dist_label, "--");
        }
        if (s_speed_value_label) {
            lv_label_set_text(s_speed_value_label, "--");
        }
        if (s_connection_badge) {
            lv_label_set_text(s_connection_badge, "Ngắt kết nối");
            lv_obj_set_style_bg_color(s_connection_badge, lv_color_hex(COLOR_ERROR), LV_PART_MAIN);
        }
        return;
    }

    sx_nav_instruction_t instr = {0};
    if (sx_navigation_get_next_instruction(&instr) != ESP_OK) {
        return;
    }

    // Update Next Road
    if (s_next_road_label) {
        if (instr.next_road[0] != '\0') {
            lv_label_set_text(s_next_road_label, instr.next_road);
            lv_obj_set_style_text_color(s_next_road_label, lv_color_hex(COLOR_PRIMARY), 0);
        } else {
            lv_label_set_text(s_next_road_label, "");
        }
    }
    
    // Update Next Road Description
    if (s_next_road_desc_label) {
        if (instr.next_road_desc[0] != '\0') {
            lv_label_set_text(s_next_road_desc_label, instr.next_road_desc);
            lv_obj_set_style_text_color(s_next_road_desc_label, lv_color_hex(COLOR_TEXT_SECONDARY), 0);
        } else {
            lv_label_set_text(s_next_road_desc_label, "");
        }
    }

    // Update instruction text
    if (s_instruction_label) {
        lv_label_set_text(s_instruction_label, instr.text[0] ? instr.text : "Đang điều hướng...");
    }

    // Format distance
    char distance_buf[32];
    if (instr.distance_m >= 1000) {
        float km = instr.distance_m / 1000.0f;
        snprintf(distance_buf, sizeof(distance_buf), "%.1f km", km);
    } else {
        snprintf(distance_buf, sizeof(distance_buf), "%" PRIu32 " m", instr.distance_m);
    }

    // Format time
    char time_buf[32];
    uint32_t minutes = instr.time_s / 60;
    if (minutes == 0 && instr.time_s > 0) {
        minutes = 1;
    }
    snprintf(time_buf, sizeof(time_buf), "%" PRIu32 " phút", minutes);

    if (s_distance_label) {
        lv_label_set_text(s_distance_label, distance_buf);
    }
    if (s_time_label) {
        lv_label_set_text(s_time_label, time_buf);
    }
    
    // Update ETE
    if (s_ete_label) {
        if (instr.ete[0] != '\0') {
            lv_label_set_text(s_ete_label, instr.ete);
        } else {
            lv_label_set_text(s_ete_label, "--");
        }
    }
    
    // Update Total Distance
    if (s_total_dist_label) {
        if (instr.total_dist[0] != '\0') {
            lv_label_set_text(s_total_dist_label, instr.total_dist);
        } else {
            lv_label_set_text(s_total_dist_label, "--");
        }
    }
    
    // Update speed display
    if (s_speed_value_label && s_speed_unit_label) {
        if (instr.speed_kmh >= 0) {
            char speed_buf[16];
            snprintf(speed_buf, sizeof(speed_buf), "%d", instr.speed_kmh);
            lv_label_set_text(s_speed_value_label, speed_buf);
            lv_label_set_text(s_speed_unit_label, "km/h");
            
            // Get speed limit
            int32_t speed_limit = 60;
            sx_settings_get_int_default("nav_speed_limit", &speed_limit, 60);
            
            // Color based on speed
            bool is_overspeed = (instr.speed_kmh >= speed_limit);
            if (is_overspeed) {
                lv_obj_set_style_text_color(s_speed_value_label, lv_color_hex(COLOR_ERROR), 0);
                lv_obj_set_style_text_color(s_speed_unit_label, lv_color_hex(COLOR_ERROR), 0);
                
                // Trigger overspeed alert (only on state change)
                if (!s_last_overspeed_state) {
                    s_last_overspeed_state = true;
                    trigger_overspeed_alert();
                }
            } else {
                lv_obj_set_style_text_color(s_speed_value_label, lv_color_hex(COLOR_ACCENT), 0);
                lv_obj_set_style_text_color(s_speed_unit_label, lv_color_hex(COLOR_ACCENT), 0);
                s_last_overspeed_state = false;
            }
        } else {
            lv_label_set_text(s_speed_value_label, "--");
            lv_label_set_text(s_speed_unit_label, "");
            lv_obj_set_style_text_color(s_speed_value_label, lv_color_hex(COLOR_TEXT_TERTIARY), 0);
            s_last_overspeed_state = false;
        }
    }
    
    // Update connection status
    update_connection_status();
    
    // Update icon
    if (s_icon_image) {
        sx_nav_icon_t icon = {0};
        if (sx_navigation_ble_get_icon(&icon) == ESP_OK && icon.valid) {
            memcpy(s_icon_bitmap_buffer, icon.bitmap, SX_NAV_ICON_SIZE);
            s_icon_img_desc.header.w = SX_NAV_ICON_WIDTH;
            s_icon_img_desc.header.h = SX_NAV_ICON_HEIGHT;
            s_icon_img_desc.header.cf = LV_COLOR_FORMAT_RGB565;
            s_icon_img_desc.data_size = SX_NAV_ICON_SIZE;
            s_icon_img_desc.data = s_icon_bitmap_buffer;
            lv_img_set_src(s_icon_image, &s_icon_img_desc);
            lv_obj_clear_flag(s_icon_image, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_icon_image, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void on_create(void) {
    ESP_LOGI(TAG, "Navigation screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Modern dark background
    lv_obj_set_style_bg_color(container, lv_color_hex(COLOR_BG_DARK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, LV_PART_MAIN);
    
    // Top bar
    s_top_bar = screen_common_create_top_bar_with_back(container, "Điều hướng");
    
    // Content area - optimized for 320x480
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), CONTENT_HEIGHT);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, TOP_BAR_HEIGHT);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, PADDING_SMALL, LV_PART_MAIN);  // Reduced padding
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(s_content, GAP_SMALL, LV_PART_MAIN);  // Reduced gap
    
    // Speed Card (Top right, compact) - Optimized for small screen
    s_speed_card = create_card(s_content, 85, 70);  // Smaller size
    lv_obj_align(s_speed_card, LV_ALIGN_TOP_RIGHT, -PADDING_SMALL, PADDING_SMALL);
    lv_obj_set_flex_flow(s_speed_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_speed_card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(s_speed_card, PADDING_SMALL, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_speed_card, 2, LV_PART_MAIN);
    
    // Speed value (larger font, centered)
    s_speed_value_label = lv_label_create(s_speed_card);
    lv_label_set_text(s_speed_value_label, "--");
    lv_obj_set_style_text_font(s_speed_value_label, &lv_font_montserrat_14, 0);  // Largest available
    lv_obj_set_style_text_color(s_speed_value_label, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_text_align(s_speed_value_label, LV_TEXT_ALIGN_CENTER, 0);
    // Make text appear larger by using scale (if supported) or bold
    lv_obj_set_style_text_letter_space(s_speed_value_label, 1, 0);
    
    // Speed unit (smaller)
    s_speed_unit_label = lv_label_create(s_speed_card);
    lv_label_set_text(s_speed_unit_label, "");
    lv_obj_set_style_text_font(s_speed_unit_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_speed_unit_label, lv_color_hex(COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_align(s_speed_unit_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // Map/Icon Card (compact for small screen)
    s_map_card = create_card(s_content, LV_PCT(100), 100);  // Reduced from 140 to 100
    lv_obj_set_flex_flow(s_map_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_map_card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(s_map_card, PADDING_MEDIUM, LV_PART_MAIN);
    
    // Navigation icon (smaller for compact layout)
    s_icon_image = lv_img_create(s_map_card);
    lv_obj_set_size(s_icon_image, 64, 64);  // Reduced from 80 to 64
    lv_obj_add_flag(s_icon_image, LV_OBJ_FLAG_HIDDEN);
    
    // Placeholder icon
    lv_obj_t *map_icon = lv_label_create(s_map_card);
    lv_label_set_text(map_icon, "🗺️");
    lv_obj_set_style_text_font(map_icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(map_icon, lv_color_hex(COLOR_TEXT_SECONDARY), 0);
    
    // Instruction Card (main content) - Compact
    s_instruction_card = create_card(s_content, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_instruction_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_instruction_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(s_instruction_card, PADDING_MEDIUM, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_instruction_card, GAP_SMALL, LV_PART_MAIN);
    
    // Next Road (prominent, but compact)
    s_next_road_label = lv_label_create(s_instruction_card);
    lv_label_set_text(s_next_road_label, "");
    lv_obj_set_style_text_font(s_next_road_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_next_road_label, lv_color_hex(COLOR_PRIMARY), 0);
    lv_label_set_long_mode(s_next_road_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_next_road_label, LV_PCT(100));
    lv_obj_set_style_text_align(s_next_road_label, LV_TEXT_ALIGN_LEFT, 0);
    
    // Next Road Description
    s_next_road_desc_label = lv_label_create(s_instruction_card);
    lv_label_set_text(s_next_road_desc_label, "");
    lv_obj_set_style_text_font(s_next_road_desc_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_next_road_desc_label, lv_color_hex(COLOR_TEXT_SECONDARY), 0);
    lv_label_set_long_mode(s_next_road_desc_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_next_road_desc_label, LV_PCT(100));
    
    // Instruction text
    s_instruction_label = lv_label_create(s_instruction_card);
    lv_label_set_text(s_instruction_label, "Chưa có điều hướng");
    lv_obj_set_style_text_font(s_instruction_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_instruction_label, lv_color_hex(COLOR_TEXT_PRIMARY), 0);
    lv_label_set_long_mode(s_instruction_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_instruction_label, LV_PCT(100));
    
    // Info Card (distance, time, ETE, total) - Compact layout
    s_info_card = create_card(s_content, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_info_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_info_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(s_info_card, PADDING_MEDIUM, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_info_card, GAP_SMALL, LV_PART_MAIN);
    
    // ETE and Total Distance row (compact)
    lv_obj_t *eta_row = lv_obj_create(s_info_card);
    lv_obj_set_size(eta_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(eta_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(eta_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(eta_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(eta_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(eta_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    s_ete_label = lv_label_create(eta_row);
    lv_label_set_text(s_ete_label, "ETE: --");
    lv_obj_set_style_text_font(s_ete_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_ete_label, lv_color_hex(COLOR_TEXT_SECONDARY), 0);
    
    s_total_dist_label = lv_label_create(eta_row);
    lv_label_set_text(s_total_dist_label, "Tổng: --");
    lv_obj_set_style_text_font(s_total_dist_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_total_dist_label, lv_color_hex(COLOR_TEXT_SECONDARY), 0);
    
    // Distance and Time row (compact)
    lv_obj_t *dist_time_row = lv_obj_create(s_info_card);
    lv_obj_set_size(dist_time_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(dist_time_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(dist_time_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dist_time_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(dist_time_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dist_time_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    s_distance_label = lv_label_create(dist_time_row);
    lv_label_set_text(s_distance_label, "--");
    lv_obj_set_style_text_font(s_distance_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_distance_label, lv_color_hex(COLOR_TEXT_SECONDARY), 0);
    
    s_time_label = lv_label_create(dist_time_row);
    lv_label_set_text(s_time_label, "--");
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_time_label, lv_color_hex(COLOR_TEXT_SECONDARY), 0);
    
    // Connection badge (bottom right, compact)
    s_connection_badge = lv_obj_create(s_content);
    lv_obj_set_size(s_connection_badge, LV_SIZE_CONTENT, 24);  // Smaller height
    lv_obj_align(s_connection_badge, LV_ALIGN_BOTTOM_RIGHT, -PADDING_SMALL, -PADDING_SMALL);
    lv_obj_set_style_bg_color(s_connection_badge, lv_color_hex(COLOR_ERROR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_connection_badge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_connection_badge, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(s_connection_badge, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(s_connection_badge, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_connection_badge, 0, LV_PART_MAIN);
    
    lv_obj_t *conn_label = lv_label_create(s_connection_badge);
    lv_label_set_text(conn_label, "Ngắt kết nối");
    lv_obj_set_style_text_font(conn_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(conn_label, lv_color_hex(COLOR_TEXT_PRIMARY), 0);
    lv_obj_center(conn_label);
    
    // Create update timer
    s_nav_timer = lv_timer_create(nav_timer_cb, 1000, NULL);
    if (s_nav_timer != NULL) {
        lv_timer_set_repeat_count(s_nav_timer, LV_ANIM_REPEAT_INFINITE);
    }

    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_GOOGLE_NAVIGATION, "Google Navigation", container, s_content);
    #endif
}

static void on_show(void) {
    ESP_LOGI(TAG, "Navigation screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_GOOGLE_NAVIGATION);
    #endif
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Navigation screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_GOOGLE_NAVIGATION);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Navigation screen onDestroy");
    
    if (s_nav_timer != NULL) {
        lv_timer_del(s_nav_timer);
        s_nav_timer = NULL;
    }
    if (s_overspeed_timer != NULL) {
        lv_timer_del(s_overspeed_timer);
        s_overspeed_timer = NULL;
    }

    if (s_top_bar != NULL) {
        lv_obj_del(s_top_bar);
        s_top_bar = NULL;
    }
    if (s_content != NULL) {
        lv_obj_del(s_content);
        s_content = NULL;
    }
}

static void trigger_overspeed_alert(void) {
    // Visual alert: flash card
    if (s_speed_card && s_overspeed_timer == NULL) {
        s_overspeed_timer = lv_timer_create(overspeed_flash_cb, 200, NULL);
        lv_timer_set_repeat_count(s_overspeed_timer, 5);
    }
    
    // Audio alert: TTS
    sx_tts_speak_simple("Cảnh báo vượt tốc độ");
}

static void overspeed_flash_cb(lv_timer_t *timer) {
    (void)timer;
    static bool flash_state = false;
    
    if (s_speed_card) {
        flash_state = !flash_state;
        if (flash_state) {
            lv_obj_set_style_bg_color(s_speed_card, lv_color_hex(COLOR_ERROR), LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(s_speed_card, lv_color_hex(COLOR_CARD_DARK), LV_PART_MAIN);
        }
    }
}

static void update_connection_status(void) {
    if (!s_connection_badge) {
        return;
    }
    
    bool connected = sx_navigation_ble_is_connected();
    lv_obj_t *label = lv_obj_get_child(s_connection_badge, 0);
    
    if (connected) {
        if (label) lv_label_set_text(label, "Đã kết nối");
        lv_obj_set_style_bg_color(s_connection_badge, lv_color_hex(COLOR_ACCENT), LV_PART_MAIN);
    } else {
        if (label) lv_label_set_text(label, "Ngắt kết nối");
        lv_obj_set_style_bg_color(s_connection_badge, lv_color_hex(COLOR_ERROR), LV_PART_MAIN);
    }
}

void screen_google_navigation_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
    };
    ui_screen_registry_register(SCREEN_ID_GOOGLE_NAVIGATION, &callbacks);
}
