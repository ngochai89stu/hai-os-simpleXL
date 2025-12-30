# Rà Soát Luồng Khởi Tạo Screen Theo Kiến Trúc SimpleXL

## Tổng Quan Kiến Trúc SimpleXL

Theo `docs/SIMPLEXL_ARCH.md`:

### Nguyên Tắc Cốt Lõi
1. **Single UI owner task** cho tất cả LVGL calls
2. **Services không include UI headers**
3. **UI ↔ services communication** chỉ qua events và state snapshots
4. **Orchestrator** là single writer cho state
5. **UI task** chỉ đọc state snapshots, không gọi service APIs trực tiếp

### Component Boundaries
- `sx_core`: Event/State/Dispatcher/Orchestrator
- `sx_ui`: UI task (LVGL init + render loop)
- `sx_platform`: LCD/touch/backlight/SD drivers
- `sx_services`: Audio/WiFi/IR/MCP/Chatbot

## Luồng Khởi Tạo Hiện Tại

### 1. Bootstrap Phase (sx_bootstrap.c)

```
1. NVS Flash Init
2. Settings Service Init (Phase 5)
3. Theme Service Init (Phase 5)
4. OTA Service Init (Phase 5) - DISABLED FOR TEST
5. Dispatcher Init
6. Orchestrator Start (single writer for state)
7. Platform Display Init
8. Platform Touch Init
9. SPI Bus Manager Init - DISABLED FOR TEST
10. SD Service Init - DISABLED FOR TEST
11. Assets Init
12. UI Start (display + touch handles)
13. Brightness Restore
14. Audio Services Init (ducking, power, router)
15. Audio Service Init
```

### 2. UI Task Phase (sx_ui_task.c)

```
1. LVGL Port Init
2. LVGL Lock
3. Validate Display Handles
4. LVGL Display Add
5. UI Router Init
   - Screen Registry Init
   - Create Screen Container
6. Register All Screens (28 screens)
7. Touch Input Device Add (if available)
8. Navigate to Boot Screen
9. Post UI Ready Event
10. Main Render Loop
```

## Phân Tích Dependencies

### Screen Dependencies

#### Screens Có Dependencies Vào Services

1. **screen_flash.c**
   - `sx_settings_service.h` (screensaver_bg_image, screensaver_custom_bg_path)
   - **Vấn đề:** Nếu settings service không init, sẽ crash khi truy cập

2. **screen_google_navigation.c**
   - `sx_settings_service.h` (nav_speed_limit)
   - **Vấn đề:** Tương tự screen_flash

3. **screen_radio.c**
   - `sx_state.h`, `sx_dispatcher.h`
   - **Vấn đề:** Cần orchestrator và dispatcher init

4. **screen_sd_card_music.c**
   - `sx_state.h`
   - **Vấn đề:** Cần orchestrator init

5. **screen_settings.c**
   - `sx_state.h`
   - **Vấn đề:** Cần orchestrator init

6. **screen_equalizer.c**
   - `sx_state.h`
   - **Vấn đề:** Cần orchestrator init

#### Screens Không Có Dependencies (Safe)

- screen_boot.c
- screen_home.c (có thể có dependencies nhưng chưa rõ)
- screen_touch_debug.c
- screen_about.c
- screen_permission.c
- screen_screensaver.c

### Core Dependencies

#### Orchestrator và Dispatcher

```c
// sx_bootstrap.c
sx_dispatcher_init();        // Line 89
sx_orchestrator_start();     // Line 95
```

**Vấn đề:** Nếu orchestrator không start, state sẽ không được init, các screen dùng `sx_state` sẽ crash.

#### Settings Service

```c
// sx_bootstrap.c
sx_settings_service_init();  // Line 63
```

**Vấn đề:** Nếu settings service không init, các screen dùng `sx_settings_*` sẽ crash.

## Nguyên Nhân Lỗi Khi Chỉ Giữ UI và Touch

### Vấn Đề 1: Screen Dependencies Không Được Kiểm Tra

Khi tắt các services (audio, wifi, etc.), các screen vẫn được register và có thể được navigate đến. Khi `onCreate()` hoặc `onShow()` được gọi:

1. **screen_flash.c** gọi `sx_settings_get_string_default()` → Settings service không init → **LoadProhibited**
2. **screen_radio.c** truy cập `sx_state` → Orchestrator không init state → **LoadProhibited**
3. **screen_sd_card_music.c** truy cập `sx_state` → **LoadProhibited**

### Vấn Đề 2: State Không Được Init Đầy Đủ

Nếu orchestrator không start hoặc state không được init đúng, các screen dùng `sx_state` sẽ truy cập vào memory không hợp lệ.

### Vấn Đề 3: Settings Service Không Init

Nếu settings service không init, các screen dùng `sx_settings_*` sẽ crash.

## Giải Pháp Đề Xuất

### Giải Pháp 1: Kiểm Tra NULL Trong Screens (Ngắn Hạn)

Thêm NULL checks trong các screen có dependencies:

```c
// screen_flash.c
static void onCreate(void) {
    // Check settings service
    if (sx_settings_service_is_initialized()) {
        sx_settings_get_string_default("screensaver_bg_image", ...);
    } else {
        // Use default values
        strcpy(bg_setting, "Embedded");
    }
}
```

### Giải Pháp 2: Conditional Screen Registration (Trung Hạn)

Chỉ register screens khi dependencies sẵn sàng:

```c
// register_all_screens.c
void register_all_screens(void) {
    // Core screens (always available)
    screen_boot_register();
    screen_flash_register();
    screen_home_register();
    
    // Screens requiring settings service
    if (sx_settings_service_is_initialized()) {
        screen_google_navigation_register();
    }
    
    // Screens requiring state/orchestrator
    if (sx_orchestrator_is_initialized()) {
        screen_radio_register();
        screen_sd_card_music_register();
        screen_settings_register();
        screen_equalizer_register();
    }
}
```

### Giải Pháp 3: Stub Services (Dài Hạn)

Tạo stub implementations cho các services khi chúng bị disable:

```c
// sx_settings_service_stub.c
bool sx_settings_service_is_initialized(void) {
    return false;
}

esp_err_t sx_settings_get_string_default(const char *key, char *value, size_t len, const char *default_value) {
    if (default_value) {
        strncpy(value, default_value, len - 1);
        value[len - 1] = '\0';
    } else {
        value[0] = '\0';
    }
    return ESP_OK;
}
```

### Giải Pháp 4: Đảm Bảo Core Services Luôn Init (Khuyến Nghị)

Theo kiến trúc SimpleXL, các core services (dispatcher, orchestrator, settings) nên luôn được init, ngay cả khi các tính năng khác bị tắt:

```c
// sx_bootstrap.c
// 1. Core services (REQUIRED)
sx_dispatcher_init();
sx_orchestrator_start();
sx_settings_service_init();  // REQUIRED for screens

// 2. Platform (REQUIRED for UI)
sx_platform_display_init();
sx_platform_touch_init();

// 3. UI (REQUIRED)
sx_ui_start();

// 4. Optional services (can be disabled)
// Audio, WiFi, etc.
```

## Kiểm Tra Luồng Khởi Tạo Đúng

### Thứ Tự Init Đúng (Theo SimpleXL)

```
1. NVS Flash
2. Core Services:
   - Dispatcher (REQUIRED)
   - Orchestrator (REQUIRED)
   - Settings Service (REQUIRED for some screens)
3. Platform:
   - Display (REQUIRED)
   - Touch (OPTIONAL but recommended)
4. UI:
   - UI Task Start (REQUIRED)
   - Router Init
   - Screen Registration
   - Navigate to Boot Screen
5. Optional Services:
   - Audio, WiFi, etc. (can be lazy-loaded)
```

### Checklist

- [ ] Dispatcher được init trước UI
- [ ] Orchestrator được start trước UI
- [ ] Settings service được init nếu có screen dùng nó
- [ ] Display được init trước UI
- [ ] Touch được init (optional) trước UI
- [ ] UI task được start sau tất cả dependencies
- [ ] Screens có NULL checks cho dependencies

## Kết Luận

**Nguyên nhân lỗi khi chỉ giữ UI và Touch:**

1. **Screen dependencies không được kiểm tra:** Các screen như `screen_flash`, `screen_radio` vẫn được register và gọi services không tồn tại
2. **Core services không được init:** Dispatcher, Orchestrator, Settings service cần được init trước UI
3. **Thiếu NULL checks:** Screens không kiểm tra services có sẵn sàng không

**Giải pháp khuyến nghị:**

1. **Ngắn hạn:** Thêm NULL checks trong screens
2. **Trung hạn:** Conditional screen registration
3. **Dài hạn:** Đảm bảo core services (dispatcher, orchestrator, settings) luôn được init, ngay cả khi các tính năng khác bị tắt

**Theo kiến trúc SimpleXL:** Core services (dispatcher, orchestrator, settings) là **REQUIRED** và không nên bị tắt, chỉ các tính năng optional (audio, wifi, etc.) mới có thể lazy-load hoặc disable.


