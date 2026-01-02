# ROADMAP: HYBRID MUSIC SCREEN GIỐNG LVGL DEMO 1-1

> **Mục tiêu:** Tạo music screen giống LVGL Demo 1-1, giữ SimpleXL architecture
> **Timeline:** 6 tuần
> **Status:** 🟡 Planning

---

## 📋 TỔNG QUAN

### Mục tiêu:
- ✅ **UI giống LVGL Demo 1-1** - Visual fidelity 100%
- ✅ **Architecture giữ SimpleXL** - Event-driven, service layer
- ✅ **Integration giữ SimpleXL** - sx_audio_service, sx_playlist_manager
- ✅ **Best of both worlds**

### Scope:
- ✅ Copy assets từ LVGL Demo (45 files)
- ✅ Copy code selectively (spectrum, playlist, animations)
- ✅ Extend services (audio, playlist)
- ✅ Implement UI components mới
- ✅ Add animations và visual effects
- ✅ Integration testing

---

## 🗓️ TIMELINE

```
Week 1: Setup & Assets
Week 2-3: Core Features (Spectrum, Time, Slider, Playlist)
Week 4-5: Visual Enhancements (Animations, Typography)
Week 6: Polish & Testing
```

---

## 📅 PHASE 1: SETUP & ASSETS (Week 1)

### **Day 1-2: Copy Assets** 📦

#### Task 1.1: Run Copy Script
- [ ] **Action:** Run `scripts/copy_lvgl_demo_assets.ps1`
- [ ] **Verify:** Check 45 files copied to `components/sx_ui/assets/`
- [ ] **Time:** 5 phút

#### Task 1.2: Create Asset Header
- [ ] **File:** `components/sx_ui/assets/sx_ui_assets.h`
- [ ] **Content:** LV_IMAGE_DECLARE cho tất cả 45 assets
- [ ] **Time:** 30 phút

**Code Template:**
```c
#pragma once

#include "lvgl.h"

// Button images
LV_IMAGE_DECLARE(img_lv_demo_music_btn_play);
LV_IMAGE_DECLARE(img_lv_demo_music_btn_pause);
LV_IMAGE_DECLARE(img_lv_demo_music_btn_prev);
LV_IMAGE_DECLARE(img_lv_demo_music_btn_next);
LV_IMAGE_DECLARE(img_lv_demo_music_btn_loop);
LV_IMAGE_DECLARE(img_lv_demo_music_btn_rnd);
LV_IMAGE_DECLARE(img_lv_demo_music_btn_list_play);
LV_IMAGE_DECLARE(img_lv_demo_music_btn_list_pause);

// Icon images
LV_IMAGE_DECLARE(img_lv_demo_music_icon_1);
LV_IMAGE_DECLARE(img_lv_demo_music_icon_2);
LV_IMAGE_DECLARE(img_lv_demo_music_icon_3);
LV_IMAGE_DECLARE(img_lv_demo_music_icon_4);

// Decorative elements
LV_IMAGE_DECLARE(img_lv_demo_music_wave_top);
LV_IMAGE_DECLARE(img_lv_demo_music_wave_bottom);
LV_IMAGE_DECLARE(img_lv_demo_music_corner_left);
LV_IMAGE_DECLARE(img_lv_demo_music_corner_right);
LV_IMAGE_DECLARE(img_lv_demo_music_list_border);
LV_IMAGE_DECLARE(img_lv_demo_music_logo);
LV_IMAGE_DECLARE(img_lv_demo_music_slider_knob);

// Album covers
LV_IMAGE_DECLARE(img_lv_demo_music_cover_1);
LV_IMAGE_DECLARE(img_lv_demo_music_cover_2);
LV_IMAGE_DECLARE(img_lv_demo_music_cover_3);
```

#### Task 1.3: Update CMakeLists.txt
- [ ] **File:** `components/sx_ui/CMakeLists.txt`
- [ ] **Action:** Add assets directory to build
- [ ] **Time:** 15 phút

**Code:**
```cmake
# Add assets
file(GLOB ASSET_FILES "assets/*.c")
target_sources(${COMPONENT_LIB} PRIVATE ${ASSET_FILES})
```

#### Task 1.4: Test Build
- [ ] **Action:** Build project, verify no errors
- [ ] **Verify:** Assets compile successfully
- [ ] **Time:** 10 phút

**Milestone:** ✅ Assets copied và build thành công

---

### **Day 3-4: Enable Fonts** 🔤

#### Task 1.5: Enable Montserrat Fonts
- [ ] **File:** `sdkconfig` hoặc Kconfig
- [ ] **Action:** Enable 4 font sizes
- [ ] **Time:** 30 phút

**Config:**
```
CONFIG_LV_FONT_MONTSERRAT_12=y
CONFIG_LV_FONT_MONTSERRAT_16=y
CONFIG_LV_FONT_MONTSERRAT_22=y
CONFIG_LV_FONT_MONTSERRAT_32=y
```

#### Task 1.6: Create Font Helper Functions
- [ ] **File:** `components/sx_ui/screens/screen_music_player.c`
- [ ] **Action:** Add font helper functions
- [ ] **Time:** 30 phút

**Code:**
```c
static const lv_font_t *get_font_small(void) {
#if LV_FONT_MONTSERRAT_12
    return &lv_font_montserrat_12;
#else
    return &lv_font_montserrat_14;
#endif
}

static const lv_font_t *get_font_medium(void) {
#if LV_FONT_MONTSERRAT_16
    return &lv_font_montserrat_16;
#else
    return &lv_font_montserrat_14;
#endif
}

static const lv_font_t *get_font_large(void) {
#if LV_FONT_MONTSERRAT_22
    return &lv_font_montserrat_22;
#else
    return &lv_font_montserrat_16;
#endif
}
```

#### Task 1.7: Test Fonts
- [ ] **Action:** Build và test fonts hiển thị đúng
- [ ] **Time:** 15 phút

**Milestone:** ✅ Fonts enabled và tested

---

### **Day 5: Extend Audio Service** 🔊

#### Task 1.8: Add Position/Duration Functions
- [ ] **File:** `components/sx_services/include/sx_audio_service.h`
- [ ] **Action:** Add function declarations
- [ ] **Time:** 30 phút

**Code:**
```c
// Position và Duration
uint32_t sx_audio_get_position(void);      // Current position (seconds)
uint32_t sx_audio_get_duration(void);      // Total duration (seconds)
esp_err_t sx_audio_seek(uint32_t position); // Seek to position (seconds)
```

#### Task 1.9: Implement Position/Duration
- [ ] **File:** `components/sx_services/sx_audio_service.c`
- [ ] **Action:** Implement functions
- [ ] **Dependencies:** Audio decoder support
- [ ] **Time:** 4 giờ

**Implementation Notes:**
- `sx_audio_get_position()` - Get từ audio decoder state
- `sx_audio_get_duration()` - Get từ file metadata hoặc decoder
- `sx_audio_seek()` - Seek trong decoder (có thể cần decoder support)

#### Task 1.10: Add Spectrum Function
- [ ] **File:** `components/sx_services/include/sx_audio_service.h`
- [ ] **Action:** Add spectrum function declaration
- [ ] **Time:** 15 phút

**Code:**
```c
// Spectrum/FFT Data
esp_err_t sx_audio_get_spectrum(uint16_t *bands, size_t band_count);
// Returns: bands[0] = Bass, bands[1] = Mid-low, bands[2] = Mid-high, bands[3] = High
// Range: 0-255 for each band
```

#### Task 1.11: Implement Spectrum Function
- [ ] **File:** `components/sx_services/sx_audio_service.c`
- [ ] **Action:** Implement FFT processing
- [ ] **Dependencies:** FFT library (ESP-DSP hoặc custom)
- [ ] **Time:** 6 giờ

**Implementation Notes:**
- Process audio samples với FFT
- Extract 4 frequency bands
- Return normalized values (0-255)

**Milestone:** ✅ Audio service extended

---

### **Day 6-7: Extend Playlist Manager** 📋

#### Task 1.12: Add Track Info Functions
- [ ] **File:** `components/sx_services/include/sx_playlist_manager.h`
- [ ] **Action:** Add function declarations
- [ ] **Time:** 30 phút

**Code:**
```c
// Track Info
const char* sx_playlist_get_title(size_t track_index);
const char* sx_playlist_get_artist(size_t track_index);
const char* sx_playlist_get_genre(size_t track_index);
uint32_t sx_playlist_get_duration(size_t track_index);  // seconds
size_t sx_playlist_get_count(void);

// Album Art
esp_err_t sx_playlist_get_cover_path(size_t track_index, char *path, size_t path_len);
```

#### Task 1.13: Implement Track Info Functions
- [ ] **File:** `components/sx_services/sx_playlist_manager.c`
- [ ] **Action:** Implement functions
- [ ] **Dependencies:** Metadata parsing (ID3 tags hoặc filename)
- [ ] **Time:** 8 giờ

**Implementation Notes:**
- Parse metadata từ file (ID3, Vorbis comments, etc.)
- Fallback to filename parsing nếu không có metadata
- Find album art trong cùng folder với track

**Milestone:** ✅ Playlist manager extended

---

## 📅 PHASE 2: CORE FEATURES (Week 2-3)

### **Week 2: Spectrum & Time Display** 📊

### **Day 8-10: Spectrum Visualization**

#### Task 2.1: Create Spectrum File
- [ ] **File:** `components/sx_ui/screens/screen_music_player_spectrum.c`
- [ ] **Action:** Create new file
- [ ] **Time:** 15 phút

#### Task 2.2: Copy Spectrum Constants
- [ ] **File:** `screen_music_player_spectrum.c`
- [ ] **Source:** `managed_components/lvgl__lvgl/demos/music/lv_demo_music_main.c`
- [ ] **Action:** Copy constants
- [ ] **Time:** 30 phút

**Code:**
```c
#define BAR_CNT             20
#define DEG_STEP            (180/BAR_CNT)
#define BAND_CNT            4
#define BAR_PER_BAND_CNT    (BAR_CNT / BAND_CNT)
#define BAR_COLOR1          lv_color_hex(0xe9dbfc)
#define BAR_COLOR2          lv_color_hex(0x6f8af6)
#define BAR_COLOR3          lv_color_hex(0xffffff)
```

#### Task 2.3: Copy Helper Functions
- [ ] **Action:** Copy `get_cos()`, `get_sin()` functions
- [ ] **Time:** 30 phút

**Code:**
```c
static int32_t get_cos(int32_t deg, int32_t a) {
    int32_t res = (lv_trigo_sin(deg + 90) * a) >> LV_TRIGO_SHIFT;
    return res;
}

static int32_t get_sin(int32_t deg, int32_t a) {
    int32_t res = (lv_trigo_sin(deg) * a) >> LV_TRIGO_SHIFT;
    return res;
}
```

#### Task 2.4: Copy Spectrum Draw Function
- [ ] **Action:** Copy `spectrum_draw_event_cb()` (~150 lines)
- [ ] **Modify:** Integrate với `sx_audio_get_spectrum()`
- [ ] **Time:** 4 giờ

**Key Changes:**
```c
// Original (LVGL Demo):
uint32_t ampl_main = spectrum[spectrum_i][s];

// Modified (SimpleXL):
uint16_t bands[4];
sx_audio_get_spectrum(bands, 4);
uint32_t ampl_main = bands[s];
```

#### Task 2.5: Copy Spectrum Animation
- [ ] **Action:** Copy `spectrum_anim_cb()` (~30 lines)
- [ ] **Modify:** Integrate với audio service
- [ ] **Time:** 2 giờ

**Key Changes:**
```c
// Add album art scale sync
if (s_album_art != NULL) {
    uint16_t bands[4];
    sx_audio_get_spectrum(bands, 4);
    lv_img_set_zoom(s_album_art, LV_SCALE_NONE + (bands[0] / 10));
}
```

#### Task 2.6: Create Spectrum Object
- [ ] **File:** `components/sx_ui/screens/screen_music_player.c`
- [ ] **Action:** Add spectrum object creation
- [ ] **Time:** 1 giờ

**Code:**
```c
static lv_obj_t *s_spectrum_obj = NULL;

// In on_create():
s_spectrum_obj = lv_obj_create(s_content);
lv_obj_remove_style_all(s_spectrum_obj);
lv_obj_set_size(s_spectrum_obj, LV_PCT(100), 250);
lv_obj_add_event_cb(s_spectrum_obj, spectrum_draw_event_cb, LV_EVENT_ALL, NULL);
```

#### Task 2.7: Start Spectrum Animation
- [ ] **Action:** Start animation khi playing
- [ ] **Time:** 1 giờ

**Code:**
```c
// In on_update() khi playing:
if (is_playing && s_spectrum_obj != NULL) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_spectrum_obj);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_exec_cb(&a, spectrum_anim_cb);
    lv_anim_set_duration(&a, 10000);
    lv_anim_start(&a);
}
```

**Milestone:** ✅ Spectrum visualization working

---

### **Day 11-12: Time Display**

#### Task 2.8: Add Time Labels
- [ ] **File:** `screen_music_player.c`
- [ ] **Action:** Add current và total time labels
- [ ] **Time:** 30 phút

**Code:**
```c
static lv_obj_t *s_time_current = NULL;
static lv_obj_t *s_time_total = NULL;

// In on_create():
s_time_current = lv_label_create(s_content);
lv_label_set_text(s_time_current, "0:00");
lv_obj_set_style_text_font(s_time_current, get_font_small(), 0);
lv_obj_set_style_text_color(s_time_current, lv_color_hex(0xFFFFFF), 0);

s_time_total = lv_label_create(s_content);
lv_label_set_text(s_time_total, "/ 0:00");
lv_obj_set_style_text_font(s_time_total, get_font_small(), 0);
lv_obj_set_style_text_color(s_time_total, lv_color_hex(0x888888), 0);
```

#### Task 2.9: Create Time Timer
- [ ] **Action:** Create timer để update time mỗi giây
- [ ] **Time:** 1 giờ

**Code:**
```c
static lv_timer_t *s_time_timer = NULL;

static void time_timer_cb(lv_timer_t *t) {
    uint32_t current = sx_audio_get_position();
    uint32_t total = sx_audio_get_duration();
    
    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d", current / 60, current % 60);
    lv_label_set_text(s_time_current, buf);
    
    snprintf(buf, sizeof(buf), "/ %d:%02d", total / 60, total % 60);
    lv_label_set_text(s_time_total, buf);
}

// In on_create():
s_time_timer = lv_timer_create(time_timer_cb, 1000, NULL);
```

#### Task 2.10: Update Progress Slider từ Timer
- [ ] **Action:** Update progress slider trong timer
- [ ] **Time:** 30 phút

**Code:**
```c
// In time_timer_cb():
if (total > 0 && s_progress_slider != NULL) {
    lv_slider_set_value(s_progress_slider, (current * 100) / total, LV_ANIM_ON);
}
```

**Milestone:** ✅ Time display working

---

### **Day 13: Interactive Progress Slider**

#### Task 2.11: Replace Progress Bar với Slider
- [ ] **Action:** Replace `s_progress_bar` với `s_progress_slider`
- [ ] **Time:** 1 giờ

**Code:**
```c
// Remove:
lv_obj_del(s_progress_bar);
s_progress_bar = NULL;

// Add:
s_progress_slider = lv_slider_create(s_content);
lv_obj_set_size(s_progress_slider, LV_PCT(90), 6);
lv_obj_add_flag(s_progress_slider, LV_OBJ_FLAG_CLICKABLE);
lv_obj_set_style_anim_duration(s_progress_slider, 100, 0);
```

#### Task 2.12: Add Custom Knob
- [ ] **Action:** Add slider knob image
- [ ] **Time:** 30 phút

**Code:**
```c
LV_IMAGE_DECLARE(img_lv_demo_music_slider_knob);
lv_obj_set_style_bg_image_src(s_progress_slider, &img_lv_demo_music_slider_knob, LV_PART_KNOB);
lv_obj_set_style_pad_all(s_progress_slider, 20, LV_PART_KNOB);
```

#### Task 2.13: Add Gradient Indicator
- [ ] **Action:** Add gradient cho slider indicator
- [ ] **Time:** 30 phút

**Code:**
```c
lv_obj_set_style_bg_grad_dir(s_progress_slider, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
lv_obj_set_style_bg_color(s_progress_slider, lv_color_hex(0x569af8), LV_PART_INDICATOR);
lv_obj_set_style_bg_grad_color(s_progress_slider, lv_color_hex(0xa666f1), LV_PART_INDICATOR);
```

#### Task 2.14: Add Seek Handler
- [ ] **Action:** Add event handler cho seek
- [ ] **Time:** 1 giờ

**Code:**
```c
static void progress_slider_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        int32_t value = lv_slider_get_value(s_progress_slider);
        uint32_t total = sx_audio_get_duration();
        if (total > 0) {
            uint32_t seek_pos = (value * total) / 100;
            sx_audio_seek(seek_pos);
        }
    }
}

// In on_create():
lv_obj_add_event_cb(s_progress_slider, progress_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
```

**Milestone:** ✅ Interactive progress slider working

---

### **Week 3: Playlist View** 📋

### **Day 14-16: Playlist View**

#### Task 2.15: Create Playlist File
- [ ] **File:** `components/sx_ui/screens/screen_music_player_list.c`
- [ ] **Action:** Create new file
- [ ] **Time:** 15 phút

#### Task 2.16: Copy Playlist Creation Code
- [ ] **Source:** `managed_components/lvgl__lvgl/demos/music/lv_demo_music_list.c`
- [ ] **Action:** Copy `_lv_demo_music_list_create()` function
- [ ] **Modify:** Integrate với `sx_playlist_manager`
- [ ] **Time:** 4 giờ

**Key Changes:**
```c
// Original:
for(track_id = 0; _lv_demo_music_get_title(track_id); track_id++) {
    // ...
}

// Modified:
size_t count = sx_playlist_get_count();
for(size_t i = 0; i < count; i++) {
    const char *title = sx_playlist_get_title(i);
    // ...
}
```

#### Task 2.17: Copy List Item Creation
- [ ] **Action:** Copy `add_list_button()` function
- [ ] **Modify:** Use playlist functions
- [ ] **Time:** 2 giờ

#### Task 2.18: Copy Click Handler
- [ ] **Action:** Copy `btn_click_event_cb()`
- [ ] **Modify:** Use `sx_playlist_play_index()`
- [ ] **Time:** 1 giờ

**Code:**
```c
static void playlist_item_click_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    size_t track_index = (size_t)lv_obj_get_user_data(btn);
    
    esp_err_t ret = sx_playlist_play_index(track_index);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to play track %d: %s", track_index, esp_err_to_name(ret));
    }
}
```

#### Task 2.19: Add Playlist View to Screen
- [ ] **File:** `screen_music_player.c`
- [ ] **Action:** Add playlist view object
- [ ] **Time:** 1 giờ

**Code:**
```c
static lv_obj_t *s_playlist_list = NULL;

// In on_create():
s_playlist_list = create_playlist_view(s_content);
lv_obj_set_size(s_playlist_list, LV_PCT(100), LV_PCT(100) - 200);
```

#### Task 2.20: Add Toggle Button
- [ ] **Action:** Add button để toggle playlist view
- [ ] **Time:** 1 giờ

**Code:**
```c
static lv_obj_t *s_list_btn = NULL;

// In on_create():
s_list_btn = lv_img_create(s_content);
LV_IMAGE_DECLARE(img_lv_demo_music_btn_list_play);
lv_img_set_src(s_list_btn, &img_lv_demo_music_btn_list_play);
lv_obj_add_flag(s_list_btn, LV_OBJ_FLAG_CLICKABLE);
lv_obj_add_event_cb(s_list_btn, toggle_list_cb, LV_EVENT_CLICKED, NULL);
```

**Milestone:** ✅ Playlist view working

---

## 📅 PHASE 3: VISUAL ENHANCEMENTS (Week 4-5)

### **Week 4: Typography & Animations** 🎨

### **Day 17-18: Typography Hierarchy**

#### Task 3.1: Update Title Font
- [ ] **Action:** Change title font to large
- [ ] **Time:** 15 phút

**Code:**
```c
lv_obj_set_style_text_font(s_track_title, get_font_large(), 0);
```

#### Task 3.2: Update Artist Font
- [ ] **Action:** Keep artist font medium
- [ ] **Time:** 15 phút

**Code:**
```c
lv_obj_set_style_text_font(s_track_artist, get_font_medium(), 0);
```

#### Task 3.3: Add Genre Label
- [ ] **Action:** Add genre label với small font
- [ ] **Time:** 30 phút

**Code:**
```c
static lv_obj_t *s_track_genre = NULL;

s_track_genre = lv_label_create(s_content);
lv_label_set_text(s_track_genre, "Unknown genre");
lv_obj_set_style_text_font(s_track_genre, get_font_small(), 0);
lv_obj_set_style_text_color(s_track_genre, lv_color_hex(0x8a86b8), 0);
```

**Milestone:** ✅ Typography hierarchy complete

---

### **Day 19-21: Album Art Animations**

#### Task 3.4: Copy Album Art Fade Animation
- [ ] **Source:** `lv_demo_music_main.c`
- [ ] **Action:** Copy fade out/in code
- [ ] **Time:** 2 giờ

**Code:**
```c
// Fade out old album
lv_obj_fade_out(s_album_art, 500, 0);

// Create new album
// ...

// Fade in new album
lv_obj_fade_in(s_album_art, 500, 100);
```

#### Task 3.5: Copy Album Art Move Animation
- [ ] **Action:** Copy move animation code
- [ ] **Time:** 2 giờ

**Code:**
```c
lv_anim_t a;
lv_anim_init(&a);
lv_anim_set_var(&a, s_album_art);
lv_anim_set_values(&a, 0, next ? -LV_HOR_RES/7 : LV_HOR_RES/7);
lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
lv_anim_set_duration(&a, 500);
lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
lv_anim_start(&a);
```

#### Task 3.6: Copy Album Art Zoom Animation
- [ ] **Action:** Copy zoom animation code
- [ ] **Time:** 2 giờ

**Code:**
```c
// Zoom out
lv_img_set_zoom(s_album_art, LV_SCALE_NONE / 2);

// Zoom in với overshoot
lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
lv_anim_set_values(&a, LV_SCALE_NONE / 4, LV_SCALE_NONE);
lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_img_set_zoom);
lv_anim_start(&a);
```

#### Task 3.7: Trigger Animations on Track Change
- [ ] **Action:** Add animations khi track changes
- [ ] **Time:** 2 giờ

**Code:**
```c
// In on_update() khi track changes:
if (track_changed) {
    // Trigger animations
    animate_album_art_change(next);
}
```

**Milestone:** ✅ Album art animations working

---

### **Day 22-23: Intro Animations**

#### Task 3.8: Copy Intro Animation Code
- [ ] **Source:** `lv_demo_music_main.c`
- [ ] **Action:** Copy intro animation setup
- [ ] **Time:** 2 giờ

**Code:**
```c
#define INTRO_TIME 2000

// Initially hide elements
lv_obj_set_style_opa(s_album_art, LV_OPA_TRANSP, 0);
lv_obj_set_style_opa(s_track_title, LV_OPA_TRANSP, 0);
lv_obj_set_style_opa(s_track_artist, LV_OPA_TRANSP, 0);
```

#### Task 3.9: Add Staggered Fade In
- [ ] **Action:** Add staggered fade in animations
- [ ] **Time:** 2 giờ

**Code:**
```c
lv_obj_fade_in(s_album_art, 800, INTRO_TIME + 500);
lv_obj_fade_in(s_track_title, 1000, INTRO_TIME + 1000);
lv_obj_fade_in(s_track_artist, 1000, INTRO_TIME + 1200);
```

**Milestone:** ✅ Intro animations working

---

### **Week 5: Album Art Scale Sync & Image Buttons** 🖼️

### **Day 24-25: Album Art Scale Sync**

#### Task 3.10: Add Scale Sync trong Spectrum Animation
- [ ] **File:** `screen_music_player_spectrum.c`
- [ ] **Action:** Add album art scale sync
- [ ] **Time:** 1 giờ

**Code:**
```c
// In spectrum_anim_cb():
if (s_album_art != NULL) {
    uint16_t bands[4];
    sx_audio_get_spectrum(bands, 4);
    lv_img_set_zoom(s_album_art, LV_SCALE_NONE + (bands[0] / 10));
}
```

**Milestone:** ✅ Album art scale sync working

---

### **Day 26-28: Image Buttons**

#### Task 3.11: Replace Symbol Buttons với Image Buttons
- [ ] **Action:** Replace prev/next buttons
- [ ] **Time:** 1 giờ

**Code:**
```c
// Remove old buttons
lv_obj_del(s_prev_btn);
lv_obj_del(s_next_btn);

// Create image buttons
LV_IMAGE_DECLARE(img_lv_demo_music_btn_prev);
LV_IMAGE_DECLARE(img_lv_demo_music_btn_next);

s_prev_btn = lv_img_create(controls);
lv_img_set_src(s_prev_btn, &img_lv_demo_music_btn_prev);
lv_obj_add_flag(s_prev_btn, LV_OBJ_FLAG_CLICKABLE);
lv_obj_add_event_cb(s_prev_btn, prev_btn_cb, LV_EVENT_CLICKED, NULL);

s_next_btn = lv_img_create(controls);
lv_img_set_src(s_next_btn, &img_lv_demo_music_btn_next);
lv_obj_add_flag(s_next_btn, LV_OBJ_FLAG_CLICKABLE);
lv_obj_add_event_cb(s_next_btn, next_btn_cb, LV_EVENT_CLICKED, NULL);
```

#### Task 3.12: Replace Play/Pause Button
- [ ] **Action:** Replace với imagebutton widget
- [ ] **Time:** 1 giờ

**Code:**
```c
LV_IMAGE_DECLARE(img_lv_demo_music_btn_play);
LV_IMAGE_DECLARE(img_lv_demo_music_btn_pause);

s_play_btn = lv_imagebutton_create(controls);
lv_imagebutton_set_src(s_play_btn, LV_IMAGEBUTTON_STATE_RELEASED, NULL, &img_lv_demo_music_btn_play, NULL);
lv_imagebutton_set_src(s_play_btn, LV_IMAGEBUTTON_STATE_CHECKED_RELEASED, NULL, &img_lv_demo_music_btn_pause, NULL);
lv_obj_add_flag(s_play_btn, LV_OBJ_FLAG_CHECKABLE);
lv_obj_add_event_cb(s_play_btn, play_pause_btn_cb, LV_EVENT_CLICKED, NULL);
```

**Milestone:** ✅ Image buttons working

---

## 📅 PHASE 4: POLISH & TESTING (Week 6)

### **Day 29-30: Decorative Elements** 🎨

#### Task 4.1: Add Waves
- [ ] **Action:** Add wave decorations
- [ ] **Time:** 1 giờ

**Code:**
```c
LV_IMAGE_DECLARE(img_lv_demo_music_wave_top);
LV_IMAGE_DECLARE(img_lv_demo_music_wave_bottom);

static void add_wave_decorations(lv_obj_t *parent) {
    lv_obj_t *wave_top = lv_img_create(parent);
    lv_img_set_src(wave_top, &img_lv_demo_music_wave_top);
    lv_obj_set_width(wave_top, LV_PCT(100));
    lv_obj_align(wave_top, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_flag(wave_top, LV_OBJ_FLAG_IGNORE_LAYOUT);
    
    lv_obj_t *wave_bottom = lv_img_create(parent);
    lv_img_set_src(wave_bottom, &img_lv_demo_music_wave_bottom);
    lv_obj_set_width(wave_bottom, LV_PCT(100));
    lv_obj_align(wave_bottom, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(wave_bottom, LV_OBJ_FLAG_IGNORE_LAYOUT);
}
```

#### Task 4.2: Add Corners (Optional)
- [ ] **Action:** Add corner decorations
- [ ] **Time:** 1 giờ

**Milestone:** ✅ Decorative elements added

---

### **Day 31-32: Grid Layout** 📐

#### Task 4.3: Replace Flex với Grid Layout
- [ ] **Action:** Replace flex layout với grid
- [ ] **Time:** 2 giờ

**Code:**
```c
static const int32_t grid_cols[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static const int32_t grid_rows[] = {
    40,                    // Top bar
    LV_GRID_FR(1),        // Spacer
    LV_GRID_CONTENT,      // Spectrum
    LV_GRID_CONTENT,      // Album art
    LV_GRID_FR(1),        // Spacer
    LV_GRID_CONTENT,      // Title
    LV_GRID_CONTENT,      // Artist
    LV_GRID_CONTENT,      // Genre
    LV_GRID_FR(1),        // Spacer
    LV_GRID_CONTENT,      // Progress slider
    LV_GRID_CONTENT,      // Time labels
    LV_GRID_CONTENT,      // Control buttons
    LV_GRID_FR(1),        // Spacer
    LV_GRID_CONTENT,      // Volume slider
    LV_GRID_TEMPLATE_LAST
};

lv_obj_set_grid_dsc_array(s_content, grid_cols, grid_rows);
lv_obj_set_grid_cell(s_spectrum_obj, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
// ... (set grid cells for all elements)
```

**Milestone:** ✅ Grid layout complete

---

### **Day 33-35: Track Info Updates & Testing** ✅

#### Task 4.4: Update Track Info trong on_update()
- [ ] **Action:** Update title, artist, genre từ playlist
- [ ] **Time:** 2 giờ

**Code:**
```c
static void on_update(const sx_state_t *state) {
    // ... existing code ...
    
    // Update track info
    int current_index = sx_playlist_get_current_index();
    if (current_index >= 0) {
        size_t track_index = (size_t)current_index;
        const char *title = sx_playlist_get_title(track_index);
        const char *artist = sx_playlist_get_artist(track_index);
        const char *genre = sx_playlist_get_genre(track_index);
        
        if (title && s_track_title) {
            lv_label_set_text(s_track_title, title);
        }
        if (artist && s_track_artist) {
            lv_label_set_text(s_track_artist, artist);
        }
        if (genre && s_track_genre) {
            lv_label_set_text(s_track_genre, genre);
        }
        
        // Load album art
        char cover_path[256];
        if (sx_playlist_get_cover_path(track_index, cover_path, sizeof(cover_path)) == ESP_OK) {
            load_album_art(cover_path);
        }
    }
}
```

#### Task 4.5: Load Album Art Image
- [ ] **Action:** Implement album art loading
- [ ] **Time:** 2 giờ

**Code:**
```c
static void load_album_art(const char *path) {
    if (s_album_art == NULL) return;
    
    // Remove old image
    lv_obj_clean(s_album_art);
    
    // Create new image
    lv_obj_t *img = lv_img_create(s_album_art);
    lv_img_set_src(img, path);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
}
```

#### Task 4.6: Integration Testing
- [ ] **Action:** Test tất cả features
- [ ] **Time:** 4 giờ

**Test Cases:**
- [ ] Play/pause works
- [ ] Previous/next works
- [ ] Spectrum visualization works
- [ ] Time display updates
- [ ] Progress slider seek works
- [ ] Playlist view works
- [ ] Animations work
- [ ] Track info updates
- [ ] Album art loads

#### Task 4.7: Bug Fixes
- [ ] **Action:** Fix any bugs found
- [ ] **Time:** 4 giờ

#### Task 4.8: Performance Optimization
- [ ] **Action:** Optimize animations và rendering
- [ ] **Time:** 2 giờ

**Milestone:** ✅ All features working, tested, và optimized

---

## 📊 PROGRESS TRACKING

### Overall Progress: 0%

**Phase 1:** 0% (0/14 tasks)
- [ ] Assets copied
- [ ] Fonts enabled
- [ ] Services extended

**Phase 2:** 0% (0/20 tasks)
- [ ] Spectrum working
- [ ] Time display working
- [ ] Slider working
- [ ] Playlist working

**Phase 3:** 0% (0/12 tasks)
- [ ] Typography complete
- [ ] Animations working
- [ ] Image buttons working

**Phase 4:** 0% (0/8 tasks)
- [ ] Decorative elements added
- [ ] Grid layout complete
- [ ] Testing complete

---

## 🎯 MILESTONES

| Milestone | Week | Status |
|-----------|------|--------|
| Assets & Setup | Week 1 | 🔴 Not Started |
| Core Features | Week 2-3 | 🔴 Not Started |
| Visual Enhancements | Week 4-5 | 🔴 Not Started |
| Polish & Testing | Week 6 | 🔴 Not Started |

---

## 📝 NOTES

### Dependencies:
- Audio decoder support cho position/duration/seek
- FFT library cho spectrum
- Metadata parsing cho track info
- Image loading cho album art

### Risks:
- Audio decoder có thể không support seek
- FFT processing có thể tốn CPU
- Metadata parsing có thể chậm
- Image loading có thể tốn memory

### Mitigation:
- Fallback nếu decoder không support seek
- Optimize FFT processing
- Cache metadata
- Use compressed images

---

## ✅ COMPLETION CRITERIA

### Must Have:
- [ ] Spectrum visualization working
- [ ] Time display working
- [ ] Interactive progress slider working
- [ ] Playlist view working
- [ ] Animations working
- [ ] Image buttons working
- [ ] Track info updates working

### Nice to Have:
- [ ] Decorative elements
- [ ] Grid layout
- [ ] Album art loading
- [ ] Performance optimizations

---

*Roadmap này chi tiết từng task, timeline, và dependencies để làm hybrid music screen giống LVGL Demo 1-1.*









