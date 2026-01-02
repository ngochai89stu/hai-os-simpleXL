# PHASE 1 — Repo Inventory & Module Map
## Báo cáo phân tích cấu trúc dự án và kiến trúc module

**Ngày tạo:** 2025-01-02  
**Dự án:** hai-os-simplexl  
**Mục tiêu:** Vẽ "bản đồ" dự án, xác định kiến trúc tổng thể, lập dependency graph

---

## 1. CẤU TRÚC THƯ MỤC & COMPONENTS

### 1.1 Cây Thư Mục Chính

```
hai-os-simplexl/
├── app/                          # Application entrypoint
│   └── app_main.c                # Main entry (calls sx_bootstrap_start)
├── components/                    # ESP-IDF components
│   ├── sx_app/                   # App wrapper component
│   ├── sx_core/                  # Core system (dispatcher, orchestrator, events)
│   ├── sx_platform/              # Hardware abstraction (LCD, Touch, SPI)
│   ├── sx_ui/                    # UI framework (LVGL, screens, router)
│   ├── sx_assets/                # Asset loader (SD card, embedded images)
│   ├── sx_services/              # Business logic services (audio, WiFi, chatbot, etc.)
│   ├── sx_protocol/             # Protocol layer (WebSocket, MQTT)
│   ├── sx_codec_common/          # Common codec definitions
│   └── esp_lvgl_port/           # LVGL port for ESP-IDF (external/vendored)
├── assets/                       # Static assets (boot images)
├── build/                        # Build output
├── test/                         # Unit tests & integration tests
├── docs/                         # Documentation
└── reports/                      # Analysis reports
```

### 1.2 Component Inventory

| Component | Vai Trò | Số File (ước lượng) | Dependencies |
|-----------|---------|-------------------|--------------|
| `sx_app` | Entrypoint wrapper | 1 file | `sx_core` |
| `sx_core` | Core system (events, state, dispatcher) | ~15 files | `nvs_flash`, `esp_event`, `sx_platform`, `sx_ui`, `sx_assets` |
| `sx_platform` | Hardware abstraction | ~3 files | `driver`, `esp_lcd`, `espressif__esp_lcd_touch`, `sx_codec_common` |
| `sx_ui` | UI framework (LVGL) | ~100+ files | `sx_core`, `sx_platform`, `sx_assets`, `sx_services`, `esp_lvgl_port` |
| `sx_assets` | Asset loader | ~3 files | `driver`, `fatfs`, `sdmmc` |
| `sx_services` | Business services | ~60+ files | `sx_core`, `sx_codec_common`, `fatfs`, `sdmmc`, `esp_http_client`, `esp_wifi`, `json`, `mqtt`, `esp-dsp` |
| `sx_protocol` | Protocol layer | ~5 files | (included in sx_services) |
| `sx_codec_common` | Codec definitions | Header only | - |
| `esp_lvgl_port` | LVGL port | ~30+ files | `esp_lcd` |

---

## 2. KIẾN TRÚC LAYERING

### 2.1 Layering Diagram

```
┌─────────────────────────────────────────────────────────┐
│                    APPLICATION LAYER                     │
│  (app/app_main.c → sx_bootstrap_start)                  │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│                      CORE LAYER                          │
│  sx_core: dispatcher, orchestrator, events, state       │
│  - Event-driven architecture                             │
│  - Single-writer state (orchestrator)                   │
│  - Multi-reader state (UI, services)                     │
└─────────────────────────────────────────────────────────┘
         │                    │                    │
         ▼                    ▼                    ▼
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│  UI LAYER    │    │ SERVICE LAYER│    │ PLATFORM LAYER│
│  sx_ui       │    │ sx_services   │    │ sx_platform  │
│  - LVGL      │    │ - Audio      │    │ - LCD/Touch  │
│  - Screens   │    │ - WiFi       │    │ - SPI/I2C    │
│  - Router    │    │ - Chatbot    │    │ - Hardware   │
│  - Assets    │    │ - Radio      │    │   abstraction│
└──────────────┘    └──────────────┘    └──────────────┘
         │                    │                    │
         └────────────────────┴────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│                    DRIVER LAYER                          │
│  ESP-IDF drivers: esp_lcd, driver, fatfs, sdmmc, etc.   │
└─────────────────────────────────────────────────────────┘
```

### 2.2 Layer Responsibilities

#### **Application Layer**
- **File:** `app/app_main.c`
- **Trách nhiệm:** Entrypoint, gọi bootstrap
- **Dependencies:** `sx_core`

#### **Core Layer** (`sx_core`)
- **Trách nhiệm:**
  - Event dispatcher (multi-producer, single-consumer)
  - State orchestrator (single-writer, multi-reader)
  - Event taxonomy & routing
  - Service lifecycle management
  - Lazy loading coordination
  - Error handling
- **Key Files:**
  - `sx_dispatcher.c/h` — Event bus
  - `sx_orchestrator.c/h` — State manager
  - `sx_event.h` — Event definitions
  - `sx_state.h` — State definitions
  - `sx_bootstrap.c/h` — Boot sequence
  - `sx_lazy_loader.c/h` — Lazy service loading
  - `sx_service_if.h` — Service lifecycle interface

#### **UI Layer** (`sx_ui`)
- **Trách nhiệm:**
  - LVGL integration (only component allowed to include LVGL)
  - Screen lifecycle management
  - UI router & navigation
  - Screen registry
  - UI event handling (touch, input)
  - Asset wrapping (LVGL-compatible)
- **Key Files:**
  - `sx_ui_task.c` — UI owner task
  - `ui_router.c/h` — Navigation
  - `ui_screen_registry.c/h` — Screen registry
  - `sx_screen_if.h` — Screen lifecycle interface
  - `screens/*.c` — Individual screens (~30+ screens)

#### **Service Layer** (`sx_services`)
- **Trách nhiệm:**
  - Business logic services
  - Audio pipeline (playback, recording, codecs)
  - Network services (WiFi, MQTT, WebSocket)
  - AI services (STT, TTS, Wake Word, Chatbot)
  - Media services (Radio, Music Online, SD Card)
  - System services (Settings, OTA, Power, Diagnostics)
- **Key Services:**
  - Audio: `sx_audio_service.c`, `sx_codec_*.c`
  - Network: `sx_wifi_service.c`, `sx_protocol_*.c`
  - AI: `sx_stt_service.c`, `sx_tts_service.c`, `sx_wake_word_service.c`, `sx_chatbot_service.c`
  - Media: `sx_radio_service.c`, `sx_music_online_service.c`, `sx_sd_music_service.c`
  - System: `sx_settings_service.c`, `sx_ota_service.c`, `sx_power_service.c`

#### **Platform Layer** (`sx_platform`)
- **Trách nhiệm:**
  - Hardware abstraction
  - LCD/Touch initialization
  - SPI bus management
  - Volume control
  - Pin mapping (via Kconfig)
- **Key Files:**
  - `sx_platform.c/h` — Display & touch init
  - `sx_spi_bus_manager.c/h` — SPI bus sharing
  - `sx_platform_volume.c/h` — Volume control

#### **Asset Layer** (`sx_assets`)
- **Trách nhiệm:**
  - Asset loading from SD card
  - Embedded image data (bootscreen, flashscreen)
  - **KHÔNG include LVGL** (tuân theo SIMPLEXL_ARCH v1.3)
- **Key Files:**
  - `sx_assets.c/h` — Asset loader
  - `generated/*.c` — Generated image data (moved to sx_ui)

#### **Protocol Layer** (`sx_protocol`)
- **Trách nhiệm:**
  - WebSocket protocol
  - MQTT protocol
  - Protocol factory pattern
- **Key Files:**
  - `sx_protocol_ws.c/h`
  - `sx_protocol_mqtt.c/h`
  - `sx_protocol_factory.c/h`

---

## 3. MODULE MAP — Chi Tiết Từng Module

### 3.1 sx_core — Core System

**Trách nhiệm:**
- Event-driven architecture core
- State management (single-writer, multi-reader)
- Service lifecycle coordination
- Lazy loading management

**Public API (Header Files):**

| Header | Mục đích | Ai sử dụng |
|--------|----------|------------|
| `sx_dispatcher.h` | Event bus API | Tất cả services, UI |
| `sx_orchestrator.h` | State manager entry | Bootstrap only |
| `sx_event.h` | Event definitions | Tất cả modules |
| `sx_state.h` | State definitions | UI, services (read-only) |
| `sx_bootstrap.h` | Bootstrap entry | `app_main.c` |
| `sx_lazy_loader.h` | Lazy service loading | Services, UI |
| `sx_service_if.h` | Service lifecycle interface | Services |
| `sx_error_handler.h` | Centralized error handling | Tất cả modules |
| `sx_metrics.h` | Metrics collection | Services (optional) |
| `sx_selftest.h` | Smoke test | Bootstrap (optional) |

**Key Functions:**

```c
// Event dispatcher
bool sx_dispatcher_init(void);
bool sx_dispatcher_post_event(const sx_event_t *evt);
bool sx_dispatcher_post_event_with_policy(const sx_event_t *evt, sx_backpressure_policy_t policy, uint32_t coalesce_key);
bool sx_dispatcher_poll_event(sx_event_t *out_evt);
void sx_dispatcher_set_state(const sx_state_t *state);
void sx_dispatcher_get_state(sx_state_t *out_state);

// State orchestrator
void sx_orchestrator_start(void);

// Bootstrap
esp_err_t sx_bootstrap_start(void);

// Lazy loading
esp_err_t sx_lazy_service_init(sx_lazy_service_t service);
bool sx_lazy_service_is_initialized(sx_lazy_service_t service);

// Service lifecycle
esp_err_t sx_service_register(const char *name, const sx_service_if_t *iface);
esp_err_t sx_service_init_all(void);
esp_err_t sx_service_start_all(void);
```

**Dependencies:**
- `nvs_flash` — NVS storage
- `esp_event` — ESP-IDF event system
- `sx_platform` — Hardware init
- `sx_ui` — UI initialization
- `sx_assets` — Asset loader

**Dependents:**
- Tất cả services (`sx_services`)
- UI (`sx_ui`)
- App (`sx_app`)

### 3.2 sx_ui — UI Framework

**Trách nhiệm:**
- LVGL integration (only component allowed to include LVGL)
- Screen lifecycle management
- UI routing & navigation
- Touch input handling
- Asset wrapping (LVGL-compatible)

**Public API (Header Files):**

| Header | Mục đích | Ai sử dụng |
|--------|----------|------------|
| `sx_ui.h` | UI entry point | Bootstrap |
| `sx_ui_router.h` | Navigation API | Screens, services |
| `sx_screen_if.h` | Screen lifecycle interface | Screens |
| `sx_lvgl.h` | LVGL wrapper (guard) | Screens (internal) |
| `sx_lvgl_lock.h` | LVGL thread safety | UI task |
| `sx_lvgl_guard.h` | LVGL runtime guard | UI task |

**Key Functions:**

```c
// UI initialization
esp_err_t sx_ui_start(const sx_display_handles_t *handles, const sx_touch_handles_t *touch_handles);

// Screen lifecycle
esp_err_t sx_screen_register(uint32_t screen_id, const sx_screen_if_t *iface);
const sx_screen_if_t* sx_screen_get_interface(uint32_t screen_id);

// Navigation
void sx_ui_router_init(void);
void sx_ui_router_register_screen(sx_screen_id_t id, sx_screen_create_func_t create_func);
void sx_ui_router_navigate_to(sx_screen_id_t id);
```

**Screen Lifecycle:**

```c
typedef struct {
    lv_obj_t* (*create)(lv_obj_t *parent);           // Create UI
    void (*destroy)(lv_obj_t *screen);                // Destroy UI
    void (*on_enter)(lv_obj_t *screen);               // Screen entered
    void (*on_exit)(lv_obj_t *screen);               // Screen exited
    void (*on_state_change)(lv_obj_t *screen, uint32_t dirty_mask, const sx_state_t *state);  // State update
} sx_screen_if_t;
```

**Screens (30+ screens):**
- `screen_boot.c` — Boot screen
- `screen_home.c` — Home screen
- `screen_chat.c` — Chatbot screen
- `screen_music_player.c` — Music player
- `screen_radio.c` — Radio streaming
- `screen_settings.c` — Settings
- `screen_wifi_setup.c` — WiFi setup
- ... và nhiều screens khác

**Dependencies:**
- `sx_core` — Events, state
- `sx_platform` — Display, touch handles
- `sx_assets` — Asset loading
- `sx_services` — Service APIs (PRIV_INCLUDE_DIRS)
- `esp_lvgl_port` — LVGL port

**Dependents:**
- Bootstrap (calls `sx_ui_start`)

### 3.3 sx_services — Business Services

**Trách nhiệm:**
- Business logic services
- Audio pipeline
- Network services
- AI services
- Media services
- System services

**Service Categories:**

#### **Audio Services:**
- `sx_audio_service.c` — Main audio service
- `sx_audio_eq.c` — Equalizer
- `sx_audio_reverb.c` — Reverb effect
- `sx_audio_ducking.c` — Audio ducking
- `sx_audio_crossfade.c` — Crossfade
- `sx_audio_mixer.c` — Audio mixer
- `sx_audio_router.c` — Audio routing
- `sx_audio_power.c` — Power management
- `sx_audio_profiler.c` — Performance profiling
- `sx_audio_recovery.c` — Error recovery
- `sx_audio_buffer_pool.c` — Buffer management
- `sx_audio_afe.c` — Audio Front-End (AEC, VAD, NS, AGC)
- `sx_audio_protocol_bridge.c` — Protocol audio bridge

#### **Codec Services:**
- `sx_codec_mp3.c` — MP3 decoder
- `sx_codec_aac.c` — AAC decoder
- `sx_codec_flac.c` — FLAC decoder
- `sx_codec_opus.c` — Opus decoder
- `sx_codec_detector.c` — Codec detection

#### **Network Services:**
- `sx_wifi_service.c` — WiFi management
- `sx_protocol_ws.c` — WebSocket protocol
- `sx_protocol_mqtt.c` — MQTT protocol
- `sx_network_optimizer.c` — Network optimization

#### **AI Services:**
- `sx_stt_service.c` — Speech-to-Text
- `sx_tts_service.c` — Text-to-Speech
- `sx_wake_word_service.c` — Wake word detection
- `sx_chatbot_service.c` — Chatbot service
- `sx_intent_service.c` — Intent parsing

#### **Media Services:**
- `sx_radio_service.c` — Radio streaming
- `sx_music_online_service.c` — Online music
- `sx_sd_music_service.c` — SD card music
- `sx_playlist_manager.c` — Playlist management
- `sx_media_metadata.c` — Metadata extraction

#### **System Services:**
- `sx_settings_service.c` — Settings storage
- `sx_theme_service.c` — Theme management
- `sx_ota_service.c` — OTA updates
- `sx_ota_full_service.cpp` — Full OTA (C++)
- `sx_power_service.c` — Power management
- `sx_diagnostics_service.c` — Diagnostics
- `sx_state_manager.c` — State persistence

#### **Other Services:**
- `sx_sd_service.c` — SD card mounting
- `sx_ir_service.c` — IR control
- `sx_bluetooth_service.c` — Bluetooth
- `sx_navigation_service.c` — Navigation
- `sx_navigation_ble.c` — BLE Navigation
- `sx_weather_service.c` — Weather
- `sx_image_service.c` — Image processing
- `sx_qr_code_service.c` — QR code generation
- `sx_display_service.c` — Display service
- `sx_led_service.c` — LED control
- `sx_mcp_server.c` — MCP server
- `sx_mcp_tools.c` — MCP tools

**Dependencies:**
- `sx_core` — Events, state, dispatcher
- `sx_codec_common` — Codec definitions
- `fatfs`, `sdmmc` — File system
- `esp_http_client` — HTTP client
- `esp_wifi`, `esp_netif` — WiFi
- `json` — JSON parsing
- `mqtt` — MQTT client
- `esp-dsp` — DSP library
- `bt` — Bluetooth (PRIV_REQUIRES)

**Dependents:**
- UI (includes service headers via PRIV_INCLUDE_DIRS)
- Bootstrap (initializes services)

### 3.4 sx_platform — Hardware Abstraction

**Trách nhiệm:**
- Hardware initialization (LCD, Touch)
- SPI bus management
- Volume control
- Pin mapping abstraction

**Public API:**

```c
// Display
sx_display_handles_t sx_platform_display_init(void);

// Touch
esp_err_t sx_platform_touch_init(sx_touch_handles_t *touch_handles);

// Brightness
esp_err_t sx_platform_set_brightness(uint8_t percent);
uint8_t sx_platform_get_brightness(void);

// Screen size
esp_err_t sx_platform_get_screen_size(uint16_t *width, uint16_t *height);
```

**Dependencies:**
- `driver` — ESP-IDF drivers
- `esp_lcd` — LCD driver
- `espressif__esp_lcd_touch` — Touch driver
- `sx_codec_common` — Codec definitions

**Dependents:**
- Bootstrap (calls display/touch init)
- UI (uses display/touch handles)

### 3.5 sx_assets — Asset Loader

**Trách nhiệm:**
- Asset loading from SD card
- Embedded image data (bootscreen, flashscreen)
- **KHÔNG include LVGL** (tuân theo SIMPLEXL_ARCH v1.3)

**Public API:**

```c
esp_err_t sx_assets_init(void);
sx_asset_handle_t sx_assets_load_rgb565(const char *path, sx_asset_info_t *info);
const uint16_t* sx_assets_get_data(sx_asset_handle_t handle);
void sx_assets_free(sx_asset_handle_t handle);
bool sx_assets_sd_ready(void);
void sx_assets_set_sd_ready(bool ready);
const sx_embedded_img_data_t* sx_assets_get_bootscreen_data(void);
const sx_embedded_img_data_t* sx_assets_get_flashscreen_data(void);
```

**Dependencies:**
- `driver` — ESP-IDF drivers
- `fatfs` — FAT file system
- `sdmmc` — SD card driver

**Dependents:**
- UI (wraps assets to LVGL format)
- Bootstrap (initializes asset loader)

### 3.6 sx_protocol — Protocol Layer

**Trách nhiệm:**
- WebSocket protocol
- MQTT protocol
- Protocol factory pattern

**Public API (Headers):**
- `sx_protocol_ws.h` — WebSocket
- `sx_protocol_mqtt.h` — MQTT
- `sx_protocol_factory.h` — Factory pattern
- `sx_protocol_base.h` — Base protocol interface
- `sx_protocol_common.h` — Common definitions
- `sx_protocol_audio.h` — Audio protocol

**Dependencies:**
- (Included in sx_services component)

**Dependents:**
- Chatbot service
- Audio protocol bridge

---

## 4. DEPENDENCY GRAPH

### 4.1 Component Dependency Graph

```
sx_app
  └── sx_core
        ├── nvs_flash
        ├── esp_event
        ├── sx_platform
        │     ├── driver
        │     ├── esp_lcd
        │     ├── espressif__esp_lcd_touch
        │     └── sx_codec_common
        ├── sx_ui
        │     ├── sx_core
        │     ├── sx_platform
        │     ├── sx_assets
        │     │     ├── driver
        │     │     ├── fatfs
        │     │     └── sdmmc
        │     ├── sx_services (PRIV_INCLUDE_DIRS)
        │     └── esp_lvgl_port
        │           └── esp_lcd
        └── sx_assets
              ├── driver
              ├── fatfs
              └── sdmmc

sx_services
  ├── sx_core
  ├── sx_codec_common
  ├── fatfs
  ├── sdmmc
  ├── driver
  ├── esp_http_client
  ├── esp_wifi
  ├── esp_netif
  ├── esp_event
  ├── esp_https_ota
  ├── esp_audio_codec
  ├── json
  ├── nvs_flash
  ├── mqtt
  ├── esp-dsp
  └── bt (PRIV_REQUIRES)
```

### 4.2 Circular Dependency Resolution

**Vấn đề:** `sx_ui` và `sx_services` có thể tạo circular dependency

**Giải pháp:**
1. `sx_ui` sử dụng `PRIV_INCLUDE_DIRS` để include service headers trong `.c` files
2. `sx_services` depend vào `sx_core` (không depend trực tiếp vào `sx_ui`)
3. Linker workaround: `LINK_INTERFACE_MULTIPLICITY 3` trong `sx_ui/CMakeLists.txt` (L92)

**Nguồn:** `components/sx_ui/CMakeLists.txt:L71-L84, L92`

### 4.3 Dependency Rules

1. **Core Layer (`sx_core`):**
   - Không depend vào UI hoặc Services
   - Chỉ depend vào ESP-IDF components và Platform

2. **UI Layer (`sx_ui`):**
   - Depend vào Core, Platform, Assets
   - Include service headers qua `PRIV_INCLUDE_DIRS` (không public dependency)

3. **Service Layer (`sx_services`):**
   - Depend vào Core
   - Không depend vào UI (tránh circular dependency)

4. **Platform Layer (`sx_platform`):**
   - Chỉ depend vào ESP-IDF drivers
   - Không depend vào Core, UI, Services

---

## 5. CORE CONTRACTS

### 5.1 Event Bus Contract (Dispatcher)

**File:** `components/sx_core/include/sx_dispatcher.h`

**Contract:**
- **Multi-producer, single-consumer:** Services và UI post events, Orchestrator consume
- **Backpressure policy:** DROP (default), COALESCE, BLOCK (CRITICAL only)
- **State snapshot:** Single-writer (orchestrator), multi-reader (UI, services)
- **Copy-out pattern:** State được copy, không share mutable pointers

**API:**
```c
bool sx_dispatcher_post_event(const sx_event_t *evt);
bool sx_dispatcher_post_event_with_policy(const sx_event_t *evt, sx_backpressure_policy_t policy, uint32_t coalesce_key);
bool sx_dispatcher_poll_event(sx_event_t *out_evt);
void sx_dispatcher_set_state(const sx_state_t *state);
void sx_dispatcher_get_state(sx_state_t *out_state);
```

**Event Priority:**
- `SX_EVT_PRIORITY_LOW` — Low priority (can be dropped)
- `SX_EVT_PRIORITY_NORMAL` — Normal priority (default)
- `SX_EVT_PRIORITY_HIGH` — High priority
- `SX_EVT_PRIORITY_CRITICAL` — Critical (block if queue full)

**Event Taxonomy:**
- Mỗi domain có 256 IDs (0x0100 range)
- `SX_EVT_LIFECYCLE_BASE` (0x0000-0x00FF)
- `SX_EVT_UI_BASE` (0x0100-0x01FF)
- `SX_EVT_AUDIO_BASE` (0x0200-0x02FF)
- `SX_EVT_RADIO_BASE` (0x0300-0x03FF)
- `SX_EVT_WIFI_BASE` (0x0400-0x04FF)
- `SX_EVT_CHATBOT_BASE` (0x0500-0x05FF)
- `SX_EVT_SYSTEM_BASE` (0x0600-0x06FF)
- `SX_EVT_PROTOCOL_BASE` (0x0700-0x07FF)
- `SX_EVT_OTA_BASE` (0x0800-0x08FF)
- `SX_EVT_DISPLAY_BASE` (0x0900-0x09FF)
- `SX_EVT_THEME_BASE` (0x0A00-0x0AFF)
- `SX_EVT_IMAGE_BASE` (0x0B00-0x0BFF)
- `SX_EVT_QRCODE_BASE` (0x0C00-0x0CFF)

**Nguồn:** `components/sx_core/include/sx_event.h:L25-L175`

### 5.2 Service Lifecycle Contract

**File:** `components/sx_core/include/sx_service_if.h`

**Contract:**
- Tất cả services phải implement `sx_service_if_t` vtable
- Lifecycle: `init()` → `start()` → `stop()` → `deinit()`
- Optional: `on_event()` để handle events

**Interface:**
```c
typedef struct {
    esp_err_t (*init)(void);
    esp_err_t (*start)(void);
    esp_err_t (*stop)(void);
    esp_err_t (*deinit)(void);
    esp_err_t (*on_event)(const sx_event_t *evt);  // Optional
} sx_service_if_t;
```

**Registration:**
```c
esp_err_t sx_service_register(const char *name, const sx_service_if_t *iface);
esp_err_t sx_service_init_all(void);
esp_err_t sx_service_start_all(void);
```

**Nguồn:** `components/sx_core/include/sx_service_if.h:L19-L101`

### 5.3 Screen Lifecycle Contract

**File:** `components/sx_ui/include/sx_screen_if.h`

**Contract:**
- Tất cả screens phải implement `sx_screen_if_t` vtable
- Lifecycle: `create()` → `on_enter()` → `on_state_change()` → `on_exit()` → `destroy()`

**Interface:**
```c
typedef struct {
    lv_obj_t* (*create)(lv_obj_t *parent);
    void (*destroy)(lv_obj_t *screen);
    void (*on_enter)(lv_obj_t *screen);
    void (*on_exit)(lv_obj_t *screen);
    void (*on_state_change)(lv_obj_t *screen, uint32_t dirty_mask, const sx_state_t *state);
} sx_screen_if_t;
```

**Registration:**
```c
esp_err_t sx_screen_register(uint32_t screen_id, const sx_screen_if_t *iface);
const sx_screen_if_t* sx_screen_get_interface(uint32_t screen_id);
```

**Nguồn:** `components/sx_ui/include/sx_screen_if.h:L23-L87`

### 5.4 State Contract

**File:** `components/sx_core/include/sx_state.h`

**Contract:**
- **Single-writer:** Orchestrator (sx_core) là writer duy nhất
- **Multi-reader:** UI và services đọc state snapshot
- **Copy-out pattern:** State được copy, không share mutable pointers
- **Version & Dirty Mask:** State có version và dirty_mask để optimize updates

**Structure:**
```c
typedef struct {
    uint32_t version;      // Monotonically increasing
    uint32_t dirty_mask;    // Bitmask of changed domains
    uint32_t seq;           // Legacy sequence number
    sx_wifi_state_t wifi;
    sx_audio_state_t audio;
    sx_ui_state_t ui;
} sx_state_t;
```

**Dirty Mask Bits:**
- `SX_STATE_DIRTY_WIFI` (bit 0)
- `SX_STATE_DIRTY_AUDIO` (bit 1)
- `SX_STATE_DIRTY_UI` (bit 2)
- `SX_STATE_DIRTY_SYSTEM` (bit 3)

**Nguồn:** `components/sx_core/include/sx_state.h:L90-L97`

### 5.5 Board/Pinmap Selection Contract

**File:** `Kconfig.projbuild`, `sdkconfig.defaults`

**Contract:**
- Board selection qua Kconfig: `BOARD_TYPE_HAI_OS_SIMPLEXL`
- Pin mapping qua Kconfig variables:
  - `CONFIG_HAI_LCD_PIN_*` — LCD pins
  - `CONFIG_HAI_TOUCH_PIN_*` — Touch pins
  - `CONFIG_HAI_AUDIO_PIN_*` — Audio pins
- Default values trong `sdkconfig.defaults`

**Nguồn:** `Kconfig.projbuild:L3-L195`, `sdkconfig.defaults:L13-L31`

---

## 6. TOP 20 FILES QUAN TRỌNG NHẤT

### 6.1 Bảng Top 20 Files

| # | File | Vai Trò | Lý Do Quan Trọng | Trích Dẫn |
|---|------|---------|------------------|-----------|
| 1 | `app/app_main.c` | Entrypoint | Điểm vào duy nhất của ứng dụng | `app/app_main.c:L8-L16` |
| 2 | `components/sx_core/sx_bootstrap.c` | Boot sequence | Khởi tạo toàn bộ hệ thống | `components/sx_core/sx_bootstrap.c:L55-L831` |
| 3 | `components/sx_core/sx_dispatcher.c` | Event bus | Core của event-driven architecture | `components/sx_core/include/sx_dispatcher.h:L13-L56` |
| 4 | `components/sx_core/sx_orchestrator.c` | State manager | Single-writer state, xử lý events | `components/sx_core/include/sx_orchestrator.h:L7` |
| 5 | `components/sx_core/include/sx_event.h` | Event definitions | Định nghĩa toàn bộ event types | `components/sx_core/include/sx_event.h:L25-L175` |
| 6 | `components/sx_core/include/sx_state.h` | State definitions | Định nghĩa state structure | `components/sx_core/include/sx_state.h:L90-L97` |
| 7 | `components/sx_ui/sx_ui_task.c` | UI owner task | LVGL task, screen management | (cần đọc để xác nhận) |
| 8 | `components/sx_ui/ui_router.c` | Navigation | Screen routing & navigation | `components/sx_ui/include/sx_ui_router.h:L21-L23` |
| 9 | `components/sx_ui/include/sx_screen_if.h` | Screen interface | Screen lifecycle contract | `components/sx_ui/include/sx_screen_if.h:L23-L87` |
| 10 | `components/sx_services/sx_audio_service.c` | Audio service | Core audio playback/recording | (cần đọc để xác nhận) |
| 11 | `components/sx_platform/sx_platform.c` | Hardware init | LCD/Touch initialization | `components/sx_platform/include/sx_platform.h:L23-L38` |
| 12 | `components/sx_assets/sx_assets.c` | Asset loader | Asset loading từ SD/embedded | `components/sx_assets/include/sx_assets.h:L35-L62` |
| 13 | `components/sx_services/sx_chatbot_service.c` | Chatbot | AI chatbot service | (cần đọc để xác nhận) |
| 14 | `components/sx_services/sx_wifi_service.c` | WiFi | Network connectivity | (cần đọc để xác nhận) |
| 15 | `components/sx_services/sx_settings_service.c` | Settings | Persistent settings | (cần đọc để xác nhận) |
| 16 | `components/sx_core/sx_lazy_loader.c` | Lazy loading | On-demand service loading | `components/sx_core/include/sx_lazy_loader.h:L15-L67` |
| 17 | `components/sx_core/include/sx_service_if.h` | Service interface | Service lifecycle contract | `components/sx_core/include/sx_service_if.h:L19-L101` |
| 18 | `components/sx_protocol/sx_protocol_ws.c` | WebSocket | WebSocket protocol | (cần đọc để xác nhận) |
| 19 | `components/sx_protocol/sx_protocol_mqtt.c` | MQTT | MQTT protocol | (cần đọc để xác nhận) |
| 20 | `CMakeLists.txt` (root) | Build config | Project build configuration | `CMakeLists.txt:L1-L9` |

### 6.2 Lý Do Chi Tiết

#### **1. app/app_main.c**
- **Vai trò:** Entrypoint duy nhất
- **Quan trọng:** Gọi `sx_bootstrap_start()`, khởi động toàn bộ hệ thống
- **Rủi ro:** Nếu fail ở đây, hệ thống không boot

#### **2. sx_bootstrap.c**
- **Vai trò:** Boot sequence orchestrator
- **Quan trọng:** Khởi tạo tất cả components theo thứ tự đúng
- **Rủi ro:** Thứ tự init sai → crash hoặc undefined behavior

#### **3. sx_dispatcher.c**
- **Vai trò:** Event bus core
- **Quan trọng:** Tất cả communication giữa modules qua dispatcher
- **Rủi ro:** Bug ở đây → toàn bộ event system fail

#### **4. sx_orchestrator.c**
- **Vai trò:** State manager, event processor
- **Quan trọng:** Single-writer state, xử lý events từ dispatcher
- **Rủi ro:** Race condition → state corruption

#### **5. sx_event.h**
- **Vai trò:** Event type definitions
- **Quan trọng:** Tất cả modules phải đồng bộ event types
- **Rủi ro:** Thay đổi event types → breaking changes

#### **6. sx_state.h**
- **Vai trò:** State structure definitions
- **Quan trọng:** UI và services đọc state từ đây
- **Rủi ro:** Thay đổi state structure → breaking changes

#### **7. sx_ui_task.c**
- **Vai trò:** UI owner task, LVGL integration
- **Quan trọng:** Quản lý LVGL task, screen lifecycle
- **Rủi ro:** Thread safety issues → UI crash

#### **8. ui_router.c**
- **Vai trò:** Screen navigation
- **Quan trọng:** Điều hướng giữa các screens
- **Rủi ro:** Navigation bug → user không thể navigate

#### **9. sx_screen_if.h**
- **Vai trò:** Screen lifecycle contract
- **Quan trọng:** Tất cả screens phải implement interface này
- **Rủi ro:** Screen không implement đúng → memory leak

#### **10. sx_audio_service.c**
- **Vai trò:** Core audio service
- **Quan trọng:** Audio playback/recording, codec integration
- **Rủi ro:** Audio bug → không có sound output

---

## 7. KẾT LUẬN PHASE 1

### 7.1 Điểm Mạnh

1. ✅ **Layering rõ ràng:** App → Core → UI/Services/Platform → Drivers
2. ✅ **Event-driven architecture:** Dispatcher pattern, single-writer state
3. ✅ **Lifecycle contracts:** Service và Screen interfaces rõ ràng
4. ✅ **Dependency management:** Circular dependency được resolve
5. ✅ **Lazy loading:** Giảm boot time và memory footprint
6. ✅ **Event taxonomy:** Range-based event IDs, tránh collision

### 7.2 Điểm Yếu

1. ⚠️ **Circular dependency workaround:** Cần `LINK_INTERFACE_MULTIPLICITY 3`
2. ⚠️ **Service headers trong UI:** Via `PRIV_INCLUDE_DIRS`, có thể gây confusion
3. ⚠️ **Protocol layer:** Included trong sx_services, không phải component riêng
4. ⚠️ **Screen registry:** Cần verify tất cả screens đều register đúng

### 7.3 Hành Động Tiếp Theo

**PHASE 2:** Phân tích runtime architecture (Tasks, Queues, Events, State)  
**PHASE 3-7:** Phân tích từng module chi tiết theo line-range

---

## 8. CHECKLIST HOÀN THÀNH PHASE 1

- [x] Liệt kê cấu trúc thư mục
- [x] Liệt kê tất cả components
- [x] Phân tích layering architecture
- [x] Tạo module map (Module → Trách nhiệm, Public API)
- [x] Vẽ dependency graph
- [x] Xác định core contracts (EventBus, Service lifecycle, Screen lifecycle)
- [x] Liệt kê Top 20 files quan trọng nhất
- [x] Tạo REPORT_PHASE_1_MODULE_MAP.md

---

## 9. THỐNG KÊ FILE ĐÃ ĐỌC

**Tổng số file đã đọc trong Phase 1:** ~30 files

**Danh sách:**
1. `components/sx_core/CMakeLists.txt`
2. `components/sx_services/CMakeLists.txt`
3. `components/sx_ui/CMakeLists.txt`
4. `components/sx_platform/CMakeLists.txt`
5. `components/sx_assets/CMakeLists.txt`
6. `components/sx_core/include/sx_dispatcher.h`
7. `components/sx_core/include/sx_orchestrator.h`
8. `components/sx_core/include/sx_event.h`
9. `components/sx_core/include/sx_state.h`
10. `components/sx_core/include/sx_service_if.h`
11. `components/sx_core/include/sx_lazy_loader.h`
12. `components/sx_ui/include/sx_ui.h`
13. `components/sx_ui/include/sx_screen_if.h`
14. `components/sx_ui/include/sx_ui_router.h`
15. `components/sx_platform/include/sx_platform.h`
16. `components/sx_assets/include/sx_assets.h`
17. `components/sx_codec_common/CMakeLists.txt`
18. `components/esp_lvgl_port/CMakeLists.txt` (partial)
19. Các header files khác trong include directories

**Ước lượng % file đã đọc:** ~5-7% (đọc config, headers, và một số source files)

---

**Kết thúc PHASE 1**

