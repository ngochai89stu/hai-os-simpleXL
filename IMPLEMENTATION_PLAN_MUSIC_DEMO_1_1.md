# IMPLEMENTATION PLAN: Copy LVGL Music Demo 1-1

> **Mục tiêu:** Copy LVGL music demo 1-1 vào project và tích hợp với `sx_audio_service`

---

## 📋 TỔNG QUAN

**Phương án:** Copy toàn bộ demo code → Rename functions → Tích hợp audio service

**Thời gian ước tính:** 2-3 ngày

---

## 🎯 CÁC BƯỚC THỰC HIỆN

### Phase 1: Setup Structure ✅

1. **Tạo directory structure:**
   ```
   components/sx_ui/screens/music_player_demo/
   ├── CMakeLists.txt
   ├── include/
   │   └── sx_music_player_demo.h
   ├── src/
   │   ├── sx_music_player_demo.c (copy từ lv_demo_music.c)
   │   ├── sx_music_player_main.c (copy từ lv_demo_music_main.c)
   │   ├── sx_music_player_list.c (copy từ lv_demo_music_list.c)
   │   └── sx_music_player_audio.c (NEW - audio integration)
   └── assets/ (copy từ demo assets)
   ```

### Phase 2: Copy Files

1. **Copy demo source files:**
   - `lv_demo_music.c` → `sx_music_player_demo.c`
   - `lv_demo_music_main.c` → `sx_music_player_main.c`
   - `lv_demo_music_list.c` → `sx_music_player_list.c`
   - `lv_demo_music.h` → `sx_music_player_demo.h`
   - `lv_demo_music_main.h` → `sx_music_player_main.h`
   - `lv_demo_music_list.h` → `sx_music_player_list.h`

2. **Copy demo assets:**
   - Copy toàn bộ `assets/` folder
   - Giữ nguyên structure

### Phase 3: Rename Functions

**Rename pattern:** `lv_demo_music_*` → `sx_music_player_demo_*`

**Functions cần rename:**
- `lv_demo_music()` → `sx_music_player_demo_create()`
- `_lv_demo_music_main_create()` → `_sx_music_player_main_create()`
- `_lv_demo_music_list_create()` → `_sx_music_player_list_create()`
- `_lv_demo_music_play()` → `_sx_music_player_play()`
- `_lv_demo_music_pause()` → `_sx_music_player_pause()`
- `_lv_demo_music_resume()` → `_sx_music_player_resume()`
- `_lv_demo_music_album_next()` → `_sx_music_player_album_next()`
- `_lv_demo_music_get_title()` → `_sx_music_player_get_title()`
- `_lv_demo_music_get_artist()` → `_sx_music_player_get_artist()`
- `_lv_demo_music_get_genre()` → `_sx_music_player_get_genre()`
- `_lv_demo_music_get_track_length()` → `_sx_music_player_get_track_length()`
- `_lv_demo_music_list_button_check()` → `_sx_music_player_list_button_check()`

### Phase 4: Audio Integration

**Tạo file:** `sx_music_player_audio.c`

**Chức năng:**
- Replace demo internal audio với `sx_audio_service`
- Sync state: UI ↔ Audio Service
- Update UI khi audio state thay đổi

### Phase 5: Screen Integration

**Modify:** `components/sx_ui/screens/screen_music_player.c`

**Thay đổi:**
- Sử dụng custom demo thay vì LVGL demo gốc
- Integrate với audio service
- Update state trong `on_update()`

### Phase 6: CMakeLists.txt

**Update:** `components/sx_ui/CMakeLists.txt`

**Thêm:**
- Music player demo source files
- Include directories
- Dependencies

---

## 🔧 CHI TIẾT IMPLEMENTATION

### Step 1: Tạo Directory Structure

```bash
mkdir -p components/sx_ui/screens/music_player_demo/include
mkdir -p components/sx_ui/screens/music_player_demo/src
mkdir -p components/sx_ui/screens/music_player_demo/assets
```

### Step 2: Copy Files

**Source files:**
```bash
# Copy từ managed_components
cp managed_components/lvgl__lvgl/demos/music/lv_demo_music.c \
   components/sx_ui/screens/music_player_demo/src/sx_music_player_demo.c

cp managed_components/lvgl__lvgl/demos/music/lv_demo_music_main.c \
   components/sx_ui/screens/music_player_demo/src/sx_music_player_main.c

cp managed_components/lvgl__lvgl/demos/music/lv_demo_music_list.c \
   components/sx_ui/screens/music_player_demo/src/sx_music_player_list.c

# Headers
cp managed_components/lvgl__lvgl/demos/music/lv_demo_music.h \
   components/sx_ui/screens/music_player_demo/include/sx_music_player_demo.h

cp managed_components/lvgl__lvgl/demos/music/lv_demo_music_main.h \
   components/sx_ui/screens/music_player_demo/include/sx_music_player_main.h

cp managed_components/lvgl__lvgl/demos/music/lv_demo_music_list.h \
   components/sx_ui/screens/music_player_demo/include/sx_music_player_list.h
```

**Assets:**
```bash
cp -r managed_components/lvgl__lvgl/demos/music/assets/* \
      components/sx_ui/screens/music_player_demo/assets/
```

### Step 3: Rename Functions

**Sử dụng find & replace:**
- `lv_demo_music` → `sx_music_player_demo`
- `_lv_demo_music` → `_sx_music_player`
- `LV_DEMO_MUSIC` → `SX_MUSIC_PLAYER_DEMO` (trong defines)

### Step 4: Audio Integration Layer

**File:** `sx_music_player_audio.c`

```c
#include "sx_audio_service.h"
#include "sx_playlist_manager.h"

// Replace demo play/pause với sx_audio_service
void _sx_music_player_play(uint32_t id) {
    // Load track từ playlist
    sx_playlist_play_track(id);
    sx_audio_resume();
}

void _sx_music_player_pause(void) {
    sx_audio_pause();
}

void _sx_music_player_resume(void) {
    sx_audio_resume();
}

void _sx_music_player_album_next(bool next) {
    if (next) {
        sx_playlist_next();
    } else {
        sx_playlist_previous();
    }
}
```

### Step 5: Update Screen

**File:** `screen_music_player.c`

```c
#include "sx_music_player_demo.h"

static void on_create(void) {
    // ...
    
#if CONFIG_UI_USE_LVGL_MUSIC_DEMO && LV_USE_DEMO_MUSIC
    // Use custom demo
    s_demo_screen = lv_obj_create(NULL);
    lv_obj_set_size(s_demo_screen, 320, 480);
    lv_scr_load(s_demo_screen);
    
    // Call custom demo
    sx_music_player_demo_create();
#endif
}
```

---

## ✅ CHECKLIST

- [ ] Tạo directory structure
- [ ] Copy demo source files
- [ ] Copy demo headers
- [ ] Copy demo assets
- [ ] Rename functions trong tất cả files
- [ ] Update includes trong files
- [ ] Tạo audio integration layer
- [ ] Update screen_music_player.c
- [ ] Update CMakeLists.txt
- [ ] Test compile
- [ ] Test play/pause
- [ ] Test prev/next
- [ ] Test volume
- [ ] Test track switching
- [ ] Test animations

---

## 🎯 KẾT QUẢ MONG ĐỢI

Sau khi hoàn thành:
- ✅ UI giống hệt LVGL music demo (100%)
- ✅ Tích hợp hoàn toàn với `sx_audio_service`
- ✅ Tất cả controls hoạt động (play/pause/prev/next/volume)
- ✅ Animations và transitions giống demo
- ✅ Spectrum visualization hoạt động

---

**Bắt đầu implementation ngay!**











