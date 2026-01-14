# HƯỚNG DẪN CHI TIẾT: COPY LVGL MUSIC DEMO 1-1

> **Mục tiêu:** Copy toàn bộ LVGL music demo vào project và tích hợp với `sx_audio_service` để có UI giống hệt demo

---

## 📋 TỔNG QUAN

**Ý tưởng:** Thay vì tự code lại UI từ đầu (rất khó và dễ miss details), chúng ta sẽ:
1. Copy toàn bộ code của LVGL demo
2. Rename functions để tránh conflict
3. Thay thế phần audio handling bằng `sx_audio_service`
4. Tích hợp vào screen hiện tại

**Kết quả:** UI giống hệt demo (100%) + tích hợp với audio service thực tế

---

## 🎯 CÁC BƯỚC THỰC HIỆN

### BƯỚC 1: Copy Files (Tự động)

#### 1.1 Chạy script copy

```powershell
# Mở PowerShell trong thư mục project
cd d:\NEWESP32\hai-os-simplexl

# Chạy script
.\scripts\copy_music_demo.ps1
```

**Script sẽ làm gì:**
- Tạo thư mục: `components/sx_ui/screens/music_player_demo/`
- Copy source files từ `managed_components/lvgl__lvgl/demos/music/`
- Copy headers
- Copy assets (images, spectrum data)

**Kết quả sau khi chạy:**
```
components/sx_ui/screens/music_player_demo/
├── include/
│   ├── sx_music_player_demo.h      (copy từ lv_demo_music.h)
│   ├── sx_music_player_main.h      (copy từ lv_demo_music_main.h)
│   └── sx_music_player_list.h      (copy từ lv_demo_music_list.h)
├── src/
│   ├── sx_music_player_demo.c      (copy từ lv_demo_music.c)
│   ├── sx_music_player_main.c      (copy từ lv_demo_music_main.c)
│   └── sx_music_player_list.c      (copy từ lv_demo_music_list.c)
└── assets/
    └── [tất cả assets từ demo]
```

#### 1.2 Verify files đã copy

Kiểm tra xem files đã được copy đúng chưa:
- `components/sx_ui/screens/music_player_demo/src/` có 3 file .c
- `components/sx_ui/screens/music_player_demo/include/` có 3 file .h
- `components/sx_ui/screens/music_player_demo/assets/` có assets

---

### BƯỚC 2: Rename Functions (Quan trọng nhất!)

**Tại sao cần rename?**
- Tránh conflict với LVGL demo gốc (nếu có enable)
- Dễ phân biệt custom version
- Có thể customize mà không ảnh hưởng demo gốc

#### 2.1 Rename trong sx_music_player_demo.c

**File:** `components/sx_ui/screens/music_player_demo/src/sx_music_player_demo.c`

**Tìm và thay thế:**

1. **Function names:**
   ```
   lv_demo_music → sx_music_player_demo_create
   _lv_demo_music_get_title → _sx_music_player_get_title
   _lv_demo_music_get_artist → _sx_music_player_get_artist
   _lv_demo_music_get_genre → _sx_music_player_get_genre
   _lv_demo_music_get_track_length → _sx_music_player_get_track_length
   ```

2. **Includes:**
   ```
   #include "lv_demo_music.h" → #include "sx_music_player_demo.h"
   #include "lv_demo_music_main.h" → #include "sx_music_player_main.h"
   #include "lv_demo_music_list.h" → #include "sx_music_player_list.h"
   ```

3. **Function calls:**
   ```
   _lv_demo_music_main_create → _sx_music_player_main_create
   _lv_demo_music_list_create → _sx_music_player_list_create
   _lv_demo_music_play → _sx_music_player_play
   _lv_demo_music_pause → _sx_music_player_pause
   _lv_demo_music_album_next → _sx_music_player_album_next
   ```

**Ví dụ sau khi rename:**

```c
// TRƯỚC (từ LVGL demo):
void lv_demo_music(void)
{
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x343247), 0);
    list = _lv_demo_music_list_create(lv_screen_active());
    ctrl = _lv_demo_music_main_create(lv_screen_active());
}

// SAU (custom version):
void sx_music_player_demo_create(void)
{
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x343247), 0);
    list = _sx_music_player_list_create(lv_screen_active());
    ctrl = _sx_music_player_main_create(lv_screen_active());
}
```

#### 2.2 Rename trong sx_music_player_main.c

**File:** `components/sx_ui/screens/music_player_demo/src/sx_music_player_main.c`

**Tìm và thay thế:**

1. **Function definition:**
   ```
   _lv_demo_music_main_create → _sx_music_player_main_create
   ```

2. **Function calls:**
   ```
   _lv_demo_music_play → _sx_music_player_play
   _lv_demo_music_pause → _sx_music_player_pause
   _lv_demo_music_resume → _sx_music_player_resume
   _lv_demo_music_album_next → _sx_music_player_album_next
   _lv_demo_music_get_title → _sx_music_player_get_title
   _lv_demo_music_get_artist → _sx_music_player_get_artist
   _lv_demo_music_get_genre → _sx_music_player_get_genre
   _lv_demo_music_get_track_length → _sx_music_player_get_track_length
   ```

3. **Includes:**
   ```
   #include "lv_demo_music_main.h" → #include "sx_music_player_main.h"
   #include "lv_demo_music_list.h" → #include "sx_music_player_list.h"
   #include "lv_demo_music.h" → #include "sx_music_player_demo.h"
   ```

#### 2.3 Rename trong sx_music_player_list.c

**File:** `components/sx_ui/screens/music_player_demo/src/sx_music_player_list.c`

**Tìm và thay thế:**

1. **Function definition:**
   ```
   _lv_demo_music_list_create → _sx_music_player_list_create
   _lv_demo_music_list_button_check → _sx_music_player_list_button_check
   ```

2. **Function calls:**
   ```
   _lv_demo_music_get_title → _sx_music_player_get_title
   _lv_demo_music_get_artist → _sx_music_player_get_artist
   _lv_demo_music_get_genre → _sx_music_player_get_genre
   _lv_demo_music_get_track_length → _sx_music_player_get_track_length
   _lv_demo_music_play → _sx_music_player_play
   ```

3. **Includes:**
   ```
   #include "lv_demo_music_list.h" → #include "sx_music_player_list.h"
   #include "lv_demo_music.h" → #include "sx_music_player_demo.h"
   ```

#### 2.4 Rename trong Headers

**File:** `components/sx_ui/screens/music_player_demo/include/sx_music_player_demo.h`

**Tìm và thay thế:**

1. **Header guard:**
   ```
   #ifndef LV_DEMO_MUSIC_H → #ifndef SX_MUSIC_PLAYER_DEMO_H
   #define LV_DEMO_MUSIC_H → #define SX_MUSIC_PLAYER_DEMO_H
   #endif /*LV_DEMO_MUSIC_H*/ → #endif /*SX_MUSIC_PLAYER_DEMO_H*/
   ```

2. **Function declarations:**
   ```
   void lv_demo_music(void); → void sx_music_player_demo_create(void);
   const char * _lv_demo_music_get_title(...) → const char * _sx_music_player_get_title(...)
   ```

**Tương tự cho các headers khác.**

---

### BƯỚC 3: Tạo Audio Integration Layer

**Mục đích:** Thay thế demo internal audio với `sx_audio_service`

#### 3.1 Tạo file mới

**File:** `components/sx_ui/screens/music_player_demo/src/sx_music_player_audio.c`

```c
#include "sx_music_player_audio.h"
#include "sx_audio_service.h"
#include "sx_playlist_manager.h"
#include <esp_log.h>

static const char *TAG = "sx_music_player_audio";

// Replace demo play function với sx_audio_service
void _sx_music_player_play(uint32_t track_id)
{
    ESP_LOGI(TAG, "Play track: %lu", (unsigned long)track_id);
    
    // Load track từ playlist
    esp_err_t ret = sx_playlist_play_track(track_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to play track %lu: %s", 
                 (unsigned long)track_id, esp_err_to_name(ret));
        return;
    }
    
    // Resume audio
    ret = sx_audio_resume();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to resume audio: %s", esp_err_to_name(ret));
    }
}

void _sx_music_player_pause(void)
{
    ESP_LOGI(TAG, "Pause");
    esp_err_t ret = sx_audio_pause();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to pause audio: %s", esp_err_to_name(ret));
    }
}

void _sx_music_player_resume(void)
{
    ESP_LOGI(TAG, "Resume");
    esp_err_t ret = sx_audio_resume();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to resume audio: %s", esp_err_to_name(ret));
    }
}

void _sx_music_player_album_next(bool next)
{
    ESP_LOGI(TAG, "Album next: %s", next ? "next" : "prev");
    
    esp_err_t ret;
    if (next) {
        ret = sx_playlist_next();
    } else {
        ret = sx_playlist_previous();
    }
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to change track: %s", esp_err_to_name(ret));
    }
}

// Get track info từ playlist (thay vì hardcode arrays)
const char * _sx_music_player_get_title(uint32_t track_id)
{
    // TODO: Get từ playlist manager
    // Tạm thời return NULL hoặc default
    return NULL;
}

const char * _sx_music_player_get_artist(uint32_t track_id)
{
    // TODO: Get từ playlist manager
    return NULL;
}

const char * _sx_music_player_get_genre(uint32_t track_id)
{
    // TODO: Get từ playlist manager
    return NULL;
}

uint32_t _sx_music_player_get_track_length(uint32_t track_id)
{
    // TODO: Get từ audio service hoặc playlist
    return 0;
}
```

#### 3.2 Tạo header

**File:** `components/sx_ui/screens/music_player_demo/include/sx_music_player_audio.h`

```c
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Audio control functions (thay thế demo internal audio)
void _sx_music_player_play(uint32_t track_id);
void _sx_music_player_pause(void);
void _sx_music_player_resume(void);
void _sx_music_player_album_next(bool next);

// Track info functions (get từ playlist thay vì hardcode)
const char * _sx_music_player_get_title(uint32_t track_id);
const char * _sx_music_player_get_artist(uint32_t track_id);
const char * _sx_music_player_get_genre(uint32_t track_id);
uint32_t _sx_music_player_get_track_length(uint32_t track_id);

#ifdef __cplusplus
}
#endif
```

#### 3.3 Update demo code để sử dụng audio layer

**Trong `sx_music_player_main.c`:**

Tìm các chỗ gọi `_lv_demo_music_play()`, `_lv_demo_music_pause()`, etc. và đảm bảo chúng đã được rename thành `_sx_music_player_play()`, etc.

**Trong `sx_music_player_demo.c`:**

Thay thế hardcode arrays bằng functions từ audio layer:

```c
// TRƯỚC (hardcode):
static const char * title_list[] = {
    "Waiting for true love",
    "Need a Better Future",
    // ...
};

const char * _lv_demo_music_get_title(uint32_t track_id)
{
    if(track_id >= sizeof(title_list) / sizeof(title_list[0])) return NULL;
    return title_list[track_id];
}

// SAU (từ playlist):
const char * _sx_music_player_get_title(uint32_t track_id)
{
    return _sx_music_player_get_title(track_id);  // Call audio layer
}
```

---

### BƯỚC 4: Update Screen Integration

#### 4.1 Modify screen_music_player.c

**File:** `components/sx_ui/screens/screen_music_player.c`

**Thêm include:**
```c
#if CONFIG_UI_USE_LVGL_MUSIC_DEMO && LV_USE_DEMO_MUSIC
#include "sx_music_player_demo.h"
#endif
```

**Modify on_create():**
```c
static void on_create(void) {
    ESP_LOGI(TAG, "Music Player screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
#if CONFIG_UI_USE_LVGL_MUSIC_DEMO && LV_USE_DEMO_MUSIC
    // Use custom demo (not LVGL demo)
    ESP_LOGI(TAG, "[UI] Using custom Music Player Demo");
    
    // Create demo screen
    s_demo_screen = lv_obj_create(NULL);
    if (s_demo_screen == NULL) {
        ESP_LOGE(TAG, "Failed to create demo screen");
        return;
    }
    
    // Set screen size
    lv_obj_set_size(s_demo_screen, 320, 480);
    
    // Load as active screen
    lv_scr_load(s_demo_screen);
    
    // Call custom demo (thay vì lv_demo_music())
    sx_music_player_demo_create();
#else
    // Custom UI mode (original implementation)
    // ... existing code ...
#endif
}
```

**Modify on_update():**
```c
static void on_update(const sx_state_t *state) {
#if CONFIG_UI_USE_LVGL_MUSIC_DEMO && LV_USE_DEMO_MUSIC
    // Demo tự quản lý state, không cần update manual
    // (hoặc có thể sync state nếu cần)
#else
    // Custom UI mode update logic
    // ... existing code ...
#endif
}
```

**Modify on_destroy():**
```c
static void on_destroy(void) {
    ESP_LOGI(TAG, "Music Player screen onDestroy");
    
#if CONFIG_UI_USE_LVGL_MUSIC_DEMO && LV_USE_DEMO_MUSIC
    // Restore default screen
    if (s_demo_screen != NULL) {
        lv_obj_t *router_screen = lv_obj_get_parent(ui_router_get_container());
        if (router_screen != NULL) {
            lv_scr_load(router_screen);
        }
        lv_obj_del(s_demo_screen);
        s_demo_screen = NULL;
    }
#else
    // Custom UI mode cleanup
    // ... existing code ...
#endif
}
```

---

### BƯỚC 5: Update CMakeLists.txt

#### 5.1 Update sx_ui/CMakeLists.txt

**File:** `components/sx_ui/CMakeLists.txt`

**Thêm source files:**
```cmake
idf_component_register(
    SRCS
        # ... existing files ...
        "screens/screen_music_player.c"
        # Thêm music player demo files
        "screens/music_player_demo/src/sx_music_player_demo.c"
        "screens/music_player_demo/src/sx_music_player_main.c"
        "screens/music_player_demo/src/sx_music_player_list.c"
        "screens/music_player_demo/src/sx_music_player_audio.c"
    INCLUDE_DIRS
        "include"
        "ui_verify"
        "screens/music_player_demo/include"  # Thêm include directory
    REQUIRES
        sx_core
        sx_platform
        sx_assets
        sx_services  # Cần cho audio service
        esp_lvgl_port
)
```

---

### BƯỚC 6: Update Asset Paths (Nếu cần)

**Trong `sx_music_player_main.c`:**

Kiểm tra các includes assets:
```c
#include "assets/spectrum_1.h"
#include "assets/spectrum_2.h"
#include "assets/spectrum_3.h"
```

**Cần đảm bảo:** Asset paths đúng với structure mới:
- Assets nằm ở: `components/sx_ui/screens/music_player_demo/assets/`
- Include path: `assets/spectrum_1.h` (relative từ src/)

**Nếu cần, update include paths:**
```c
// Nếu assets ở cùng level với src/
#include "../assets/spectrum_1.h"

// Hoặc nếu assets ở trong src/assets/
#include "assets/spectrum_1.h"
```

---

## ✅ CHECKLIST TỔNG HỢP

### Phase 1: Copy ✅
- [ ] Chạy script copy
- [ ] Verify files đã copy đúng

### Phase 2: Rename
- [ ] Rename trong `sx_music_player_demo.c`
- [ ] Rename trong `sx_music_player_main.c`
- [ ] Rename trong `sx_music_player_list.c`
- [ ] Rename trong `sx_music_player_demo.h`
- [ ] Rename trong `sx_music_player_main.h`
- [ ] Rename trong `sx_music_player_list.h`
- [ ] Update tất cả includes

### Phase 3: Audio Integration
- [ ] Tạo `sx_music_player_audio.c`
- [ ] Tạo `sx_music_player_audio.h`
- [ ] Implement play/pause/resume
- [ ] Implement prev/next
- [ ] Implement get track info functions
- [ ] Update demo code để sử dụng audio layer

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

## 🎯 KẾT QUẢ MONG ĐỢI

Sau khi hoàn thành:
- ✅ UI giống hệt LVGL music demo (100%)
- ✅ Tích hợp hoàn toàn với `sx_audio_service`
- ✅ Tất cả controls hoạt động (play/pause/prev/next/volume)
- ✅ Animations và transitions giống demo
- ✅ Spectrum visualization hoạt động

---

## ⚠️ LƯU Ý QUAN TRỌNG

1. **Rename phải đầy đủ:** Nếu miss một function, sẽ có compile error
2. **Asset paths:** Đảm bảo asset paths đúng
3. **Audio integration:** Cần implement đầy đủ để sync với audio service
4. **Testing:** Test kỹ từng feature sau mỗi bước

---

## 🚀 BẮT ĐẦU NGAY

1. **Chạy script copy:**
   ```powershell
   .\scripts\copy_music_demo.ps1
   ```

2. **Rename functions:** Sử dụng Find & Replace trong IDE

3. **Tạo audio layer:** Copy code mẫu ở trên

4. **Update screen:** Modify `screen_music_player.c`

5. **Update CMakeLists:** Thêm files vào build

6. **Test:** Compile và test từng feature

---

**Chúc bạn thành công! 🎉**











