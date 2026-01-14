# UI VALIDATION CHECKLIST EXTENDED

> **Mục tiêu:** Checklist chi tiết để validate screen mới hoặc screen đã migrate, đảm bảo tuân thủ design system và best practices.

---

## 📋 TỔNG QUAN

Checklist này dùng cho:
1. **Code Review** - Reviewer kiểm tra PR screen mới/migrate
2. **Self-Check** - Developer tự kiểm tra trước khi submit PR
3. **QA Testing** - Tester kiểm tra visual và functional

---

## ✅ DESIGN SYSTEM COMPLIANCE

### Colors

- [ ] **Không có hardcode `lv_color_hex(...)`**
  - ✅ Good: `lv_obj_set_style_bg_color(obj, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);`
  - ❌ Bad: `lv_obj_set_style_bg_color(obj, lv_color_hex(0x1a1a1a), LV_PART_MAIN);`
  
- [ ] **Background colors dùng đúng tokens**
  - Container: `UI_COLOR_BG_PRIMARY`
  - Card/Item: `UI_COLOR_BG_SECONDARY`
  - Pressed state: `UI_COLOR_BG_PRESSED`
  
- [ ] **Text colors dùng đúng tokens**
  - Title/Primary text: `UI_COLOR_TEXT_PRIMARY`
  - Subtitle/Secondary text: `UI_COLOR_TEXT_SECONDARY`
  - Error text: `UI_COLOR_TEXT_ERROR` (nếu có)
  
- [ ] **Primary/Accent colors dùng đúng tokens**
  - Primary button: `UI_COLOR_PRIMARY`
  - Primary button pressed: `UI_COLOR_PRIMARY_DARK`
  - Gradient end: `UI_COLOR_ACCENT` (nếu dùng gradient)

### Fonts

- [ ] **Không có hardcode `&lv_font_montserrat_*`**
  - ✅ Good: `lv_obj_set_style_text_font(label, UI_FONT_MEDIUM, 0);`
  - ❌ Bad: `lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);`
  
- [ ] **Font hierarchy đúng**
  - Title: `UI_FONT_XLARGE` (với fallback `UI_FONT_MEDIUM`)
  - Subtitle: `UI_FONT_LARGE` (với fallback `UI_FONT_MEDIUM`)
  - Body: `UI_FONT_MEDIUM` (default)
  - Metadata: `UI_FONT_SMALL`
  
- [ ] **Có fallback cho large fonts**
  ```c
  #if LV_FONT_MONTSERRAT_22
      lv_obj_set_style_text_font(title, UI_FONT_XLARGE, 0);
  #else
      lv_obj_set_style_text_font(title, UI_FONT_MEDIUM, 0);
  #endif
  ```

### Spacing

- [ ] **Không có hardcode spacing values**
  - ✅ Good: `lv_obj_set_style_pad_all(container, UI_SPACE_XL, LV_PART_MAIN);`
  - ❌ Bad: `lv_obj_set_style_pad_all(container, 20, LV_PART_MAIN);`
  
- [ ] **Spacing tokens dùng đúng**
  - Container padding: `UI_SPACE_XL` (20px)
  - List item padding: `UI_SPACE_MD` (12px) vertical, `UI_SPACE_LG` (16px) horizontal
  - Button padding: `UI_SPACE_SM` (8px) minimum
  - Section spacing: `UI_SPACE_LG` (16px)

### Component States

- [ ] **Pressed state đúng**
  - Background: `UI_COLOR_BG_PRESSED`
  - Code: `lv_obj_set_style_bg_color(btn, UI_COLOR_BG_PRESSED, LV_PART_MAIN | LV_STATE_PRESSED);`
  
- [ ] **Disabled state đúng**
  - Opacity: 50% (`LV_OPA_50`)
  - Code:
    ```c
    lv_obj_add_state(btn, LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_50, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_text_opa(btn, LV_OPA_50, LV_PART_MAIN | LV_STATE_DISABLED);
    ```
  
- [ ] **Checked state đúng**
  - Background: `UI_COLOR_PRIMARY` hoặc `UI_COLOR_BG_PRESSED` tùy context
  - Code: `lv_obj_set_style_bg_color(btn, UI_COLOR_PRIMARY, LV_PART_MAIN | LV_STATE_CHECKED);`

---

## 🧩 COMPONENT USAGE

### Buttons

- [ ] **Dùng shared components thay vì tạo từ đầu**
  - ✅ Good: `ui_image_button_create(parent, &img_icon, cb, user_data);`
  - ❌ Bad: `lv_img_create()` + `LV_OBJ_FLAG_CLICKABLE` + manual event handler
  
- [ ] **Checkable button dùng shared component**
  - ✅ Good: `ui_checkable_image_button_create(parent, &img_released, &img_checked, cb, user_data);`
  - ❌ Bad: `lv_imagebutton_create()` + manual state management

### Sliders

- [ ] **Gradient slider dùng shared component**
  - ✅ Good: `ui_gradient_slider_create(parent, cb, user_data);`
  - ❌ Bad: `lv_slider_create()` + manual gradient style setup

### Lists

- [ ] **Scrollable list dùng shared component**
  - ✅ Good: `ui_scrollable_list_create(parent);`
  - ❌ Bad: `lv_obj_create()` + `LV_FLEX_FLOW_COLUMN` + manual scrollbar style
  
- [ ] **List items dùng shared component**
  - ✅ Good: `ui_list_item_two_line_create(list, icon, title, subtitle, extra, cb, user_data);`
  - ❌ Bad: Manual `lv_obj_create()` + `lv_label_create()` + manual layout

### Animations

- [ ] **Intro animation dùng shared helper**
  - ✅ Good: `ui_helper_fade_in_staggered(elements, count, base_delay, step_delay);`
  - ❌ Bad: Manual `lv_obj_fade_in()` với hardcoded delays

---

## 📐 LAYOUT TEMPLATE

### Top Bar

- [ ] **Dùng shared top bar function**
  - ✅ Good: `screen_common_create_top_bar_with_back(container, "Title");`
  - ❌ Bad: Manual `lv_obj_create()` + `lv_label_create()` + back button

### Content Area

- [ ] **Content area setup đúng**
  - Size: `LV_PCT(100)` width, `LV_PCT(100) - 40` height
  - Align: `LV_ALIGN_TOP_LEFT, 0, 40`
  - Padding: `UI_SPACE_XL`
  - Background: `UI_COLOR_BG_PRIMARY` (hoặc transparent)
  
- [ ] **Layout type phù hợp**
  - List-based: Dùng `ui_scrollable_list_create()`
  - Grid: Dùng `lv_obj_set_grid_dsc_array()` với tokens spacing
  - Flex: Dùng `lv_obj_set_flex_flow()` cho simple layout
  - Fullscreen: Background + floating controls

---

## ⚡ PERFORMANCE

### Timer Rate

- [ ] **Timer rate không quá 10Hz (100ms)**
  - ✅ Good: `lv_timer_create(cb, 100, NULL);` (10Hz)
  - ❌ Bad: `lv_timer_create(cb, 10, NULL);` (100Hz - quá nhanh)
  
- [ ] **Chỉ update khi giá trị thay đổi**
  - ✅ Good:
    ```c
    static uint32_t s_last_value = 0;
    if (new_value != s_last_value) {
        lv_label_set_text(label, buf);
        s_last_value = new_value;
    }
    ```
  - ❌ Bad: Update mỗi frame không cần thiết

### Redraw Optimization

- [ ] **Dùng `LV_ANIM_OFF` cho timer update**
  - ✅ Good: `lv_slider_set_value(slider, value, LV_ANIM_OFF);`
  - ❌ Bad: `lv_slider_set_value(slider, value, LV_ANIM_ON);` (gây lag)
  
- [ ] **Chỉ update khi user không tương tác**
  - ✅ Good: `if (!lv_obj_has_state(slider, LV_STATE_PRESSED)) { update(); }`
  - ❌ Bad: Update ngay cả khi user đang drag

### Memory

- [ ] **Dùng hide/show thay vì create/destroy**
  - ✅ Good:
    ```c
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    ```
  - ❌ Bad:
    ```c
    lv_obj_del(obj);
    obj = lv_label_create(parent);
    ```
  
- [ ] **String pool cho event payload**
  - ✅ Good: `ptr = sx_event_alloc_string("message");`
  - ❌ Bad: `ptr = "static string";` (có thể gây memory issue)

---

## 🎨 VISUAL TEST

### Colors

- [ ] **Màu sắc giống design system**
  - Background: Dark gray (`0x1a1a1a`)
  - Cards: Medium gray (`0x2a2a2a`)
  - Primary: Blue (`0x5b7fff`)
  - Text: White (`0xFFFFFF`) / Gray (`0x888888`)
  
- [ ] **Button states visible**
  - Pressed: Darker background
  - Disabled: 50% opacity
  - Checked: Primary color

### Typography

- [ ] **Font sizes đúng hierarchy**
  - Title: Largest (22px nếu có)
  - Subtitle: Large (16px)
  - Body: Medium (14px - default)
  - Metadata: Small (12px)
  
- [ ] **Text colors đúng contrast**
  - Primary text: White trên dark background
  - Secondary text: Gray trên dark background

### Spacing

- [ ] **Spacing consistent**
  - Container padding: 20px
  - List item padding: 12px vertical, 16px horizontal
  - Section spacing: 16px

### Layout

- [ ] **Layout balanced**
  - Elements không quá sát nhau
  - Elements không quá xa nhau
  - Alignment đúng (center, left, right)

---

## 🔧 FUNCTIONAL TEST

### Events

- [ ] **Button click events hoạt động**
  - Test tất cả buttons trong screen
  - Verify callback được gọi đúng
  
- [ ] **Slider value change events hoạt động**
  - Test drag slider
  - Verify value update đúng
  
- [ ] **List item click events hoạt động**
  - Test click list items
  - Verify navigation/action đúng

### Navigation

- [ ] **Back button hoạt động**
  - Test back button trong top bar
  - Verify quay về screen trước
  
- [ ] **Navigation giữa screens hoạt động**
  - Test navigate từ screen này sang screen khác
  - Verify state được preserve (nếu cần)

### State Management

- [ ] **Screen lifecycle hoạt động**
  - `on_create`: UI được tạo đúng
  - `on_show`: UI hiển thị đúng
  - `on_hide`: UI được hide đúng
  - `on_update`: UI update khi state thay đổi
  - `on_destroy`: Resources được cleanup

### Error Handling

- [ ] **Error states được xử lý**
  - Disabled state khi không available
  - Error message hiển thị đúng
  - Retry button hoạt động (nếu có)

---

## 📝 CODE QUALITY

### Includes

- [ ] **Include đúng headers**
  - `ui_theme_tokens.h` (bắt buộc)
  - `ui_components/ui_*.h` (nếu dùng shared components)
  - `screen_common.h` (cho top bar)
  
- [ ] **Không include thừa**
  - Chỉ include những gì cần dùng

### Naming

- [ ] **Naming convention đúng**
  - Static variables: `s_*` prefix
  - Functions: `screen_xxx_*` hoặc `*_cb` cho callbacks
  - Constants: `UPPER_CASE`

### Comments

- [ ] **Comments rõ ràng**
  - Giải thích logic phức tạp
  - TODO/FIXME nếu có
  - Section comments cho các phần lớn

### Error Handling

- [ ] **Null checks**
  - Check `container == NULL` trong `on_create`
  - Check `obj != NULL` trước khi dùng
  
- [ ] **Error logging**
  - `ESP_LOGE` cho errors
  - `ESP_LOGW` cho warnings
  - `ESP_LOGI` cho info (lifecycle)

---

## 🚀 DEPLOYMENT CHECKLIST

### Pre-Commit

- [ ] **Code compiles không có warnings**
  - Fix tất cả compiler warnings
  - Fix tất cả linter errors
  
- [ ] **Visual test passed**
  - Màu, font, spacing đúng
  - Layout balanced
  
- [ ] **Functional test passed**
  - Events hoạt động
  - Navigation hoạt động
  - State management đúng

### Pre-Merge

- [ ] **Code review passed**
  - Reviewer đã approve
  - Comments đã được address
  
- [ ] **CI/CD passed**
  - Build thành công
  - Tests pass (nếu có)

### Post-Merge

- [ ] **Documentation updated**
  - Screen được thêm vào screen list (nếu cần)
  - Features được document (nếu có)

---

## 📊 SCORING (Optional)

Đánh giá screen theo thang điểm:

- **Design System Compliance:** 0-25 điểm
  - Colors: 0-8 điểm
  - Fonts: 0-8 điểm
  - Spacing: 0-5 điểm
  - Component States: 0-4 điểm

- **Component Usage:** 0-20 điểm
  - Buttons: 0-5 điểm
  - Sliders: 0-5 điểm
  - Lists: 0-5 điểm
  - Animations: 0-5 điểm

- **Layout Template:** 0-15 điểm
  - Top Bar: 0-5 điểm
  - Content Area: 0-5 điểm
  - Layout Type: 0-5 điểm

- **Performance:** 0-20 điểm
  - Timer Rate: 0-8 điểm
  - Redraw Optimization: 0-7 điểm
  - Memory: 0-5 điểm

- **Code Quality:** 0-20 điểm
  - Includes: 0-5 điểm
  - Naming: 0-5 điểm
  - Comments: 0-5 điểm
  - Error Handling: 0-5 điểm

**Tổng điểm:** 0-100 điểm

- **90-100:** Excellent - Sẵn sàng merge
- **70-89:** Good - Cần fix minor issues
- **50-69:** Fair - Cần fix major issues
- **0-49:** Poor - Cần refactor lại

---

## 🎓 TÀI LIỆU THAM KHẢO

- **Design System:** `ui_design_system_consistency_rules.md`
- **Migration Guide:** `ui_migration_guide_detailed.md`
- **New Screen Templates:** `ui_new_screen_templates.md`
- **Master Guide:** `ui_standardization_master_guide.md`

---

**Checklist này sẽ được cập nhật khi có thêm rules hoặc best practices.**








