# REQUIREMENTS: HYBRID MUSIC SCREEN GIỐNG LVGL DEMO

> **Mục tiêu:** Liệt kê chi tiết những gì cần để làm hybrid music screen giống LVGL Demo, giữ architecture của SimpleXL

---

## 📋 TỔNG QUAN

### Mục tiêu:
- ✅ **UI giống LVGL Demo** - Modern, professional, animations
- ✅ **Architecture giữ SimpleXL** - Event-driven, service layer
- ✅ **Integration giữ SimpleXL** - sx_audio_service, sx_playlist_manager
- ✅ **Best of both worlds**

---

## 🎯 REQUIREMENTS CHI TIẾT

### 1. **ASSETS CẦN COPY TỪ LVGL DEMO** 📦

#### **Button Images** (8 buttons × 2 sizes = 16 images)
```
managed_components/lvgl__lvgl/demos/music/assets/
├── img_lv_demo_music_btn_play.c
├── img_lv_demo_music_btn_play_large.c
├── img_lv_demo_music_btn_pause.c
├── img_lv_demo_music_btn_pause_large.c
├── img_lv_demo_music_btn_prev.c
├── img_lv_demo_music_btn_prev_large.c
├── img_lv_demo_music_btn_next.c
├── img_lv_demo_music_btn_next_large.c
├── img_lv_demo_music_btn_loop.c
├── img_lv_demo_music_btn_loop_large.c
├── img_lv_demo_music_btn_rnd.c
├── img_lv_demo_music_btn_rnd_large.c
├── img_lv_demo_music_btn_list_play.c
├── img_lv_demo_music_btn_list_play_large.c
├── img_lv_demo_music_btn_list_pause.c
└── img_lv_demo_music_btn_list_pause_large.c
```

**Action:** Copy 16 button image files vào `components/sx_ui/assets/`

#### **Icon Images** (4 icons × 2 sizes = 8 images)
```
├── img_lv_demo_music_icon_1.c (Chart)
├── img_lv_demo_music_icon_1_large.c
├── img_lv_demo_music_icon_2.c (Chat)
├── img_lv_demo_music_icon_2_large.c
├── img_lv_demo_music_icon_3.c (Download)
├── img_lv_demo_music_icon_3_large.c
├── img_lv_demo_music_icon_4.c (Heart)
└── img_lv_demo_music_icon_4_large.c
```

**Action:** Copy 8 icon image files vào `components/sx_ui/assets/`

#### **Decorative Elements** (6 elements × 2 sizes = 12 images)
```
├── img_lv_demo_music_wave_top.c
├── img_lv_demo_music_wave_top_large.c
├── img_lv_demo_music_wave_bottom.c
├── img_lv_demo_music_wave_bottom_large.c
├── img_lv_demo_music_corner_left.c
├── img_lv_demo_music_corner_left_large.c
├── img_lv_demo_music_corner_right.c
├── img_lv_demo_music_corner_right_large.c
├── img_lv_demo_music_list_border.c
├── img_lv_demo_music_list_border_large.c
├── img_lv_demo_music_logo.c
└── img_lv_demo_music_slider_knob.c (2 sizes)
```

**Action:** Copy 12 decorative image files vào `components/sx_ui/assets/`

#### **Album Covers** (3 covers × 2 sizes = 6 images)
```
├── img_lv_demo_music_cover_1.c
├── img_lv_demo_music_cover_1_large.c
├── img_lv_demo_music_cover_2.c
├── img_lv_demo_music_cover_2_large.c
├── img_lv_demo_music_cover_3.c
└── img_lv_demo_music_cover_3_large.c
```

**Action:** Copy 6 cover image files vào `components/sx_ui/assets/`

#### **Spectrum Data** (3 files)
```
├── spectrum_1.h
├── spectrum_2.h
└── spectrum_3.h
```

**Action:** Copy 3 spectrum data files vào `components/sx_ui/assets/`

**Tổng cộng:** ~45 asset files cần copy

---

### 2. **FONTS CẦN ENABLE** 🔤

#### **Montserrat Fonts** (4 sizes)
```c
// Cần enable trong sdkconfig hoặc Kconfig
CONFIG_LV_FONT_MONTSERRAT_12=y
CONFIG_LV_FONT_MONTSERRAT_16=y
CONFIG_LV_FONT_MONTSERRAT_22=y
CONFIG_LV_FONT_MONTSERRAT_32=y
```

**Action:** Enable 4 Montserrat font sizes trong build config

**Usage:**
```c
// Font helpers
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

---

### 3. **AUDIO SERVICE EXTENSIONS** 🔊

#### **Cần thêm vào sx_audio_service.h:**

```c
// Position và Duration
uint32_t sx_audio_get_position(void);      // Current position (seconds)
uint32_t sx_audio_get_duration(void);      // Total duration (seconds)
esp_err_t sx_audio_seek(uint32_t position); // Seek to position (seconds)

// Spectrum/FFT Data
esp_err_t sx_audio_get_spectrum(uint16_t *bands, size_t band_count);
// Returns: bands[0] = Bass, bands[1] = Mid-low, bands[2] = Mid-high, bands[3] = High
// Range: 0-255 for each band
```

**Action:** Extend `sx_audio_service` với position, duration, seek, spectrum functions

**Implementation notes:**
- `sx_audio_get_position()` - Get current playback position
- `sx_audio_get_duration()` - Get total track duration
- `sx_audio_seek()` - Seek to specific position
- `sx_audio_get_spectrum()` - Get FFT data for 4 frequency bands

---

### 4. **PLAYLIST MANAGER EXTENSIONS** 📋

#### **Cần thêm vào sx_playlist_manager.h:**

**Hiện tại có:**
```c
// ✅ Đã có
esp_err_t sx_playlist_next(void);
esp_err_t sx_playlist_previous(void);
esp_err_t sx_playlist_play_index(size_t index);
int sx_playlist_get_current_index(void);
const char* sx_playlist_get_current_track(void);
sx_playlist_t* sx_playlist_get_current(void);
```

**Cần thêm:**
```c
// Track Info (từ metadata)
const char* sx_playlist_get_title(size_t track_id);
const char* sx_playlist_get_artist(size_t track_id);
const char* sx_playlist_get_genre(size_t track_id);
uint32_t sx_playlist_get_duration(size_t track_id);  // seconds
size_t sx_playlist_get_count(void);

// Album Art
esp_err_t sx_playlist_get_cover_path(size_t track_id, char *path, size_t path_len);
```

**Action:** Extend `sx_playlist_manager` với track info functions

**Implementation notes:**
- Track metadata (title, artist, genre, duration) - Parse từ file metadata hoặc filename
- Track count - Từ playlist structure
- Album art path - Tìm cover image trong cùng folder với track

---

### 5. **CODE STRUCTURE CẦN THÊM** 💻

#### **File Structure:**
```
components/sx_ui/screens/
├── screen_music_player.c          (existing - modify)
├── screen_music_player.h          (existing - modify)
└── screen_music_player_spectrum.c (new - spectrum drawing)
└── screen_music_player_list.c     (new - playlist view)
```

#### **New Files cần tạo:**

**1. `screen_music_player_spectrum.c`**
```c
// Spectrum visualization code
// Copy từ LVGL Demo lv_demo_music_main.c
// - spectrum_draw_event_cb()
// - spectrum_anim_cb()
// - get_cos(), get_sin()
// - BAR_CNT, DEG_STEP, etc.
```

**2. `screen_music_player_list.c`**
```c
// Playlist view code
// Copy từ LVGL Demo lv_demo_music_list.c
// - create_playlist_view()
// - add_list_button()
// - btn_click_event_cb()
```

**3. `components/sx_ui/assets/sx_ui_assets.h`**
```c
// Asset declarations
LV_IMAGE_DECLARE(img_lv_demo_music_btn_play);
LV_IMAGE_DECLARE(img_lv_demo_music_btn_pause);
// ... (all assets)
```

---

### 6. **UI COMPONENTS CẦN THÊM** 🎨

#### **Spectrum Object**
```c
static lv_obj_t *s_spectrum_obj = NULL;

// Create spectrum visualization
s_spectrum_obj = lv_obj_create(s_content);
lv_obj_remove_style_all(s_spectrum_obj);
lv_obj_set_size(s_spectrum_obj, LV_PCT(100), 250);
lv_obj_add_event_cb(s_spectrum_obj, spectrum_draw_event_cb, LV_EVENT_ALL, NULL);
```

#### **Time Display**
```c
static lv_obj_t *s_time_current = NULL;
static lv_obj_t *s_time_total = NULL;
static lv_timer_t *s_time_timer = NULL;

// Current time label
s_time_current = lv_label_create(s_content);
lv_label_set_text(s_time_current, "0:00");
lv_obj_set_style_text_font(s_time_current, get_font_small(), 0);

// Total time label
s_time_total = lv_label_create(s_content);
lv_label_set_text(s_time_total, "/ 0:00");
lv_obj_set_style_text_font(s_time_total, get_font_small(), 0);
```

#### **Progress Slider** (thay thế bar)
```c
static lv_obj_t *s_progress_slider = NULL;  // Thay s_progress_bar

// Interactive slider
s_progress_slider = lv_slider_create(s_content);
lv_obj_set_size(s_progress_slider, LV_PCT(90), 6);
lv_obj_add_flag(s_progress_slider, LV_OBJ_FLAG_CLICKABLE);
lv_obj_set_style_anim_duration(s_progress_slider, 100, 0);

// Custom knob
LV_IMAGE_DECLARE(img_lv_demo_music_slider_knob);
lv_obj_set_style_bg_image_src(s_progress_slider, &img_lv_demo_music_slider_knob, LV_PART_KNOB);
lv_obj_set_style_pad_all(s_progress_slider, 20, LV_PART_KNOB);

// Gradient indicator
lv_obj_set_style_bg_grad_dir(s_progress_slider, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
lv_obj_set_style_bg_color(s_progress_slider, lv_color_hex(0x569af8), LV_PART_INDICATOR);
lv_obj_set_style_bg_grad_color(s_progress_slider, lv_color_hex(0xa666f1), LV_PART_INDICATOR);
```

#### **Playlist View**
```c
static lv_obj_t *s_playlist_list = NULL;

// Scrollable list
s_playlist_list = lv_obj_create(s_content);
lv_obj_set_size(s_playlist_list, LV_PCT(100), LV_PCT(100) - 200);
lv_obj_set_flex_flow(s_playlist_list, LV_FLEX_FLOW_COLUMN);
lv_obj_set_style_pad_all(s_playlist_list, 0, LV_PART_MAIN);

// Add list items
for(uint32_t i = 0; i < sx_playlist_get_count(); i++) {
    create_playlist_item(s_playlist_list, i);
}
```

#### **Genre Label**
```c
static lv_obj_t *s_track_genre = NULL;

s_track_genre = lv_label_create(s_content);
lv_label_set_text(s_track_genre, "Unknown genre");
lv_obj_set_style_text_font(s_track_genre, get_font_small(), 0);
lv_obj_set_style_text_color(s_track_genre, lv_color_hex(0x8a86b8), 0);
```

---

### 7. **ANIMATIONS CẦN THÊM** 🎬

#### **Album Art Animations**
```c
// Fade out old album
lv_obj_fade_out(s_album_art, 500, 0);

// Move out animation
lv_anim_t a;
lv_anim_init(&a);
lv_anim_set_var(&a, s_album_art);
lv_anim_set_values(&a, 0, next ? -LV_HOR_RES/7 : LV_HOR_RES/7);
lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
lv_anim_set_duration(&a, 500);
lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
lv_anim_start(&a);

// Zoom out
lv_img_set_zoom(s_album_art, LV_SCALE_NONE / 2);

// Create new album
// ... create new album art ...

// Fade in new album
lv_obj_fade_in(s_album_art, 500, 100);

// Zoom in with overshoot
lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
lv_anim_set_values(&a, LV_SCALE_NONE / 4, LV_SCALE_NONE);
lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_img_set_zoom);
lv_anim_start(&a);
```

#### **Intro Animations**
```c
#define INTRO_TIME 2000

// Initially hide elements
lv_obj_set_style_opa(s_album_art, LV_OPA_TRANSP, 0);
lv_obj_set_style_opa(s_track_title, LV_OPA_TRANSP, 0);
lv_obj_set_style_opa(s_track_artist, LV_OPA_TRANSP, 0);

// Fade in after intro
lv_obj_fade_in(s_album_art, 800, INTRO_TIME + 500);
lv_obj_fade_in(s_track_title, 1000, INTRO_TIME + 1000);
lv_obj_fade_in(s_track_artist, 1000, INTRO_TIME + 1200);
```

#### **Spectrum Animation**
```c
// Spectrum animation với album art sync
static void spectrum_anim_cb(void * a, int32_t v) {
    spectrum_i = v;
    
    // Get spectrum data từ audio service
    uint16_t bands[4];
    sx_audio_get_spectrum(bands, 4);
    
    // Album art scaling sync
    lv_img_set_zoom(s_album_art, LV_SCALE_NONE + (bands[0] / 10));
    
    lv_obj_invalidate(s_spectrum_obj);
}
```

---

### 8. **EVENT HANDLERS CẦN THÊM** 🎯

#### **Progress Slider Handler**
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
```

#### **Time Timer Handler**
```c
static void time_timer_cb(lv_timer_t *t) {
    uint32_t current = sx_audio_get_position();
    uint32_t total = sx_audio_get_duration();
    
    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d", current / 60, current % 60);
    lv_label_set_text(s_time_current, buf);
    
    snprintf(buf, sizeof(buf), "/ %d:%02d", total / 60, total % 60);
    lv_label_set_text(s_time_total, buf);
    
    // Update progress slider
    if (total > 0 && s_progress_slider != NULL) {
        lv_slider_set_value(s_progress_slider, (current * 100) / total, LV_ANIM_ON);
    }
}
```

#### **Playlist Item Click Handler**
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

---

### 9. **STATE UPDATES CẦN THÊM** 📊

#### **Track Info Updates**
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
            // Load và display album art image
            load_album_art(cover_path);
        }
    }
}
```

#### **Progress Updates**
```c
// Update progress trong timer
static void time_timer_cb(lv_timer_t *t) {
    // ... time display ...
    
    // Update progress slider
    uint32_t current = sx_audio_get_position();
    uint32_t total = sx_audio_get_duration();
    if (total > 0 && s_progress_slider != NULL) {
        lv_slider_set_value(s_progress_slider, (current * 100) / total, LV_ANIM_ON);
    }
}
```

---

### 10. **LAYOUT SYSTEM CẦN THAY ĐỔI** 📐

#### **Từ Flex → Grid Layout**
```c
// Thay thế flex layout bằng grid
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
lv_obj_set_grid_cell(s_album_art, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 3, 1);
// ... (tương tự cho các elements khác)
```

---

### 11. **IMAGE BUTTONS CẦN THAY ĐỔI** 🖼️

#### **Từ Symbol Icons → Image Buttons**
```c
// Thay thế symbol buttons bằng image buttons
LV_IMAGE_DECLARE(img_lv_demo_music_btn_play);
LV_IMAGE_DECLARE(img_lv_demo_music_btn_pause);
LV_IMAGE_DECLARE(img_lv_demo_music_btn_prev);
LV_IMAGE_DECLARE(img_lv_demo_music_btn_next);

// Play/Pause button
s_play_btn = lv_imagebutton_create(controls);
lv_imagebutton_set_src(s_play_btn, LV_IMAGEBUTTON_STATE_RELEASED, NULL, &img_lv_demo_music_btn_play, NULL);
lv_imagebutton_set_src(s_play_btn, LV_IMAGEBUTTON_STATE_CHECKED_RELEASED, NULL, &img_lv_demo_music_btn_pause, NULL);
lv_obj_add_flag(s_play_btn, LV_OBJ_FLAG_CHECKABLE);

// Prev button
s_prev_btn = lv_img_create(controls);
lv_img_set_src(s_prev_btn, &img_lv_demo_music_btn_prev);
lv_obj_add_flag(s_prev_btn, LV_OBJ_FLAG_CLICKABLE);

// Next button
s_next_btn = lv_img_create(controls);
lv_img_set_src(s_next_btn, &img_lv_demo_music_btn_next);
lv_obj_add_flag(s_next_btn, LV_OBJ_FLAG_CLICKABLE);
```

---

### 12. **DECORATIVE ELEMENTS** 🎨

#### **Waves**
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

---

## 📋 CHECKLIST IMPLEMENTATION

### Phase 1: Setup & Assets (1 tuần)

- [ ] **Copy Assets**
  - [ ] Copy 16 button images
  - [ ] Copy 8 icon images
  - [ ] Copy 12 decorative images
  - [ ] Copy 6 cover images
  - [ ] Copy 3 spectrum data files
  - [ ] Create `components/sx_ui/assets/` directory
  - [ ] Create `sx_ui_assets.h` với all declarations

- [ ] **Enable Fonts**
  - [ ] Enable Montserrat 12 trong config
  - [ ] Enable Montserrat 16 trong config
  - [ ] Enable Montserrat 22 trong config
  - [ ] Enable Montserrat 32 trong config
  - [ ] Create font helper functions

- [ ] **Extend Audio Service**
  - [ ] Add `sx_audio_get_position()`
  - [ ] Add `sx_audio_get_duration()`
  - [ ] Add `sx_audio_seek()`
  - [ ] Add `sx_audio_get_spectrum()`

- [ ] **Extend Playlist Manager**
  - [ ] Add `sx_playlist_get_title()`
  - [ ] Add `sx_playlist_get_artist()`
  - [ ] Add `sx_playlist_get_genre()`
  - [ ] Add `sx_playlist_get_duration()`
  - [ ] Add `sx_playlist_get_count()`
  - [ ] Add `sx_playlist_play()`
  - [ ] Add `sx_playlist_get_current_id()`
  - [ ] Add `sx_playlist_get_cover_path()`

### Phase 2: Core Features (2 tuần)

- [ ] **Spectrum Visualization**
  - [ ] Create `screen_music_player_spectrum.c`
  - [ ] Copy spectrum drawing code từ LVGL Demo
  - [ ] Integrate với `sx_audio_get_spectrum()`
  - [ ] Add spectrum object vào UI
  - [ ] Add spectrum animation

- [ ] **Time Display**
  - [ ] Add current time label
  - [ ] Add total time label
  - [ ] Create time timer
  - [ ] Update timer callback
  - [ ] Format time (0:00)

- [ ] **Interactive Progress Slider**
  - [ ] Replace progress bar với slider
  - [ ] Add custom knob image
  - [ ] Add gradient indicator
  - [ ] Add seek event handler
  - [ ] Integrate với `sx_audio_seek()`

- [ ] **Playlist View**
  - [ ] Create `screen_music_player_list.c`
  - [ ] Copy playlist code từ LVGL Demo
  - [ ] Create scrollable list
  - [ ] Add list items với grid layout
  - [ ] Add click handlers
  - [ ] Add current track highlight

### Phase 3: Visual Enhancements (2 tuần)

- [ ] **Typography Hierarchy**
  - [ ] Update title font (large)
  - [ ] Update artist font (small)
  - [ ] Add genre font (small)
  - [ ] Update time font (small)
  - [ ] Add color differentiation

- [ ] **Album Art Animations**
  - [ ] Add fade animations
  - [ ] Add move animations
  - [ ] Add zoom animations
  - [ ] Add overshoot effect
  - [ ] Trigger on track change

- [ ] **Intro Animations**
  - [ ] Add INTRO_TIME constant
  - [ ] Hide elements initially
  - [ ] Add staggered fade in
  - [ ] Add random delays

- [ ] **Album Art Scale Sync**
  - [ ] Sync với spectrum amplitude
  - [ ] Real-time updates
  - [ ] Visual feedback

### Phase 4: Polish (1 tuần)

- [ ] **Decorative Elements**
  - [ ] Add waves (top/bottom)
  - [ ] Add corners (optional)
  - [ ] Add borders (optional)

- [ ] **Image Buttons**
  - [ ] Replace symbol buttons
  - [ ] Use imagebutton widget
  - [ ] Add button assets

- [ ] **Grid Layout**
  - [ ] Replace flex với grid
  - [ ] Define grid columns/rows
  - [ ] Set grid cells

- [ ] **Track Info Updates**
  - [ ] Update title từ playlist
  - [ ] Update artist từ playlist
  - [ ] Update genre từ playlist
  - [ ] Load album art image

---

## 📊 TỔNG HỢP REQUIREMENTS

### Files cần tạo:
1. `components/sx_ui/assets/sx_ui_assets.h` - Asset declarations
2. `components/sx_ui/screens/screen_music_player_spectrum.c` - Spectrum code
3. `components/sx_ui/screens/screen_music_player_list.c` - Playlist code

### Files cần modify:
1. `components/sx_ui/screens/screen_music_player.c` - Main screen
2. `components/sx_ui/screens/screen_music_player.h` - Header
3. `components/sx_services/sx_audio_service.h` - Add functions
4. `components/sx_services/sx_audio_service.c` - Implement functions
5. `components/sx_services/sx_playlist_manager.h` - Add functions
6. `components/sx_services/sx_playlist_manager.c` - Implement functions
7. `sdkconfig` hoặc Kconfig - Enable fonts

### Assets cần copy:
- 16 button images
- 8 icon images
- 12 decorative images
- 6 cover images
- 3 spectrum data files
- **Total: ~45 files**

### Functions cần implement:
- **Audio Service:** 4 functions mới
  - `sx_audio_get_position()` - Get current position
  - `sx_audio_get_duration()` - Get total duration
  - `sx_audio_seek()` - Seek to position
  - `sx_audio_get_spectrum()` - Get FFT spectrum data
  
- **Playlist Manager:** 6 functions mới
  - `sx_playlist_get_title()` - Get track title
  - `sx_playlist_get_artist()` - Get track artist
  - `sx_playlist_get_genre()` - Get track genre
  - `sx_playlist_get_duration()` - Get track duration
  - `sx_playlist_get_count()` - Get playlist count
  - `sx_playlist_get_cover_path()` - Get album art path
  
- **Note:** Đã có sẵn:
  - `sx_playlist_get_current_index()` ✅
  - `sx_playlist_play_index()` ✅

### Code cần copy/modify:
- Spectrum drawing code (~200 lines)
- Playlist view code (~300 lines)
- Animation code (~150 lines)
- **Total: ~650 lines**

---

## 🎯 ESTIMATED EFFORT

### Phase 1: Setup (1 tuần)
- Copy assets: 2 hours
- Enable fonts: 1 hour
- Extend services: 2 days
- **Total: ~1 tuần**

### Phase 2: Core Features (2 tuần)
- Spectrum: 3 days
- Time display: 1 day
- Progress slider: 1 day
- Playlist view: 3 days
- **Total: ~2 tuần**

### Phase 3: Visual Enhancements (2 tuần)
- Typography: 1 day
- Animations: 3 days
- Intro animations: 1 day
- Album art sync: 1 day
- **Total: ~2 tuần**

### Phase 4: Polish (1 tuần)
- Decorative elements: 1 day
- Image buttons: 1 day
- Grid layout: 1 day
- Track info updates: 1 day
- **Total: ~1 tuần**

**Tổng cộng: ~6 tuần**

---

## 💡 KẾT LUẬN

**Để làm hybrid music screen giống LVGL Demo cần:**

1. ✅ **45 asset files** - Copy từ LVGL Demo
2. ✅ **4 font sizes** - Enable Montserrat fonts (12, 16, 22, 32)
3. ✅ **10 service functions** - Extend audio (4) và playlist (6) services
4. ✅ **3 new files** - Spectrum, playlist, assets header
5. ✅ **~650 lines code** - Copy và modify từ LVGL Demo
6. ✅ **6 tuần effort** - Implementation time

**Kết quả:**
- ✅ UI giống LVGL Demo (modern, professional)
- ✅ Architecture giữ SimpleXL (event-driven, service layer)
- ✅ Integration giữ SimpleXL (sx_audio_service, sx_playlist_manager)
- ✅ Best of both worlds

---

*Tài liệu này liệt kê chi tiết requirements để làm hybrid music screen giống LVGL Demo.*

