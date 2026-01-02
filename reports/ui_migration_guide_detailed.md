# UI MIGRATION GUIDE CHI TIẾT

> **Mục tiêu:** Hướng dẫn step-by-step để migrate các screen cũ sang dùng design tokens và shared components, với ví dụ cụ thể từ code hiện tại.

---

## 📋 TỔNG QUAN

Migration guide này giúp bạn:
1. **Identify** hardcoded values trong screen cũ
2. **Replace** bằng design tokens
3. **Refactor** sang dùng shared components (optional)
4. **Test** đảm bảo không thay đổi behavior/visual

---

## 🎯 SCREEN ƯU TIÊN MIGRATE

### Priority 1: Screen đơn giản (dễ migrate)

1. **screen_radio.c** - Có nhiều hardcode colors, fonts
2. **screen_ota.c** - Có progress bar, buttons
3. **screen_wifi_setup.c** - Có list, buttons
4. **screen_about.c** - Screen đơn giản, ít logic

### Priority 2: Screen phức tạp (migrate sau)

1. **screen_music_player.c** - Đã có nhiều features, migrate cẩn thận
2. **screen_chat.c** - Có nhiều state management
3. **screen_google_navigation.c** - Có nhiều custom logic

---

## 📝 STEP-BY-STEP MIGRATION PROCESS

### Step 1: Audit Screen (Kiểm kê)

**Mục tiêu:** Liệt kê tất cả hardcoded values cần thay thế.

#### 1.1 Tìm hardcoded colors

```bash
# Tìm tất cả lv_color_hex trong file
grep -n "lv_color_hex" components/sx_ui/screens/screen_xxx.c
```

**Ví dụ output:**
```
50:    lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
62:    lv_obj_set_style_bg_color(current_station, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
88:    lv_obj_set_style_text_color(s_station_title, lv_color_hex(0xFFFFFF), 0);
94:    lv_obj_set_style_text_color(s_station_info, lv_color_hex(0x888888), 0);
110:    lv_obj_set_style_bg_color(s_retry_btn, lv_color_hex(0xFF4444), LV_PART_MAIN);
123:    lv_obj_set_style_bg_color(s_play_btn, lv_color_hex(0x5b7fff), LV_PART_MAIN);
```

**Tạo bảng mapping:**
| Hardcoded Value | Token | Usage |
|-----------------|-------|-------|
| `0x1a1a1a` | `UI_COLOR_BG_PRIMARY` | Container background |
| `0x2a2a2a` | `UI_COLOR_BG_SECONDARY` | Card background |
| `0xFFFFFF` | `UI_COLOR_TEXT_PRIMARY` | Title text |
| `0x888888` | `UI_COLOR_TEXT_SECONDARY` | Subtitle text |
| `0x5b7fff` | `UI_COLOR_PRIMARY` | Primary button |
| `0xFF4444` | `UI_COLOR_ERROR` (cần thêm) | Error button |

#### 1.2 Tìm hardcoded fonts

```bash
grep -n "lv_font_montserrat" components/sx_ui/screens/screen_xxx.c
```

**Ví dụ output:**
```
87:    lv_obj_set_style_text_font(s_station_title, &lv_font_montserrat_14, 0);
93:    lv_obj_set_style_text_font(s_station_info, &lv_font_montserrat_14, 0);
```

**Mapping:**
| Hardcoded Font | Token | Usage |
|----------------|-------|-------|
| `&lv_font_montserrat_14` | `UI_FONT_MEDIUM` | Body text (default) |
| `&lv_font_montserrat_16` | `UI_FONT_LARGE` | Subtitle |
| `&lv_font_montserrat_22` | `UI_FONT_XLARGE` | Title |

#### 1.3 Tìm hardcoded spacing

```bash
grep -n "pad_all\|pad_left\|pad_right\|pad_top\|pad_bottom" components/sx_ui/screens/screen_xxx.c
```

**Ví dụ output:**
```
50:    lv_obj_set_style_pad_all(container, 20, LV_PART_MAIN);
```

**Mapping:**
| Hardcoded Value | Token | Usage |
|----------------|-------|-------|
| `20` | `UI_SPACE_XL` | Container padding |
| `12` | `UI_SPACE_MD` | List item padding |
| `16` | `UI_SPACE_LG` | Section spacing |

#### 1.4 Tìm components có thể dùng shared

**Buttons:**
- `lv_img_create()` + `LV_OBJ_FLAG_CLICKABLE` → `ui_image_button_create()`
- `lv_imagebutton_create()` → `ui_checkable_image_button_create()`

**Sliders:**
- `lv_slider_create()` với gradient style → `ui_gradient_slider_create()`

**Lists:**
- `lv_obj_create()` + `LV_FLEX_FLOW_COLUMN` → `ui_scrollable_list_create()`
- List items với icon + text → `ui_list_item_two_line_create()`

---

### Step 2: Include Tokens Header

**File:** `components/sx_ui/screens/screen_xxx.c`

**Thêm vào đầu file (sau các include khác):**

```c
#include "screen_xxx.h"

// ... existing includes ...

// Add this line
#include "ui_helpers/ui_theme_tokens.h"

// If using shared components (optional):
// #include "ui_components/ui_buttons.h"
// #include "ui_components/ui_slider.h"
// #include "ui_components/ui_list.h"
```

---

### Step 3: Replace Colors

#### 3.1 Background Colors

**Before:**
```c
lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
lv_obj_set_style_bg_color(card, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
lv_obj_set_style_bg_color(btn, lv_color_hex(0x3a3a3a), LV_PART_MAIN | LV_STATE_PRESSED);
```

**After:**
```c
lv_obj_set_style_bg_color(container, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
lv_obj_set_style_bg_color(card, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
lv_obj_set_style_bg_color(btn, UI_COLOR_BG_PRESSED, LV_PART_MAIN | LV_STATE_PRESSED);
```

#### 3.2 Text Colors

**Before:**
```c
lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
lv_obj_set_style_text_color(subtitle, lv_color_hex(0x888888), 0);
```

**After:**
```c
lv_obj_set_style_text_color(title, UI_COLOR_TEXT_PRIMARY, 0);
lv_obj_set_style_text_color(subtitle, UI_COLOR_TEXT_SECONDARY, 0);
```

#### 3.3 Primary/Accent Colors

**Before:**
```c
lv_obj_set_style_bg_color(primary_btn, lv_color_hex(0x5b7fff), LV_PART_MAIN);
lv_obj_set_style_bg_color(primary_btn, lv_color_hex(0x4a6fff), LV_PART_MAIN | LV_STATE_PRESSED);
```

**After:**
```c
lv_obj_set_style_bg_color(primary_btn, UI_COLOR_PRIMARY, LV_PART_MAIN);
lv_obj_set_style_bg_color(primary_btn, UI_COLOR_PRIMARY_DARK, LV_PART_MAIN | LV_STATE_PRESSED);
```

---

### Step 4: Replace Fonts

#### 4.1 Basic Replacement

**Before:**
```c
lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
```

**After:**
```c
lv_obj_set_style_text_font(label, UI_FONT_MEDIUM, 0);
```

#### 4.2 With Fallback (for large fonts)

**Before:**
```c
#if LV_FONT_MONTSERRAT_22
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
#else
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
#endif
```

**After:**
```c
#if LV_FONT_MONTSERRAT_22
    lv_obj_set_style_text_font(title, UI_FONT_XLARGE, 0);
#else
    lv_obj_set_style_text_font(title, UI_FONT_MEDIUM, 0);
#endif
```

**Note:** Tokens đã có fallback logic, nhưng vẫn cần `#if` nếu muốn dùng font lớn hơn.

---

### Step 5: Replace Spacing

**Before:**
```c
lv_obj_set_style_pad_all(container, 20, LV_PART_MAIN);
lv_obj_set_style_pad_ver(item, 12, LV_PART_MAIN);
lv_obj_set_style_pad_hor(item, 16, LV_PART_MAIN);
```

**After:**
```c
lv_obj_set_style_pad_all(container, UI_SPACE_XL, LV_PART_MAIN);
lv_obj_set_style_pad_ver(item, UI_SPACE_MD, LV_PART_MAIN);
lv_obj_set_style_pad_hor(item, UI_SPACE_LG, LV_PART_MAIN);
```

---

### Step 6: Optional - Use Shared Components

#### 6.1 Migrate Image Button

**Before:**
```c
lv_obj_t *prev_btn = lv_img_create(parent);
lv_img_set_src(prev_btn, &img_prev);
lv_obj_add_flag(prev_btn, LV_OBJ_FLAG_CLICKABLE);
lv_obj_add_event_cb(prev_btn, prev_btn_cb, LV_EVENT_CLICKED, NULL);
lv_obj_set_size(prev_btn, 40, 40);
lv_obj_set_style_bg_color(prev_btn, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
```

**After:**
```c
lv_obj_t *prev_btn = ui_image_button_create(parent, &img_prev, prev_btn_cb, NULL);
lv_obj_set_size(prev_btn, 40, 40);
lv_obj_set_style_bg_color(prev_btn, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
// Style vẫn có thể set sau khi tạo, nhưng logic tạo button đã được đóng gói
```

#### 6.2 Migrate Checkable Image Button

**Before:**
```c
lv_obj_t *play_btn = lv_imagebutton_create(parent);
lv_imagebutton_set_src(play_btn, LV_IMGBTN_STATE_RELEASED, &img_play);
lv_imagebutton_set_src(play_btn, LV_IMGBTN_STATE_CHECKED_RELEASED, &img_pause);
lv_obj_add_event_cb(play_btn, play_btn_cb, LV_EVENT_CLICKED, NULL);
```

**After:**
```c
lv_obj_t *play_btn = ui_checkable_image_button_create(parent, &img_play, &img_pause, play_btn_cb, NULL);
// Component đã xử lý checked state
```

#### 6.3 Migrate Gradient Slider

**Before:**
```c
lv_obj_t *slider = lv_slider_create(parent);
lv_obj_set_size(slider, LV_PCT(90), 6);
lv_obj_set_style_bg_color(slider, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
lv_obj_set_style_bg_color(slider, lv_color_hex(0x5b7fff), LV_PART_INDICATOR);
lv_obj_set_style_bg_grad_color(slider, lv_color_hex(0xa666f1), LV_PART_INDICATOR);
lv_obj_set_style_bg_color(slider, lv_color_hex(0x5b7fff), LV_PART_KNOB);
```

**After:**
```c
lv_obj_t *slider = ui_gradient_slider_create(parent, slider_cb, NULL);
lv_obj_set_size(slider, LV_PCT(90), 6);
// Gradient colors đã được set tự động
```

#### 6.4 Migrate Scrollable List

**Before:**
```c
lv_obj_t *list = lv_obj_create(parent);
lv_obj_set_size(list, LV_PCT(100), LV_PCT(100) - 200);
lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, LV_PART_MAIN);
lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
// ... custom scrollbar style ...
```

**After:**
```c
lv_obj_t *list = ui_scrollable_list_create(parent);
lv_obj_set_size(list, LV_PCT(100), LV_PCT(100) - 200);
// Scrollbar style đã được set tự động
```

---

### Step 7: Test Migration

#### 7.1 Visual Test

- [ ] Màu sắc giống hệt trước khi migrate
- [ ] Font size giống hệt
- [ ] Spacing/padding giống hệt
- [ ] Button states (pressed, disabled) hoạt động đúng

#### 7.2 Functional Test

- [ ] Button click events hoạt động
- [ ] Slider value change events hoạt động
- [ ] List scroll hoạt động
- [ ] Screen lifecycle (on_create, on_show, on_hide) hoạt động

#### 7.3 Performance Test

- [ ] Không có memory leak
- [ ] Timer rate không thay đổi
- [ ] Animation smooth như cũ

---

## 📚 VÍ DỤ MIGRATION CỤ THỂ

### Example 1: screen_radio.c (Simple Migration)

#### Before Migration

```c
// screen_radio.c
static void on_create(void) {
    lv_obj_t *container = ui_router_get_container();
    
    // Hardcoded background
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    
    // Hardcoded card background
    lv_obj_t *current_station = lv_obj_create(container);
    lv_obj_set_style_bg_color(current_station, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    
    // Hardcoded text colors
    lv_obj_t *title = lv_label_create(container);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    
    // Hardcoded button
    lv_obj_t *play_btn = lv_btn_create(container);
    lv_obj_set_style_bg_color(play_btn, lv_color_hex(0x5b7fff), LV_PART_MAIN);
    lv_obj_set_style_bg_color(play_btn, lv_color_hex(0x4a6fff), LV_PART_MAIN | LV_STATE_PRESSED);
}
```

#### After Migration

```c
// screen_radio.c
#include "ui_helpers/ui_theme_tokens.h"
#include "ui_components/ui_buttons.h"

static void on_create(void) {
    lv_obj_t *container = ui_router_get_container();
    
    // Use token
    lv_obj_set_style_bg_color(container, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // Use token
    lv_obj_t *current_station = lv_obj_create(container);
    lv_obj_set_style_bg_color(current_station, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
    
    // Use tokens
    lv_obj_t *title = lv_label_create(container);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(title, UI_FONT_MEDIUM, 0);
    
    // Use shared component (optional)
    lv_obj_t *play_btn = ui_checkable_image_button_create(container, &img_play, &img_pause, play_btn_cb, NULL);
    lv_obj_set_style_bg_color(play_btn, UI_COLOR_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(play_btn, UI_COLOR_PRIMARY_DARK, LV_PART_MAIN | LV_STATE_PRESSED);
}
```

---

### Example 2: screen_wifi_setup.c (List Migration)

#### Before Migration

```c
// screen_wifi_setup.c
static void create_network_list(void) {
    lv_obj_t *list = lv_obj_create(s_content);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100) - 200);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
    
    // Custom scrollbar
    lv_obj_set_style_bg_color(list, lv_color_hex(0xeee), LV_PART_SCROLLBAR);
    
    // Create list items manually
    for (int i = 0; i < network_count; i++) {
        lv_obj_t *item = lv_obj_create(list);
        lv_obj_set_style_bg_color(item, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        lv_obj_set_style_bg_color(item, lv_color_hex(0x3a3a3a), LV_PART_MAIN | LV_STATE_PRESSED);
        
        lv_obj_t *label = lv_label_create(item);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_label_set_text(label, networks[i].ssid);
    }
}
```

#### After Migration

```c
// screen_wifi_setup.c
#include "ui_helpers/ui_theme_tokens.h"
#include "ui_components/ui_list.h"

static void create_network_list(void) {
    // Use shared component
    lv_obj_t *list = ui_scrollable_list_create(s_content);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100) - 200);
    
    // Create list items using shared component
    for (int i = 0; i < network_count; i++) {
        lv_obj_t *item = ui_list_item_two_line_create(
            list,
            NULL,  // No icon
            networks[i].ssid,
            networks[i].security,  // Subtitle
            NULL,  // No extra text
            network_item_cb,
            (void*)(intptr_t)i
        );
        // Colors và fonts đã được set tự động bởi component
    }
}
```

---

## ⚠️ LƯU Ý QUAN TRỌNG

### 1. Không thay đổi behavior

- Migration chỉ thay đổi **implementation**, không thay đổi **behavior**.
- Nếu screen hoạt động khác sau migration → **rollback** và kiểm tra lại.

### 2. Test từng bước

- Không migrate tất cả cùng lúc.
- Migrate từng section (colors → fonts → spacing → components).
- Test sau mỗi bước.

### 3. Giữ backward compatibility

- Nếu screen cũ có logic đặc biệt, giữ nguyên logic đó.
- Chỉ thay đổi **style values**, không thay đổi **logic flow**.

### 4. Document changes

- Ghi chú trong commit message: "Migrate screen_xxx to use design tokens"
- List các thay đổi chính (colors, fonts, components).

---

## ✅ CHECKLIST MIGRATION

### Pre-Migration

- [ ] Đã audit screen (tìm tất cả hardcoded values)
- [ ] Đã tạo bảng mapping (hardcoded → token)
- [ ] Đã backup code hiện tại (git commit)

### During Migration

- [ ] Đã include tokens header
- [ ] Đã replace colors
- [ ] Đã replace fonts
- [ ] Đã replace spacing
- [ ] Đã migrate components (optional)

### Post-Migration

- [ ] Visual test passed (màu, font, spacing giống hệt)
- [ ] Functional test passed (events, lifecycle hoạt động)
- [ ] Performance test passed (không có regression)
- [ ] Code review passed
- [ ] Git commit với message rõ ràng

---

## 🎓 TÀI LIỆU THAM KHẢO

- **Design Tokens:** `ui_design_system_consistency_rules.md`
- **Shared Components:** `ui_components_to_extract.md`
- **Refactor Plan:** `refactor_plan_shared_ui_and_service_patterns.md`

---

**Tài liệu này sẽ được cập nhật khi có thêm patterns hoặc thay đổi design system.**






