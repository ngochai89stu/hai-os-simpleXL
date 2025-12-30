# Thiết Kế Lại Icon System

## Vấn Đề Hiện Tại

### Icon System Cũ
- Sử dụng **emoji** (🎵, 📻, 💾, ⚙️, 💬, 🌐, 📱, 📁, ▶, ⏸, ⏹, ⏮, ⏭)
- **Vấn đề**: Emoji không hiển thị đúng trên ESP32 display
- Emoji phụ thuộc vào font support, có thể không render được
- Không nhất quán giữa các screen

### Các Screen Bị Ảnh Hưởng
1. **screen_home.c**: Menu items sử dụng emoji
2. **screen_music_player.c**: Play/Pause/Prev/Next buttons, album icon
3. **screen_radio.c**: Play/Pause button
4. **screen_sd_card_music.c**: File/folder icons
5. **screen_chat.c**: Welcome message emoji

## Giải Pháp: LVGL Symbol System

### Tạo Icon System Mới

**File mới**: `components/sx_ui/ui_icons.c` và `components/sx_ui/include/ui_icons.h`

**Tính năng**:
- Sử dụng LVGL built-in symbols thay vì emoji
- Icon types enum để quản lý dễ dàng
- Functions: `ui_icon_create()`, `ui_icon_button_create()`, `ui_icon_get_symbol()`
- Hỗ trợ scaling (size parameter)
- Màu sắc có thể customize

### Icon Mapping

| Icon Type | LVGL Symbol | Mô Tả |
|-----------|-------------|-------|
| `UI_ICON_MUSIC_PLAYER` | `LV_SYMBOL_AUDIO` | Music player |
| `UI_ICON_MUSIC_ONLINE` | `LV_SYMBOL_FILE` | Online music |
| `UI_ICON_RADIO` | `LV_SYMBOL_RADIO` | Radio |
| `UI_ICON_SD_CARD` | `LV_SYMBOL_DRIVE` | SD card |
| `UI_ICON_IR_CONTROL` | `LV_SYMBOL_REFRESH` | IR control |
| `UI_ICON_SETTINGS` | `LV_SYMBOL_SETTINGS` | Settings |
| `UI_ICON_CHAT` | `LV_SYMBOL_EDIT` | Chat |
| `UI_ICON_DISPLAY` | `LV_SYMBOL_VIDEO` | Display |
| `UI_ICON_BLUETOOTH` | `LV_SYMBOL_BLUETOOTH` | Bluetooth |
| `UI_ICON_SCREENSAVER` | `LV_SYMBOL_IMAGE` | Screensaver |
| `UI_ICON_EQUALIZER` | `LV_SYMBOL_VOLUME_MID` | Equalizer |
| `UI_ICON_WIFI` | `LV_SYMBOL_WIFI` | WiFi |
| `UI_ICON_VOICE` | `LV_SYMBOL_MIC` | Voice |
| `UI_ICON_ABOUT` | `LV_SYMBOL_INFO` | About |
| `UI_ICON_PLAY` | `LV_SYMBOL_PLAY` | Play |
| `UI_ICON_PAUSE` | `LV_SYMBOL_PAUSE` | Pause |
| `UI_ICON_STOP` | `LV_SYMBOL_STOP` | Stop |
| `UI_ICON_PREV` | `LV_SYMBOL_PREV` | Previous |
| `UI_ICON_NEXT` | `LV_SYMBOL_NEXT` | Next |
| `UI_ICON_VOLUME` | `LV_SYMBOL_VOLUME_MAX` | Volume |
| `UI_ICON_BRIGHTNESS` | `LV_SYMBOL_SUN` | Brightness |
| `UI_ICON_BACK` | `LV_SYMBOL_LEFT` | Back |
| `UI_ICON_MENU` | `LV_SYMBOL_LIST` | Menu |
| `UI_ICON_CLOSE` | `LV_SYMBOL_CLOSE` | Close |
| `UI_ICON_CHECK` | `LV_SYMBOL_OK` | Check |

## Các Thay Đổi Đã Thực Hiện

### 1. screen_home.c
**Trước**:
```c
{"Music Player", "🎵", SCREEN_ID_MUSIC_PLAYER},
lv_label_set_text(icon, menu_item->icon);  // Emoji
```

**Sau**:
```c
{"Music Player", UI_ICON_MUSIC_PLAYER, SCREEN_ID_MUSIC_PLAYER},
lv_obj_t *icon = ui_icon_create(item, menu_item->icon_type, 24);
```

### 2. screen_music_player.c
**Trước**:
```c
lv_label_set_text(s_play_label, "▶");  // Emoji
lv_label_set_text(album_icon, "🎵");   // Emoji
```

**Sau**:
```c
s_play_label = ui_icon_create(s_play_btn, UI_ICON_PLAY, 24);
lv_label_set_text(s_play_label, is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
album_icon = ui_icon_create(s_album_art, UI_ICON_MUSIC_PLAYER, 48);
```

### 3. screen_radio.c
**Trước**:
```c
lv_label_set_text(s_play_label, "▶");  // Emoji
```

**Sau**:
```c
s_play_label = ui_icon_create(s_play_btn, UI_ICON_PLAY, 20);
lv_label_set_text(s_play_label, is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
```

### 4. screen_sd_card_music.c
**Trước**:
```c
const char *icon = entries[i].is_dir ? "📁" : "🎵";  // Emoji
snprintf(display_text, "%s %s", icon, entries[i].name);
```

**Sau**:
```c
ui_icon_type_t icon_type = entries[i].is_dir ? UI_ICON_SD_CARD : UI_ICON_MUSIC_PLAYER;
lv_obj_t *icon = ui_icon_create(file_item, icon_type, 16);
lv_obj_align(label, LV_ALIGN_LEFT_MID, 35, 0);  // Position after icon
```

### 5. screen_chat.c
**Trước**:
```c
lv_label_set_text(welcome_msg, "💬 Start a conversation...");
```

**Sau**:
```c
lv_label_set_text(welcome_msg, "Start a conversation...");  // Removed emoji
```

## Ưu Điểm Của Icon System Mới

### 1. Tương Thích Tốt Hơn
- ✅ LVGL symbols được hỗ trợ native
- ✅ Không phụ thuộc vào font emoji
- ✅ Hiển thị nhất quán trên mọi display

### 2. Dễ Quản Lý
- ✅ Centralized icon system
- ✅ Enum-based icon types
- ✅ Dễ thêm icon mới

### 3. Linh Hoạt
- ✅ Có thể scale icon (size parameter)
- ✅ Có thể customize màu sắc
- ✅ Có thể tạo icon button

### 4. Professional
- ✅ Icon system giống web demo
- ✅ Consistent design language
- ✅ Better UX

## API Usage

### Tạo Icon Đơn Giản
```c
lv_obj_t *icon = ui_icon_create(parent, UI_ICON_MUSIC_PLAYER, 24);
lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 5);
```

### Tạo Icon Button
```c
lv_obj_t *btn = ui_icon_button_create(parent, UI_ICON_PLAY, 40);
lv_obj_add_event_cb(btn, play_btn_cb, LV_EVENT_CLICKED, NULL);
```

### Customize Icon Color
```c
lv_obj_t *icon = ui_icon_create(parent, UI_ICON_SETTINGS, 20);
lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFF), 0);  // White
```

### Update Icon Dynamically
```c
// For play/pause toggle
const char *symbol = is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY;
lv_label_set_text(icon, symbol);
```

## Files Đã Thay Đổi

1. ✅ `components/sx_ui/include/ui_icons.h` - **NEW**: Icon system header
2. ✅ `components/sx_ui/ui_icons.c` - **NEW**: Icon system implementation
3. ✅ `components/sx_ui/CMakeLists.txt` - Added `ui_icons.c`
4. ✅ `components/sx_ui/screens/screen_home.c` - Updated menu items
5. ✅ `components/sx_ui/screens/screen_music_player.c` - Updated controls
6. ✅ `components/sx_ui/screens/screen_radio.c` - Updated play button
7. ✅ `components/sx_ui/screens/screen_sd_card_music.c` - Updated file icons
8. ✅ `components/sx_ui/screens/screen_chat.c` - Removed emoji

## Kết Quả

### Trước
- ❌ Emoji không hiển thị đúng
- ❌ Inconsistent icon usage
- ❌ Phụ thuộc vào font support

### Sau
- ✅ LVGL symbols hiển thị đúng
- ✅ Consistent icon system
- ✅ Professional appearance
- ✅ Dễ maintain và extend

## Next Steps (Optional)

1. **Custom Icon Images**: Nếu cần, có thể thêm custom icon images từ SD card
2. **Icon Themes**: Hỗ trợ different icon themes (light/dark)
3. **Animated Icons**: Thêm animation cho một số icons (loading, etc.)
4. **Icon Font**: Sử dụng icon font thay vì symbols nếu cần nhiều icons hơn










