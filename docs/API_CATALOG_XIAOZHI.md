# API Catalog - XIAOZHI (xiaozhi-esp32_vietnam_ref)

**Ngày tạo:** 2025-01-02  
**Mục đích:** Index tổng quan về API, data model, concurrency, ownership rules của XIAOZHI

## Cấu Trúc Component

### main/application
- **Application Class:** Singleton orchestrator, main event loop
- **Device State:** State machine (11 states)
- **Event Group:** Main event bits (SCHEDULE, SEND_AUDIO, WAKE_WORD, VAD, CLOCK_TICK, ERROR)

**Tài liệu chi tiết:** [docs/API_CATALOG/application.md](../xiaozhi-esp32_vietnam_ref/docs/API_CATALOG/application.md)

### main/audio
- **AudioService:** Audio pipeline management, codec, processor
- **AudioCodec:** Codec interface (I2S input/output)
- **AudioProcessor:** AFE (AEC, VAD, NS, AGC), wake word encoding

**Tài liệu chi tiết:** [docs/API_CATALOG/audio_service.md](../xiaozhi-esp32_vietnam_ref/docs/API_CATALOG/audio_service.md)

### main/display
- **Display:** Display abstraction (LCD/OLED)
- **LCDDisplay:** LVGL-based LCD implementation
- **OLEDDisplay:** OLED implementation
- **EmoteDisplay:** Emoji/emotion display

**Tài liệu chi tiết:** [docs/API_CATALOG/display.md](../xiaozhi-esp32_vietnam_ref/docs/API_CATALOG/display.md)

### main/protocols
- **Protocol:** Base protocol class
- **MqttProtocol:** MQTT + UDP implementation
- **WebsocketProtocol:** WebSocket implementation

**Tài liệu chi tiết:** [docs/API_CATALOG/protocol.md](../xiaozhi-esp32_vietnam_ref/docs/API_CATALOG/protocol.md)

### main/system
- **Settings:** NVS wrapper
- **Assets:** Assets partition management (SPIFFS)
- **OTA:** OTA firmware update
- **SystemInfo:** System information utilities

**Tài liệu chi tiết:** [docs/API_CATALOG/settings.md](../xiaozhi-esp32_vietnam_ref/docs/API_CATALOG/settings.md)

## Entry Point

**File:** `main/main.cc:55-74`

```cpp
extern "C" void app_main(void) {
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    auto& app = Application::GetInstance();
    app.Start();
}
```

**Boot Chain:**
1. `app_main()` → ESP event loop → NVS init
2. `Application::GetInstance()` → `Application::Start()`
3. Display init → Audio service init → Main event loop task
4. Clock timer → Network start → Music/Radio services
5. SD card mount → Assets check → OTA check → Protocol start

## Build Configuration

**CMakeLists.txt:** `CMakeLists.txt:7-13`
- Project name: `xiaozhi_vn`
- Project version: `2.0.5.06`
- CMake minimum: 3.16
- Compiler flags: `-Wno-missing-field-initializers`

**Partition Table:** `partitions/v2/16m.csv`
- nvs: 0x9000, 16KB
- otadata: 0xd000, 8KB
- phy_init: 0xf000, 4KB
- ota_0: 0x20000, ~4MB
- ota_1: auto, ~4MB
- assets: 0x800000, 8MB (SPIFFS)

**SDKConfig:** `sdkconfig.defaults`
- Optimization: Size-optimized
- C++ Exceptions: Enabled (pool 1024 bytes)
- RTTI: Enabled
- Bootloader: Skip validation (P1 security risk)
- FreeRTOS: WDT 10s, runtime stats enabled
- LVGL: OS_NONE, CLIB malloc, large font format

## Key Architectural Contracts

### Event System
- **Event Group:** `EventGroupHandle_t` với 7 event bits
- **Main Event Loop:** Single task (priority 3, stack 3.5KB)
- **Schedule Pattern:** `Application::Schedule()` → `main_tasks_` queue → Main event loop

### State Management
- **Device State:** Enum `DeviceState` (11 states)
- **State Events:** ESP-IDF event system (`XIAOZHI_STATE_EVENTS`)
- **State Callbacks:** Vector of callbacks (no unregister mechanism - P1 risk)

### Thread Safety
- **LVGL:** Chạy trong UI task (core 1, priority 5)
- **Audio:** Separate tasks (input priority 8, output priority 4, opus priority 2)
- **Protocol:** Callbacks schedule vào main event loop

## Concurrency Model

**Tasks:**
- `main_event_loop` (priority 3, stack 3.5KB): Main event dispatcher
- `audio_input` (priority 8, core 0): Audio input, wake word feed
- `audio_output` (priority 4): Audio playback
- `opus_codec` (priority 2, stack 25KB): Opus encode/decode
- `touch_task` (priority 5, core 1): Touch input processing
- `lvgl_task` (variable): LVGL rendering
- Board-specific tasks: `batt_mon_task`, `otto_action`, `electron_bot_action`, `weather_task`

**Queues:**
- `main_tasks_` (std::deque): Scheduled callbacks
- `audio_encode_queue` (std::deque, max 2): Audio encode tasks
- `audio_playback_queue` (std::deque, max 2): Audio playback tasks
- `audio_decode_queue` (std::deque, max 40): Audio decode packets
- `audio_send_queue` (std::deque, max 40): Audio send packets
- FreeRTOS queues: `gpio_evt_queue`, `action_queue_`, `jpeg_queue`

**Mutexes:**
- `audio_queue_mutex_` (AudioService): Protects all audio queues
- `mutex_` (Application): Protects `main_tasks_` queue
- Protocol-specific mutexes: MQTT, WebSocket, UDP

## Memory Ownership

- **Audio queues:** AudioService owns (std::deque với mutex)
- **Event payload:** Caller owns (must be valid until consumed)
- **Assets:** Assets singleton owns (mmap partition)
- **Settings:** Settings class owns (NVS handle, RAII pattern)

## Known Issues (from Phase 0/1/2 reports)

**P0 Issues:**
- P0-001: Partition overlap risk (ota_1 vs assets)
- P0-002: Blocking CheckNewVersion() trong Start()
- P0-003: Asset table bounds không validate
- P0-004: Sector erase có thể gây corruption
- P0-005: Blocking HTTP trong main thread
- P0-006: Multipart parser có thể miss boundary

**P1 Issues:**
- P1-001: Bootloader skip validation (security risk)
- P1-002: LVGL OS_NONE với multi-thread
- P1-003: Main event loop stack có thể thiếu
- P1-005: GetCallbacks() return copy
- P1-008: Không check nvs_open() return
- P1-013: OTA server không có authentication

## Tài Liệu Liên Quan

- [docs/REPORT_PHASE_0_BASELINE.md](../xiaozhi-esp32_vietnam_ref/docs/REPORT_PHASE_0_BASELINE.md) - Baseline analysis
- [docs/REPORT_PHASE_1_MODULE_MAP.md](../xiaozhi-esp32_vietnam_ref/docs/REPORT_PHASE_1_MODULE_MAP.md) - Module map
- [docs/REPORT_PHASE_2_RUNTIME.md](../xiaozhi-esp32_vietnam_ref/docs/REPORT_PHASE_2_RUNTIME.md) - Runtime architecture

