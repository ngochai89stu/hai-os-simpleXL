# UI LAYOUT TEMPLATES RÚT RA TỪ MUSIC DEMO

> **Mục tiêu:** Chuẩn hóa các mẫu layout hữu ích từ Music Demo thành các template có thể áp dụng cho nhiều màn hình khác nhau trong OS.

---

## Template 1: Header + Content (Base Screen Template)

### Mô tả layout
- Một top bar cố định (chứa title + back button)
- Một vùng content bên dưới, chiếm phần còn lại của màn hình

### Mapping từ Music Screen
- **Path:** `components/sx_ui/screens/screen_music_player.c`
- **Code:**
  - `s_top_bar = screen_common_create_top_bar_with_back(container, "Music Player");`
  - `s_content = lv_obj_create(container); ... lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);`

### Screen nào nên dùng
- Settings
- About
- Diagnostics
- Network Settings
- Bluetooth Settings
- AC Control

### Ghi chú
- Đây là template cơ bản nhất, phù hợp với kiến trúc router hiện tại.
- Không cần trích thành code mới (đã có `screen_common`), chỉ cần chuẩn hóa cách dùng.

---

## Template 2: Fullscreen Background + Floating Controls

### Mô tả layout
- Background toàn màn hình (có thể là image layer hoặc gradient)
- Các controls nổi phía trên (album art, buttons, info)

### Mapping từ Music Screen
- **Path:** `components/sx_ui/screens/screen_music_player.c`
- **Code:** `add_wave_decorations(s_content);` (wave top/bottom)

### Screen nào nên dùng
- Home (hero section + quick actions)
- Navigation (map background + floating controls)
- Radio (background + station controls)

### Đề xuất chuẩn hóa
- Tạo helper `ui_background_layer_add(parent, assets...)`
- Background assets nên thuộc theme, không thuộc music.

---

## Template 3: Vertical Centered Grid (Dashboard-like)

### Mô tả layout
- Các widget xếp theo chiều dọc
- Căn giữa và chia hàng rõ ràng (grid rows)

### Mapping từ Music Screen
- **Path:** `components/sx_ui/screens/screen_music_player.c`
- **Code:** Grid layout cho `s_content`

### Screen nào nên dùng
- Audio Effects (EQ controls)
- AC Control (temp + fan + mode)
- Voice Settings

### Ghi chú
- Grid giúp layout ổn định hơn flex khi có nhiều widget khác kích thước.

---

## Template 4: List-based Screen (Scrollable List)

### Mô tả layout
- Một danh sách scroll dọc
- Mỗi item có icon + text + optional right label

### Mapping từ Music Screen
- **Path:** `components/sx_ui/screens/screen_music_player_list.c`
- **Code:** `create_playlist_view()` + `add_list_button()`

### Screen nào nên dùng
- Settings (menu list)
- WiFi scan list
- Bluetooth device list
- Chatbot history
- Navigation steps list

### Đề xuất chuẩn hóa
- Trích `UiScrollableList` và `UiListItemTwoLine`

---

## Template 5: Modal/Dialog Pattern (Pattern-only)

### Mô tả layout
- Một overlay modal (semi-transparent)
- Dialog box nổi ở giữa

### Mapping từ Music Demo
- Music demo hiện chưa có dialog rõ ràng, nhưng các animation fade/opacity có thể dùng làm pattern.

### Screen nào nên dùng
- Confirm actions (factory reset)
- Permission prompts
- OTA update confirmation

### Đề xuất
- Dùng pattern `fade_in/fade_out` và `LV_OBJ_FLAG_CLICKABLE` overlay.

---

## Kết luận

Top template nên chuẩn hóa sớm:
1) Header + Content
2) List-based Screen
3) Fullscreen Background + Floating Controls







