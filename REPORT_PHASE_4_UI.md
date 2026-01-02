# PHASE 4 — LVGL/UI System
## Báo cáo phân tích UI framework: screen manager, navigation, lifecycle, và asset loading

**Ngày tạo:** 2025-01-02  
**Dự án:** hai-os-simplexl  
**Mục tiêu:** Phân tích UI framework, screen lifecycle, asset loading, và performance

---

## 1. UI FRAMEWORK ARCHITECTURE

### 1.1 Kiến Trúc Tổng Thể

**Layering:**

```
┌─────────────────────────────────────────┐
│         UI Task (sx_ui)                 │
│  - LVGL rendering loop                  │
│  - State polling                        │
│  - Event handling                       │
└─────────────────────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────┐
│         UI Router                        │
│  - Navigation management                 │
│  - Screen container                     │
│  - Lifecycle coordination               │
└─────────────────────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────┐
│      Screen Registry                     │
│  - Screen callbacks storage              │
│  - Screen name mapping                   │
│  - 29 screens registered                 │
└─────────────────────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────┐
│      Individual Screens                  │
│  - on_create, on_show, on_hide,          │
│    on_destroy, on_update                 │
│  - LVGL objects creation                 │
└─────────────────────────────────────────┘
```

**Nguồn:** `components/sx_ui/sx_ui_task.c`, `components/sx_ui/ui_router.c`, `components/sx_ui/ui_screen_registry.c`

### 1.2 UI Task Architecture

**Nguồn:** `components/sx_ui/sx_ui_task.c:L34-L293`

**Task Configuration:**
- **Name:** `sx_ui`
- **Stack:** 8192 bytes (8KB)
- **Priority:** 5
- **Core:** `tskNO_AFFINITY`

**Main Loop:**

```c
for (;;) {
    // 1. Poll state snapshot (read-only)
    sx_dispatcher_get_state(&state);
    
    // 2. Check state change
    bool state_changed = (state.seq != last_state_seq);
    
    // 3. Navigation logic (boot sequence)
    ui_screen_id_t target_screen = determine_target_screen();
    
    // 4. Navigate if needed
    if (target_screen != last_screen) {
        ui_router_navigate_to(target_screen);
    }
    
    // 5. Update current screen UI from state
    if (state_changed) {
        callbacks->on_update(&state);
    }
    
    // 6. Poll display service events
    while (sx_dispatcher_poll_event(&ui_evt)) {
        screen_display_helper_handle_event(&ui_evt);
    }
    
    // 7. LVGL tick (render)
    if (lvgl_port_lock(0)) {
        lv_timer_handler();
        lvgl_port_unlock();
    }
    
    // 8. Fixed interval (60 FPS)
    vTaskDelayUntil(&last_wake_time, render_interval);  // 16ms
}
```

**Phân tích:**
- ✅ **60 FPS target:** `render_interval = 16ms` (~60 FPS)
- ✅ **State polling:** Read-only state snapshot
- ✅ **LVGL lock:** Tất cả LVGL calls protected by mutex
- ⚠️ **Navigation trong main loop:** Có thể gây confusion, nên move vào event handler

### 1.3 LVGL Integration

**LVGL Port Configuration:**

**Nguồn:** `components/sx_ui/sx_ui_task.c:L44-L110`

```c
lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
lvgl_cfg.task_stack = 6144;  // LVGL internal task stack
esp_err_t err = lvgl_port_init(&lvgl_cfg);
```

**Display Configuration:**

```c
lvgl_port_display_cfg_t disp_cfg = {
    .buffer_size = 320 * 30,      // 9600 pixels (~1/15 screen)
    .double_buffer = true,         // Enable double buffering
    .hres = 320,
    .vres = 480,
    .rotation = {
        .swap_xy = false,
        .mirror_x = true,         // Fix reversed text
        .mirror_y = false,
    },
    .color_format = LV_COLOR_FORMAT_RGB565,
    .flags = {
        .buff_spiram = 1,         // Use PSRAM for buffers
        .swap_bytes = 1,          // Byte swap for BGR mode
    },
};
```

**Phân tích:**
- ✅ **Double buffering:** Enabled cho smooth rendering
- ✅ **PSRAM buffers:** Sử dụng PSRAM để tiết kiệm internal RAM
- ✅ **Buffer size:** 9600 pixels (~1/15 screen) — hợp lý cho scrolling
- ✅ **Mirror X:** Fix reversed text issue

**LVGL Thread Safety:**

**Nguồn:** `components/sx_ui/sx_lvgl_lock.c`, `components/sx_ui/sx_lvgl_guard.c`

```c
// Lock guard pattern
sx_lvgl_lock_guard_t guard = sx_lvgl_lock_acquire();
if (guard.locked) {
    // LVGL API calls
    sx_lvgl_lock_release(&guard);
}
```

**Phân tích:**
- ✅ **Nested lock detection:** Có check nested locks
- ✅ **Guard pattern:** RAII-style lock management
- ⚠️ **Runtime guard:** Có UI task handle storage nhưng chưa dùng để verify

---

## 2. SCREEN LIFECYCLE MANAGEMENT

### 2.1 Lifecycle Callbacks

**Nguồn:** `components/sx_ui/include/ui_screen_registry.h:L56-L63`

```c
typedef struct {
    void (*on_create)(void);      // Called once when screen is created
    void (*on_show)(void);        // Called when screen becomes visible
    void (*on_hide)(void);        // Called when screen is hidden
    void (*on_destroy)(void);     // Called when screen is destroyed
    void (*on_update)(const sx_state_t *state);  // Called periodically to update UI
} ui_screen_callbacks_t;
```

**Lifecycle Flow:**

```
Navigation Request
    ↓
ui_router_navigate_to(screen_id)
    ↓
[LVGL Lock]
    ↓
1. on_hide() (old screen)
    ↓
2. lv_obj_clean(container)  // Clear container
    ↓
3. on_destroy() (old screen)
    ↓
4. on_create() (new screen)  // Create LVGL objects
    ↓
5. s_current_screen = screen_id
    ↓
6. on_show() (new screen)  // Start animations, timers
    ↓
[LVGL Unlock]
```

**Nguồn:** `components/sx_ui/ui_router.c:L48-L125`

### 2.2 Screen Registration

**Nguồn:** `components/sx_ui/ui_screen_registry.c:L54-L85`

**Registration Pattern:**

```c
void screen_boot_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
    };
    ui_screen_registry_register(SCREEN_ID_BOOT, &callbacks);
}
```

**All Screens Registration:**

**Nguồn:** `components/sx_ui/screens/register_all_screens.c:L45-L84`

**Total Screens:** 29 screens (32 IDs, 3 reserved)

**Screen Categories:**
- **P0 - Core Product Screens:** 20 screens
- **P1 - Advanced Feature Screens:** 1 screen (audio_effects merged into equalizer)
- **P2 - Developer & Diagnostic Screens:** 7 screens

**Phân tích:**
- ✅ **Centralized registration:** Tất cả screens register trong `register_all_screens()`
- ✅ **Category-based:** Phân loại theo priority (P0/P1/P2)
- ⚠️ **Screen count:** 29 screens — có thể memory-intensive nếu tất cả screens tạo objects lớn

### 2.3 Screen Example: Boot Screen

**Nguồn:** `components/sx_ui/screens/screen_boot.c`

**on_create() Implementation:**

```c
static void on_create(void) {
    // 1. Get container
    lv_obj_t *container = ui_router_get_container();
    
    // 2. Get bootscreen image from assets
    const lv_img_dsc_t *bootscreen_img = ui_assets_get_bootscreen_img();
    
    // 3. Create image object
    s_bootscreen_img = lv_img_create(container);
    
    // 4. Set image source
    lv_img_set_src(s_bootscreen_img, bootscreen_img);
    
    // 5. Configure size and position
    lv_obj_set_size(s_bootscreen_img, 320, 480);
    lv_obj_set_pos(s_bootscreen_img, 0, 0);
}
```

**on_show() Implementation:**

```c
static void on_show(void) {
    // Create timer to auto-navigate after 3 seconds
    s_boot_timer = lv_timer_create(boot_timer_cb, 3000, NULL);
    lv_timer_set_repeat_count(s_boot_timer, 1);  // Run once
}
```

**on_hide() Implementation:**

```c
static void on_hide(void) {
    // Delete timer
    if (s_boot_timer != NULL) {
        lv_timer_del(s_boot_timer);
        s_boot_timer = NULL;
    }
}
```

**on_destroy() Implementation:**

```c
static void on_destroy(void) {
    // Ensure timer is deleted
    if (s_boot_timer != NULL) {
        lv_timer_del(s_boot_timer);
        s_boot_timer = NULL;
    }
    
    // Delete image object
    if (s_bootscreen_img != NULL) {
        lv_obj_del(s_bootscreen_img);
        s_bootscreen_img = NULL;
    }
}
```

**Phân tích:**
- ✅ **Clean lifecycle:** Timer cleanup trong on_hide và on_destroy
- ✅ **Resource cleanup:** Image object deleted trong on_destroy
- ✅ **Timer pattern:** LVGL timer cho auto-navigation

### 2.4 Screen Container Pattern

**Nguồn:** `components/sx_ui/ui_router.c:L17-L46`

**Container Creation:**

```c
void ui_router_init(void) {
    // Get default screen
    lv_obj_t *scr = lv_scr_act();
    
    // Create container that holds all screen content
    s_screen_container = lv_obj_create(scr);
    lv_obj_set_size(s_screen_container, LV_PCT(100), LV_PCT(100));
    lv_obj_align(s_screen_container, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(s_screen_container, LV_OPA_TRANSP, LV_PART_MAIN);
}
```

**Navigation Pattern:**

```c
void ui_router_navigate_to(ui_screen_id_t screen_id) {
    // 1. Hide old screen
    if (old_callbacks && old_callbacks->on_hide) {
        old_callbacks->on_hide();
    }
    
    // 2. Clear container content
    lv_obj_clean(s_screen_container);
    
    // 3. Destroy old screen
    if (old_callbacks && old_callbacks->on_destroy) {
        old_callbacks->on_destroy();
    }
    
    // 4. Create new screen
    if (callbacks->on_create) {
        callbacks->on_create();
    }
    
    // 5. Show new screen
    if (callbacks->on_show) {
        callbacks->on_show();
    }
}
```

**Phân tích:**
- ✅ **Container pattern:** Tất cả screens share một container
- ✅ **Clean before create:** `lv_obj_clean()` trước khi create mới
- ✅ **Lifecycle order:** hide → clean → destroy → create → show
- ⚠️ **Container không destroy:** Container tồn tại suốt lifetime, chỉ clean content

---

## 3. NAVIGATION SYSTEM

### 3.1 Navigation API

**Nguồn:** `components/sx_ui/include/ui_router.h`

```c
void ui_router_init(void);
void ui_router_navigate_to(ui_screen_id_t screen_id);
ui_screen_id_t ui_router_get_current_screen(void);
lv_obj_t* ui_router_get_container(void);
```

**Navigation Flow:**

```
Caller (Screen/Service)
    ↓
ui_router_navigate_to(screen_id)
    ↓
[Check: Already on screen?] → Skip if yes
    ↓
[LVGL Lock]
    ↓
1. on_hide() (old)
    ↓
2. lv_obj_clean(container)
    ↓
3. on_destroy() (old)
    ↓
4. on_create() (new)
    ↓
5. on_show() (new)
    ↓
[LVGL Unlock]
```

**Nguồn:** `components/sx_ui/ui_router.c:L48-L125`

### 3.2 Navigation Triggers

**1. Boot Sequence (Automatic):**

**Nguồn:** `components/sx_ui/sx_ui_task.c:L207-L233`

```c
// Boot sequence: BOOT (3s) → FLASH (screensaver) → HOME
if (state.ui.device_state == SX_DEV_BOOTING) {
    target_screen = SCREEN_ID_BOOT;
} else if (state.ui.device_state == SX_DEV_IDLE) {
    ui_screen_id_t current_screen = ui_router_get_current_screen();
    if (current_screen == SCREEN_ID_BOOT) {
        // Let timer handle navigation to FLASH
        target_screen = SCREEN_ID_BOOT;
    } else if (current_screen == SCREEN_ID_FLASH) {
        // Stay on flash screen - user must swipe to unlock
        target_screen = SCREEN_ID_FLASH;
    }
}
```

**2. Timer-Based Navigation:**

**Nguồn:** `components/sx_ui/screens/screen_boot.c:L93-L99`

```c
static void boot_timer_cb(lv_timer_t *timer) {
    // Navigate to flash screen after 3 seconds
    ui_router_navigate_to(SCREEN_ID_FLASH);
}
```

**3. User Input Navigation:**

**Nguồn:** `components/sx_ui/screens/screen_common.c:L12-L20`

```c
static void back_btn_event_cb(lv_event_t *e) {
    if (code == LV_EVENT_CLICKED) {
        if (lvgl_port_lock(0)) {
            ui_router_navigate_to(SCREEN_ID_HOME);
            lvgl_port_unlock();
        }
    }
}
```

**4. List Item Navigation:**

**Nguồn:** `components/sx_ui/screens/screen_common.c:L72-L82`

```c
static void list_item_click_cb(lv_event_t *e) {
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *item = lv_event_get_target(e);
        ui_screen_id_t screen_id = (ui_screen_id_t)(intptr_t)lv_obj_get_user_data(item);
        if (lvgl_port_lock(0)) {
            ui_router_navigate_to(screen_id);
            lvgl_port_unlock();
        }
    }
}
```

**Phân tích:**
- ✅ **Multiple triggers:** Timer, user input, list items
- ✅ **LVGL lock:** Tất cả navigation calls protected
- ⚠️ **Navigation trong main loop:** Có thể gây confusion, nên move vào event handler

### 3.3 Navigation Safety

**Duplicate Prevention:**

**Nguồn:** `components/sx_ui/ui_router.c:L71-L76`

```c
// Prevent duplicate navigation to same screen
if (s_current_screen == screen_id) {
    ESP_LOGD(TAG, "Already on screen %s (id: %d), skipping navigation",
             ui_screen_registry_get_name(screen_id), screen_id);
    return;
}
```

**Invalid Screen Check:**

```c
if (screen_id >= SCREEN_ID_MAX) {
    ESP_LOGE(TAG, "Invalid screen_id: %d", screen_id);
    return;
}
```

**Unregistered Screen Check:**

```c
const ui_screen_callbacks_t *callbacks = ui_screen_registry_get(screen_id);
if (callbacks == NULL) {
    ESP_LOGW(TAG, "Screen %s (id: %d) not registered, skipping navigation",
             ui_screen_registry_get_name(screen_id), screen_id);
    return;
}
```

**Phân tích:**
- ✅ **Safety checks:** Duplicate, invalid, unregistered screen checks
- ✅ **Error handling:** Log warnings/errors, skip invalid navigation

---

## 4. ASSET LOADING STRATEGY

### 4.1 Asset Types

**1. Embedded Images (Bootscreen, Flashscreen):**

**Nguồn:** `components/sx_ui/ui_helpers/ui_assets_wrapper.c`

```c
const lv_img_dsc_t* ui_assets_get_bootscreen_img(void) {
    return &bootscreen_img;  // Generated file: bootscreen_img.h
}

const lv_img_dsc_t* ui_assets_get_flashscreen_img(void) {
    return &flashscreen_img;  // Generated file: flashscreen_img.h
}
```

**Generated Files:**
- `components/sx_assets/generated/bootscreen_img.c`
- `components/sx_assets/generated/flashscreen_img.c`

**Phân tích:**
- ✅ **Embedded in flash:** Bootscreen và flashscreen embedded trong binary
- ✅ **LVGL-compatible:** Wrapped thành `lv_img_dsc_t` trong `sx_ui`
- ✅ **Architecture compliance:** `sx_assets` không include LVGL, wrapper trong `sx_ui`

**2. SD Card Assets (Future):**

**Nguồn:** `components/sx_assets/sx_assets.c:L49-L75`

```c
sx_asset_handle_t sx_assets_load_rgb565(const char *path, sx_asset_info_t *info) {
    // Stub implementation - will be implemented in Phase 3
    ESP_LOGW(TAG, "Asset loading stub - will be fully implemented in Phase 3");
    return NULL;
}
```

**Phân tích:**
- ⚠️ **Not implemented:** SD card asset loading chưa implement
- ✅ **API ready:** Interface đã định nghĩa, chờ implementation

### 4.2 Asset Loading Flow

**Embedded Image Flow:**

```
Screen on_create()
    ↓
ui_assets_get_bootscreen_img()
    ↓
bootscreen_img (generated file)
    ↓
lv_img_set_src(image_obj, bootscreen_img)
    ↓
LVGL renders image
```

**SD Card Asset Flow (Future):**

```
Screen on_create()
    ↓
sx_assets_load_rgb565("assets/image.bin", &info)
    ↓
Read from SD card
    ↓
Allocate buffer (PSRAM)
    ↓
Load RGB565 data
    ↓
Wrap to LVGL format
    ↓
lv_img_set_src(image_obj, wrapped_img)
```

### 4.3 Memory Usage

**Embedded Images:**
- **Bootscreen:** 320x480x2 = 307,200 bytes (~300KB) in flash
- **Flashscreen:** 320x480x2 = 307,200 bytes (~300KB) in flash
- **Total:** ~600KB embedded images

**SD Card Assets:**
- **Future:** Loaded into PSRAM dynamically
- **Buffer management:** Cần implement buffer pool pattern

**LVGL Buffers:**
- **Display buffer:** 320*30*2 = 19,200 bytes (~19KB) in PSRAM
- **Double buffer:** 2x = 38,400 bytes (~38KB) in PSRAM

**Phân tích:**
- ✅ **PSRAM usage:** Display buffers trong PSRAM, tiết kiệm internal RAM
- ⚠️ **Embedded images:** 600KB trong flash — có thể optimize bằng compression
- ⚠️ **SD asset loading:** Chưa implement, cần buffer management

---

## 5. INPUT DEVICE HANDLING

### 5.1 Touch Input Flow

**Touch Driver → LVGL:**

```
FT5x06 Touch Controller (I2C)
    ↓
esp_lcd_touch driver
    ↓
lvgl_port_touch (esp_lvgl_port)
    ↓
LVGL Input Device (lv_indev_t)
    ↓
LVGL Event System
    ↓
Screen Event Handlers
```

**Nguồn:** `components/sx_ui/sx_ui_task.c:L123-L158`

**Touch Registration:**

```c
if (touch_handles != NULL && touch_handles->touch_handle != NULL) {
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = touch_handles->touch_handle,
    };
    lv_indev_t *touch_indev = lvgl_port_add_touch(&touch_cfg);
}
```

**Phân tích:**
- ✅ **Standard LVGL input:** Sử dụng esp_lvgl_port touch driver
- ✅ **Optional touch:** Có thể boot không có touch
- ✅ **Event-driven:** Touch events qua LVGL event system

### 5.2 Touch Event Handling

**Screen Event Handlers:**

**Nguồn:** `components/sx_ui/screens/screen_common.c:L12-L20`

```c
static void back_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        if (lvgl_port_lock(0)) {
            ui_router_navigate_to(SCREEN_ID_HOME);
            lvgl_port_unlock();
        }
    }
}
```

**List Item Click:**

```c
static void list_item_click_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *item = lv_event_get_target(e);
        ui_screen_id_t screen_id = (ui_screen_id_t)(intptr_t)lv_obj_get_user_data(item);
        if (lvgl_port_lock(0)) {
            ui_router_navigate_to(screen_id);
            lvgl_port_unlock();
        }
    }
}
```

**Phân tích:**
- ✅ **LVGL events:** Sử dụng standard LVGL event system
- ✅ **LVGL lock:** Tất cả navigation calls protected
- ✅ **User data pattern:** Screen ID stored trong `lv_obj_set_user_data()`

---

## 6. PERFORMANCE & MEMORY ANALYSIS

### 6.1 Rendering Performance

**Frame Rate Target:**

**Nguồn:** `components/sx_ui/sx_ui_task.c:L197-L198`

```c
const TickType_t render_interval = pdMS_TO_TICKS(16);  // ~60 FPS
vTaskDelayUntil(&last_wake_time, render_interval);
```

**LVGL Timer Handler:**

```c
if (lvgl_port_lock(0)) {
    lv_timer_handler();  // Process LVGL timers and rendering
    lvgl_port_unlock();
}
```

**Render Time Tracking:**

```c
TickType_t render_start = xTaskGetTickCount();
if (lvgl_port_lock(0)) {
    lv_timer_handler();
    lvgl_port_unlock();
}
TickType_t render_end = xTaskGetTickCount();
uint32_t render_ms = (render_end - render_start) * portTICK_PERIOD_MS;
sx_metrics_update_ui_render(render_ms);
```

**Phân tích:**
- ✅ **60 FPS target:** 16ms interval
- ✅ **Metrics tracking:** Render time được track
- ⚠️ **No FPS monitoring:** Chưa có FPS drop detection

### 6.2 Memory Usage

**UI Task Stack:**
- **Size:** 8192 bytes (8KB)
- **Usage:** LVGL objects, state polling, event handling

**LVGL Internal Task:**
- **Stack:** 6144 bytes (6KB)
- **Usage:** LVGL rendering, timer processing

**Display Buffers:**
- **Single buffer:** 320*30*2 = 19,200 bytes (~19KB)
- **Double buffer:** 2x = 38,400 bytes (~38KB)
- **Location:** PSRAM

**Embedded Images:**
- **Bootscreen:** 307,200 bytes (~300KB) in flash
- **Flashscreen:** 307,200 bytes (~300KB) in flash
- **Total:** ~600KB in flash

**Screen Objects:**
- **Per screen:** Variable (depends on screen complexity)
- **Container:** Shared across all screens
- **Memory:** LVGL object pool (managed by LVGL)

**Phân tích:**
- ✅ **PSRAM buffers:** Display buffers trong PSRAM
- ⚠️ **Embedded images:** 600KB trong flash — có thể optimize
- ⚠️ **Screen objects:** Không có limit, có thể memory leak nếu không cleanup đúng

### 6.3 Performance Optimizations

**1. State Change Detection:**

**Nguồn:** `components/sx_ui/sx_ui_task.c:L194-L205`

```c
uint32_t last_state_seq = 0;

bool state_changed = (state.seq != last_state_seq);
if (state_changed) {
    last_state_seq = state.seq;
    // Update UI only if state changed
    callbacks->on_update(&state);
}
```

**Phân tích:**
- ✅ **Optimization:** Chỉ update UI khi state thay đổi
- ✅ **Sequence tracking:** Dùng `state.seq` để detect changes

**2. Fixed Interval Rendering:**

```c
TickType_t last_wake_time = xTaskGetTickCount();
const TickType_t render_interval = pdMS_TO_TICKS(16);

vTaskDelayUntil(&last_wake_time, render_interval);
```

**Phân tích:**
- ✅ **Consistent frame rate:** `vTaskDelayUntil` đảm bảo consistent timing
- ✅ **No busy waiting:** Sleep khi không có work

**3. Double Buffering:**

```c
.double_buffer = true,  // Enable double buffering for smoother rendering
```

**Phân tích:**
- ✅ **Smooth rendering:** Double buffering giảm tearing
- ⚠️ **Memory cost:** 2x buffer size (38KB thay vì 19KB)

---

## 7. ANIMATIONS SYSTEM

### 7.1 Animation API

**Nguồn:** `components/sx_ui/ui_animations.c`

**Animation Types:**

```c
typedef enum {
    UI_ANIM_FADE_IN,
    UI_ANIM_FADE_OUT,
    UI_ANIM_SLIDE_IN_LEFT,
    UI_ANIM_SLIDE_IN_RIGHT,
    UI_ANIM_SLIDE_IN_TOP,
    UI_ANIM_SLIDE_IN_BOTTOM,
    UI_ANIM_SCALE_IN,
    UI_ANIM_SCALE_OUT,
} ui_anim_type_t;
```

**Animation Config:**

```c
typedef struct {
    ui_anim_type_t type;
    uint32_t duration_ms;
    uint32_t delay_ms;
    lv_anim_path_cb_t path_cb;  // Optional custom path
} ui_anim_config_t;
```

**Usage:**

```c
ui_anim_config_t config = {
    .type = UI_ANIM_FADE_IN,
    .duration_ms = 300,
    .delay_ms = 0,
};
ui_anim_apply(obj, &config);
```

**Phân tích:**
- ✅ **LVGL-based:** Sử dụng LVGL animation system
- ✅ **Multiple types:** Fade, slide, scale animations
- ✅ **Custom path:** Support custom animation paths

### 7.2 Animation Performance

**LVGL Animation:**
- **CPU:** Animation calculations trong LVGL timer
- **Memory:** Animation state trong LVGL object
- **Performance:** Phụ thuộc vào số lượng animations active

**Phân tích:**
- ⚠️ **No animation limit:** Không có limit số animations, có thể impact performance
- ✅ **LVGL optimized:** LVGL animation system đã optimized

---

## 8. LỖI TIỀM ẨN & NỢ KỸ THUẬT

### 8.1 P0 (Critical) Issues

**Không có P0 issues**

### 8.2 P1 (High) Issues

1. **Navigation trong Main Loop**
   - **Vị trí:** `components/sx_ui/sx_ui_task.c:L207-L247`
   - **Vấn đề:** Navigation logic trong UI task main loop, không phải event-driven
   - **Hậu quả:** Có thể gây confusion, khó debug
   - **Cách sửa:** Move navigation logic vào event handler hoặc screen callbacks

2. **SD Asset Loading Chưa Implement**
   - **Vị trí:** `components/sx_assets/sx_assets.c:L49-L75`
   - **Vấn đề:** `sx_assets_load_rgb565()` chỉ là stub
   - **Hậu quả:** Không thể load assets từ SD card
   - **Cách sửa:** Implement SD card asset loading với buffer management

3. **Screen Object Memory Leak Risk**
   - **Vị trí:** Tất cả screens
   - **Vấn đề:** Không có limit số LVGL objects per screen
   - **Hậu quả:** Memory leak nếu screen tạo nhiều objects không cleanup
   - **Cách sửa:** Add object counting, cleanup verification

### 8.3 P2 (Medium) Issues

1. **Embedded Images Size**
   - **Vị trí:** `components/sx_assets/generated/bootscreen_img.c`, `flashscreen_img.c`
   - **Vấn đề:** 600KB embedded images trong flash
   - **Cách sửa:** Compress images, hoặc load từ SD card

2. **No FPS Monitoring**
   - **Vị trí:** `components/sx_ui/sx_ui_task.c:L281-L288`
   - **Vấn đề:** Có render time tracking nhưng không có FPS drop detection
   - **Cách sửa:** Add FPS calculation và drop detection

3. **Animation Limit**
   - **Vị trí:** `components/sx_ui/ui_animations.c`
   - **Vấn đề:** Không có limit số animations active
   - **Cách sửa:** Add animation count limit, hoặc priority system

---

## 9. KẾT LUẬN PHASE 4

### 9.1 Điểm Mạnh

1. ✅ **Clear lifecycle:** Screen lifecycle rõ ràng (create, show, hide, destroy)
2. ✅ **Container pattern:** Tất cả screens share container, dễ manage
3. ✅ **LVGL integration:** Standard LVGL port, thread-safe
4. ✅ **State-driven updates:** UI update từ state snapshot
5. ✅ **60 FPS target:** Consistent frame rate với fixed interval
6. ✅ **PSRAM buffers:** Display buffers trong PSRAM
7. ✅ **Animation system:** LVGL-based animations

### 9.2 Điểm Yếu

1. ⚠️ **Navigation trong main loop:** Không event-driven
2. ⚠️ **SD asset loading:** Chưa implement
3. ⚠️ **Memory leak risk:** Không có object counting
4. ⚠️ **Embedded images:** 600KB trong flash
5. ⚠️ **No FPS monitoring:** Chưa có FPS drop detection

### 9.3 Hành Động Tiếp Theo

**PHASE 5:** Phân tích Audio Pipeline  
**PHASE 6:** Phân tích Network/AI/Protocols  
**PHASE 7:** Phân tích Storage & Persistence

---

## 10. CHECKLIST HOÀN THÀNH PHASE 4

- [x] Phân tích UI framework architecture
- [x] Phân tích screen lifecycle (create, show, hide, destroy)
- [x] Phân tích navigation system
- [x] Phân tích asset loading strategy (embedded vs SD)
- [x] Phân tích touch input handling
- [x] Phân tích memory usage
- [x] Phân tích performance (FPS, render time)
- [x] Phân tích animations system
- [x] Xác định lỗi tiềm ẩn và nợ kỹ thuật
- [x] Tạo REPORT_PHASE_4_UI.md

---

## 11. THỐNG KÊ FILE ĐÃ ĐỌC

**Tổng số file đã đọc trong Phase 4:** ~15 files

**Danh sách:**
1. `components/sx_ui/sx_ui_task.c`
2. `components/sx_ui/ui_router.c`
3. `components/sx_ui/ui_screen_registry.c`
4. `components/sx_ui/sx_screen_if.c`
5. `components/sx_ui/sx_lvgl_lock.c`
6. `components/sx_ui/sx_lvgl_guard.c`
7. `components/sx_ui/screens/register_all_screens.c`
8. `components/sx_ui/screens/screen_boot.c`
9. `components/sx_ui/ui_helpers/ui_assets_wrapper.c`
10. `components/sx_ui/screens/screen_common.c`
11. `components/sx_ui/ui_animations.c` (partial)
12. `components/sx_ui/include/ui_screen_registry.h`
13. `components/sx_ui/include/ui_router.h`
14. `components/sx_assets/sx_assets.c` (partial)

**Ước lượng % file đã đọc:** ~15-18% (đọc UI-critical files)

---

**Kết thúc PHASE 4**

