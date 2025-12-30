# MODULE_CATALOG — Danh mục module `hai-os-simplexl`

> Mục tiêu: liệt kê module theo layer + vai trò + dependency + entrypoints.
>
> Lưu ý: danh mục này dựa trên source trong `app/` + `components/` + `managed_components/` và CMakeLists. Không dựa vào `build/`.

---

## 1) Entry points & top-level build

### 1.1 Entry point runtime
- `app/app_main.c::app_main()`
  - Entrypoint ESP-IDF.
  - Gọi: `sx_bootstrap_start()`.

### 1.2 Project CMake
- `CMakeLists.txt`
  - `project(hai_os_simplexl)`.

---

## 2) `sx_core` — Core runtime (events/state/orchestrator/bootstrap)

**Đường dẫn:** `components/sx_core/`

### Vai trò
- Khởi động hệ thống (bootstrap).
- Cung cấp event queue + state snapshot.
- Orchestrator: single-consumer events + single-writer state.
- Lazy loader (cơ chế bật services theo nhu cầu).

### Key files
- `include/sx_bootstrap.h`, `sx_bootstrap.c`
- `include/sx_dispatcher.h`, `sx_dispatcher.c`
- `include/sx_event.h`
- `include/sx_state.h`
- `include/sx_event_string_pool.h`, `sx_event_string_pool.c`
- `include/sx_orchestrator.h`, `sx_orchestrator.c`
- `include/sx_lazy_loader.h`, `sx_lazy_loader.c`

### Dependencies (REQUIRES)
- FreeRTOS, ESP-IDF base.

### Entrypoints
- `sx_bootstrap_start()`
- `sx_dispatcher_init()`, `sx_dispatcher_post_event()`, `sx_dispatcher_get_state()`
- `sx_orchestrator_start()`
- `sx_event_alloc_string()`, `sx_event_free_string()`

---

## 3) `sx_ui` — UI layer (LVGL task + screen framework)

**Đường dẫn:** `components/sx_ui/`

### Vai trò
- UI owner task: init LVGL port, add display, add touch, render loop.
- Screen router + screen registry.
- Screens: boot/flash/home/chat/music/settings/...

### Key files
- `include/sx_ui.h`, `sx_ui_task.c`
- `include/ui_router.h`, `ui_router.c`
- `include/ui_screen_registry.h`, `ui_screen_registry.c`
- `screens/register_all_screens.c`
- `screens/screen_*.c` (~31 files)

### Dependencies
- `esp_lvgl_port` (vendored)
- `lvgl` (managed component `lvgl__lvgl`)
- `sx_core` (dispatcher/state)

### Entrypoints
- `sx_ui_start(const sx_display_handles_t*, const sx_touch_handles_t*)`
- `ui_router_init()`, `ui_router_navigate_to()`

---

## 4) `sx_platform` — Hardware abstraction

**Đường dẫn:** `components/sx_platform/`

### Vai trò
- Init display ST7796 (SPI) + backlight PWM.
- Init touch FT5x06 (I2C).
- SPI bus manager.
- Hardware volume interface.

### Key files
- `include/sx_platform.h`, `sx_platform.c`
- `include/sx_spi_bus_manager.h`, `sx_spi_bus_manager.c`
- `include/sx_platform_volume.h`, `sx_platform_volume.c`

### Dependencies
- `esp_lcd_*`, `driver/spi_master`, `driver/i2c_master`, `driver/ledc`

### Entrypoints
- `sx_platform_display_init()` → trả `sx_display_handles_t`
- `sx_platform_touch_init(sx_touch_handles_t*)`
- `sx_platform_set_brightness(uint8_t)`

---

## 5) `sx_services` — Services layer

**Đường dẫn:** `components/sx_services/`

### Vai trò
- Cung cấp tính năng hệ thống: audio/SD/radio/network/chatbot/OTA/settings/theme/voice/... 

### Evidence build graph
- `components/sx_services/CMakeLists.txt` liệt kê SRCS và `REQUIRES` (fatfs/sdmmc/esp_wifi/esp_https_ota/mqtt/json...).

### Nhóm module chính (theo file)

#### 5.1 Audio stack
- `sx_audio_service.c` (play/record, I2S)
- `sx_audio_eq.c`, `sx_audio_crossfade.c`, `sx_audio_ducking.c`, `sx_audio_reverb.c`
- `sx_audio_power.c`, `sx_audio_router.c`, `sx_audio_mixer.c`, `sx_audio_profiler.c`
- `sx_audio_buffer_pool.c`
- Codec: `sx_codec_mp3.c`, `sx_codec_flac.c`, `sx_codec_aac.c`, `sx_codec_detector.c`, `sx_codec_opus.c` + wrappers
- Voice: `sx_audio_afe.c`, `sx_audio_afe_esp_sr.cpp`, `sx_wake_word_service.c`, `sx_wake_word_esp_sr.cpp`

Entrypoints tiêu biểu:
- `sx_audio_service_init()`, `sx_audio_service_start()`, `sx_audio_play_file()`
- `sx_audio_start_recording()`, `sx_audio_set_volume()`

#### 5.2 Storage/assets
- `sx_sd_service.c` (mount)
- `sx_sd_music_service.c`
- `sx_assets` nằm ở component riêng nhưng được service dùng.
- `sx_image_service.c` (load/decode image)

#### 5.3 Network
- `sx_wifi_service.c`
- `sx_network_optimizer.c`

#### 5.4 Chatbot/MCP
- `sx_chatbot_service.c`
- `sx_mcp_server.c`
- `sx_mcp_tools*.c` (device/radio/navigation/ir)
- `sx_intent_service.c`

#### 5.5 OTA/Settings/Theme
- `sx_ota_service.c`
- `sx_settings_service.c`
- `sx_theme_service.c`

#### 5.6 Radio/Music online
- `sx_radio_service.c`, `sx_radio_online_service.c`
- `sx_music_online_service.c`
- `sx_playlist_manager.c`

#### 5.7 BLE/Navigation
- `sx_bluetooth_service.c`
- `sx_navigation_service.c`, `sx_navigation_ble.c`, `sx_navigation_ble_parser.c`
- `sx_navigation_icon_cache.c`, `sx_geocoding.c`

#### 5.8 System
- `sx_power_service.c`
- `sx_led_service.c`
- `sx_state_manager.c`
- `sx_diagnostics_service.c`
- `sx_weather_service.c`

---

## 6) `sx_protocol` — Protocol layer

**Đường dẫn:** `components/sx_protocol/`

### Vai trò
- WebSocket protocol + parse JSON + audio binary framing.
- MQTT protocol + parse JSON + mở kênh UDP audio.

### Key files
- `sx_protocol_ws.c`, `include/sx_protocol_ws.h`
- `sx_protocol_mqtt.c`, `include/sx_protocol_mqtt.h`
- `sx_protocol_mqtt_udp.c`
- `include/sx_protocol_audio.h`

---

## 7) `sx_assets` — Assets

**Đường dẫn:** `components/sx_assets/`

### Vai trò
- Cung cấp assets embed (bootscreen/flashscreen) và quản lý assets SD.

### Key files
- `sx_assets.c`, `include/sx_assets.h`
- `generated/bootscreen_img.c`, `generated/flashscreen_img.c`

---

## 8) `esp_lvgl_port` (vendored)

**Đường dẫn:** `components/esp_lvgl_port/`

### Vai trò
- Port LVGL cho ESP-IDF: mutex, display flush integration, touch integration.

---

## 9) `managed_components` (không liệt kê từng file)

**Vai trò** (theo thư mục đã thấy trong workspace):
- `lvgl__lvgl`: LVGL core
- `espressif__esp_lcd_st7796`, `espressif__esp_lcd_touch_ft5x06`: driver LCD/touch
- `espressif__esp_websocket_client`, `mqtt`: network
- `espressif__esp-sr`, audio codec, dsp...


