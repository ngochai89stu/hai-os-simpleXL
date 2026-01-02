# PHASE 0 — Baseline & Build Intelligence
## Báo cáo phân tích cấu hình build và entrypoint

**Ngày tạo:** 2025-01-02  
**Dự án:** hai-os-simplexl  
**Mục tiêu:** Xác định nền tảng kỹ thuật, entrypoint, và luồng khởi động

---

## 1. THÔNG TIN NỀN TẢNG KỸ THUẬT

### 1.1 ESP-IDF Version & Target Chip

**Nguồn:** `sdkconfig:L3`

```
ESP-IDF Version: 5.5.1
Target Chip: ESP32-S3
Architecture: Xtensa
```

**Chi tiết từ sdkconfig:**
- `CONFIG_IDF_TARGET="esp32s3"` (sdkconfig:L395)
- `CONFIG_IDF_TARGET_ARCH_XTENSA=y` (sdkconfig:L391)
- `CONFIG_ESP32S3_REV_MIN_0=y` (sdkconfig:L1252)
- `CONFIG_ESP32S3_REV_MIN_FULL=0` (sdkconfig:L1255)

### 1.2 CPU & Memory Configuration

**CPU Frequency:**
- `CONFIG_ESP32S3_DEFAULT_CPU_FREQ_MHZ=160` (sdkconfig:L3046)
- CPU chạy ở 160MHz (không phải 240MHz để tiết kiệm năng lượng)

**Cache Configuration:**
- Instruction Cache: 16KB, 8-way, 32B line (sdkconfig:L1488-L1496)
- Data Cache: 32KB, 8-way, 32B line (sdkconfig:L1498-L1507)

**SPIRAM Configuration:**
- `CONFIG_SPIRAM=y` (sdkconfig:L1427) — **PSRAM được bật**
- `CONFIG_SPIRAM_MODE_OCT=y` (sdkconfig:L1433) — **Octal mode (8-bit)**
- `CONFIG_SPIRAM_SPEED=80` (sdkconfig:L1443) — **80MHz**
- `CONFIG_SPIRAM_CLK_IO=30` (sdkconfig:L1436)
- `CONFIG_SPIRAM_CS_IO=26` (sdkconfig:L1437)
- `CONFIG_SPIRAM_USE_MALLOC=y` (sdkconfig:L1450) — **Heap allocator sử dụng PSRAM**
- `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384` (sdkconfig:L1452) — **16KB luôn dùng internal RAM**
- `CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768` (sdkconfig:L1454) — **32KB reserve internal**
- `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=y` (sdkconfig:L3110) — **Stack có thể ở PSRAM**

### 1.3 FreeRTOS Configuration

**Nguồn:** `sdkconfig:L1740-L1798`

- **Tick Rate:** `CONFIG_FREERTOS_HZ=100` (100Hz = 10ms/tick)
- **Stack Overflow Check:** `CONFIG_FREERTOS_CHECK_STACKOVERFLOW_CANARY=y` (sdkconfig:L1743)
- **Idle Task Stack:** 1536 bytes (sdkconfig:L1745)
- **Max Task Name Length:** 16 chars (sdkconfig:L1748)
- **Timer Service:**
  - Task Name: "Tmr Svc" (sdkconfig:L1751)
  - Stack Depth: **4096 bytes** (sdkconfig:L1757) — **Đã tăng từ mặc định để fix overflow**
  - Priority: 1 (sdkconfig:L1756)
  - Queue Length: 10 (sdkconfig:L1758)
- **Number of Cores:** 2 (sdkconfig:L1798)
- **ISR Stack:** 1536 bytes (sdkconfig:L1775)
- **Task Create Allow External Memory:** `y` (sdkconfig:L1789) — **Task có thể tạo stack ở PSRAM**

### 1.4 Main Task & System Stack Sizes

**Nguồn:** `sdkconfig:L3049, L1542`

- **Main Task Stack:** `CONFIG_ESP_MAIN_TASK_STACK_SIZE=3584` (3.5KB)
- **System Event Task Stack:** 2304 bytes (sdkconfig:L3048)
- **IPC Task Stack:** 1280 bytes (sdkconfig:L3068)
- **Timer Task Stack:** 3584 bytes (sdkconfig:L3069)

### 1.5 Compiler & Optimization

**Nguồn:** `sdkconfig:L835-L861`

- **Optimization Level:** `CONFIG_COMPILER_OPTIMIZATION_SIZE=y` (sdkconfig:L835) — **Size optimization**
- **Assertions:** Enabled, Level 2 (sdkconfig:L838, L842)
- **Stack Check:** Disabled (sdkconfig:L848) — **⚠️ Rủi ro: không check stack overflow tại compile time**
- **RT Lib:** GCC (sdkconfig:L860)

### 1.6 Logging Configuration

**Nguồn:** `sdkconfig:L1831-L1879`

- **Default Level:** INFO (Level 3) (sdkconfig:L1844)
- **Maximum Level:** INFO (Level 3) (sdkconfig:L1848)
- **Dynamic Level Control:** Enabled (sdkconfig:L1854)
- **Timestamp Source:** RTOS tick (sdkconfig:L1868)
- **Log Mode:** Text (sdkconfig:L1876)
- **Log in IRAM:** Enabled (sdkconfig:L1879) — **Log có thể chạy từ IRAM**

---

## 2. PARTITION TABLE

**Nguồn:** `partitions.csv`

### 2.1 Partition Layout

| Tên | Type | SubType | Offset | Size | Mục đích |
|-----|------|---------|--------|------|----------|
| `nvs` | data | nvs | 0x9000 | 24KB | Non-volatile storage (WiFi, settings) |
| `phy_init` | data | phy | 0xf000 | 4KB | PHY calibration data |
| `factory` | app | factory | 0x10000 | 3MB | Application binary |
| `spiffs` | data | spiffs | 0x310000 | 1MB | SPIFFS filesystem (assets) |
| `model` | data | fat | 0x410000 | 2MB | FAT filesystem (models, media) |

**Tổng Flash sử dụng:** ~6MB (trên ESP32-S3 thường có 8MB hoặc 16MB)

**Phân tích:**
- **Factory partition (3MB):** Đủ lớn cho ứng dụng hiện tại, nhưng cần monitor khi thêm tính năng
- **SPIFFS (1MB):** Dùng cho assets tĩnh (fonts, icons, images)
- **FAT (2MB):** Dùng cho models (AI), media files, có thể mount từ SD card

### 2.2 Partition Table Build Output

**Nguồn:** `build_output_20260102_091519.log:L101-L126`

Build log xác nhận partition table được generate đúng:
```
nvs,data,nvs,0x9000,24K,
phy_init,data,phy,0xf000,4K,
factory,app,factory,0x10000,3M,
spiffs,data,spiffs,0x310000,1M,
model,data,fat,0x410000,2M,
```

---

## 3. ENTRYPOINT & BOOT FLOW

### 3.1 Entrypoint Chain

**File:** `app/app_main.c:L8-L16`

```c
void app_main(void) {
    ESP_LOGI(TAG, "hai-os-simplexl starting...");
    ESP_ERROR_CHECK(sx_bootstrap_start());
}
```

**Luồng:**
1. ESP-IDF bootloader khởi động
2. `app_main()` được gọi (ESP-IDF main task)
3. `sx_bootstrap_start()` được gọi ngay lập tức

### 3.2 Bootstrap Flow Chi Tiết

**File:** `components/sx_core/sx_bootstrap.c:L55-L831`

Bootstrap được chia thành các phase rõ ràng:

#### **Phase 1: NVS & Core Infrastructure** (L61-L128)

```55:128:components/sx_core/sx_bootstrap.c
esp_err_t sx_bootstrap_start(void) {
    // 1) Init NVS (required for WiFi, settings, etc.)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs erase (no free pages / new version)");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Phase 4: Initialize centralized error handler
    ret = sx_error_handler_init();
    
    // Init settings service (Phase 5)
    ret = sx_settings_service_init();
    
    // Init theme service (Phase 5)
    ret = sx_theme_service_init();
    
    // Init OTA service (Phase 5)
    ret = sx_ota_service_init();
    
    // Init OTA full service
    ret = sx_ota_full_service_init();

    // 1.5) Initialize MCP server early (before dispatcher consumers)
    esp_err_t mcp_ret = sx_mcp_server_init();
    mcp_ret = sx_mcp_tools_register_all();

    // 2) Init dispatcher
    if (!sx_dispatcher_init()) {
        ESP_LOGE(TAG, "dispatcher init failed");
        return ESP_FAIL;
    }

    // 3) Start orchestrator (single writer for state)
    sx_orchestrator_start();
```

**Phân tích:**
- NVS được init với auto-erase nếu cần (L63-L68)
- Error handler, settings, theme, OTA được init sớm (L70-L108)
- MCP server được init trước dispatcher (L110-L121) — **Thứ tự quan trọng**
- Dispatcher (event bus) được init (L123-L129)
- Orchestrator (state manager) được start (L131-L134)

#### **Phase 2: Platform Hardware** (L136-L189)

```136:189:components/sx_core/sx_bootstrap.c
    // 4) Init platform hardware (display)
    sx_display_handles_t display_handles = sx_platform_display_init();
    if (display_handles.panel_handle == NULL) {
        ESP_LOGE(TAG, "Display platform init failed");
        return ESP_FAIL;
    }

    // 5) Phase 3: Init touch driver
#if CONFIG_HAI_TOUCH_ENABLE
    sx_touch_handles_t touch_handles = {0};
    esp_err_t touch_ret = sx_platform_touch_init(&touch_handles);
    if (touch_ret != ESP_OK) {
        ESP_LOGW(TAG, "Touch init failed (non-critical): %s", esp_err_to_name(touch_ret));
        // Continue without touch - not critical for boot
    }
#else
    sx_touch_handles_t touch_handles = {0};  // Zero-initialized for disabled touch
#endif

    // 5.5) Init SPI bus manager (required for SD service)
    ret = sx_spi_bus_manager_init();
```

**Phân tích:**
- Display init là **critical** — nếu fail thì return ESP_FAIL (L140-L143)
- Touch init là **non-critical** — có thể boot không có touch (L145-L180)
- SPI bus manager được init để chuẩn bị cho SD service (L183-L189)

#### **Phase 3: Storage & Assets** (L191-L219)

```191:219:components/sx_core/sx_bootstrap.c
    // 6) Phase 4: Init SD service (mount SD card)
    sx_sd_config_t sd_cfg = {
        .mount_point = SX_SD_MOUNT_POINT,
        .spi_host = SPI3_HOST,
        .miso_gpio = 12,  // SD MISO (SDO)
        .mosi_gpio = 47,  // SD MOSI
        .sclk_gpio = 21,  // SD SCLK
        .cs_gpio = 10,    // SD CS
    };
    esp_err_t sd_ret = sx_sd_service_init(&sd_cfg);
    if (sd_ret != ESP_OK) {
        ESP_LOGW(TAG, "SD service init failed (non-critical): %s", esp_err_to_name(sd_ret));
    } else {
        sd_ret = sx_sd_service_start();
        if (sd_ret != ESP_OK) {
            ESP_LOGW(TAG, "SD mount failed (non-critical): %s", esp_err_to_name(sd_ret));
        } else {
            ESP_LOGI(TAG, "SD mounted");
            sx_assets_set_sd_ready(true);
        }
    }

    // 7) Phase 3/4: Init asset loader (does not mount, only prepares)
    esp_err_t assets_ret = sx_assets_init();
```

**Phân tích:**
- SD service init với pin config hardcoded (L192-L199) — **⚠️ Nên move vào Kconfig**
- SD mount là non-critical (L200-L211)
- Asset loader chỉ prepare, không mount (L213-L219)

#### **Phase 4: UI Initialization** (L221-L240)

```221:240:components/sx_core/sx_bootstrap.c
    // 8) Start UI (owner task) with the display handle and optional touch
#if CONFIG_HAI_TOUCH_ENABLE
    ESP_ERROR_CHECK(sx_ui_start(&display_handles, &touch_handles));
#else
    sx_touch_handles_t touch_handles = {0};  // Zero-initialized for disabled touch
    ESP_ERROR_CHECK(sx_ui_start(&display_handles, &touch_handles));
#endif
    
    // Load and apply saved brightness setting
    int32_t saved_brightness = CONFIG_SX_DISPLAY_BRIGHTNESS_DEFAULT;
    if (sx_settings_get_int_default("display_brightness", &saved_brightness, CONFIG_SX_DISPLAY_BRIGHTNESS_DEFAULT) == ESP_OK) {
        if (saved_brightness < 0) saved_brightness = 0;
        if (saved_brightness > 100) saved_brightness = 100;
        esp_err_t brightness_ret = sx_platform_set_brightness((uint8_t)saved_brightness);
        if (brightness_ret == ESP_OK) {
            ESP_LOGI(TAG, "Restored brightness to %d%%", saved_brightness);
        }
    }
```

**Phân tích:**
- UI được start với display và touch handles (L222-L227)
- Brightness được restore từ settings (L229-L240)

#### **Phase 5: Essential Services** (L242-L412)

```367:412:components/sx_core/sx_bootstrap.c
    // Audio Ducking, Power, Router - KEEP IN BOOTSTRAP (needed for audio service)
    // Audio Ducking (must be initialized before audio service)
    sx_audio_ducking_config_t ducking_cfg = {
        .duck_level = 0.3f,        // 30% volume when ducked
        .fade_duration_ms = 200,    // 200ms fade duration
    };
    esp_err_t ducking_ret = sx_audio_ducking_init(&ducking_cfg);
    
    // Audio Power Management (must be before audio service)
    sx_audio_power_config_t power_cfg = {
        .timeout_ms = CONFIG_SX_AUDIO_POWER_SAVE_TIMEOUT_MS,
        .check_interval_ms = 1000,
        .enable_auto_power_save = CONFIG_SX_AUDIO_POWER_SAVE_ENABLE,
    };
    esp_err_t audio_power_ret = sx_audio_power_init(&power_cfg);
    
    // Audio Router
    esp_err_t audio_router_ret = sx_audio_router_init();
    
    // Audio
    esp_err_t audio_ret = sx_audio_service_init();
    if (audio_ret != ESP_OK) {
        ESP_LOGW(TAG, "Audio service init failed (non-critical): %s", esp_err_to_name(audio_ret));
    } else {
        audio_ret = sx_audio_service_start();
        if (audio_ret != ESP_OK) {
            ESP_LOGW(TAG, "Audio service start failed (non-critical): %s", esp_err_to_name(audio_ret));
        } else {
            ESP_LOGI(TAG, "Audio service started");
        }
    }
```

**Phân tích:**
- Audio Ducking, Power, Router được init trước Audio Service (L367-L399)
- Audio Service được init và start (L401-L412)
- **Lưu ý:** Nhiều service khác đã được chuyển sang **lazy loading** (commented out, L246-L805)

#### **Phase 6: Lazy Loading Strategy**

**Nguồn:** `components/sx_core/sx_bootstrap.c:L242-L805`

Các service sau đã được **chuyển sang lazy loading** (không init trong bootstrap):
- STT Service (L246-L283)
- Audio AFE (L285-L301)
- Wake Word Service (L303-L354)
- Playlist Manager (L356-L365)
- Radio Service (L414-L432)
- IR Service (L434-L461)
- Intent Service (L454-L461)
- Chatbot/MCP Protocol (L463-L555)
- Audio Protocol Bridge (L557-L573)
- WiFi Service (L575-L593)
- Music Online Service (L595-L636)
- TTS Service (L638-L674)
- Navigation Service (L676-L705)
- Telegram Service (L707-L743) — **DISABLED**
- Bluetooth Service (L745-L760)
- Diagnostics Service (L762-L771)
- Weather Service (L773-L805)

**Lợi ích:**
- Giảm boot time
- Giảm memory footprint lúc boot
- Chỉ load service khi cần

**Rủi ro:**
- Cần đảm bảo lazy loader hoạt động đúng
- First access có thể chậm hơn

#### **Phase 7: Self-Test (Optional)** (L807-L827)

```807:827:components/sx_core/sx_bootstrap.c
    // 10) Initialize and run smoke test (if enabled)
    // Note: Smoke test runs after UI is initialized to test LVGL and screen draw
    #ifdef CONFIG_SX_SELFTEST_ENABLE
    esp_err_t selftest_ret = sx_selftest_init();
    if (selftest_ret == ESP_OK) {
        // Wait a bit for UI to fully initialize
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        sx_selftest_result_t test_result;
        selftest_ret = sx_selftest_run(&test_result);
        if (selftest_ret == ESP_OK) {
            sx_selftest_print_result(&test_result);
        }
    }
    #endif
```

**Phân tích:**
- Self-test chỉ chạy nếu `CONFIG_SX_SELFTEST_ENABLE` được bật
- Chờ 2 giây để UI initialize xong
- Test LVGL và screen draw

---

## 4. BOARD CONFIGURATION

**Nguồn:** `Kconfig.projbuild`, `sdkconfig.defaults`

### 4.1 Board Type

- **Board:** `BOARD_TYPE_HAI_OS_SIMPLEXL` (sdkconfig.defaults:L14)
- **Target:** ESP32-S3 only (Kconfig.projbuild:L11)

### 4.2 LCD Configuration

**Nguồn:** `sdkconfig.defaults:L16-L23`, `Kconfig.projbuild:L16-L103`

- **LCD Type:** ST7796 320x480 IPS (mặc định)
- **Pins:**
  - MOSI: GPIO 47
  - CLK: GPIO 21
  - CS: GPIO 41
  - DC: GPIO 40
  - RST: GPIO 45
  - Backlight: GPIO 42 (PWM)

**Lưu ý:** LCD config có thể chọn từ Kconfig (ST7796, ST7789, ILI9341, Custom)

### 4.3 Touch Configuration

**Nguồn:** `sdkconfig.defaults:L25-L30`, `Kconfig.projbuild:L105-L138`

- **Touch Enabled:** `CONFIG_HAI_TOUCH_ENABLE=y` (mặc định)
- **Pins (I2C):**
  - SDA: GPIO 8
  - SCL: GPIO 11
  - RST: GPIO 9
  - INT: GPIO 13

### 4.4 Audio Configuration

**Nguồn:** `Kconfig.projbuild:L140-L193`

- **I2S Port:** 0 (mặc định)
- **Sample Rate:** 16000 Hz (mặc định)
- **DMA Descriptors:** 6
- **DMA Frame Size:** 240
- **Pins:**
  - MCLK: -1 (unused)
  - BCLK: GPIO 15
  - WS/LRCLK: GPIO 16
  - DOUT: GPIO 7
  - DIN: GPIO 6

---

## 5. BUILD STATUS & WARNINGS

### 5.1 Build Status

**Nguồn:** `build_output_20260102_091519.log`

- **Build thành công:** Có (2125 files compiled)
- **Partition table:** Generated successfully
- **No critical errors:** Không thấy error trong log mẫu

### 5.2 Known Issues & Fixes

**FreeRTOS Timer Task Stack Overflow:**

**Nguồn:** `sdkconfig.defaults:L9-L11`

```9:11:sdkconfig.defaults
# FreeRTOS Timer Service task stack (Tmr Svc)
# Fix stack overflow when sx_audio_power timer callback runs
CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH=4096
```

**Phân tích:**
- Timer task stack đã được tăng từ mặc định (thường 2048) lên 4096
- Lý do: `sx_audio_power` timer callback cần nhiều stack
- **⚠️ Rủi ro:** Nếu có thêm timer callback phức tạp, có thể cần tăng thêm

---

## 6. COMPONENT DEPENDENCIES

### 6.1 Core Component Structure

**Nguồn:** `components/sx_core/CMakeLists.txt`

```1:38:components/sx_core/CMakeLists.txt
idf_component_register(
    SRCS
        "sx_bootstrap.c"
        "sx_dispatcher.c"
        "sx_orchestrator.c"
        "sx_event_string_pool.c"
        "sx_lazy_loader.c"
        "sx_event_handler.c"
        "sx_error_handler.c"
        "sx_event_handlers/ui_input_handler.c"
        "sx_event_handlers/chatbot_handler.c"
        "sx_event_handlers/audio_handler.c"
        "sx_event_handlers/radio_handler.c"
        "sx_event_handlers/event_handlers_ota.c"
        "sx_service_if.c"
        "sx_metrics.c"
        "sx_selftest.c"
    REQUIRES
        nvs_flash
        esp_event
        sx_platform
        sx_ui
        sx_assets
    PRIV_REQUIRES
        esp_http_client
        json
)
```

**Phân tích:**
- `sx_core` là core component, không depend trực tiếp vào `sx_services` (L30-L32) — **Tránh circular dependency**
- Event handlers include service headers qua `PRIV_INCLUDE_DIRS` (L22-L23)
- Link được resolve qua `sx_services → sx_core` dependency

### 6.2 App Component

**Nguồn:** `components/sx_app/CMakeLists.txt`

```1:9:components/sx_app/CMakeLists.txt
idf_component_register(
    SRCS
        "app_main.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        sx_core
)
```

**Phân tích:**
- `sx_app` chỉ depend vào `sx_core`
- Entrypoint đơn giản, chỉ gọi bootstrap

---

## 7. ĐIỂM RỦI RO & CẢNH BÁO

### 7.1 Rủi ro P0 (Critical)

1. **Stack Overflow Risk — Main Task**
   - **Vị trí:** `sdkconfig:L1542`
   - **Vấn đề:** Main task stack chỉ 3584 bytes
   - **Hậu quả:** Nếu bootstrap phức tạp hơn, có thể stack overflow
   - **Điều kiện:** Khi thêm nhiều service vào bootstrap
   - **Cách tái hiện:** Monitor stack usage trong bootstrap
   - **Cách sửa:** Tăng `CONFIG_ESP_MAIN_TASK_STACK_SIZE` lên 4096 hoặc 6144

2. **Stack Check Disabled**
   - **Vị trí:** `sdkconfig:L848`
   - **Vấn đề:** `CONFIG_COMPILER_STACK_CHECK_MODE_NONE=y`
   - **Hậu quả:** Không phát hiện stack overflow tại compile time
   - **Cách sửa:** Enable stack check (nhưng có thể ảnh hưởng performance)

### 7.2 Rủi ro P1 (High)

1. **SD Pin Config Hardcoded**
   - **Vị trí:** `components/sx_core/sx_bootstrap.c:L192-L199`
   - **Vấn đề:** SD pin config hardcoded trong code
   - **Hậu quả:** Khó thay đổi pin khi thay board
   - **Cách sửa:** Move vào Kconfig như LCD/Touch

2. **Touch Init Non-Critical nhưng có thể gây confusion**
   - **Vị trí:** `components/sx_core/sx_bootstrap.c:L163-L167`
   - **Vấn đề:** Touch init fail nhưng vẫn boot, có thể user không biết
   - **Hậu quả:** UI không có touch input nhưng không có warning rõ ràng
   - **Cách sửa:** Thêm UI indicator khi touch không available

### 7.3 Rủi ro P2 (Medium)

1. **Lazy Loading Dependencies**
   - **Vị trí:** `components/sx_core/sx_bootstrap.c:L242-L805`
   - **Vấn đề:** Nhiều service lazy load, cần đảm bảo lazy loader hoạt động đúng
   - **Hậu quả:** Service có thể không init khi cần
   - **Cách sửa:** Test kỹ lazy loader, thêm fallback

2. **Factory Partition Size (3MB)**
   - **Vị trí:** `partitions.csv:L5`
   - **Vấn đề:** 3MB có thể không đủ khi thêm tính năng
   - **Hậu quả:** Build fail nếu binary > 3MB
   - **Cách sửa:** Monitor binary size, tăng partition nếu cần

---

## 8. KẾT LUẬN PHASE 0

### 8.1 Điểm Mạnh

1. ✅ **Bootstrap flow rõ ràng:** Phân chia phase logic, dễ debug
2. ✅ **Lazy loading strategy:** Giảm boot time và memory footprint
3. ✅ **Error handling:** Có error handler centralized
4. ✅ **PSRAM enabled:** Có thêm memory cho ứng dụng
5. ✅ **FreeRTOS timer stack fix:** Đã fix overflow issue

### 8.2 Điểm Yếu

1. ⚠️ **Main task stack nhỏ:** 3584 bytes có thể không đủ
2. ⚠️ **Stack check disabled:** Không phát hiện overflow tại compile time
3. ⚠️ **SD pin hardcoded:** Khó maintain
4. ⚠️ **Touch init non-critical:** Có thể gây confusion

### 8.3 Hành Động Tiếp Theo

**PHASE 1:** Lập bản đồ dự án (Module Map, Dependency Graph)  
**PHASE 2:** Phân tích runtime architecture (Tasks, Queues, Events)  
**PHASE 3-7:** Phân tích từng module chi tiết

---

## 9. CHECKLIST HOÀN THÀNH PHASE 0

- [x] Đọc CMakeLists.txt root
- [x] Đọc sdkconfig và sdkconfig.defaults
- [x] Đọc partition_table.csv
- [x] Xác định entrypoint (app_main.c)
- [x] Phân tích bootstrap flow (sx_bootstrap.c)
- [x] Xác định ESP-IDF version (5.5.1)
- [x] Xác định target chip (ESP32-S3)
- [x] Phân tích memory/PSRAM config
- [x] Phân tích FreeRTOS config
- [x] Phân tích board config (LCD, Touch, Audio)
- [x] Xác định build status
- [x] Liệt kê rủi ro và cảnh báo
- [x] Tạo REPORT_PHASE_0_BASELINE.md

---

## 10. THỐNG KÊ FILE ĐÃ ĐỌC

**Tổng số file đã đọc trong Phase 0:** ~15 files

**Danh sách:**
1. `CMakeLists.txt` (root)
2. `sdkconfig.defaults`
3. `sdkconfig` (partial)
4. `partitions.csv`
5. `app/app_main.c`
6. `components/sx_app/app_main.c`
7. `Kconfig.projbuild`
8. `components/sx_core/sx_bootstrap.c`
9. `components/sx_core/include/sx_bootstrap.h`
10. `components/sx_core/CMakeLists.txt`
11. `components/sx_app/CMakeLists.txt`
12. `build_output_20260102_091519.log` (partial)

**Ước lượng % file đã đọc:** ~2-3% (chỉ đọc config và entrypoint)

---

**Kết thúc PHASE 0**

