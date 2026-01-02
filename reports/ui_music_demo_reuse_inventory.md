# BÁO CÁO TỒN KHO TÁI SỬ DỤNG UI TỪ MUSIC DEMO

> **Mục tiêu:** Liệt kê và phân loại toàn bộ thành phần UI trong Music Demo có thể được tái sử dụng cho các màn hình khác trong hệ điều hành, nhằm xây dựng một UI infrastructure nhất quán.

---

## 1. UI Primitives (Thành phần giao diện cơ bản)

| Tên thành phần | Loại | File Path + Function/Widget | Phụ thuộc | Tái sử dụng |
| :--- | :--- | :--- | :--- | :--- |
| **TopBarWithBack** | UI Primitive | `components/sx_ui/screens/screen_common.c` -> `screen_common_create_top_bar_with_back()` | `lv_obj`, `lv_label`, `lv_btn`, `ui_router` | **A** (Đã là component chung) |
| **ImageButton** | UI Primitive | `components/sx_ui/screens/screen_music_player.c` -> `s_prev_btn`, `s_next_btn` (tạo bằng `lv_img_create`) | `lv_img`, `LV_OBJ_FLAG_CLICKABLE`, `lv_event_cb_t` | **B** (Cần tạo hàm helper `ui_create_image_button(parent, src, cb)`) |
| **CheckableImageButton** | UI Primitive | `components/sx_ui/screens/screen_music_player.c` -> `s_play_btn` (tạo bằng `lv_imagebutton_create`) | `lv_imagebutton`, `LV_OBJ_FLAG_CHECKABLE`, `lv_event_cb_t` | **B** (Cần tạo hàm helper `ui_create_checkable_image_button(...)`) |
| **GradientSlider** | UI Primitive | `components/sx_ui/screens/screen_music_player.c` -> `s_progress_slider` | `lv_slider`, `lv_style_t` (gradient, knob) | **B** (Tách style và tạo hàm `ui_create_gradient_slider()`) |
| **InfoLabel** | UI Primitive | `components/sx_ui/screens/screen_music_player.c` -> `s_track_title`, `s_track_artist` | `lv_label`, `LV_LABEL_LONG_SCROLL_CIRCULAR` | **A** (Chỉ là `lv_label` với style, có thể chuẩn hóa style) |
| **IconWithTextListItem** | UI Primitive | `components/sx_ui/screens/screen_music_player_list.c` -> `add_list_button()` | `lv_obj` (grid layout), `lv_label`, `lv_image` | **B** (Tách thành `ui_create_list_item(parent, icon, title, subtitle, extra_text)`) |

---

## 2. Layout Templates (Mẫu bố cục)

| Tên thành phần | Loại | File Path + Function/Widget | Phụ thuộc | Tái sử dụng |
| :--- | :--- | :--- | :--- | :--- |
| **HeaderContentLayout** | Layout Template | `components/sx_ui/screens/screen_music_player.c` -> `on_create()` (cấu trúc `s_top_bar` và `s_content`) | `lv_obj` | **C** (Pattern: màn hình luôn có top bar và vùng nội dung chính bên dưới) |
| **VerticalCenteredGridLayout** | Layout Template | `components/sx_ui/screens/screen_music_player.c` -> `s_content` (sử dụng Grid Layout) | `lv_obj`, `lv_grid_dsc_array` | **C** (Pattern: dùng grid để căn chỉnh các widget theo chiều dọc, rất hữu ích cho Settings, About) |
| **ScrollableListLayout** | Layout Template | `components/sx_ui/screens/screen_music_player_list.c` -> `create_playlist_view()` | `lv_obj`, `LV_FLEX_FLOW_COLUMN`, custom scrollbar style | **B** (Tạo hàm `ui_create_scrollable_list(parent)`) |

---

## 3. Animation / Interaction Patterns (Mẫu tương tác & hoạt ảnh)

| Tên thành phần | Loại | File Path + Function/Widget | Phụ thuộc | Tái sử dụng |
| :--- | :--- | :--- | :--- | :--- |
| **StaggeredFadeIn** | Animation Pattern | `components/sx_ui/screens/screen_music_player.c` -> `on_create()` (phần Intro animations) | `lv_obj_fade_in`, `lv_anim_t` | **C** (Pattern: các widget xuất hiện nối tiếp nhau, tạo cảm giác mượt mà khi vào màn hình) |
| **SlideAndFadeTransition** | Animation Pattern | `components/sx_ui/screens/screen_music_player.c` -> `animate_album_art_change()` | `lv_obj_fade_out`, `lv_obj_fade_in`, `lv_anim_t` (set x) | **C** (Pattern: dùng cho chuyển đổi giữa các item trong carousel hoặc khi thay đổi nội dung chính) |
| **PressFeedback** | Interaction Pattern | `components/sx_ui/screens/screen_music_player.c` -> style `LV_STATE_PRESSED` cho các button | `lv_style_t` | **A** (Đã là pattern chuẩn, cần chuẩn hóa style `pressed` cho toàn OS) |
| **DisabledState** | Interaction Pattern | `components/sx_ui/screens/screen_music_player.c` -> `s_progress_slider` khi seek không được hỗ trợ | `lv_obj_add_state(LV_STATE_DISABLED)` | **A** (Pattern chuẩn của LVGL, cần chuẩn hóa style `disabled` cho toàn OS) |

---

## 4. Assets / Styles (Tài nguyên & Phong cách)

| Tên thành phần | Loại | File Path + Function/Widget | Phụ thuộc | Tái sử dụng |
| :--- | :--- | :--- | :--- | :--- |
| **WaveBackgroundLayer** | Asset / Style | `components/sx_ui/screens/screen_music_player.c` -> `add_wave_decorations()` | `img_lv_demo_music_wave_top`, `img_lv_demo_music_wave_bottom` | **B** (Hàm có thể tái sử dụng, nhưng asset cần được thay thế hoặc coi là một phần của theme) |
| **TypographyHierarchy** | Asset / Style | `components/sx_ui/screens/screen_music_player.c` (styles cho `s_track_title`, `s_track_artist`, `s_track_genre`) | `lv_font_t` (Montserrat 12, 16, 22) | **C** (Pattern: định nghĩa các cấp font (lớn, vừa, nhỏ) cho toàn OS để đảm bảo tính nhất quán) |
| **ColorPalette** | Asset / Style | Toàn bộ file `screen_music_player.c` (sử dụng `lv_color_hex`) | `lv_color_t` | **C** (Pattern: định nghĩa một bảng màu chung (primary, secondary, accent, text, background...) cho toàn OS) |
| **ListItemStyle** | Asset / Style | `components/sx_ui/screens/screen_music_player_list.c` (các `lv_style_t` trong `create_playlist_view`) | `lv_style_t` | **B** (Tách các style này ra file `ui_styles.c` để định nghĩa style chung cho các list item) |

---

### Chú giải Mức độ tái sử dụng:

- **A — Dùng nguyên (Direct Reuse):** Có thể gọi trực tiếp từ các màn hình khác mà không cần sửa đổi.
- **B — Tách nhẹ (Minor Refactor):** Cần một chút nỗ lực để tách code vào một file/hàm chung (ví dụ: `ui_components/`, `ui_helpers.c`) để tái sử dụng.
- **C — Chỉ dùng pattern (Pattern Only):** Code gốc quá đặc thù, nhưng ý tưởng/kiến trúc/cách tiếp cận có thể được áp dụng để xây dựng các thành phần tương tự cho các màn hình khác.

---

## KẾT LUẬN NHANH

### Top 10 UI elements đáng tái sử dụng nhất

> Tiêu chí: (1) dùng được cho nhiều screen, (2) ít phụ thuộc domain music, (3) giúp tăng tốc dev 2–3 lần, (4) không phá kiến trúc SimpleXL.

1. **TopBarWithBack** (A) — nền tảng cho mọi screen có điều hướng back
2. **ScrollableListLayout / UiScrollableList** (B) — nền tảng cho Settings/WiFi/BT/Chat history
3. **IconWithTextListItem / UiListItemTwoLine** (B) — list item chuẩn cho Settings, Navigation steps, Chat items
4. **GradientSlider** (B) — dùng cho brightness, temperature, volume, EQ
5. **ImageButton** (B) — icon button cho action nhanh (home tiles, navigation actions)
6. **CheckableImageButton** (B) — toggle state rõ ràng (mute, power, mic)
7. **InfoLabel (Long-scroll label)** (A/C) — label cuộn cho text dài (SSID, message preview, route name)
8. **PressFeedback pattern (LV_STATE_PRESSED)** (A) — chuẩn hóa phản hồi nhấn trên toàn OS
9. **DisabledState pattern (LV_STATE_DISABLED)** (A) — chuẩn hóa UX disable (không “bấm mà không tác dụng”)
10. **StaggeredFadeIn intro pattern** (C) — nâng chất UI, dùng cho transition vào screen

### Top 5 pattern UI nên chuẩn hóa toàn OS

1. **Screen lifecycle chuẩn SimpleXL** (`on_create/on_show/on_hide/on_destroy/on_update`) — mọi screen dùng
2. **Data → UI update split**: `on_update()` cho state ít đổi + `lv_timer` cho update tần suất cao
3. **Stateful controls pattern**: CHECKED / PRESSED / DISABLED dùng LVGL state thay vì tự quản
4. **List-first navigation**: list layout + item template cho Settings/menus
5. **Asset strategy 2 lớp**: built-in assets cho hệ thống + filesystem cho dynamic content

### Sau report này, dev screen mới nhanh hơn 2–3 lần bằng cách nào?

- **Settings/WiFi/BT:** dùng `TopBarWithBack + ScrollableList + ListItemTwoLine` → dựng UI rất nhanh
- **Navigation:** dùng pattern “floating controls trên background” + “data→UI timer”
- **Chatbot:** dùng list layout + label long-scroll + press feedback chuẩn

> Lưu ý: Các mục B (tách nhẹ) nên trích ra theo PR nhỏ, không đụng Music Player.

