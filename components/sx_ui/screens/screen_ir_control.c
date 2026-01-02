#include "screen_ir_control.h"

#include <esp_log.h>
#include <string.h>
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#include "ui_router.h"
#include "screen_common.h"
#include "sx_ui_verify.h"
#include "sx_ir_service.h"
#include "ui_theme_tokens.h"
#include "ui_list.h"

static const char *TAG = "screen_ir_control";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_device_list = NULL;
static lv_obj_t *s_control_panel = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_container = NULL;
static int s_selected_device = -1;

// IR device types
typedef enum {
    IR_DEVICE_AC = 0,  // Air Conditioner
    IR_DEVICE_TV = 1,  // TV
    IR_DEVICE_FAN = 2, // Fan
    IR_DEVICE_MAX
} ir_device_type_t;

// IR command types
typedef enum {
    IR_CMD_POWER = 0,
    IR_CMD_TEMP_UP = 1,
    IR_CMD_TEMP_DOWN = 2,
    IR_CMD_MODE = 3,
    IR_CMD_FAN = 4,
    IR_CMD_TIMER = 5,
    IR_CMD_MAX
} ir_command_type_t;

// Forward declarations
static void device_item_click_cb(lv_event_t *e);
static void control_btn_cb(lv_event_t *e);
static void send_ir_command(ir_device_type_t device, ir_command_type_t command);

static void on_create(void) {
    ESP_LOGI(TAG, "IR Control screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Set background using token
    lv_obj_set_style_bg_color(container, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "IR Control");
    
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
    
    // Status label
    s_status_label = lv_label_create(s_content);
    lv_label_set_text(s_status_label, "Select a device to control");
    lv_obj_set_style_text_font(s_status_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(s_status_label, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_width(s_status_label, LV_PCT(100));
    
    // Device list (scrollable) - using shared component
    s_device_list = ui_scrollable_list_create(s_content);
    lv_obj_set_size(s_device_list, LV_PCT(100), 150);
    
    // Device list using shared component
    const char* device_names[] = {"Air Conditioner", "TV", "Fan"};
    for (int i = 0; i < IR_DEVICE_MAX; i++) {
        ui_list_item_two_line_create(
            s_device_list,
            NULL,  // No icon
            device_names[i],
            NULL,  // No subtitle
            NULL,  // No extra text
            device_item_click_cb,
            (void*)(intptr_t)i
        );
    }
    
    // Control panel (buttons for selected device) - matching web demo
    s_control_panel = lv_obj_create(s_content);
    lv_obj_set_size(s_control_panel, LV_PCT(100), LV_PCT(100) - 160);
    lv_obj_set_style_bg_color(s_control_panel, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_control_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_control_panel, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_control_panel, UI_SPACE_LG, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_control_panel, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(s_control_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_control_panel, UI_SPACE_XL, LV_PART_MAIN);
    lv_obj_set_style_pad_column(s_control_panel, UI_SPACE_XL, LV_PART_MAIN);
    
    // Control buttons (Power, Temp+, Temp-, Mode, etc.)
    const char* control_labels[] = {"Power", "Temp+", "Temp-", "Mode", "Fan", "Timer"};
    for (int i = 0; i < IR_CMD_MAX; i++) {
        lv_obj_t *btn = lv_btn_create(s_control_panel);
        lv_obj_set_size(btn, 80, 50);
        lv_obj_set_style_bg_color(btn, UI_COLOR_BG_PRESSED, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn, UI_COLOR_PRIMARY, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn, 5, LV_PART_MAIN);
        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, control_btn_cb, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, control_labels[i]);
        lv_obj_set_style_text_font(label, UI_FONT_MEDIUM, 0);
        lv_obj_set_style_text_color(label, UI_COLOR_TEXT_PRIMARY, 0);
        lv_obj_center(label);
    }
    
    // Initially disable control panel (no device selected)
    lv_obj_add_flag(s_control_panel, LV_OBJ_FLAG_HIDDEN);
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_IR_CONTROL, "IR Control", container, s_content);
    #endif
}

static void device_item_click_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_obj_t *item = lv_event_get_target(e);
        int device_idx = (int)(intptr_t)lv_obj_get_user_data(item);
        
        if (device_idx >= 0 && device_idx < IR_DEVICE_MAX) {
            s_selected_device = device_idx;
            ESP_LOGI(TAG, "Device selected: %d", device_idx);
            
            // Show control panel
            if (s_control_panel != NULL && lvgl_port_lock(0)) {
                lv_obj_clear_flag(s_control_panel, LV_OBJ_FLAG_HIDDEN);
                lvgl_port_unlock();
            }
            
            // Update status
            const char* device_names[] = {"Air Conditioner", "TV", "Fan"};
            if (s_status_label != NULL && lvgl_port_lock(0)) {
                char status[64];
                snprintf(status, sizeof(status), "Selected: %s", device_names[device_idx]);
                lv_label_set_text(s_status_label, status);
                lvgl_port_unlock();
            }
        }
    }
}

static void control_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (s_selected_device < 0 || s_selected_device >= IR_DEVICE_MAX) {
            ESP_LOGW(TAG, "No device selected");
            return;
        }
        
        lv_obj_t *btn = lv_event_get_target(e);
        int cmd_idx = (int)(intptr_t)lv_obj_get_user_data(btn);
        
        if (cmd_idx >= 0 && cmd_idx < IR_CMD_MAX) {
            ESP_LOGI(TAG, "IR command: device=%d, command=%d", s_selected_device, cmd_idx);
            send_ir_command((ir_device_type_t)s_selected_device, (ir_command_type_t)cmd_idx);
        }
    }
}

static void send_ir_command(ir_device_type_t device, ir_command_type_t command) {
    // Note: IR service requires raw pulse patterns
    // For now, we'll create simple patterns based on device and command
    // In a real implementation, these would be learned or loaded from a database
    
    // Simple IR pattern: header + data
    // Format: [header_mark, header_space, data_mark, data_space, ...]
    // This is a placeholder - real IR patterns are device-specific
    
    // Fixed: Array size increased from 10 to 20 to accommodate full pattern
    // Pattern needs: 3 (header) + 8*2 (8 bits, each with mark+space) = 19 elements
    #define IR_PATTERN_MAX_SIZE 20
    uint16_t ir_pattern[IR_PATTERN_MAX_SIZE] = {0};
    size_t pattern_count = 0;
    
    // Generate a simple pattern based on device and command
    // Real implementation would use learned patterns or IR code database
    ir_pattern[0] = 9000;  // Header mark (9ms)
    ir_pattern[1] = 4500;  // Header space (4.5ms)
    ir_pattern[2] = 560;   // Data mark (0.56ms)
    
    // Encode device and command into pattern
    uint8_t data = (device << 4) | command;
    for (int i = 0; i < 8; i++) {
        int mark_idx = 3 + i * 2;
        int space_idx = mark_idx + 1;
        
        // Bounds check to prevent undefined behavior
        if (mark_idx >= IR_PATTERN_MAX_SIZE || space_idx >= IR_PATTERN_MAX_SIZE) {
            ESP_LOGE(TAG, "IR pattern buffer overflow at index %d", mark_idx);
            return;
        }
        
        ir_pattern[mark_idx] = 560;  // Mark
        if (data & (1 << i)) {
            ir_pattern[space_idx] = 1690;  // Space for '1' (1.69ms)
        } else {
            ir_pattern[space_idx] = 560;   // Space for '0' (0.56ms)
        }
    }
    // Fixed: Correct pattern count = 3 (header) + 8*2 (8 bits) = 19
    pattern_count = 3 + 8 * 2;
    
    // Send IR command
    esp_err_t ret = sx_ir_send_raw(ir_pattern, pattern_count);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "IR command sent: device=%d, command=%d", device, command);
        if (s_status_label != NULL && lvgl_port_lock(0)) {
            const char* cmd_names[] = {"Power", "Temp+", "Temp-", "Mode", "Fan", "Timer"};
            char status[64];
            snprintf(status, sizeof(status), "Sent: %s", cmd_names[command]);
            lv_label_set_text(s_status_label, status);
            lvgl_port_unlock();
        }
    } else {
        ESP_LOGE(TAG, "Failed to send IR command: %s", esp_err_to_name(ret));
        if (s_status_label != NULL && lvgl_port_lock(0)) {
            lv_label_set_text(s_status_label, "Failed to send IR command");
            lvgl_port_unlock();
        }
    }
}

static void on_show(void) {
    ESP_LOGI(TAG, "IR Control screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_IR_CONTROL);
    #endif
    
    // Reset selected device
    s_selected_device = -1;
    
    // Hide control panel
    if (s_control_panel != NULL && lvgl_port_lock(0)) {
        lv_obj_add_flag(s_control_panel, LV_OBJ_FLAG_HIDDEN);
        lvgl_port_unlock();
    }
    
    // Reset status
    if (s_status_label != NULL && lvgl_port_lock(0)) {
        lv_label_set_text(s_status_label, "Select a device to control");
        lvgl_port_unlock();
    }
}

static void on_hide(void) {
    ESP_LOGI(TAG, "IR Control screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_IR_CONTROL);
    #endif
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "IR Control screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_IR_CONTROL);
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
void screen_ir_control_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
    };
    ui_screen_registry_register(SCREEN_ID_IR_CONTROL, &callbacks);
}
