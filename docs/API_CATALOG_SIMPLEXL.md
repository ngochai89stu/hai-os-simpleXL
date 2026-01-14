# API Catalog - SIMPLEXL (hai-os-simplexl)

**Ngày tạo:** 2025-01-02  
**Mục đích:** Index tổng quan về API, data model, concurrency, ownership rules của SIMPLEXL

## Cấu Trúc Component

### sx_core
- **Dispatcher:** Event posting/polling, priority queues, backpressure
- **Orchestrator:** Single-writer state management, event processing
- **Bootstrap:** System initialization sequence
- **State:** Double-buffer pattern, lock-free read

**Tài liệu chi tiết:** [docs/API_CATALOG/sx_core.md](./API_CATALOG/sx_core.md)

### sx_platform
- **Display:** LCD initialization, brightness control
- **Touch:** I2C touch controller (FT5x06)
- **SPI Bus Manager:** Shared SPI bus (LCD + SD card)
- **Volume:** Hardware volume control (I2C codec chips)

**Tài liệu chi tiết:** [docs/API_CATALOG/sx_platform.md](./API_CATALOG/sx_platform.md)

### sx_ui
- **UI Task:** LVGL rendering, state polling, screen lifecycle
- **Router:** Screen navigation, lifecycle callbacks
- **Screens:** 29+ screen implementations
- **LVGL Guard:** Compile-time + runtime protection

**Tài liệu chi tiết:** [docs/API_CATALOG/sx_ui.md](./API_CATALOG/sx_ui.md)

### sx_services
- **Audio:** Playback, recording, codec, router, EQ, ducking
- **Network:** WiFi, STT, TTS, Wake Word, Chatbot, Protocol (WS/MQTT)
- **Storage:** Settings (NVS), SD card, Assets, Playlist, Metadata
- **System:** OTA, MCP, IR, LED, Power, Navigation, Weather

**Tài liệu chi tiết:** 
- [docs/API_CATALOG/sx_services_audio_part1_core.md](./API_CATALOG/sx_services_audio_part1_core.md)
- [docs/API_CATALOG/sx_services_network_part1_wifi.md](./API_CATALOG/sx_services_network_part1_wifi.md)
- [docs/API_CATALOG/sx_services_storage_part1_settings.md](./API_CATALOG/sx_services_storage_part1_settings.md)

## Entry Point

**File:** `app/app_main.c:8-11`

```c
void app_main(void) {
    ESP_LOGI(TAG, "hai-os-simplexl starting...");
    ESP_ERROR_CHECK(sx_bootstrap_start());
}
```

**Boot Chain:**
1. `app_main()` → `sx_bootstrap_start()`
2. NVS init → Error handler → Settings → Theme → OTA → MCP
3. Dispatcher init → Orchestrator start
4. Platform init (display, touch, SPI bus)
5. SD service init → Assets init
6. UI start
7. Audio services init (ducking, power, router, service)
8. Lazy-loaded services (STT, TTS, Wake Word, Chatbot, WiFi, etc.)

## Build Configuration

**CMakeLists.txt:** `CMakeLists.txt:8`
- Project name: `hai-os-simplexl`
- CMake minimum: 3.16

**Partition Table:** `partitions.csv`
- nvs: 0x9000, 24KB
- phy_init: 0xf000, 4KB
- factory: 0x10000, 3MB
- spiffs: auto, 1MB
- model: auto, 2MB

**SDKConfig:** `sdkconfig.defaults`
- FreeRTOS timer stack: 4096 bytes
- Board: HAI_OS_SIMPLEXL
- LCD: ST7796 320x480
- Touch: FT5x06 (GPIO 8/11)

## Key Architectural Contracts

### Event System
- **Multi-producer, Single-consumer:** Services/UI → Dispatcher → Orchestrator
- **Priority queues:** CRITICAL (8), HIGH (16), NORMAL (32), LOW (16)
- **Backpressure:** DROP (default), COALESCE, BLOCK (CRITICAL only)

### State Management
- **Single-writer:** Orchestrator task only
- **Multi-reader:** Lock-free read (double-buffer pattern)
- **Version + Dirty Mask:** State versioning, dirty mask for UI updates

### Thread Safety
- **LVGL:** Chỉ được gọi từ UI task (compile-time + runtime guard)
- **Services:** Không được include LVGL, không được gọi UI APIs trực tiếp
- **State:** Lock-free read, mutex-protected write

## Concurrency Model

**Tasks:**
- `sx_orch` (priority 8): Orchestrator, event processing
- `sx_ui` (priority 7): UI rendering, LVGL owner
- `sx_audio_file` (priority 4): Audio playback
- `sx_audio_rec` (priority 5): Audio recording
- Service tasks (priority 5): WiFi, Chatbot, STT, TTS, Wake Word, etc.

**Queues:**
- Event dispatcher queues: 4 priority queues
- Service-specific queues: Chatbot, Wake Word, STT, TTS, Audio Protocol

**Mutexes:**
- State write mutex: `s_state_write_mutex`
- LVGL mutex: `lvgl_port_ctx.lvgl_mux`
- Service-specific mutexes: Audio, Playlist, Protocol, etc.

## Memory Ownership

- **Event payload:** Producer owns, must be valid until consumed
- **State buffers:** Dispatcher owns (persistent)
- **State snapshots:** Caller owns (temporary copy)
- **Service instances:** Service module owns (persistent)

## Tài Liệu Liên Quan

- [CALL_GRAPH.md](./CALL_GRAPH.md) - Call graph và dataflow
- [OWNERSHIP_THREADING_RULES.md](./OWNERSHIP_THREADING_RULES.md) - Ownership và threading rules
- [SIMPLEXL_ARCH_v1.3.md](./SIMPLEXL_ARCH_v1.3.md) - Kiến trúc v1.3

