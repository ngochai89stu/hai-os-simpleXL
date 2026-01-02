# UI NEW SCREEN TEMPLATES

> **Mục tiêu:** Cung cấp code templates/boilerplate cho các loại screen mới trong SimpleXL OS, tuân thủ design system và dùng shared components.

---

## 📋 TỔNG QUAN

Tài liệu này cung cấp:
1. **Base Template** - Template cơ bản cho mọi screen
2. **Layout Templates** - Template cho từng loại layout (List, Grid, Fullscreen)
3. **Component Examples** - Ví dụ sử dụng shared components
4. **Complete Examples** - Screen hoàn chỉnh (Settings, Chatbot, AC Control)

---

## 🎯 BASE TEMPLATE (Template cơ bản)

### File Structure

```c
// screen_xxx.h
#pragma once

#include "screen_base.h"

// Screen registration (nếu cần)
void screen_xxx_register(void);

// ... other declarations
```

```c
// screen_xxx.c
#include "screen_xxx.h"
#include "ui_router.h"
#include "screen_common.h"
#include "ui_helpers/ui_theme_tokens.h"
#include "ui_components/ui_buttons.h"      // PR-2
#include "ui_components/ui_list.h"         // PR-4
#include "ui_components/ui_slider.h"       // PR-4
#include "ui_helpers/ui_animation_helpers.h" // PR-1

static const char *TAG = "screen_xxx";

// Screen state
static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
// ... other UI elements

// Event handlers
static void some_button_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Handle click
    }
}

// Screen lifecycle
static void on_create(void) {
    ESP_LOGI(TAG, "Screen xxx onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    // 1. Top bar
    s_top_bar = screen_common_create_top_bar_with_back(container, "Screen Title");
    
    // 2. Content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, UI_SPACE_XL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_content, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // 3. Create UI elements (see layout templates below)
    // ...
    
    // 4. Optional: Intro animation
    // lv_obj_t *elements[] = {element1, element2, ...};
    // ui_helper_fade_in_staggered(elements, sizeof(elements)/sizeof(elements[0]), 2000, 200);
}

static void on_show(void) {
    ESP_LOGI(TAG, "Screen xxx onShow");
    // Update UI state if needed
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Screen xxx onHide");
    // Cleanup if needed
}

static void on_update(const sx_state_t *state) {
    // Update UI based on state
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Screen xxx onDestroy");
    // Cleanup resources
}

// Register screen
const sx_screen_t screen_xxx = {
    .id = SX_SCREEN_XXX,
    .on_create = on_create,
    .on_show = on_show,
    .on_hide = on_hide,
    .on_update = on_update,
    .on_destroy = on_destroy,
};
```

---

## 📐 LAYOUT TEMPLATES

### Template 1: Header + Content (Simple Screen)

**Use case:** Settings, About, Diagnostics

```c
static void on_create(void) {
    // ... base template code ...
    
    // Content: Simple vertical layout
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Title
    lv_obj_t *title = lv_label_create(s_content);
    lv_label_set_text(title, "Section Title");
#if LV_FONT_MONTSERRAT_22
    lv_obj_set_style_text_font(title, UI_FONT_XLARGE, 0);
#else
    lv_obj_set_style_text_font(title, UI_FONT_MEDIUM, 0);
#endif
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_pad_bottom(title, UI_SPACE_LG, LV_PART_MAIN);
    
    // Content items
    // ...
}
```

---

### Template 2: List-based Screen

**Use case:** Settings list, Chat history, Network list

```c
#include "ui_components/ui_list.h"

static lv_obj_t *s_list = NULL;

static void list_item_cb(lv_event_t *e) {
    lv_obj_t *item = lv_event_get_target(e);
    void *user_data = lv_event_get_user_data(e);
    // Handle item click
}

static void on_create(void) {
    // ... base template code ...
    
    // Create scrollable list
    s_list = ui_scrollable_list_create(s_content);
    lv_obj_set_size(s_list, LV_PCT(100), LV_PCT(100) - 200);
    lv_obj_align(s_list, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // Add list items
    const char *items[] = {"Item 1", "Item 2", "Item 3"};
    for (size_t i = 0; i < sizeof(items)/sizeof(items[0]); i++) {
        lv_obj_t *item = ui_list_item_two_line_create(
            s_list,
            NULL,  // No icon
            items[i],
            "Subtitle",  // Optional subtitle
            NULL,  // No extra text
            list_item_cb,
            (void*)(intptr_t)i  // User data
        );
    }
}
```

---

### Template 3: Grid Layout (Dashboard-like)

**Use case:** AC Control, Audio Effects, Voice Settings

```c
static void on_create(void) {
    // ... base template code ...
    
    // Grid layout
    static const int32_t grid_cols[] = {
        LV_GRID_FR(1),
        LV_GRID_FR(1),
        LV_GRID_TEMPLATE_LAST
    };
    static const int32_t grid_rows[] = {
        LV_GRID_CONTENT,  // Row 1
        LV_GRID_CONTENT,  // Row 2
        LV_GRID_FR(1),    // Spacer
        LV_GRID_CONTENT,  // Row 3
        LV_GRID_TEMPLATE_LAST
    };
    lv_obj_set_grid_dsc_array(s_content, grid_cols, grid_rows);
    
    // Grid item 1
    lv_obj_t *item1 = lv_obj_create(s_content);
    lv_obj_set_style_bg_color(item1, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
    lv_obj_set_grid_cell(item1, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    
    // Grid item 2
    lv_obj_t *item2 = lv_obj_create(s_content);
    lv_obj_set_style_bg_color(item2, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
    lv_obj_set_grid_cell(item2, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    
    // ... more grid items
}
```

---

### Template 4: Fullscreen + Floating Controls

**Use case:** Navigation, Home, Radio

```c
static void on_create(void) {
    // ... base template code ...
    
    // Fullscreen background (optional)
    lv_obj_set_style_bg_color(s_content, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    // Or use image background
    // lv_obj_set_style_bg_img_src(s_content, &img_background, 0);
    
    // Floating controls container
    lv_obj_t *controls = lv_obj_create(s_content);
    lv_obj_set_size(controls, LV_PCT(90), 100);
    lv_obj_align(controls, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_opa(controls, LV_OPA_80, LV_PART_MAIN);  // Semi-transparent
    lv_obj_set_style_bg_color(controls, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_border_width(controls, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(controls, 16, LV_PART_MAIN);
    
    // Floating buttons
    lv_obj_t *btn1 = ui_image_button_create(controls, &img_icon1, btn1_cb, NULL);
    lv_obj_align(btn1, LV_ALIGN_LEFT_MID, 20, 0);
    
    // ... more floating controls
}
```

---

## 🎨 COMPLETE SCREEN EXAMPLES

### Example 1: Settings Screen (List-based)

```c
// screen_settings.c
#include "screen_settings.h"
#include "ui_router.h"
#include "screen_common.h"
#include "ui_helpers/ui_theme_tokens.h"
#include "ui_components/ui_list.h"
#include "ui_components/ui_slider.h"
#include "ui_components/ui_buttons.h"

static const char *TAG = "screen_settings";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_list = NULL;

static void brightness_item_cb(lv_event_t *e) {
    // Open brightness slider dialog
}

static void wifi_item_cb(lv_event_t *e) {
    // Navigate to WiFi screen
    ui_router_navigate(SX_SCREEN_WIFI_SETUP);
}

static void bluetooth_item_cb(lv_event_t *e) {
    // Navigate to Bluetooth screen
    ui_router_navigate(SX_SCREEN_BLUETOOTH_SETTING);
}

static void on_create(void) {
    ESP_LOGI(TAG, "Settings screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) return;
    
    // Top bar
    s_top_bar = screen_common_create_top_bar_with_back(container, "Settings");
    
    // Content
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, UI_SPACE_XL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_content, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // List
    s_list = ui_scrollable_list_create(s_content);
    lv_obj_set_size(s_list, LV_PCT(100), LV_PCT(100) - 200);
    
    // Settings items
    struct {
        const char *title;
        const char *subtitle;
        lv_event_cb_t cb;
    } items[] = {
        {"WiFi", "Configure WiFi network", wifi_item_cb},
        {"Bluetooth", "Configure Bluetooth", bluetooth_item_cb},
        {"Display", "Brightness and theme", brightness_item_cb},
        {"Audio", "Volume and effects", NULL},
        {"Voice", "Wake word and STT/TTS", NULL},
    };
    
    for (size_t i = 0; i < sizeof(items)/sizeof(items[0]); i++) {
        ui_list_item_two_line_create(
            s_list,
            NULL,  // No icon (or add icon later)
            items[i].title,
            items[i].subtitle,
            NULL,
            items[i].cb,
            NULL
        );
    }
}

static void on_show(void) {
    ESP_LOGI(TAG, "Settings screen onShow");
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Settings screen onHide");
}

static void on_update(const sx_state_t *state) {
    // Update if needed
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Settings screen onDestroy");
}

const sx_screen_t screen_settings = {
    .id = SX_SCREEN_SETTINGS,
    .on_create = on_create,
    .on_show = on_show,
    .on_hide = on_hide,
    .on_update = on_update,
    .on_destroy = on_destroy,
};
```

---

### Example 2: Chatbot Screen (List-based with Input)

```c
// screen_chatbot.c
#include "screen_chatbot.h"
#include "ui_router.h"
#include "screen_common.h"
#include "ui_helpers/ui_theme_tokens.h"
#include "ui_components/ui_list.h"
#include "ui_components/ui_buttons.h"

static const char *TAG = "screen_chatbot";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_chat_list = NULL;
static lv_obj_t *s_input_area = NULL;
static lv_obj_t *s_mic_btn = NULL;
static bool s_mic_enabled = false;

static void mic_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        s_mic_enabled = !s_mic_enabled;
        // Update button checked state
        if (s_mic_btn != NULL) {
            lv_obj_add_state(s_mic_btn, s_mic_enabled ? LV_STATE_CHECKED : 0);
        }
        // Toggle microphone
        // sx_chatbot_set_mic_enabled(s_mic_enabled);
    }
}

static void send_btn_cb(lv_event_t *e) {
    // Get text from input area and send
}

static void on_create(void) {
    ESP_LOGI(TAG, "Chatbot screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) return;
    
    // Top bar
    s_top_bar = screen_common_create_top_bar_with_back(container, "Chatbot");
    
    // Content
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, UI_SPACE_XL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_content, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // Chat list (scrollable)
    s_chat_list = ui_scrollable_list_create(s_content);
    lv_obj_set_size(s_chat_list, LV_PCT(100), LV_PCT(100) - 120);  // Reserve space for input
    
    // Input area (bottom)
    lv_obj_t *input_container = lv_obj_create(s_content);
    lv_obj_set_size(input_container, LV_PCT(100), 80);
    lv_obj_align(input_container, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(input_container, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_border_width(input_container, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(input_container, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(input_container, UI_SPACE_MD, LV_PART_MAIN);
    
    // Mic button (checkable)
    s_mic_btn = ui_checkable_image_button_create(input_container, &img_mic_off, &img_mic_on, mic_btn_cb, NULL);
    lv_obj_align(s_mic_btn, LV_ALIGN_LEFT_MID, 0, 0);
    
    // Send button
    lv_obj_t *send_btn = ui_image_button_create(input_container, &img_send, send_btn_cb, NULL);
    lv_obj_align(send_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    
    // Text input (optional, if needed)
    // lv_obj_t *text_area = lv_textarea_create(input_container);
    // ...
}

static void add_chat_message(const char *text, bool is_user) {
    if (s_chat_list == NULL) return;
    
    ui_list_item_two_line_create(
        s_chat_list,
        NULL,
        is_user ? "You" : "Assistant",
        text,
        NULL,
        NULL,
        NULL
    );
}

static void on_show(void) {
    ESP_LOGI(TAG, "Chatbot screen onShow");
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Chatbot screen onHide");
}

static void on_update(const sx_state_t *state) {
    // Update chat messages from state
    // if (state->chatbot_new_message) {
    //     add_chat_message(state->chatbot_message_text, state->chatbot_is_user);
    // }
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Chatbot screen onDestroy");
}

const sx_screen_t screen_chatbot = {
    .id = SX_SCREEN_CHATBOT,
    .on_create = on_create,
    .on_show = on_show,
    .on_hide = on_hide,
    .on_update = on_update,
    .on_destroy = on_destroy,
};
```

---

### Example 3: AC Control Screen (Grid Layout)

```c
// screen_ac_control.c
#include "screen_ac_control.h"
#include "ui_router.h"
#include "screen_common.h"
#include "ui_helpers/ui_theme_tokens.h"
#include "ui_components/ui_slider.h"
#include "ui_components/ui_buttons.h"

static const char *TAG = "screen_ac_control";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_temp_slider = NULL;
static lv_obj_t *s_power_btn = NULL;
static lv_obj_t *s_mode_btns[3] = {NULL};

static void temp_slider_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        int32_t value = lv_slider_get_value(s_temp_slider);
        // Set AC temperature: value + 16 (16-30°C)
        // sx_ac_set_temperature(value + 16);
    }
}

static void power_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        bool checked = lv_obj_has_state(s_power_btn, LV_STATE_CHECKED);
        // sx_ac_set_power(checked);
    }
}

static void mode_btn_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    int mode = (int)(intptr_t)lv_event_get_user_data(e);
    // sx_ac_set_mode(mode);
    
    // Update button states
    for (int i = 0; i < 3; i++) {
        if (s_mode_btns[i] != NULL) {
            lv_obj_clear_state(s_mode_btns[i], LV_STATE_CHECKED);
        }
    }
    lv_obj_add_state(btn, LV_STATE_CHECKED);
}

static void on_create(void) {
    ESP_LOGI(TAG, "AC Control screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) return;
    
    // Top bar
    s_top_bar = screen_common_create_top_bar_with_back(container, "AC Control");
    
    // Content (Grid layout)
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, UI_SPACE_XL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_content, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // Grid layout
    static const int32_t grid_cols[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t grid_rows[] = {
        LV_GRID_CONTENT,  // Power button
        LV_GRID_FR(1),    // Spacer
        LV_GRID_CONTENT,  // Temperature slider
        LV_GRID_CONTENT,  // Temperature label
        LV_GRID_FR(1),    // Spacer
        LV_GRID_CONTENT,  // Mode buttons
        LV_GRID_TEMPLATE_LAST
    };
    lv_obj_set_grid_dsc_array(s_content, grid_cols, grid_rows);
    
    // Power button
    s_power_btn = ui_checkable_image_button_create(s_content, &img_ac_off, &img_ac_on, power_btn_cb, NULL);
    lv_obj_set_size(s_power_btn, 80, 80);
    lv_obj_set_grid_cell(s_power_btn, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    
    // Temperature slider
    s_temp_slider = ui_gradient_slider_create(s_content, temp_slider_cb, NULL);
    lv_obj_set_size(s_temp_slider, LV_PCT(90), 24);
    lv_slider_set_range(s_temp_slider, 16, 30);
    lv_slider_set_value(s_temp_slider, 24, LV_ANIM_OFF);
    lv_obj_set_grid_cell(s_temp_slider, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    
    // Temperature label
    lv_obj_t *temp_label = lv_label_create(s_content);
    lv_label_set_text_fmt(temp_label, "%d°C", 24);
    lv_obj_set_style_text_font(temp_label, UI_FONT_LARGE, 0);
    lv_obj_set_style_text_color(temp_label, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_grid_cell(temp_label, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 3, 1);
    
    // Mode buttons container
    lv_obj_t *mode_container = lv_obj_create(s_content);
    lv_obj_set_size(mode_container, LV_PCT(90), 60);
    lv_obj_set_style_bg_opa(mode_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(mode_container, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(mode_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mode_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_grid_cell(mode_container, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 5, 1);
    
    // Mode buttons
    const char *mode_icons[] = {&img_cool, &img_fan, &img_heat};
    for (int i = 0; i < 3; i++) {
        s_mode_btns[i] = ui_image_button_create(mode_container, mode_icons[i], mode_btn_cb, (void*)(intptr_t)i);
        lv_obj_set_size(s_mode_btns[i], 60, 60);
    }
}

static void on_show(void) {
    ESP_LOGI(TAG, "AC Control screen onShow");
}

static void on_hide(void) {
    ESP_LOGI(TAG, "AC Control screen onHide");
}

static void on_update(const sx_state_t *state) {
    // Update UI from AC state
    // if (s_temp_slider != NULL) {
    //     lv_slider_set_value(s_temp_slider, state->ac_temperature - 16, LV_ANIM_OFF);
    // }
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "AC Control screen onDestroy");
}

const sx_screen_t screen_ac_control = {
    .id = SX_SCREEN_AC_CONTROL,
    .on_create = on_create,
    .on_show = on_show,
    .on_hide = on_hide,
    .on_update = on_update,
    .on_destroy = on_destroy,
};
```

---

## ✅ CHECKLIST KHI TẠO SCREEN MỚI

- [ ] Đã include tokens header (`ui_theme_tokens.h`)
- [ ] Đã dùng `UI_COLOR_*` tokens thay vì hardcode
- [ ] Đã dùng `UI_FONT_*` tokens với fallback
- [ ] Đã dùng `UI_SPACE_*` tokens
- [ ] Đã dùng shared components (PR-2, PR-4) nếu có thể
- [ ] Đã tuân thủ layout template phù hợp
- [ ] Đã test visual (màu, font, spacing)
- [ ] Đã test functional (events, navigation)
- [ ] Đã test performance (timer rate, memory)

---

## 🎓 TÀI LIỆU THAM KHẢO

- **Design System:** `ui_design_system_consistency_rules.md`
- **Layout Templates:** `ui_layout_templates.md`
- **Shared Components:** `ui_components_to_extract.md`
- **Migration Guide:** `ui_migration_guide_detailed.md`

---

**Tài liệu này sẽ được cập nhật khi có thêm screen templates hoặc thay đổi design system.**






