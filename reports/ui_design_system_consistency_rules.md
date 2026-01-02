# UI DESIGN SYSTEM CONSISTENCY RULES

> **Mục tiêu:** Chuẩn hóa các quy tắc thiết kế UI (màu sắc, font, spacing, animation, performance) để toàn bộ OS có cảm giác "cùng một hệ", đồng thời tối ưu cho ESP32-S3 + LVGL.

---

## 1. COLOR SYSTEM (Bảng màu)

### 1.1 Background Colors

| Token | Hex Value | Usage | File Reference |
|-------|-----------|-------|----------------|
| `UI_COLOR_BG_PRIMARY` | `0x1a1a1a` | Main screen background, container background | `screen_music_player.c:266`, `screen_radio.c:62`, `screen_ota.c:50` |
| `UI_COLOR_BG_SECONDARY` | `0x2a2a2a` | Card background, button background (unpressed), album art placeholder | `screen_music_player.c:313`, `screen_common.c:89` |
| `UI_COLOR_BG_PRESSED` | `0x3a3a3a` | Button pressed state, list item pressed, slider track | `screen_music_player.c:402`, `screen_common.c:90` |
| `UI_COLOR_BG_DISABLED` | `0x1a1a1a` (same as primary) | Disabled controls (opacity reduced to 50%) | N/A (pattern only) |

**Quy tắc:**
- Tất cả screen phải dùng `UI_COLOR_BG_PRIMARY` cho container chính.
- Card/item phải dùng `UI_COLOR_BG_SECONDARY`.
- Pressed state phải dùng `UI_COLOR_BG_PRESSED` (không hardcode `0x3a3a3a`).

### 1.2 Primary/Accent Colors

| Token | Hex Value | Usage | File Reference |
|-------|-----------|-------|----------------|
| `UI_COLOR_PRIMARY` | `0x5b7fff` | Primary button, active slider indicator, active control | `screen_music_player.c:407,448`, `screen_common.c:134` |
| `UI_COLOR_PRIMARY_DARK` | `0x4a6fff` | Primary button pressed state | `screen_radio.c:124` |
| `UI_COLOR_ACCENT` | `0xa666f1` | Gradient end color, secondary accent | `screen_music_player.c:408` (gradient) |

**Quy tắc:**
- Primary action button (Play, Connect, Save...) phải dùng `UI_COLOR_PRIMARY`.
- Pressed state của primary button dùng `UI_COLOR_PRIMARY_DARK`.
- Gradient slider có thể dùng `UI_COLOR_PRIMARY` → `UI_COLOR_ACCENT`.

### 1.3 Text Colors

| Token | Hex Value | Usage | File Reference |
|-------|-----------|-------|----------------|
| `UI_COLOR_TEXT_PRIMARY` | `0xFFFFFF` | Main text, title, important labels | `screen_music_player.c:335,377`, `screen_common.c:35` |
| `UI_COLOR_TEXT_SECONDARY` | `0x888888` | Secondary text, artist, time total, hint text | `screen_music_player.c:346,386`, `screen_common.c:108` |
| `UI_COLOR_TEXT_ACCENT` | `0x8a86b8` | Genre, metadata, special info | `screen_music_player.c:358` |
| `UI_COLOR_TEXT_ERROR` | `0xFF0000` | Error messages | `screen_radio.c:101` |

**Quy tắc:**
- Title/label chính phải dùng `UI_COLOR_TEXT_PRIMARY`.
- Subtitle/hint phải dùng `UI_COLOR_TEXT_SECONDARY`.
- Không dùng màu trắng (`0xFFFFFF`) cho text trên nền sáng (nếu có light theme sau này).

### 1.4 Spectrum/Visualization Colors (Music-specific, nhưng có thể tái dùng cho AC/Chart)

| Token | Hex Value | Usage | File Reference |
|-------|-----------|-------|----------------|
| `UI_COLOR_SPECTRUM_LOW` | `0xe9dbfc` | Low frequency band | `screen_music_player_spectrum.c:19` |
| `UI_COLOR_SPECTRUM_MID` | `0x6f8af6` | Mid frequency band | `screen_music_player_spectrum.c:20` |
| `UI_COLOR_SPECTRUM_HIGH` | `0xffffff` | High frequency band | `screen_music_player_spectrum.c:21` |

**Quy tắc:**
- Chỉ dùng cho visualization (spectrum, chart, graph).
- Có thể tái dùng cho AC screen (temperature chart) hoặc Settings (battery level chart).

---

## 2. FONT SYSTEM (Typography Hierarchy)

### 2.1 Font Sizes

| Token | LVGL Font | Size (px) | Usage | File Reference |
|-------|-----------|-----------|-------|----------------|
| `UI_FONT_SMALL` | `lv_font_montserrat_12` | 12 | Genre, time labels, metadata | `screen_music_player.c:354,373,382` |
| `UI_FONT_MEDIUM` | `lv_font_montserrat_14` | 14 | **Default** - body text, labels, buttons | `screen_common.c:34`, hầu hết screen |
| `UI_FONT_LARGE` | `lv_font_montserrat_16` | 16 | Artist, subtitle, section header | `screen_music_player.c:342` |
| `UI_FONT_XLARGE` | `lv_font_montserrat_22` | 22 | Title, main heading | `screen_music_player.c:331` |

**Quy tắc:**
- **Default font:** Tất cả label/button không chỉ định font phải dùng `UI_FONT_MEDIUM` (`montserrat_14`).
- **Hierarchy:** Title > Large > Medium > Small (không nhảy cấp, ví dụ không dùng Small cho title).
- **Fallback:** Nếu font không available (ví dụ `montserrat_22` không được enable trong `sdkconfig`), fallback về `montserrat_14`.

**Code pattern:**
```c
#if LV_FONT_MONTSERRAT_22
    lv_obj_set_style_text_font(label, UI_FONT_XLARGE, 0);
#else
    lv_obj_set_style_text_font(label, UI_FONT_MEDIUM, 0);
#endif
```

### 2.2 Line Height & Spacing

- **Line height:** LVGL tự động tính (không cần set).
- **Label spacing:** Dùng `lv_obj_set_style_pad_*()` với spacing tokens (xem section 3).

---

## 3. SPACING SCALE (Khoảng cách)

### 3.1 Spacing Tokens

| Token | Value (px) | Usage | File Reference |
|-------|------------|-------|---------------|
| `UI_SPACE_XS` | 4 | Tight spacing, icon padding | N/A (pattern only) |
| `UI_SPACE_SM` | 8 | Small gap between related items | N/A (pattern only) |
| `UI_SPACE_MD` | 12 | Medium gap, list item padding | N/A (pattern only) |
| `UI_SPACE_LG` | 16 | Large gap, section spacing | N/A (pattern only) |
| `UI_SPACE_XL` | 20 | Extra large, container padding | `screen_music_player.c:277` |
| `UI_SPACE_XXL` | 24 | Very large, major section separation | N/A (pattern only) |

**Quy tắc:**
- Container padding: `UI_SPACE_XL` (20px) cho main content area.
- List item padding: `UI_SPACE_MD` (12px) vertical, `UI_SPACE_LG` (16px) horizontal.
- Button padding: `UI_SPACE_SM` (8px) minimum.
- Section spacing: `UI_SPACE_LG` (16px) giữa các section.

### 3.2 Size Tokens (Component Sizes)

| Token | Value | Usage | File Reference |
|-------|-------|-------|----------------|
| `UI_SIZE_BUTTON_HEIGHT` | 40-60 | Button height | `screen_music_player.c:424` (60) |
| `UI_SIZE_SLIDER_HEIGHT` | 6-24 | Slider track height | `screen_music_player.c:390` (6), `screen_music_player.c:475` (24) |
| `UI_SIZE_ALBUM_ART` | 220x220 | Large image/icon size | `screen_music_player.c:312` |
| `UI_SIZE_ICON_SMALL` | 24 | Small icon | N/A (pattern only) |
| `UI_SIZE_ICON_MEDIUM` | 48 | Medium icon | `screen_music_player.c:321` (48) |
| `UI_SIZE_ICON_LARGE` | 64 | Large icon | N/A (pattern only) |

**Quy tắc:**
- Button height: 40-60px (tùy screen, nhưng consistent trong cùng screen).
- Slider: 6px cho progress bar, 24px cho volume slider (có thể tương tác dễ hơn).
- Icon: 48px cho album art placeholder, 24px cho button icon.

---

## 4. COMPONENT STATE RULES (Trạng thái component)

### 4.1 Pressed State

- **Background:** `UI_COLOR_BG_PRESSED` (`0x3a3a3a`).
- **Animation:** Không cần animation (LVGL tự xử lý).
- **Code pattern:**
```c
lv_obj_set_style_bg_color(btn, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
lv_obj_set_style_bg_color(btn, UI_COLOR_BG_PRESSED, LV_PART_MAIN | LV_STATE_PRESSED);
```

### 4.2 Disabled State

- **Background:** Giữ nguyên màu, nhưng set opacity 50% (`LV_OPA_50`).
- **Text:** Giữ nguyên màu, opacity 50%.
- **Code pattern:**
```c
lv_obj_add_state(btn, LV_STATE_DISABLED);
lv_obj_set_style_bg_opa(btn, LV_OPA_50, LV_PART_MAIN | LV_STATE_DISABLED);
lv_obj_set_style_text_opa(btn, LV_OPA_50, LV_PART_MAIN | LV_STATE_DISABLED);
```

### 4.3 Checked/Selected State

- **Background:** `UI_COLOR_PRIMARY` (`0x5b7fff`) hoặc `UI_COLOR_BG_PRESSED` tùy context.
- **Code pattern:**
```c
lv_obj_set_style_bg_color(btn, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
lv_obj_set_style_bg_color(btn, UI_COLOR_PRIMARY, LV_PART_MAIN | LV_STATE_CHECKED);
```

### 4.4 Focus State (nếu có keyboard navigation)

- **Border:** `UI_COLOR_PRIMARY`, width 2px.
- **Code pattern:**
```c
lv_obj_set_style_border_color(btn, UI_COLOR_PRIMARY, LV_PART_MAIN | LV_STATE_FOCUSED);
lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN | LV_STATE_FOCUSED);
```

---

## 5. MOTION RULES (Animation)

### 5.1 Duration Tokens

| Token | Value (ms) | Usage | File Reference |
|-------|------------|-------|----------------|
| `UI_ANIM_DURATION_FAST` | 200 | Quick feedback (button press) | N/A (pattern only) |
| `UI_ANIM_DURATION_NORMAL` | 500 | Standard transition (fade, slide) | `screen_music_player.c:141,149` |
| `UI_ANIM_DURATION_SLOW` | 800-1000 | Intro animation, major transition | `screen_music_player.c:202` (800) |

**Quy tắc:**
- Fade in/out: `UI_ANIM_DURATION_NORMAL` (500ms).
- Slide transition: `UI_ANIM_DURATION_NORMAL` (500ms).
- Intro animation (staggered fade-in): `UI_ANIM_DURATION_SLOW` (800-1000ms) với delay giữa các element.

### 5.2 Easing Functions

- **Default:** `lv_anim_path_ease_out` (cho fade, slide).
- **Bounce:** `lv_anim_path_bounce` (cho button press feedback, nếu muốn).
- **Linear:** `lv_anim_path_linear` (cho progress bar update).

**Code pattern:**
```c
lv_obj_fade_in(obj, UI_ANIM_DURATION_NORMAL, 0);
lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
```

### 5.3 Intro Animation Pattern (Staggered Fade-In)

- **Base delay:** 2000ms (sau khi screen create).
- **Step delay:** 200-500ms giữa các element.
- **Order:** Background → Image → Title → Subtitle → Controls.

**Code pattern:**
```c
#define INTRO_TIME 2000
lv_obj_set_style_opa(obj, LV_OPA_TRANSP, 0);
lv_obj_fade_in(obj, 800, INTRO_TIME + 500);
```

---

## 6. PERFORMANCE RULES (Tối ưu cho ESP32-S3)

### 6.1 Timer Rate

- **Update rate:** Không quá 10Hz (100ms) cho timer update UI (progress, time label).
- **Spectrum animation:** 30-60fps (16-33ms) chỉ khi đang play, dừng khi pause.

**Code pattern:**
```c
// Good: 100ms = 10Hz
s_time_timer = lv_timer_create(time_timer_cb, 100, NULL);

// Bad: 10ms = 100Hz (quá nhanh, tốn CPU)
s_time_timer = lv_timer_create(time_timer_cb, 10, NULL);
```

### 6.2 Redraw Optimization

- **Avoid frequent redraw:** Chỉ update label text khi giá trị thay đổi (không update mỗi frame).
- **Use `LV_ANIM_OFF`:** Khi update progress bar từ timer, dùng `LV_ANIM_OFF` để tránh animation conflict.

**Code pattern:**
```c
// Good: Check if value changed
static uint32_t s_last_value = 0;
if (new_value != s_last_value) {
    lv_label_set_text(label, buf);
    s_last_value = new_value;
}

// Good: No animation for timer update
lv_slider_set_value(slider, value, LV_ANIM_OFF);
```

### 6.3 Memory Optimization

- **Limit image assets:** Chỉ load image khi cần (lazy load), unload khi không dùng.
- **Reuse objects:** Dùng `lv_obj_clear_flag()` / `lv_obj_add_flag()` để show/hide thay vì create/destroy.
- **String pool:** Dùng `sx_event_alloc_string()` cho event payload để tránh memory leak.

**Code pattern:**
```c
// Good: Hide/show instead of destroy
lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);

// Bad: Destroy/recreate
lv_obj_del(obj);
obj = lv_label_create(parent);
```

### 6.4 Grid Layout vs Flex Layout

- **Grid Layout:** Dùng cho screen có nhiều element cần alignment chính xác (Music Player).
- **Flex Layout:** Dùng cho list, button group (đơn giản hơn, ít overhead).

**Code pattern:**
```c
// Grid: For complex layout
static const int32_t grid_cols[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static const int32_t grid_rows[] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
lv_obj_set_grid_dsc_array(parent, grid_cols, grid_rows);

// Flex: For simple list/group
lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
```

---

## 7. LAYOUT TEMPLATES (Từ `ui_layout_templates.md`)

### 7.1 Header + Content Layout

- **Top bar:** Height 40px, background `UI_COLOR_BG_PRIMARY`, padding `UI_SPACE_XL`.
- **Content:** `LV_PCT(100)` width, `LV_PCT(100) - 40` height, padding `UI_SPACE_XL`.

**Code pattern:**
```c
s_top_bar = screen_common_create_top_bar_with_back(container, "Title");
s_content = lv_obj_create(container);
lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
lv_obj_set_style_pad_all(s_content, UI_SPACE_XL, LV_PART_MAIN);
```

### 7.2 List-based Screen Layout

- **List:** `LV_PCT(100)` width, `LV_PCT(100) - 200` height (trừ top bar + bottom controls).
- **List item:** Height 60-80px, padding `UI_SPACE_MD` vertical, `UI_SPACE_LG` horizontal.

---

## 8. KẾT LUẬN & ÁP DỤNG

### 8.1 Screen Mới (Settings, Chatbot, AC...)

- **Bắt buộc:** Dùng tokens từ section 1-3 (color, font, spacing).
- **Khuyến khích:** Dùng layout templates từ section 7.
- **Tối ưu:** Tuân thủ performance rules từ section 6.

### 8.2 Screen Cũ (Music Player, Radio, OTA...)

- **Không bắt buộc migrate:** Có thể giữ nguyên hardcode values.
- **Optional migration:** Nếu có thời gian, có thể refactor sang dùng tokens (PR-5).

### 8.3 Review Checklist

Khi review PR cho screen mới, kiểm tra:
- [ ] Dùng `UI_COLOR_*` tokens thay vì hardcode hex.
- [ ] Dùng `UI_FONT_*` tokens thay vì hardcode font.
- [ ] Dùng `UI_SPACE_*` tokens thay vì hardcode pixel.
- [ ] Timer rate không quá 10Hz.
- [ ] Intro animation duration hợp lý (500-1000ms).
- [ ] Disabled state có opacity 50%.

---

**Tài liệu này sẽ được cập nhật khi có thêm patterns hoặc thay đổi design system.**






