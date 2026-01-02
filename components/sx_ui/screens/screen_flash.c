#include "screen_flash.h"

#include <esp_log.h>
#include <string.h>
#include <time.h>
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#include "ui_router.h"
#include "sx_ui_verify.h"
#include "sx_weather_service.h"
#include "sx_settings_service.h"
#include "sx_image_service.h"
#include "ui_image_helpers.h"
#include "ui_assets_wrapper.h"  // Wrapper để access assets (tuân theo SIMPLEXL_ARCH v1.3)
#include "ui_theme_tokens.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

static const char *TAG = "screen_flash";

// UI elements
static lv_obj_t *s_container = NULL;
static lv_obj_t *s_background_img = NULL;
static lv_obj_t *s_clock_label = NULL;
static lv_obj_t *s_date_label = NULL;
static lv_obj_t *s_weather_widget = NULL;
static lv_obj_t *s_weather_city_label = NULL;
static lv_obj_t *s_weather_temp_label = NULL;
static lv_obj_t *s_weather_desc_label = NULL;

// Background image data (for cleanup)
static sx_lvgl_image_t *s_current_bg_img = NULL;

// Swipe detection
static lv_point_t s_touch_start = {0, 0};
static lv_point_t s_touch_end = {0, 0};
static bool s_touch_active = false;
static const int SWIPE_THRESHOLD = 50;  // Minimum swipe distance in pixels
static const int SWIPE_BOTTOM_AREA = 240;  // Bottom half of screen (480/2)

// Timer for clock update
static lv_timer_t *s_clock_timer = NULL;

// Forward declarations
static void clock_timer_cb(lv_timer_t *timer);
static void update_clock(void);
static void update_weather(void);
static void load_background_image(void);
static void touch_event_cb(lv_event_t *e);

static void update_clock(void) {
    
    // Get current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Format time (HH:MM)
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    
    if (s_clock_label != NULL) {
        lv_label_set_text(s_clock_label, time_str);
    }
    
    // Format date (Day, DD Month) - phone-style
    const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char date_str[32];
    snprintf(date_str, sizeof(date_str), "%s, %02d %s", 
             days[timeinfo.tm_wday], timeinfo.tm_mday, months[timeinfo.tm_mon]);
    
    if (s_date_label != NULL) {
        lv_label_set_text(s_date_label, date_str);
    }
}

static void update_weather(void) {
    const sx_weather_info_t *weather = sx_weather_get_info();
    if (weather != NULL && weather->valid) {
        if (s_weather_city_label != NULL && weather->city[0] != '\0') {
            lv_label_set_text(s_weather_city_label, weather->city);
        }
        if (s_weather_temp_label != NULL) {
            char temp_str[16];
            snprintf(temp_str, sizeof(temp_str), "%.0f°C", weather->temp);
            lv_label_set_text(s_weather_temp_label, temp_str);
        }
        if (s_weather_desc_label != NULL && weather->description[0] != '\0') {
            lv_label_set_text(s_weather_desc_label, weather->description);
        }
    } else {
        // Weather data not available
        if (s_weather_city_label != NULL) {
            lv_label_set_text(s_weather_city_label, "Loading...");
        }
        if (s_weather_temp_label != NULL) {
            lv_label_set_text(s_weather_temp_label, "--°C");
        }
        if (s_weather_desc_label != NULL) {
            lv_label_set_text(s_weather_desc_label, "");
        }
    }
}

static void load_background_image(void) {
    // Get background image setting
    char bg_setting[64] = {0};
    sx_settings_get_string_default("screensaver_bg_image", bg_setting, sizeof(bg_setting), "Embedded");
    
    if (s_container != NULL) {
        // First, try to use embedded flashscreen image (default)
        if (strcmp(bg_setting, "Embedded") == 0 || strcmp(bg_setting, "Default") == 0) {
            const lv_img_dsc_t *flashscreen_img = ui_assets_get_flashscreen_img();
            if (flashscreen_img != NULL && flashscreen_img->data_size > 2) {  // Check if not placeholder
                ESP_LOGI(TAG, "Using embedded flashscreen image: %dx%d, size=%zu", 
                         flashscreen_img->header.w, flashscreen_img->header.h, flashscreen_img->data_size);
                
                // Clean up previous SD card image if exists
                if (s_current_bg_img != NULL) {
                    sx_ui_image_free_lvgl(s_current_bg_img);
                    s_current_bg_img = NULL;
                }
                
                // Create or update background image object
                if (s_background_img == NULL) {
                    s_background_img = lv_img_create(s_container);
                    lv_obj_set_size(s_background_img, LV_PCT(100), LV_PCT(100));
                    lv_obj_align(s_background_img, LV_ALIGN_CENTER, 0, 0);
                    lv_obj_move_background(s_background_img); // Put behind other elements
                }
                
                // Set image source
                lv_img_set_src(s_background_img, flashscreen_img);
                ESP_LOGI(TAG, "Embedded flashscreen image displayed successfully");
                return;
            } else {
                ESP_LOGW(TAG, "Embedded flashscreen image not available or placeholder, falling back to gradient");
            }
        }
        
        if (strcmp(bg_setting, "Gradient") == 0) {
            // Gradient background (dark blue to black)
            lv_obj_set_style_bg_color(s_container, lv_color_hex(0x0E1426), LV_PART_MAIN);
            lv_obj_set_style_bg_grad_color(s_container, lv_color_hex(0x000000), LV_PART_MAIN);
            lv_obj_set_style_bg_grad_dir(s_container, LV_GRAD_DIR_VER, LV_PART_MAIN);
        } else if (strcmp(bg_setting, "Image 1") == 0 || strcmp(bg_setting, "Image 2") == 0 || strcmp(bg_setting, "Custom") == 0) {
            // Load image from SD card or flash
            char image_path[256] = {0};
            
            // Determine image path based on setting
            if (strcmp(bg_setting, "Image 1") == 0) {
                snprintf(image_path, sizeof(image_path), "/sdcard/backgrounds/bg1.jpg");
            } else if (strcmp(bg_setting, "Image 2") == 0) {
                snprintf(image_path, sizeof(image_path), "/sdcard/backgrounds/bg2.jpg");
            } else if (strcmp(bg_setting, "Custom") == 0) {
                // Get custom image path from settings
                char custom_path[256] = {0};
                sx_settings_get_string_default("screensaver_custom_bg_path", custom_path, sizeof(custom_path), "");
                if (custom_path[0] != '\0') {
                    strncpy(image_path, custom_path, sizeof(image_path) - 1);
                } else {
                    snprintf(image_path, sizeof(image_path), "/sdcard/backgrounds/custom.jpg");
                }
            }
            
            // Try to load image
            sx_image_info_t img_info;
            uint8_t *img_data = NULL;
            esp_err_t img_ret = sx_image_load_from_file(image_path, &img_info, &img_data);
            
            if (img_ret == ESP_OK && img_data != NULL && img_info.width > 0 && img_info.height > 0) {
                ESP_LOGI(TAG, "Loaded background image: %s (%dx%d, type=%d, size=%zu)", 
                         image_path, img_info.width, img_info.height, img_info.type, img_info.data_size);
                
                // Check if image is already decoded to RGB565 (PNG decoded automatically)
                // If data_size matches RGB565 size (width * height * 2), it's already decoded
                bool is_rgb565 = (img_info.data_size == (size_t)(img_info.width * img_info.height * 2));
                
                if (is_rgb565) {
                    // Create LVGL image descriptor from RGB565 data
                    sx_lvgl_image_t *lvgl_img = sx_ui_image_create_lvgl_rgb565(img_data, img_info.width, img_info.height);
                    
                    if (lvgl_img != NULL && lvgl_img->img_dsc != NULL) {
                        // Create or update background image object
                        if (s_background_img == NULL) {
                            s_background_img = lv_img_create(s_container);
                            lv_obj_set_size(s_background_img, LV_PCT(100), LV_PCT(100));
                            lv_obj_align(s_background_img, LV_ALIGN_CENTER, 0, 0);
                            lv_obj_move_background(s_background_img); // Put behind other elements
                        }
                        
                        // Set image source
                        lv_img_set_src(s_background_img, lvgl_img->img_dsc);
                        
                        // Store LVGL image pointer for cleanup
                        if (s_current_bg_img != NULL) {
                            sx_ui_image_free_lvgl(s_current_bg_img);
                        }
                        s_current_bg_img = lvgl_img;
                        
                        ESP_LOGI(TAG, "Background image displayed successfully");
                    } else {
                        ESP_LOGE(TAG, "Failed to create LVGL image descriptor");
                        sx_image_free(img_data);
                        // Fallback to gradient
                        lv_obj_set_style_bg_color(s_container, lv_color_hex(0x0E1426), LV_PART_MAIN);
                        lv_obj_set_style_bg_grad_color(s_container, lv_color_hex(0x000000), LV_PART_MAIN);
                        lv_obj_set_style_bg_grad_dir(s_container, LV_GRAD_DIR_VER, LV_PART_MAIN);
                    }
                } else {
                    // Image not decoded yet (JPEG or raw PNG)
                    ESP_LOGW(TAG, "Image not decoded to RGB565, using gradient fallback");
                    sx_image_free(img_data);
                    // Fallback to gradient
                    lv_obj_set_style_bg_color(s_container, lv_color_hex(0x0E1426), LV_PART_MAIN);
                    lv_obj_set_style_bg_grad_color(s_container, lv_color_hex(0x000000), LV_PART_MAIN);
                    lv_obj_set_style_bg_grad_dir(s_container, LV_GRAD_DIR_VER, LV_PART_MAIN);
                }
            } else {
                ESP_LOGW(TAG, "Failed to load background image: %s (error: %s), using gradient fallback", 
                         image_path, esp_err_to_name(img_ret));
                // Fallback to gradient
                lv_obj_set_style_bg_color(s_container, lv_color_hex(0x0E1426), LV_PART_MAIN);
                lv_obj_set_style_bg_grad_color(s_container, lv_color_hex(0x000000), LV_PART_MAIN);
                lv_obj_set_style_bg_grad_dir(s_container, LV_GRAD_DIR_VER, LV_PART_MAIN);
            }
        } else {
            // Default to embedded flashscreen image, fallback to gradient
            const lv_img_dsc_t *flashscreen_img = ui_assets_get_flashscreen_img();
            if (flashscreen_img != NULL && flashscreen_img->data_size > 2) {  // Check if not placeholder
                ESP_LOGI(TAG, "Using embedded flashscreen image as default: %dx%d, size=%zu", 
                         flashscreen_img->header.w, flashscreen_img->header.h, flashscreen_img->data_size);
                
                // Create or update background image object
                if (s_background_img == NULL) {
                    s_background_img = lv_img_create(s_container);
                    lv_obj_set_size(s_background_img, LV_PCT(100), LV_PCT(100));
                    lv_obj_align(s_background_img, LV_ALIGN_CENTER, 0, 0);
                    lv_obj_move_background(s_background_img); // Put behind other elements
                }
                
                // Set image source
                lv_img_set_src(s_background_img, flashscreen_img);
                ESP_LOGI(TAG, "Embedded flashscreen image displayed successfully");
            } else {
                // Fallback to gradient
                ESP_LOGW(TAG, "Embedded flashscreen image not available, using gradient fallback");
                lv_obj_set_style_bg_color(s_container, lv_color_hex(0x0E1426), LV_PART_MAIN);
                lv_obj_set_style_bg_grad_color(s_container, lv_color_hex(0x000000), LV_PART_MAIN);
                lv_obj_set_style_bg_grad_dir(s_container, LV_GRAD_DIR_VER, LV_PART_MAIN);
            }
        }
    }
}

static void touch_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_event_get_indev(e);
    
    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &s_touch_start);
        s_touch_active = true;
        ESP_LOGD(TAG, "Touch pressed: (%d, %d)", s_touch_start.x, s_touch_start.y);
    } else if (code == LV_EVENT_RELEASED && s_touch_active) {
        lv_indev_get_point(indev, &s_touch_end);
        s_touch_active = false;
        
        // Check if touch is in bottom half of screen
        if (s_touch_start.y >= SWIPE_BOTTOM_AREA && s_touch_end.y >= SWIPE_BOTTOM_AREA) {
            // Calculate swipe distance
            int dx = s_touch_end.x - s_touch_start.x;
            int dy = abs(s_touch_end.y - s_touch_start.y);
            
            // Check if horizontal swipe (more horizontal than vertical)
            if (abs(dx) > SWIPE_THRESHOLD && abs(dx) > dy) {
                ESP_LOGI(TAG, "Swipe detected: dx=%d (threshold=%d)", dx, SWIPE_THRESHOLD);
                
                // Navigate to home screen on any horizontal swipe
                ui_router_navigate_to(SCREEN_ID_HOME);
            }
        }
    }
}

static void clock_timer_cb(lv_timer_t *timer) {
    (void)timer;
    update_clock();
}

static void on_create(void) {
    ESP_LOGI(TAG, "Flash screen (Screensaver) onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Set background (will be loaded from settings or use default)
    load_background_image();
    
    // Add touch event handler for swipe detection
    lv_obj_add_event_cb(container, touch_event_cb, LV_EVENT_ALL, NULL);
    
    // Clock display (large, top center) - phone-style
    s_clock_label = lv_label_create(container);
    lv_label_set_text(s_clock_label, "00:00");
    lv_obj_set_style_text_font(s_clock_label, UI_FONT_MEDIUM, 0);  // Large font
    lv_obj_set_style_text_color(s_clock_label, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(s_clock_label, LV_ALIGN_TOP_MID, 0, 60);
    
    // Date display (below clock)
    s_date_label = lv_label_create(container);
    lv_label_set_text(s_date_label, "Mon, 01/01/2024");
    lv_obj_set_style_text_font(s_date_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(s_date_label, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(s_date_label, LV_ALIGN_TOP_MID, 0, 140);
    
    // Weather widget (bottom center, phone-style)
    s_weather_widget = lv_obj_create(container);
    lv_obj_set_size(s_weather_widget, LV_PCT(90), 100);
    lv_obj_align(s_weather_widget, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_set_style_bg_color(s_weather_widget, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_weather_widget, LV_OPA_80, LV_PART_MAIN);  // Semi-transparent
    lv_obj_set_style_border_width(s_weather_widget, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_weather_widget, 15, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_weather_widget, UI_SPACE_LG, LV_PART_MAIN);
    
    // City label (top left)
    s_weather_city_label = lv_label_create(s_weather_widget);
    lv_label_set_text(s_weather_city_label, "Loading...");
    lv_obj_set_style_text_font(s_weather_city_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(s_weather_city_label, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(s_weather_city_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // Temperature label (large, center)
    s_weather_temp_label = lv_label_create(s_weather_widget);
    lv_label_set_text(s_weather_temp_label, "--°C");
    lv_obj_set_style_text_font(s_weather_temp_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(s_weather_temp_label, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(s_weather_temp_label, LV_ALIGN_CENTER, 0, -10);
    
    // Description label (bottom center)
    s_weather_desc_label = lv_label_create(s_weather_widget);
    lv_label_set_text(s_weather_desc_label, "");
    lv_obj_set_style_text_font(s_weather_desc_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(s_weather_desc_label, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(s_weather_desc_label, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    // Swipe hint (bottom, subtle)
    lv_obj_t *swipe_hint = lv_label_create(container);
    lv_label_set_text(swipe_hint, "← Swipe to unlock →");
    lv_obj_set_style_text_font(swipe_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(swipe_hint, lv_color_hex(0x666666), 0);
    lv_obj_align(swipe_hint, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    // Create clock update timer (update every second for smooth updates)
    s_clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);  // 1 second
    lv_timer_set_repeat_count(s_clock_timer, LV_ANIM_REPEAT_INFINITE);
    
    // Initial update
    update_clock();
    update_weather();
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_FLASH, "Flash (Screensaver)", container, s_clock_label);
    #endif
}

static void on_show(void) {
    ESP_LOGI(TAG, "Flash screen (Screensaver) onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_FLASH);
    #endif
    
    // Reload background image (in case settings changed)
    load_background_image();
    
    // Update clock and weather when shown
    update_clock();
    update_weather();
    
    // Restart clock timer if needed
    if (s_clock_timer != NULL) {
        if (lv_timer_get_paused(s_clock_timer)) {
            lv_timer_resume(s_clock_timer);
        }
    }
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Flash screen (Screensaver) onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_FLASH);
    #endif
    
    // Pause clock timer
    if (s_clock_timer != NULL) {
        lv_timer_pause(s_clock_timer);
    }
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Flash screen (Screensaver) onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_FLASH);
    #endif
    
    // Delete clock timer
    if (s_clock_timer != NULL) {
        lv_timer_del(s_clock_timer);
        s_clock_timer = NULL;
    }
    
    if (s_clock_label != NULL) {
        lv_obj_del(s_clock_label);
        s_clock_label = NULL;
    }
    if (s_date_label != NULL) {
        lv_obj_del(s_date_label);
        s_date_label = NULL;
    }
    if (s_weather_widget != NULL) {
        lv_obj_del(s_weather_widget);
        s_weather_widget = NULL;
        s_weather_city_label = NULL;
        s_weather_temp_label = NULL;
        s_weather_desc_label = NULL;
    }
    if (s_background_img != NULL) {
        lv_obj_del(s_background_img);
        s_background_img = NULL;
    }
}

static void on_update(const sx_state_t *state) {
    // Update weather when weather data changes
    update_weather();
}

// Register this screen
void screen_flash_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
        .on_update = on_update,
    };
    ui_screen_registry_register(SCREEN_ID_FLASH, &callbacks);
}
