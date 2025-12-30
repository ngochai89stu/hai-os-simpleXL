#include "screen_wifi_setup.h"

#include <esp_log.h>
#include <string.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "sx_state.h"
#include "sx_wifi_service.h"
#include "sx_settings_service.h"
#include "sx_qr_code_service.h"

static const char *TAG = "screen_wifi_setup";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_network_list = NULL;
static lv_obj_t *s_scan_btn = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_container = NULL;
static sx_wifi_network_info_t s_networks[20];
static uint8_t s_network_count = 0;
static lv_obj_t *s_password_dialog = NULL;
static lv_obj_t *s_password_ta = NULL;
static char s_pending_ssid[64] = {0};
static lv_obj_t *s_qr_code_widget = NULL;  // QR code widget display
static lv_obj_t *s_ip_label = NULL;  // IP address label
static lv_obj_t *s_qr_container = NULL;  // Container for QR code and IP label

// Forward declarations
static void update_ip_qr_code(void);
static void scan_btn_cb(lv_event_t *e);
static void network_item_click_cb(lv_event_t *e);
static void show_password_dialog(const char *ssid);

static void on_create(void) {
    ESP_LOGI(TAG, "Wi-Fi Setup screen onCreate");
    
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
    
    // Set background
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "Wi-Fi Setup");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Status label
    s_status_label = lv_label_create(s_content);
    lv_label_set_text(s_status_label, "");
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x888888), 0);
    lv_obj_set_width(s_status_label, LV_PCT(100));
    
    // Scan button (matching web demo)
    s_scan_btn = lv_btn_create(s_content);
    lv_obj_set_size(s_scan_btn, LV_PCT(100), 50);
    lv_obj_set_style_bg_color(s_scan_btn, lv_color_hex(0x5b7fff), LV_PART_MAIN);
    lv_obj_set_style_radius(s_scan_btn, 5, LV_PART_MAIN);
    lv_obj_t *scan_label = lv_label_create(s_scan_btn);
    lv_label_set_text(scan_label, "Scan Networks");
    lv_obj_set_style_text_font(scan_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(scan_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(scan_label);
    
    // QR code container (initially hidden, shown when connected)
    s_qr_container = lv_obj_create(s_content);
    lv_obj_set_size(s_qr_container, LV_PCT(100), 200);
    lv_obj_set_style_bg_opa(s_qr_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_qr_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_qr_container, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_qr_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_qr_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(s_qr_container, LV_OBJ_FLAG_HIDDEN);  // Initially hidden
    
    // IP address label
    s_ip_label = lv_label_create(s_qr_container);
    lv_label_set_text(s_ip_label, "");
    lv_obj_set_style_text_font(s_ip_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_ip_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(s_ip_label, LV_PCT(100));
    lv_obj_set_style_text_align(s_ip_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // QR code widget (will be created when WiFi connects)
    s_qr_code_widget = NULL;
    
    // Network list (scrollable) - matching web demo
    s_network_list = lv_obj_create(s_content);
    lv_obj_set_size(s_network_list, LV_PCT(100), LV_PCT(100) - 120);
    lv_obj_set_style_bg_opa(s_network_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_network_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_network_list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_network_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_network_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Add event handler for scan button
    lv_obj_add_event_cb(s_scan_btn, scan_btn_cb, LV_EVENT_CLICKED, NULL);
    
    // Initialize QR code service
    sx_qr_code_service_init();
    
    lvgl_port_unlock();
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_WIFI_SETUP, "WiFi Setup", container, s_network_list);
    #endif
}

static void refresh_network_list(void) {
    if (s_network_list == NULL) {
        return;
    }
    
    // Clear existing items
    lv_obj_clean(s_network_list);
    
    // Add networks from scan results
    for (int i = 0; i < s_network_count; i++) {
        lv_obj_t *network_item = lv_obj_create(s_network_list);
        lv_obj_set_size(network_item, LV_PCT(100), 60);
        lv_obj_set_style_bg_color(network_item, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        lv_obj_set_style_bg_color(network_item, lv_color_hex(0x3a3a3a), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_border_width(network_item, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(network_item, 10, LV_PART_MAIN);
        lv_obj_set_style_radius(network_item, 5, LV_PART_MAIN);
        
        // Store network index in user data
        lv_obj_set_user_data(network_item, (void *)(intptr_t)i);
        
        // SSID label
        lv_obj_t *label = lv_label_create(network_item);
        lv_label_set_text(label, s_networks[i].ssid);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
        
        // Signal strength indicator
        char signal_str[16];
        if (s_networks[i].rssi > -50) {
            snprintf(signal_str, sizeof(signal_str), "📶 %d dBm", s_networks[i].rssi);
        } else if (s_networks[i].rssi > -70) {
            snprintf(signal_str, sizeof(signal_str), "📶 %d dBm", s_networks[i].rssi);
        } else {
            snprintf(signal_str, sizeof(signal_str), "📶 %d dBm", s_networks[i].rssi);
        }
        lv_obj_t *signal = lv_label_create(network_item);
        lv_label_set_text(signal, signal_str);
        lv_obj_set_style_text_font(signal, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(signal, lv_color_hex(0x888888), 0);
        lv_obj_align(signal, LV_ALIGN_RIGHT_MID, -10, 0);
        
        // Lock icon if encrypted
        if (s_networks[i].is_encrypted) {
            lv_obj_t *lock = lv_label_create(network_item);
            lv_label_set_text(lock, "🔒");
            lv_obj_align(lock, LV_ALIGN_RIGHT_MID, -80, 0);
        }
        
        // Add click handler
        lv_obj_add_event_cb(network_item, network_item_click_cb, LV_EVENT_CLICKED, NULL);
    }
}

static void network_item_click_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_obj_t *item = lv_event_get_target(e);
        int network_idx = (int)(intptr_t)lv_obj_get_user_data(item);
        
        if (network_idx >= 0 && network_idx < s_network_count) {
            const sx_wifi_network_info_t *network = &s_networks[network_idx];
            ESP_LOGI(TAG, "Network clicked: %s (encrypted: %d)", network->ssid, network->is_encrypted);
            
            if (network->is_encrypted) {
                // Show password input dialog
                strncpy(s_pending_ssid, network->ssid, sizeof(s_pending_ssid) - 1);
                s_pending_ssid[sizeof(s_pending_ssid) - 1] = '\0';
                show_password_dialog(network->ssid);
            } else {
                // Connect without password
                esp_err_t ret = sx_wifi_connect(network->ssid, NULL);
                if (ret == ESP_OK) {
                    // Save WiFi SSID to settings
                    sx_settings_set_string("wifi_ssid", network->ssid);
                    sx_settings_commit();
                    
                    if (s_status_label != NULL && lvgl_port_lock(0)) {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "Connecting to: %s", network->ssid);
                        lv_label_set_text(s_status_label, msg);
                        lvgl_port_unlock();
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to connect: %s", esp_err_to_name(ret));
                    if (s_status_label != NULL && lvgl_port_lock(0)) {
                        lv_label_set_text(s_status_label, "Connection failed");
                        lvgl_port_unlock();
                    }
                }
            }
        }
    }
}

static void scan_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Scan button clicked");
        
        if (s_status_label != NULL) {
            lv_label_set_text(s_status_label, "Scanning...");
        }
        
        // Perform WiFi scan
        s_network_count = sx_wifi_scan(s_networks, 20);
        
        if (s_network_count == 0) {
            ESP_LOGE(TAG, "WiFi scan failed");
            if (s_status_label != NULL) {
                lv_label_set_text(s_status_label, "Scan failed");
            }
        } else {
            ESP_LOGI(TAG, "Found %d networks", s_network_count);
            if (s_status_label != NULL) {
                char status[64];
                snprintf(status, sizeof(status), "Found %d networks", s_network_count);
                lv_label_set_text(s_status_label, status);
            }
            
            // Refresh network list
            if (lvgl_port_lock(0)) {
                refresh_network_list();
                lvgl_port_unlock();
            }
        }
    }
}

static void on_show(void) {
    ESP_LOGI(TAG, "Wi-Fi Setup screen onShow");
    
    // Auto-scan on show
    if (s_scan_btn != NULL) {
        // Trigger scan
        if (s_status_label != NULL) {
            lv_label_set_text(s_status_label, "Scanning...");
        }
        
        s_network_count = sx_wifi_scan(s_networks, 20);
        if (s_network_count > 0) {
            if (s_status_label != NULL) {
                char status[64];
                snprintf(status, sizeof(status), "Found %d networks", s_network_count);
                lv_label_set_text(s_status_label, status);
            }
            
            if (lvgl_port_lock(0)) {
                refresh_network_list();
                lvgl_port_unlock();
            }
        }
    }
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Wi-Fi Setup screen onHide");
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Wi-Fi Setup screen onDestroy");
    
    if (lvgl_port_lock(0)) {
        // Close password dialog if open
        if (s_password_dialog != NULL) {
            lv_obj_del(s_password_dialog);
            s_password_dialog = NULL;
            s_password_ta = NULL;
        }
        
        // Clean up QR code widget
        if (s_qr_code_widget != NULL) {
            lv_obj_del(s_qr_code_widget);
            s_qr_code_widget = NULL;
        }
        
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
    
    s_pending_ssid[0] = '\0';
}

static void update_ip_qr_code(void) {
    if (!lvgl_port_lock(0)) {
        return;
    }
    
    // Check if WiFi is connected
    if (!sx_wifi_is_connected()) {
        // Hide QR code container if not connected
        if (s_qr_container != NULL) {
            lv_obj_add_flag(s_qr_container, LV_OBJ_FLAG_HIDDEN);
        }
        lvgl_port_unlock();
        return;
    }
    
    // Get IP address
    const char *ip_address = sx_wifi_get_ip_address();
    if (ip_address == NULL || strlen(ip_address) == 0) {
        lvgl_port_unlock();
        return;
    }
    
    // Show QR code container
    if (s_qr_container != NULL) {
        lv_obj_clear_flag(s_qr_container, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Update IP label
    if (s_ip_label != NULL) {
        char ip_text[64];
        snprintf(ip_text, sizeof(ip_text), "IP: %s", ip_address);
        lv_label_set_text(s_ip_label, ip_text);
    }
    
    // Create or update QR code widget
    if (s_qr_container != NULL) {
        // Remove old QR code widget if exists
        if (s_qr_code_widget != NULL) {
            lv_obj_del(s_qr_code_widget);
            s_qr_code_widget = NULL;
        }
        
        // Create QR code text (URL with IP)
        char qr_text[128];
        snprintf(qr_text, sizeof(qr_text), "http://%s", ip_address);
        
        // Create LVGL QR code widget
        s_qr_code_widget = sx_qr_code_create_lvgl_widget(s_qr_container, qr_text, 150);
        if (s_qr_code_widget != NULL) {
            // Center the QR code
            lv_obj_align(s_qr_code_widget, LV_ALIGN_CENTER, 0, 20);
            ESP_LOGI(TAG, "QR code created for IP: %s", ip_address);
        } else {
            ESP_LOGW(TAG, "Failed to create QR code widget (LV_USE_QRCODE may not be enabled)");
        }
    }
    
    lvgl_port_unlock();
}

// Password dialog callbacks
static void password_dialog_connect_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (s_password_ta != NULL && strlen(s_pending_ssid) > 0) {
            const char *password = lv_textarea_get_text(s_password_ta);
            ESP_LOGI(TAG, "Connecting to %s with password", s_pending_ssid);
            
            esp_err_t ret = sx_wifi_connect(s_pending_ssid, password);
            if (ret == ESP_OK) {
                // Save WiFi credentials to settings
                sx_settings_set_string("wifi_ssid", s_pending_ssid);
                if (password != NULL && strlen(password) > 0) {
                    sx_settings_set_string("wifi_password", password);
                }
                sx_settings_commit();
                
                if (s_status_label != NULL && lvgl_port_lock(0)) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Connecting to: %s", s_pending_ssid);
                    lv_label_set_text(s_status_label, msg);
                    lvgl_port_unlock();
                }
            } else {
                ESP_LOGE(TAG, "Failed to connect: %s", esp_err_to_name(ret));
                if (s_status_label != NULL && lvgl_port_lock(0)) {
                    lv_label_set_text(s_status_label, "Connection failed");
                    lvgl_port_unlock();
                }
            }
        }
        
        // Close dialog
        if (s_password_dialog != NULL && lvgl_port_lock(0)) {
            lv_obj_del(s_password_dialog);
            s_password_dialog = NULL;
            s_password_ta = NULL;
            lvgl_port_unlock();
        }
    }
}

static void password_dialog_cancel_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Close dialog
        if (s_password_dialog != NULL && lvgl_port_lock(0)) {
            lv_obj_del(s_password_dialog);
            s_password_dialog = NULL;
            s_password_ta = NULL;
            lvgl_port_unlock();
        }
        s_pending_ssid[0] = '\0';
    }
}

static void show_password_dialog(const char *ssid) {
    if (!lvgl_port_lock(0)) {
        return;
    }
    
    // Create modal dialog
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        lvgl_port_unlock();
        return;
    }
    
    s_password_dialog = lv_obj_create(container);
    lv_obj_set_size(s_password_dialog, LV_PCT(90), 300);
    lv_obj_align(s_password_dialog, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_password_dialog, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_password_dialog, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_password_dialog, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_password_dialog, 20, LV_PART_MAIN);
    
    // Title
    lv_obj_t *title = lv_label_create(s_password_dialog);
    char title_text[128];
    snprintf(title_text, sizeof(title_text), "Enter password for:\n%s", ssid);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // Password text area
    s_password_ta = lv_textarea_create(s_password_dialog);
    lv_obj_set_size(s_password_ta, LV_PCT(100), 50);
    lv_obj_align(s_password_ta, LV_ALIGN_TOP_LEFT, 0, 60);
    lv_textarea_set_placeholder_text(s_password_ta, "Password");
    lv_textarea_set_password_mode(s_password_ta, true);
    lv_textarea_set_one_line(s_password_ta, true);
    lv_obj_set_style_bg_color(s_password_ta, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_password_ta, &lv_font_montserrat_14, 0);
    
    // Create keyboard
    lv_obj_t *kb = lv_keyboard_create(s_password_dialog);
    lv_keyboard_set_textarea(kb, s_password_ta);
    lv_obj_set_size(kb, LV_PCT(100), LV_PCT(50));
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, -80);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    
    // Buttons container
    lv_obj_t *btn_container = lv_obj_create(s_password_dialog);
    lv_obj_set_size(btn_container, LV_PCT(100), 50);
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_container, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Cancel button
    lv_obj_t *cancel_btn = lv_btn_create(btn_container);
    lv_obj_set_size(cancel_btn, LV_PCT(45), 40);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_radius(cancel_btn, 5, LV_PART_MAIN);
    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(cancel_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(cancel_label);
    lv_obj_add_event_cb(cancel_btn, password_dialog_cancel_cb, LV_EVENT_CLICKED, NULL);
    
    // Connect button
    lv_obj_t *connect_btn = lv_btn_create(btn_container);
    lv_obj_set_size(connect_btn, LV_PCT(45), 40);
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(0x5b7fff), LV_PART_MAIN);
    lv_obj_set_style_radius(connect_btn, 5, LV_PART_MAIN);
    lv_obj_t *connect_label = lv_label_create(connect_btn);
    lv_label_set_text(connect_label, "Connect");
    lv_obj_set_style_text_font(connect_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(connect_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(connect_label);
    lv_obj_add_event_cb(connect_btn, password_dialog_connect_cb, LV_EVENT_CLICKED, NULL);
    
    lvgl_port_unlock();
}

static void on_update(const sx_state_t *state) {
    if (!lvgl_port_lock(0)) {
        return;
    }
    
    // Update connection status if available
    if (state != NULL && s_status_label != NULL) {
        // Check WiFi state from state structure
        // For now, just update based on WiFi service
        if (sx_wifi_is_connected()) {
            const char *ssid = sx_wifi_get_ssid();
            if (ssid != NULL) {
                char status[64];
                snprintf(status, sizeof(status), "Connected: %s", ssid);
                lv_label_set_text(s_status_label, status);
            }
            // Update IP and QR code when WiFi connects
            update_ip_qr_code();
        }
    }
    
    lvgl_port_unlock();
}

// Register this screen
void screen_wifi_setup_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
        .on_update = on_update,
    };
    ui_screen_registry_register(SCREEN_ID_WIFI_SETUP, &callbacks);
}
