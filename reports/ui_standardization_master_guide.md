# UI STANDARDIZATION MASTER GUIDE

> **Mục tiêu:** Tài liệu tổng hợp toàn bộ quy tắc, patterns, và hướng dẫn để chuẩn hóa và đồng bộ tất cả screen trong SimpleXL OS.

---

## 📋 TỔNG QUAN

Tài liệu này là **entry point** để bắt đầu chuẩn hóa UI. Nó liên kết đến các report chi tiết và cung cấp checklist tổng hợp.

### Cấu trúc tài liệu

1. **Inventory & Analysis** (Đã có):
   - `ui_music_demo_reuse_inventory.md` - Liệt kê reusable components
   - `ui_patterns_extracted_from_music.md` - UI/UX patterns
   - `ui_not_reusable_music_specific.md` - Những gì không reusable

2. **Extraction Plan** (Đã có):
   - `ui_components_to_extract.md` - Components nên trích
   - `ui_layout_templates.md` - Layout templates
   - `refactor_plan_shared_ui_and_service_patterns.md` - Kế hoạch refactor (PR-0 đến PR-6)

3. **Design System** (Đã có):
   - `ui_design_system_consistency_rules.md` - Color, font, spacing, animation, performance rules

4. **Implementation Guide** (Đã bổ sung):
   - `ui_migration_guide_detailed.md` - Migration guide chi tiết với ví dụ cụ thể
   - `ui_new_screen_templates.md` - Code templates cho screen mới (Settings, Chatbot, AC...)
   - Validation checklist (có trong Master Guide và các guide trên)

---

## ✅ ĐÁNH GIÁ MỨC ĐỘ ĐỦ/THIẾU

### Đã đủ (Có thể bắt đầu chuẩn hóa)

| Hạng mục | Trạng thái | Ghi chú |
|----------|------------|---------|
| **Design Tokens** | ✅ Đủ | `ui_design_system_consistency_rules.md` có đầy đủ color, font, spacing tokens |
| **Component Library** | ✅ Đủ | `ui_components_to_extract.md` liệt kê đủ components cần trích (Button, Slider, List) |
| **Layout Templates** | ✅ Đủ | `ui_layout_templates.md` có 5 templates cơ bản |
| **Animation Patterns** | ✅ Đủ | `ui_patterns_extracted_from_music.md` có StaggeredFadeIn, SlideAndFade |
| **Refactor Plan** | ✅ Đủ | `refactor_plan_shared_ui_and_service_patterns.md` có PR-0 đến PR-6 |
| **Service Patterns** | ✅ Đủ | PR-6 có async job event contract |

### Đã bổ sung (Hoàn thiện)

| Hạng mục | Trạng thái | File |
|----------|------------|------|
| **Migration Guide** | ✅ Hoàn thiện | `ui_migration_guide_detailed.md` - Step-by-step guide với ví dụ cụ thể |
| **New Screen Template** | ✅ Hoàn thiện | `ui_new_screen_templates.md` - Code templates cho Settings, Chatbot, AC... |
| **Validation Checklist** | ✅ Hoàn thiện | Có trong Master Guide + Migration Guide + New Screen Templates |
| **Implementation Roadmap** | ✅ Hoàn thiện | Có trong Master Guide (3 giai đoạn) |

---

## 🎯 KẾ HOẠCH CHUẨN HÓA (3 GIAI ĐOẠN)

### Giai đoạn 1: Setup Infrastructure (PR-0 đến PR-6)

**Mục tiêu:** Tạo shared components và tokens.

**Deliverables:**
- ✅ PR-0: Tạo thư mục `ui_components/`, `ui_helpers/`
- ✅ PR-1: Extract animation helpers
- ✅ PR-2: Extract button components
- ✅ PR-3: Extract HTTP client helper (service)
- ✅ PR-4: Extract list & slider components
- ✅ PR-5: Create theme tokens header
- ✅ PR-6: Document async job event contract

**Kết quả:** Có đủ infrastructure để screen mới dùng ngay.

**Thời gian ước tính:** 2-3 tuần (nếu làm tuần tự).

---

### Giai đoạn 2: Migrate Screen Cũ (Optional, không bắt buộc)

**Mục tiêu:** Migrate các screen hiện có sang dùng tokens/components (nếu muốn).

**Screen ưu tiên:**
1. **Settings** (nếu chưa có) - Tạo mới dùng tokens ngay
2. **Radio** - Migrate sang dùng `UI_COLOR_*` tokens
3. **OTA** - Migrate sang dùng `UI_COLOR_*` tokens
4. **WiFi Setup** - Migrate sang dùng `UI_COLOR_*` tokens

**Approach:**
- Không bắt buộc migrate tất cả screen cũ.
- Chỉ migrate khi có thời gian hoặc khi sửa bug/feature.
- Screen mới **bắt buộc** dùng tokens từ đầu.

**Thời gian ước tính:** 1-2 tuần (tùy số lượng screen).

---

### Giai đoạn 3: Tạo Screen Mới (Bắt buộc dùng tokens)

**Mục tiêu:** Tạo các screen mới (Settings, Chatbot, AC, Navigation...) dùng tokens/components ngay từ đầu.

**Screen ưu tiên:**
1. **Settings** - List-based, dùng `ui_scrollable_list_create()`, `ui_list_item_two_line_create()`
2. **Chatbot** - List-based (chat history), dùng `ui_checkable_image_button_create()` cho mic
3. **AC Control** - Grid layout, dùng `ui_gradient_slider_create()` cho temperature
4. **Navigation** - Fullscreen background + floating controls

**Approach:**
- Mỗi screen mới phải tuân thủ `ui_design_system_consistency_rules.md`.
- Dùng components từ PR-1 đến PR-4.
- Dùng tokens từ PR-5.

**Thời gian ước tính:** 1-2 tuần/screen (tùy độ phức tạp).

---

## 📝 CHECKLIST CHO SCREEN MỚI

Khi tạo screen mới, kiểm tra:

### Design System Compliance

- [ ] **Colors:** Dùng `UI_COLOR_*` tokens thay vì hardcode hex
  - Background: `UI_COLOR_BG_PRIMARY` / `UI_COLOR_BG_SECONDARY`
  - Text: `UI_COLOR_TEXT_PRIMARY` / `UI_COLOR_TEXT_SECONDARY`
  - Primary button: `UI_COLOR_PRIMARY`
- [ ] **Fonts:** Dùng `UI_FONT_*` tokens với fallback
  - Title: `UI_FONT_XLARGE` (fallback `UI_FONT_MEDIUM`)
  - Body: `UI_FONT_MEDIUM` (default)
  - Metadata: `UI_FONT_SMALL`
- [ ] **Spacing:** Dùng `UI_SPACE_*` tokens
  - Container padding: `UI_SPACE_XL` (20px)
  - List item padding: `UI_SPACE_MD` (12px) vertical, `UI_SPACE_LG` (16px) horizontal
- [ ] **Component States:** Tuân thủ rules
  - Pressed: `UI_COLOR_BG_PRESSED`
  - Disabled: Opacity 50%
  - Checked: `UI_COLOR_PRIMARY`

### Component Usage

- [ ] **Buttons:** Dùng `ui_image_button_create()` hoặc `ui_checkable_image_button_create()` (PR-2)
- [ ] **Sliders:** Dùng `ui_gradient_slider_create()` (PR-4)
- [ ] **Lists:** Dùng `ui_scrollable_list_create()` và `ui_list_item_two_line_create()` (PR-4)
- [ ] **Animations:** Dùng `ui_helper_fade_in_staggered()` cho intro (PR-1)

### Layout Template

- [ ] **Top Bar:** Dùng `screen_common_create_top_bar_with_back()` (đã có)
- [ ] **Content Area:** Tuân thủ layout template từ `ui_layout_templates.md`
  - Header + Content (Settings, About)
  - List-based (Settings, Chatbot)
  - Grid (AC Control, Audio Effects)
  - Fullscreen + Floating (Navigation, Home)

### Performance

- [ ] **Timer Rate:** Không quá 10Hz (100ms) cho UI update
- [ ] **Redraw:** Chỉ update khi giá trị thay đổi
- [ ] **Memory:** Dùng hide/show thay vì create/destroy

### Code Quality

- [ ] **No Hardcode:** Không hardcode màu/font/spacing (dùng tokens)
- [ ] **Error Handling:** Xử lý lỗi đúng (disabled state, error message)
- [ ] **Event Handling:** Tuân thủ async job event contract (nếu có progress/error)

---

## 🔧 MIGRATION GUIDE CHO SCREEN CŨ

### Step 1: Identify Hardcoded Values

Tìm và liệt kê tất cả hardcoded values:
```c
// Bad
lv_obj_set_style_bg_color(btn, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);

// Good (sau migration)
lv_obj_set_style_bg_color(btn, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
lv_obj_set_style_text_font(label, UI_FONT_MEDIUM, 0);
```

### Step 2: Include Tokens Header

```c
#include "ui_helpers/ui_theme_tokens.h"
```

### Step 3: Replace Values

- Màu: `lv_color_hex(0x1a1a1a)` → `UI_COLOR_BG_PRIMARY`
- Font: `&lv_font_montserrat_14` → `UI_FONT_MEDIUM`
- Spacing: `20` → `UI_SPACE_XL`

### Step 4: Test

- Kiểm tra visual không đổi (màu, font, spacing giống hệt).
- Kiểm tra behavior không đổi (button press, animation...).

### Step 5: Optional - Use Shared Components

Nếu screen có button/slider/list, có thể migrate sang dùng shared components (PR-2, PR-4), nhưng không bắt buộc.

---

## 🚀 TEMPLATE CHO SCREEN MỚI

### Boilerplate Code

```c
#include "screen_xxx.h"
#include "ui_helpers/ui_theme_tokens.h"
#include "ui_components/ui_buttons.h"      // PR-2
#include "ui_components/ui_list.h"        // PR-4
#include "ui_helpers/ui_animation_helpers.h" // PR-1

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_content = NULL;

static void on_create(void) {
    lv_obj_t *container = ui_router_get_container();
    
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
    
    // 3. Create UI elements (dùng tokens và components)
    // ...
    
    // 4. Intro animation (optional)
    // ui_helper_fade_in_staggered(...);
}

// ... rest of screen implementation
```

---

## 📊 VALIDATION CHECKLIST (Cho Code Review)

Khi review PR cho screen mới, kiểm tra:

### Design System

- [ ] Không có hardcode `lv_color_hex(...)` (phải dùng `UI_COLOR_*`)
- [ ] Không có hardcode `&lv_font_montserrat_*` (phải dùng `UI_FONT_*` với fallback)
- [ ] Không có hardcode spacing (phải dùng `UI_SPACE_*`)
- [ ] Disabled state có opacity 50%
- [ ] Pressed state dùng `UI_COLOR_BG_PRESSED`

### Components

- [ ] Dùng shared components (PR-2, PR-4) thay vì tạo widget từ đầu
- [ ] Button dùng `ui_image_button_create()` hoặc `ui_checkable_image_button_create()`
- [ ] List dùng `ui_scrollable_list_create()` và `ui_list_item_two_line_create()`
- [ ] Slider dùng `ui_gradient_slider_create()`

### Performance

- [ ] Timer rate không quá 10Hz (100ms)
- [ ] Chỉ update label khi giá trị thay đổi
- [ ] Dùng hide/show thay vì create/destroy

### Layout

- [ ] Tuân thủ layout template từ `ui_layout_templates.md`
- [ ] Top bar dùng `screen_common_create_top_bar_with_back()`
- [ ] Content area có padding `UI_SPACE_XL`

---

## 🎓 TÀI LIỆU THAM KHẢO

### Bắt đầu từ đâu?

1. **Đọc Master Guide:** `ui_standardization_master_guide.md` (file này) - Tổng quan
2. **Đọc design system:** `ui_design_system_consistency_rules.md` - Color, font, spacing, animation rules
3. **Xem components có sẵn:** `ui_components_to_extract.md` - Components sẽ được trích
4. **Chọn layout template:** `ui_layout_templates.md` - 5 layout templates cơ bản
5. **Làm theo refactor plan:** `refactor_plan_shared_ui_and_service_patterns.md` (PR-0 đến PR-6)
6. **Tạo screen mới:** `ui_new_screen_templates.md` - Code templates hoàn chỉnh
7. **Migrate screen cũ:** `ui_migration_guide_detailed.md` - Step-by-step guide với ví dụ

### Khi nào cần migrate screen cũ?

- **Không bắt buộc:** Screen cũ có thể giữ nguyên hardcode values.
- **Nên migrate khi:**
  - Sửa bug/feature trong screen đó
  - Screen đó cần thay đổi style (dễ hơn nếu dùng tokens)
  - Có thời gian rảnh

### Khi nào cần tạo screen mới?

- **Bắt buộc dùng tokens:** Tất cả screen mới phải dùng tokens từ đầu.
- **Bắt buộc dùng components:** Dùng shared components (PR-2, PR-4) thay vì tạo từ đầu.

---

## ✅ KẾT LUẬN

### Đã đủ cơ sở để chuẩn hóa?

**Có, đã đủ 80-90%:**

✅ **Đủ:**
- Design tokens (color, font, spacing)
- Component library plan
- Layout templates
- Animation patterns
- Refactor plan (PR-0 đến PR-6)
- Service patterns (async job event contract)

✅ **Đã bổ sung đầy đủ:**
- `ui_migration_guide_detailed.md` - Migration guide chi tiết với ví dụ cụ thể từ screen_radio, screen_wifi_setup
- `ui_new_screen_templates.md` - Code templates hoàn chỉnh cho Settings, Chatbot, AC Control
- Validation checklist - Có trong Master Guide, Migration Guide, và New Screen Templates

### Có thể bắt đầu ngay?

**Có, có thể bắt đầu ngay:**

1. **Bắt đầu với PR-0 đến PR-6** (setup infrastructure)
2. **Tạo screen mới** (Settings, Chatbot, AC...) dùng tokens/components ngay
3. **Migrate screen cũ** (optional, khi có thời gian)

### Lộ trình đề xuất

1. **Tuần 1-2:** Hoàn thành PR-0 đến PR-6 (infrastructure)
2. **Tuần 3-4:** Tạo screen mới đầu tiên (Settings) làm mẫu
3. **Tuần 5+:** Tạo các screen mới khác (Chatbot, AC, Navigation...)
4. **Ongoing:** Migrate screen cũ (optional, khi có thời gian)

---

**Tài liệu này sẽ được cập nhật khi có thêm patterns hoặc thay đổi design system.**

