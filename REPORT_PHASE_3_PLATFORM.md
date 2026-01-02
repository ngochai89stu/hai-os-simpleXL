# PHASE 3 — Platform/Board/HAL
## Báo cáo phân tích platform layer: pinmap, board init, và drivers

**Ngày tạo:** 2025-01-02  
**Dự án:** hai-os-simplexl  
**Mục tiêu:** Phân tích pinmap selection, board init, peripheral init order, và drivers

---

## 1. PINMAP CONFIGURATION

### 1.1 Bảng Tổng Hợp Pin Mapping

| Peripheral | Pin | GPIO | Function | Nguồn |
|------------|-----|------|----------|-------|
| **LCD (SPI)** | | | | |
| LCD MOSI | 47 | GPIO 47 | SPI3 MOSI | `sx_platform.c:L26-L28` |
| LCD CLK | 21 | GPIO 21 | SPI3 SCLK | `sx_platform.c:L29-L31` |
| LCD CS | 41 | GPIO 41 | SPI3 CS (LCD) | `sx_platform.c:L32-L34` |
| LCD DC | 40 | GPIO 40 | Data/Command | `sx_platform.c:L35-L37` |
| LCD RST | 45 | GPIO 45 | Reset | `sx_platform.c:L38-L40` |
| LCD Backlight | 42 | GPIO 42 | PWM (LEDC) | `sx_platform.c:L41-L43` |
| **Touch (I2C)** | | | | |
| Touch SDA | 8 | GPIO 8 | I2C1 SDA | `sx_platform.c:L56-L58` |
| Touch SCL | 11 | GPIO 11 | I2C1 SCL | `sx_platform.c:L59-L61` |
| Touch RST | 9 | GPIO 9 | Reset | `sx_platform.c:L62-L64` |
| Touch INT | 13 | GPIO 13 | Interrupt | `sx_platform.c:L65-L67` |
| **SD Card (SPI)** | | | | |
| SD CS | 10 | GPIO 10 | SPI3 CS (SD) | `sx_bootstrap.c:L198` |
| SD MISO | 12 | GPIO 12 | SPI3 MISO (shared) | `sx_platform.c:L171`, `sx_bootstrap.c:L195` |
| SD MOSI | 47 | GPIO 47 | SPI3 MOSI (shared) | Shared with LCD |
| SD SCLK | 21 | GPIO 21 | SPI3 SCLK (shared) | Shared with LCD |
| **Audio (I2S)** | | | | |
| Audio BCLK | 15 | GPIO 15 | I2S BCLK | `Kconfig.projbuild:L171-L175` |
| Audio WS/LRCLK | 16 | GPIO 16 | I2S WS | `Kconfig.projbuild:L177-L181` |
| Audio DOUT | 7 | GPIO 7 | I2S DOUT | `Kconfig.projbuild:L183-L187` |
| Audio DIN | 6 | GPIO 6 | I2S DIN | `Kconfig.projbuild:L189-L193` |
| Audio MCLK | -1 | Unused | I2S MCLK | `Kconfig.projbuild:L165-L169` |
| **Volume (I2C)** | | | | |
| Volume SDA | -1 | Disabled | I2C0 SDA | `sx_platform_volume.c:L19-L20` |
| Volume SCL | -1 | Disabled | I2C0 SCL | `sx_platform_volume.c:L19-L20` |

### 1.2 Pin Configuration Source

**Nguồn:** `Kconfig.projbuild`, `sdkconfig.defaults`, `sx_platform.c`

**Configuration Method:**
- **Kconfig-based:** Tất cả pins được config qua Kconfig variables
- **Default values:** Trong `sdkconfig.defaults` và `sx_platform.c` (fallback)
- **Runtime:** Không thay đổi pins sau init

**Nguồn chi tiết:**
- LCD pins: `Kconfig.projbuild:L49-L103`, `sdkconfig.defaults:L16-L23`
- Touch pins: `Kconfig.projbuild:L112-L138`, `sdkconfig.defaults:L25-L30`
- Audio pins: `Kconfig.projbuild:L165-L193`
- SD pins: `sx_bootstrap.c:L192-L199` (hardcoded, **⚠️ nên move vào Kconfig**)

### 1.3 Pin Conflicts & Sharing

**SPI Bus Sharing:**
- **SPI3_HOST** được share giữa LCD và SD card:
  - **Shared pins:** MOSI (47), SCLK (21), MISO (12)
  - **Separate CS:** LCD CS (41), SD CS (10)
  - **Management:** `sx_spi_bus_manager` với mutex

**I2C Bus Separation:**
- **I2C0:** Volume control (disabled - GPIO -1)
- **I2C1:** Touch controller (GPIO 8/11)

**Phân tích:**
- ✅ SPI sharing được quản lý bằng mutex — **tốt**
- ⚠️ SD pins hardcoded trong bootstrap — **nên move vào Kconfig**
- ✅ I2C buses tách biệt — **tốt**, tránh conflict

---

## 2. BOARD INITIALIZATION SEQUENCE

### 2.1 Init Order trong Bootstrap

**Nguồn:** `components/sx_core/sx_bootstrap.c:L55-L831`

**Sequence:**

```
1. NVS Flash Init (L61-L68)
   └── nvs_flash_init()
   └── nvs_flash_erase() (if needed)

2. Error Handler Init (L70-L76)
   └── sx_error_handler_init()

3. Settings Service Init (L78-L84)
   └── sx_settings_service_init()

4. Theme Service Init (L86-L92)
   └── sx_theme_service_init()

5. OTA Service Init (L94-L108)
   └── sx_ota_service_init()
   └── sx_ota_full_service_init()

6. MCP Server Init (L110-L121)
   └── sx_mcp_server_init()
   └── sx_mcp_tools_register_all()

7. Dispatcher Init (L123-L129)
   └── sx_dispatcher_init()

8. Orchestrator Start (L131-L134)
   └── sx_orchestrator_start()

9. Display Platform Init (L136-L143)
   └── sx_platform_display_init()
       ├── LEDC PWM init (backlight)
       ├── SPI3 bus init
       ├── LCD panel IO init
       └── LCD panel init

10. Touch Init (L145-L180)
    └── sx_platform_touch_init()
        ├── I2C1 bus init
        ├── Touch panel IO init
        └── Touch panel init

11. SPI Bus Manager Init (L183-L189)
    └── sx_spi_bus_manager_init()

12. SD Service Init (L191-L211)
    └── sx_sd_service_init()
    └── sx_sd_service_start()
        └── SPI bus lock
        └── SD card mount

13. Assets Init (L213-L219)
    └── sx_assets_init()

14. UI Start (L221-L227)
    └── sx_ui_start()
        └── UI task creation

15. Brightness Restore (L229-L240)
    └── sx_platform_set_brightness()

16. Essential Services Init (L242-L412)
    └── Audio Ducking, Power, Router, Service
```

### 2.2 Critical Dependencies

**Dependency Chain:**

```
NVS → Settings → Theme/OTA
     ↓
Dispatcher → Orchestrator
     ↓
Platform (Display) → SPI Bus Manager
     ↓
Touch (I2C) → (independent)
     ↓
SD Service → SPI Bus Manager (uses SPI3 shared with LCD)
     ↓
Assets → SD Service (needs SD mounted)
     ↓
UI → Display + Touch handles
     ↓
Services → Dispatcher (for events)
```

**Phân tích:**
- ✅ **Logical order:** Dependencies được resolve đúng
- ⚠️ **SD pins hardcoded:** Nên init SD service sau SPI bus manager
- ✅ **Touch optional:** Có thể boot không có touch

---

## 3. PERIPHERAL INITIALIZATION ORDER

### 3.1 SPI Bus Initialization

**LCD SPI Init Sequence:**

**Nguồn:** `components/sx_platform/sx_platform.c:L133-L314`

```c
// 1. LEDC PWM init (backlight) - L138-L163
ledc_timer_config() → ledc_channel_config()

// 2. SPI bus config - L165-L176
spi_bus_config_t buscfg = {
    .sclk_io_num = 21,
    .mosi_io_num = 47,
    .miso_io_num = 12,  // Shared with SD
    .max_transfer_sz = 320 * 480 * 2,  // Full screen buffer
};
spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);

// 3. Panel IO config - L178-L197
esp_lcd_panel_io_spi_config_t io_config = {
    .cs_gpio_num = 41,
    .dc_gpio_num = 40,
    .pclk_hz = 20 * 1000 * 1000,  // 20MHz
    .trans_queue_depth = 10,
};
esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &io_handle);

// 4. Panel driver init - L200-L291
// Based on Kconfig: ST7796, ST7789, ILI9341, or Custom
esp_lcd_new_panel_st7796() / esp_lcd_new_panel_st7789() / ...

// 5. Panel reset & init - L305-L310
esp_lcd_panel_reset(panel_handle);
esp_lcd_panel_init(panel_handle);
esp_lcd_panel_disp_on_off(panel_handle, true);
```

**SD Card SPI Init Sequence:**

**Nguồn:** `components/sx_services/sx_sd_service.c:L39-L119`

```c
// 1. GPIO CS init - L57-L61
gpio_reset_pin(CS);
gpio_set_direction(CS, GPIO_MODE_OUTPUT);
gpio_set_level(CS, 1);  // CS high = inactive

// 2. SPI bus lock - L84
sx_spi_bus_lock();  // Acquire mutex (LCD and SD share SPI3)

// 3. SDSPI device config - L64-L68
sdspi_device_config_t device_config = {
    .host_id = SPI3_HOST,
    .gpio_cs = 10,
};

// 4. Mount SD card - L92
esp_vfs_fat_sdspi_mount(mount_point, &host, &device_config, &mount_cfg, &card);

// 5. SPI bus unlock - L94
sx_spi_bus_unlock();
```

**Phân tích:**
- ✅ **SPI bus init trước:** LCD init SPI bus, SD reuse
- ✅ **Mutex protection:** SD mount lock SPI bus
- ⚠️ **Max transfer size:** LCD config `320*480*2` = 307KB — có thể cần tăng nếu dùng DMA lớn hơn

### 3.2 I2C Bus Initialization

**Touch I2C Init Sequence:**

**Nguồn:** `components/sx_platform/sx_platform.c:L371-L509`

```c
// 1. I2C bus config - L391-L415
i2c_master_bus_config_t i2c_bus_config = {
    .i2c_port = I2C_NUM_1,  // I2C port 1
    .sda_io_num = 8,
    .scl_io_num = 11,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};
i2c_new_master_bus(&i2c_bus_config, &s_touch_i2c_bus_handle);

// 2. Touch panel IO config - L422-L458
esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
io_config.scl_speed_hz = 400000;  // 400kHz
esp_lcd_new_panel_io_i2c(s_touch_i2c_bus_handle, &io_config, &touch_io_handle);

// 3. Touch panel config - L461-L497
esp_lcd_touch_config_t tp_cfg = {
    .x_max = 320,
    .y_max = 480,
    .rst_gpio_num = 9,
    .int_gpio_num = 13,
};
esp_lcd_touch_new_i2c_ft5x06(touch_io_handle, &tp_cfg, &touch_handle);
```

**Volume I2C Init Sequence:**

**Nguồn:** `components/sx_platform/sx_platform_volume.c:L35-L74`

```c
// I2C disabled - GPIO -1
// System uses software volume only (PCM5102A)
if (I2C_MASTER_SCL_IO < 0 || I2C_MASTER_SDA_IO < 0) {
    return ESP_ERR_NOT_SUPPORTED;
}

// If enabled, would use I2C_NUM_0 for volume control
i2c_master_bus_config_t i2c_bus_config = {
    .i2c_port = I2C_NUM_0,
    .sda_io_num = I2C_MASTER_SDA_IO,
    .scl_io_num = I2C_MASTER_SCL_IO,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};
```

**Phân tích:**
- ✅ **I2C buses tách biệt:** Touch (I2C1), Volume (I2C0, disabled)
- ✅ **Touch I2C 400kHz:** Hợp lý cho touch controller
- ✅ **Internal pullup enabled:** Đúng cho I2C
- ⚠️ **Volume I2C disabled:** Hardware volume không available, dùng software volume

### 3.3 GPIO Initialization

**GPIO Usage:**

| GPIO | Function | Init Location | Notes |
|------|----------|---------------|-------|
| 6 | Audio DIN | Audio service | I2S input |
| 7 | Audio DOUT | Audio service | I2S output |
| 8 | Touch SDA | `sx_platform_touch_init()` | I2C1 SDA |
| 9 | Touch RST | `sx_platform_touch_init()` | Reset pin |
| 10 | SD CS | `sx_sd_service_start()` | SPI3 CS (SD) |
| 11 | Touch SCL | `sx_platform_touch_init()` | I2C1 SCL |
| 12 | SD MISO / SPI3 MISO | `sx_platform_display_init()` | Shared |
| 13 | Touch INT | `sx_platform_touch_init()` | Interrupt |
| 15 | Audio BCLK | Audio service | I2S BCLK |
| 16 | Audio WS | Audio service | I2S WS |
| 21 | SPI3 SCLK | `sx_platform_display_init()` | Shared LCD/SD |
| 40 | LCD DC | `sx_platform_display_init()` | Data/Command |
| 41 | LCD CS | `sx_platform_display_init()` | SPI3 CS (LCD) |
| 42 | LCD Backlight | `sx_platform_display_init()` | PWM (LEDC) |
| 45 | LCD RST | `sx_platform_display_init()` | Reset |
| 47 | SPI3 MOSI | `sx_platform_display_init()` | Shared LCD/SD |

**Phân tích:**
- ✅ **GPIO init trong peripheral init:** Mỗi peripheral tự init GPIO
- ✅ **No GPIO conflicts:** Không có pin bị conflict
- ⚠️ **SD CS init trong service:** Nên init trong platform layer

---

## 4. DRIVERS ANALYSIS

### 4.1 LCD Driver

**Driver:** ESP-IDF `esp_lcd` + `esp_lcd_st7796`

**Configuration:**

**Nguồn:** `components/sx_platform/sx_platform.c:L200-L291`

**LCD Types Supported:**
- **ST7796 320x480** (default): BGR color space, custom init sequence
- **ST7789 240x320**: RGB color space
- **ST7789 240x240**: RGB color space
- **ILI9341 240x320**: BGR color space
- **Custom**: Auto-detect based on resolution

**Init Sequence (ST7796):**

```c
// Custom init commands (L112-L131)
st7796u_init_cmds[] = {
    {0x11, {0x00}, 0, 120},  // SLPOUT - Sleep out, delay 120ms
    {0x3A, {0x55}, 1, 0},   // COLMOD - RGB565
    {0xF0, {0xC3}, 1, 0},   // Command Set Control 1
    // ... 28 more commands
    {0x29, {0x00}, 0, 20},  // DISPON - Display on, delay 20ms
};
```

**SPI Configuration:**
- **Clock:** 20MHz (`pclk_hz = 20 * 1000 * 1000`)
- **Mode:** Mode 0
- **Queue depth:** 10 transactions
- **Max transfer:** 307KB (320*480*2)

**Phân tích:**
- ✅ **Multiple LCD support:** Hỗ trợ nhiều loại LCD qua Kconfig
- ✅ **Custom init sequence:** ST7796 có init sequence tối ưu
- ⚠️ **20MHz clock:** Có thể tăng lên 40MHz nếu LCD support
- ✅ **DMA enabled:** `SPI_DMA_CH_AUTO` cho performance tốt

### 4.2 Touch Driver

**Driver:** ESP-IDF `esp_lcd_touch_ft5x06`

**Configuration:**

**Nguồn:** `components/sx_platform/sx_platform.c:L371-L509`

**Touch Controller:** FT5x06 (Capacitive Touch Panel)

**I2C Configuration:**
- **Bus:** I2C_NUM_1
- **Address:** `ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS` (macro từ driver)
- **Speed:** 400kHz
- **Pins:** SDA=8, SCL=11, RST=9, INT=13

**Touch Config:**
```c
esp_lcd_touch_config_t tp_cfg = {
    .x_max = 320,
    .y_max = 480,
    .rst_gpio_num = 9,
    .int_gpio_num = 13,
    .levels.reset = 0,
    .levels.interrupt = 0,
    .flags.swap_xy = 0,
    .flags.mirror_x = 0,
    .flags.mirror_y = 0,
};
```

**Phân tích:**
- ✅ **FT5x06 driver:** Standard ESP-IDF driver
- ✅ **I2C 400kHz:** Hợp lý cho touch
- ✅ **Interrupt mode:** Sử dụng INT pin (GPIO 13)
- ⚠️ **Error handling:** Có cleanup I2C bus nếu init fail

### 4.3 SPI Bus Manager

**Driver:** Custom (`sx_spi_bus_manager`)

**Nguồn:** `components/sx_platform/sx_spi_bus_manager.c`

**Purpose:** Quản lý SPI bus sharing giữa LCD và SD card

**API:**
```c
esp_err_t sx_spi_bus_manager_init(void);
void sx_spi_bus_lock(void);
void sx_spi_bus_unlock(void);
```

**Implementation:**
- **Mutex:** `s_spi_bus_mutex` (FreeRTOS mutex)
- **Lock/Unlock:** `xSemaphoreTake()` / `xSemaphoreGive()`
- **Timeout:** `portMAX_DELAY` (blocking)

**Usage Pattern:**
```c
sx_spi_bus_lock();
// SPI operations (SD mount, etc.)
sx_spi_bus_unlock();
```

**Phân tích:**
- ✅ **Simple mutex pattern:** Dễ hiểu, dễ maintain
- ⚠️ **Blocking lock:** `portMAX_DELAY` có thể gây deadlock nếu lock order sai
- ✅ **Auto-init:** Lock function tự init nếu chưa init

### 4.4 SD Card Driver

**Driver:** ESP-IDF `sdspi_host` + `esp_vfs_fat`

**Configuration:**

**Nguồn:** `components/sx_services/sx_sd_service.c:L39-L119`

**SPI Configuration:**
- **Host:** SPI3_HOST (shared with LCD)
- **CS:** GPIO 10
- **Speed:** 20MHz default (SDMMC_FREQ_DEFAULT)
- **Format:** FAT filesystem

**Mount Config:**
```c
esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024,
    .disk_status_check_enable = false
};
```

**Phân tích:**
- ✅ **SPI sharing:** Sử dụng SPI bus manager để share với LCD
- ⚠️ **Max files:** 5 files — có thể không đủ nếu nhiều services dùng SD
- ✅ **No auto-format:** `format_if_mount_failed = false` — an toàn
- ⚠️ **SD pins hardcoded:** Nên move vào Kconfig

### 4.5 Backlight Driver (LEDC PWM)

**Driver:** ESP-IDF `driver/ledc`

**Configuration:**

**Nguồn:** `components/sx_platform/sx_platform.c:L138-L163, L316-L352`

**LEDC Config:**
```c
ledc_timer_config_t ledc_timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .timer_num = LEDC_TIMER_0,
    .duty_resolution = LEDC_TIMER_13_BIT,  // 8192 steps
    .freq_hz = 5000,  // 5kHz PWM frequency
};

ledc_channel_config_t ledc_channel = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_0,
    .gpio_num = 42,
    .duty = 0,
};
```

**Brightness Control:**
- **Range:** 0-100%
- **Resolution:** 13-bit (8192 steps)
- **Mapping:** `duty = (percent * 8191) / 100`

**Fallback:**
- Nếu PWM không init, fallback sang GPIO on/off

**Phân tích:**
- ✅ **13-bit resolution:** Mịn, không flicker
- ✅ **5kHz PWM:** Đủ nhanh, không nghe thấy noise
- ✅ **GPIO fallback:** Có fallback nếu PWM fail
- ✅ **Brightness restore:** Restore từ settings sau UI init

### 4.6 Volume Driver (I2C Codec)

**Driver:** Custom (`sx_platform_volume`)

**Nguồn:** `components/sx_platform/sx_platform_volume.c`

**Status:** **DISABLED** (GPIO -1)

**Supported Codecs:**
- **ES8388:** I2C address 0x10
- **ES8311:** I2C address 0x18
- **PCM5102A:** No hardware volume (software only)

**Detection:**
```c
sx_hw_codec_chip_t sx_platform_detect_codec(void) {
    // Try ES8388 (read chip ID 0x01)
    // Try ES8311 (read chip ID 0x83/0x11)
    // Default: PCM5102A (software volume)
}
```

**Phân tích:**
- ⚠️ **I2C disabled:** Hardware volume không available
- ✅ **Software volume fallback:** System dùng software volume
- ✅ **Codec detection:** Có logic detect codec nếu I2C enable
- ⚠️ **Volume control:** Chỉ software volume, không có hardware control

---

## 5. INITIALIZATION ERRORS & RECOVERY

### 5.1 Error Handling Patterns

**Display Init Errors:**

**Nguồn:** `components/sx_platform/sx_platform.c:L189-L197, L293-L302`

```c
// Panel IO creation fail
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create panel IO: %s", esp_err_to_name(ret));
    s_backlight_initialized = false;  // Reset state
    return handles;  // Return empty handles
}

// Panel creation fail
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create panel: %s", esp_err_to_name(ret));
    if (io_handle != NULL) {
        esp_lcd_panel_io_del(io_handle);  // Cleanup
        handles.io_handle = NULL;
    }
    s_backlight_initialized = false;
    return handles;
}
```

**Touch Init Errors:**

**Nguồn:** `components/sx_platform/sx_platform.c:L406-L413, L447-L457, L483-L496`

```c
// I2C bus creation fail
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(ret));
    return ret;
}

// Touch IO creation fail
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create touch panel IO: %s", esp_err_to_name(ret));
    if (s_touch_i2c_bus_handle != NULL) {
        i2c_del_master_bus(s_touch_i2c_bus_handle);  // Cleanup
        s_touch_i2c_bus_handle = NULL;
    }
    return ret;
}

// Touch panel creation fail
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create FT5x06 touch panel: %s", esp_err_to_name(ret));
    if (touch_io_handle != NULL) {
        esp_lcd_panel_io_del(touch_io_handle);  // Cleanup
    }
    if (s_touch_i2c_bus_handle != NULL) {
        i2c_del_master_bus(s_touch_i2c_bus_handle);  // Cleanup
        s_touch_i2c_bus_handle = NULL;
    }
    return ret;
}
```

**SD Card Init Errors:**

**Nguồn:** `components/sx_services/sx_sd_service.c:L96-L107`

```c
if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
        ESP_LOGE(TAG, "Failed to mount filesystem. "
                 "If you want the card to be formatted, set format_if_mount_failed option.");
    } else {
        ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                 "Make sure SD card lines have pull-up resistors in place.",
                 esp_err_to_name(ret));
    }
    ESP_LOGW(TAG, "SD card may be missing, damaged, or incompatible. Continuing without SD card.");
    return ret;  // Non-critical, continue boot
}
```

**Phân tích:**
- ✅ **Display errors:** Cleanup IO handle, reset state
- ✅ **Touch errors:** Cleanup I2C bus và IO handle
- ✅ **SD errors:** Non-critical, continue boot
- ⚠️ **SPI bus cleanup:** Không cleanup SPI bus nếu display init fail (ESP-IDF limitation)

### 5.2 Resource Leak Risks

**Risk 1: SPI Bus Not Cleaned Up**

**Vị trí:** `components/sx_platform/sx_platform.c:L176-L197`

**Vấn đề:**
- SPI bus được init tại L176: `spi_bus_initialize(SPI3_HOST, ...)`
- Nếu panel IO creation fail (L189), SPI bus không được cleanup
- ESP-IDF không có API để deinitialize SPI bus

**Hậu quả:**
- SPI bus resources leak (không critical, system sẽ reboot)

**Cách sửa:**
- Không có cách sửa (ESP-IDF limitation)
- Acceptable vì init failures rare và system sẽ reboot

**Risk 2: I2C Bus Not Cleaned Up (Touch)**

**Vị trí:** `components/sx_platform/sx_platform.c:L405-L413`

**Vấn đề:**
- I2C bus được tạo tại L405
- Nếu touch IO creation fail, I2C bus được cleanup (L453-L456) — **✅ OK**
- Nếu touch panel creation fail, I2C bus được cleanup (L492-L495) — **✅ OK**

**Phân tích:**
- ✅ **Cleanup đúng:** I2C bus được cleanup trong error paths

---

## 6. LỖI TIỀM ẨN & NỢ KỸ THUẬT

### 6.1 P0 (Critical) Issues

**Không có P0 issues**

### 6.2 P1 (High) Issues

1. **SD Pin Config Hardcoded**
   - **Vị trí:** `components/sx_core/sx_bootstrap.c:L192-L199`
   - **Vấn đề:** SD pins hardcoded trong bootstrap, không qua Kconfig
   - **Hậu quả:** Khó thay đổi pins khi thay board
   - **Cách sửa:** Move SD pin config vào Kconfig như LCD/Touch

2. **SPI Bus Mutex Blocking**
   - **Vị trí:** `components/sx_platform/sx_spi_bus_manager.c:L37`
   - **Vấn đề:** `portMAX_DELAY` có thể gây deadlock
   - **Hậu quả:** System hang nếu lock order sai
   - **Cách sửa:** Sử dụng timeout, hoặc định nghĩa lock order

### 6.3 P2 (Medium) Issues

1. **LCD Max Transfer Size**
   - **Vị trí:** `components/sx_platform/sx_platform.c:L174`
   - **Vấn đề:** `max_transfer_sz = 320*480*2` = 307KB, có thể không đủ nếu dùng DMA lớn
   - **Cách sửa:** Tăng lên 512KB hoặc 1MB nếu cần

2. **SD Max Files Limited**
   - **Vị trí:** `components/sx_services/sx_sd_service.c:L77`
   - **Vấn đề:** `max_files = 5` có thể không đủ
   - **Cách sửa:** Tăng lên 10 hoặc 20 nếu nhiều services dùng SD

3. **Volume I2C Disabled**
   - **Vị trí:** `components/sx_platform/sx_platform_volume.c:L19-L20`
   - **Vấn đề:** Hardware volume không available
   - **Cách sửa:** Enable I2C nếu có hardware codec

---

## 7. KẾT LUẬN PHASE 3

### 7.1 Điểm Mạnh

1. ✅ **Kconfig-based pin config:** Tất cả pins (trừ SD) config qua Kconfig
2. ✅ **Multiple LCD support:** Hỗ trợ ST7796, ST7789, ILI9341, Custom
3. ✅ **SPI bus sharing:** LCD và SD share SPI3 với mutex protection
4. ✅ **Error handling:** Có cleanup resources trong error paths
5. ✅ **Touch optional:** Có thể boot không có touch
6. ✅ **Backlight PWM:** 13-bit resolution, smooth brightness control

### 7.2 Điểm Yếu

1. ⚠️ **SD pins hardcoded:** Nên move vào Kconfig
2. ⚠️ **SPI bus mutex blocking:** `portMAX_DELAY` có thể gây deadlock
3. ⚠️ **Volume I2C disabled:** Hardware volume không available
4. ⚠️ **SD max files:** 5 files có thể không đủ

### 7.3 Hành Động Tiếp Theo

**PHASE 4:** Phân tích LVGL/UI System  
**PHASE 5:** Phân tích Audio Pipeline  
**PHASE 6:** Phân tích Network/AI/Protocols  
**PHASE 7:** Phân tích Storage & Persistence

---

## 8. CHECKLIST HOÀN THÀNH PHASE 3

- [x] Phân tích pinmap configuration (LCD, Touch, Audio, SD)
- [x] Phân tích board initialization sequence
- [x] Phân tích peripheral initialization order
- [x] Phân tích LCD driver (ST7796, ST7789, ILI9341)
- [x] Phân tích Touch driver (FT5x06)
- [x] Phân tích SPI bus manager (LCD/SD sharing)
- [x] Phân tích SD card driver
- [x] Phân tích Backlight driver (LEDC PWM)
- [x] Phân tích Volume driver (I2C codec, disabled)
- [x] Phân tích error handling và resource cleanup
- [x] Xác định lỗi tiềm ẩn và nợ kỹ thuật
- [x] Tạo REPORT_PHASE_3_PLATFORM.md

---

## 9. THỐNG KÊ FILE ĐÃ ĐỌC

**Tổng số file đã đọc trong Phase 3:** ~5 files

**Danh sách:**
1. `components/sx_platform/sx_platform.c`
2. `components/sx_platform/sx_spi_bus_manager.c`
3. `components/sx_platform/sx_platform_volume.c`
4. `components/sx_services/sx_sd_service.c` (partial)
5. `Kconfig.projbuild` (referenced)

**Ước lượng % file đã đọc:** ~10-12% (đọc platform-critical files)

---

**Kết thúc PHASE 3**

