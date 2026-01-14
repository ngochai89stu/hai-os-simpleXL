# HƯỚNG DẪN COPY LVGL MUSIC DEMO 1-1

> **Mục tiêu:** Copy LVGL music demo 1-1 vào project và tích hợp với `sx_audio_service`

---

## 🚀 CÁCH THỰC HIỆN NHANH

### Bước 1: Chạy script copy

```powershell
cd d:\NEWESP32\hai-os-simplexl
.\scripts\copy_music_demo.ps1
```

Script này sẽ:
- ✅ Tạo directory structure
- ✅ Copy tất cả source files
- ✅ Copy tất cả headers
- ✅ Copy tất cả assets

### Bước 2: Rename functions

Sau khi copy, cần rename tất cả functions:

**Pattern:** `lv_demo_music_*` → `sx_music_player_demo_*`

**Files cần rename:**
1. `sx_music_player_demo.c`
2. `sx_music_player_main.c`
3. `sx_music_player_list.c`
4. `sx_music_player_demo.h`
5. `sx_music_player_main.h`
6. `sx_music_player_list.h`

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

**Defines cần rename:**
- `LV_DEMO_MUSIC_*` → `SX_MUSIC_PLAYER_DEMO_*` (trong một số trường hợp)

**Includes cần update:**
- `#include "lv_demo_music.h"` → `#include "sx_music_player_demo.h"`
- `#include "lv_demo_music_main.h"` → `#include "sx_music_player_main.h"`
- `#include "lv_demo_music_list.h"` → `#include "sx_music_player_list.h"`

### Bước 3: Tạo audio integration layer

Tạo file: `components/sx_ui/screens/music_player_demo/src/sx_music_player_audio.c`

File này sẽ thay thế demo internal audio với `sx_audio_service`.

### Bước 4: Update screen_music_player.c

Modify `screen_music_player.c` để sử dụng custom demo thay vì LVGL demo gốc.

### Bước 5: Update CMakeLists.txt

Thêm music player demo files vào `components/sx_ui/CMakeLists.txt`.

---

## 📋 CHECKLIST CHI TIẾT

### Phase 1: Copy Files ✅
- [x] Script copy đã tạo
- [ ] Chạy script copy
- [ ] Verify files đã copy đúng

### Phase 2: Rename Functions
- [ ] Rename trong `sx_music_player_demo.c`
- [ ] Rename trong `sx_music_player_main.c`
- [ ] Rename trong `sx_music_player_list.c`
- [ ] Rename trong headers
- [ ] Update includes
- [ ] Update defines

### Phase 3: Audio Integration
- [ ] Tạo `sx_music_player_audio.c`
- [ ] Implement play/pause functions
- [ ] Implement prev/next functions
- [ ] Implement volume control
- [ ] Implement state sync

### Phase 4: Screen Integration
- [ ] Update `screen_music_player.c`
- [ ] Add includes
- [ ] Modify `on_create()`
- [ ] Modify `on_update()`
- [ ] Modify `on_destroy()`

### Phase 5: Build System
- [ ] Update `CMakeLists.txt`
- [ ] Add source files
- [ ] Add include directories
- [ ] Add dependencies

### Phase 6: Testing
- [ ] Test compile
- [ ] Test play/pause
- [ ] Test prev/next
- [ ] Test volume
- [ ] Test track switching
- [ ] Test animations

---

## 🔧 CHI TIẾT RENAME

### File: sx_music_player_demo.c

**Find & Replace:**
```
lv_demo_music → sx_music_player_demo
_lv_demo_music → _sx_music_player
lv_demo_music.h → sx_music_player_demo.h
lv_demo_music_main.h → sx_music_player_main.h
lv_demo_music_list.h → sx_music_player_list.h
```

### File: sx_music_player_main.c

**Find & Replace:**
```
_lv_demo_music_main_create → _sx_music_player_main_create
_lv_demo_music_play → _sx_music_player_play
_lv_demo_music_pause → _sx_music_player_pause
_lv_demo_music_resume → _sx_music_player_resume
_lv_demo_music_album_next → _sx_music_player_album_next
lv_demo_music.h → sx_music_player_demo.h
lv_demo_music_main.h → sx_music_player_main.h
lv_demo_music_list.h → sx_music_player_list.h
assets/spectrum → assets/spectrum (giữ nguyên)
```

### File: sx_music_player_list.c

**Find & Replace:**
```
_lv_demo_music_list_create → _sx_music_player_list_create
_lv_demo_music_list_button_check → _sx_music_player_list_button_check
lv_demo_music.h → sx_music_player_demo.h
lv_demo_music_list.h → sx_music_player_list.h
```

---

## ⚠️ LƯU Ý QUAN TRỌNG

1. **Assets path:** Cần update asset paths trong code nếu cần
2. **LVGL dependencies:** Đảm bảo tất cả LVGL features được enable
3. **Audio integration:** Cần implement đầy đủ để sync với audio service
4. **Testing:** Test kỹ từng feature sau khi rename

---

## 🎯 KẾT QUẢ MONG ĐỢI

Sau khi hoàn thành:
- ✅ UI giống hệt LVGL music demo (100%)
- ✅ Tích hợp hoàn toàn với `sx_audio_service`
- ✅ Tất cả controls hoạt động
- ✅ Animations và transitions giống demo

---

**Bắt đầu ngay bằng cách chạy script copy!**











