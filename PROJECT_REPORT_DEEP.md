# PROJECT_REPORT_DEEP — Báo cáo kỹ thuật chuyên sâu `hai-os-simplexl`

> Repo: `D:\NEWESP32\hai-os-simplexl`
>
> Vai trò phân tích: Kỹ sư firmware ESP-IDF/LVGL.
>
> Nguyên tắc báo cáo: **không phỏng đoán nếu chưa đọc file**. Mọi kết luận quan trọng đều có **bằng chứng** (đường dẫn + hàm/struct/enum/macro + trích ngắn).

---

## Mục lục

1. [Executive summary](#1-executive-summary)
2. [System overview + runtime flows](#2-system-overview--runtime-flows)
   - [2.1 Boot flow](#21-boot-flow)
   - [2.2 UI flow](#22-ui-flow)
   - [2.3 Audio flow](#23-audio-flow)
   - [2.4 Network/Chatbot flow](#24-networkchatbot-flow)
3. [Compliance vs `SIMPLEXL_ARCH`](#3-compliance-vs-simplexl_arch)
4. [Deep dive: CORE (`sx_core`)](#4-deep-dive-core-sx_core)
5. [Deep dive: UI (`sx_ui`)](#5-deep-dive-ui-sx_ui)
6. [Deep dive: PLATFORM (`sx_platform`)](#6-deep-dive-platform-sx_platform)
7. [Deep dive: SERVICES (`sx_services` + `sx_protocol`)](#7-deep-dive-services-sx_services--sx_protocol)
8. [Scorecard (0–10)](#8-scorecard-010)
9. [Appendices](#9-appendices)

---

## 1) Executive summary

Dự án là firmware ESP-IDF cho thiết bị có **LCD ST7796 320x480 + touch FT5x06**, UI dùng **LVGL v9** qua `esp_lvgl_port`, có hệ thống services lớn (audio/codec/SD/WiFi/OTA/chatbot/MCP…). Kiến trúc “event queue + state snapshot + single UI owner task” được mô tả trong `docs/SIMPLEXL_ARCH.md` và đã triển khai ở mức nền tảng.

**Rủi ro P0 lớn nhất (đã có bằng chứng từ code):**
- **Vi phạm kỷ luật LVGL lock / lifecycle router** có thể gây crash/treo ngẫu nhiên.
  - Evidence: `components/sx_ui/ui_router.c` tự `lvgl_port_lock(0)` trong `ui_router_navigate_to()`, trong khi một số screen cũng tự lock; `sx_ui_task.c` có đoạn gọi `ui_router_navigate_to()` trong block đang giữ lock (đã đọc trước đó).
  - Evidence: `components/sx_ui/ui_router.c` gọi `on_hide()` **2 lần**.

**Rủi ro P0 thứ hai:**
- Dispatcher post event **không block** (`xQueueSend(..., 0)`) → dễ drop event khi burst.
  - Evidence: `components/sx_core/sx_dispatcher.c` `sx_dispatcher_post_event()`.

**Rủi ro P0/P1 khác:**
- Orchestrator có nhánh xử lý event bị trùng (double-handle) và thiếu lưu payload chat vào state.
  - Evidence: `components/sx_core/sx_orchestrator.c`.

---

## 2) System overview + runtime flows

### 2.1 Boot flow

**Call graph mức hàm (boot):**
1. `app/app_main.c::app_main()` → `sx_bootstrap_start()`
2. `components/sx_core/sx_bootstrap.c::sx_bootstrap_start()`:
   - `nvs_flash_init()`
   - `sx_settings_service_init()`, `sx_theme_service_init()`, `sx_ota_service_init()`
   - `sx_dispatcher_init()`
   - `sx_orchestrator_start()`
   - `sx_platform_display_init()`
   - `sx_platform_touch_init(&touch_handles)`
   - `sx_spi_bus_manager_init()`
   - `sx_sd_service_init()` + `sx_sd_service_start()`
   - `sx_assets_init()`
   - `sx_ui_start(&display_handles, &touch_handles)`
   - restore brightness: `sx_platform_set_brightness()`
   - audio core: `sx_audio_ducking_init()`, `sx_audio_power_init()`, `sx_audio_router_init()`
   - `sx_audio_service_init()` + `sx_audio_service_start()`

**Evidence:**
- `app/app_main.c::app_main()`
  - Trích: `ESP_ERROR_CHECK(sx_bootstrap_start());`
- `components/sx_core/sx_bootstrap.c::sx_bootstrap_start()`

### 2.2 UI flow

**Call graph mức hàm (UI):**
1. `sx_bootstrap_start()` → `sx_ui_start(display_handles, touch_handles)`
2. `components/sx_ui/sx_ui_task.c::sx_ui_start()` → tạo task `sx_ui_task`
3. `sx_ui_task()`:
   - `lvgl_port_init()`
   - `lvgl_port_add_disp(&disp_cfg)`
   - `ui_router_init()`
   - `register_all_screens()`
   - `lvgl_port_add_touch()` (nếu touch_handle != NULL)
   - `ui_router_navigate_to(SCREEN_ID_BOOT)`
   - post event: `sx_dispatcher_post_event({.type=SX_EVT_UI_READY})`
   - main loop:
     - `sx_dispatcher_get_state(&state)`
     - nếu `state.seq` đổi → `callbacks->on_update(&state)`
     - `lv_timer_handler()` mỗi frame

**Evidence:**
- `components/sx_ui/sx_ui_task.c::sx_ui_task()`
  - Trích (1–3 dòng):
    - `ui_router_init();`
    - `register_all_screens();`
    - `lv_timer_handler();`

**Ghi chú quan trọng:**
- Screen callback có thể tạo LVGL timer callback (chạy trong context LVGL). Ví dụ boot screen:
  - Evidence: `components/sx_ui/screens/screen_boot.c::on_show()`
    - Trích: `s_boot_timer = lv_timer_create(boot_timer_cb, BOOTSCREEN_DISPLAY_TIME_MS, NULL);`

### 2.3 Audio flow

**Call graph mức hàm (audio playback):**
1. UI/Service gọi `sx_audio_play_file(path)`
2. `components/sx_services/sx_audio_service.c::sx_audio_play_file()`:
   - nếu đang play → `sx_audio_stop()`
   - `fopen()`
   - tạo task `sx_audio_playback_task()`
3. `sx_audio_playback_task(FILE *f)`:
   - detect format: `sx_codec_detect_file_format()`
   - init decoder MP3/FLAC nếu cần
   - loop: read → decode → `sx_audio_eq_process()` → apply volume factor → `sx_audio_service_feed_pcm()`
   - end: post `SX_EVT_AUDIO_PLAYBACK_STOPPED`

**Evidence:**
- `components/sx_services/sx_audio_service.c::sx_audio_play_file()`
  - Trích: `xTaskCreatePinnedToCore(sx_audio_playback_task, "sx_audio_file", ...);`
- `components/sx_services/sx_audio_service.c::sx_audio_service_feed_pcm()`
  - Trích: `int16_t *pcm_processed = (int16_t *)malloc(sample_count * sizeof(int16_t));`

### 2.4 Network/Chatbot flow

**Call graph mức hàm (WS path):**
1. WebSocket connect: `sx_protocol_ws.c::websocket_event_handler(...WEBSOCKET_EVENT_CONNECTED...)` → post `SX_EVT_CHATBOT_CONNECTED`
2. WS data text: parse JSON → `sx_event_alloc_string()` → post event `SX_EVT_CHATBOT_STT/TTS/EMOTION...`
3. Orchestrator: `sx_orchestrator.c` poll event → update state → free string `sx_event_free_string()`
4. UI: đọc state, update screen

**Call graph mức hàm (MQTT + UDP audio):**
1. MQTT connected: `sx_protocol_mqtt.c::mqtt_event_handler(...MQTT_EVENT_CONNECTED...)` → post `SX_EVT_CHATBOT_CONNECTED`
2. MQTT data “hello transport=udp”: gọi `sx_protocol_mqtt_udp_init(...)` → post `SX_EVT_CHATBOT_AUDIO_CHANNEL_OPENED`

**Evidence:**
- `components/sx_protocol/sx_protocol_ws.c::websocket_event_handler()`
  - Trích: `.ptr = sx_event_alloc_string(text->valuestring)`
- `components/sx_core/sx_event_string_pool.c::sx_event_alloc_string()`
  - Trích: `return s_event_string_pool[i];` (pool) / `return strdup(src);` (fallback)

---

## 3) Compliance vs `SIMPLEXL_ARCH`

Nguồn quy tắc: `docs/SIMPLEXL_ARCH.md`.

### Rule: “Single UI owner task for all LVGL calls”
- **Đạt một phần / có vi phạm tiềm năng**.
- Evidence UI task owner: `components/sx_ui/sx_ui_task.c::sx_ui_start()` tạo task `sx_ui_task`.
- Vi phạm tiềm năng: router/screen callback tự lock và có thể được gọi sai context.
  - Evidence: `components/sx_ui/ui_router.c::ui_router_navigate_to()` có `lvgl_port_lock(0)`.
  - Evidence: `components/sx_ui/screens/screen_boot.c::on_create()` tự `lvgl_port_lock(0)`.

### Rule: “UI ↔ services communication only via events/state snapshots”
- **Đạt phần lớn**.
- Evidence: orchestrator poll event từ dispatcher; UI đọc state bằng `sx_dispatcher_get_state()`.

### Rule: “Services never include UI headers”
- Chưa audit toàn bộ include graph trong báo cáo này; tuy nhiên core/service/protocol đang tương đối tách.

---

## 4) Deep dive: CORE (`sx_core`)

### 4.1 Event system & string pool

**Key files/entrypoints:**
- `components/sx_core/include/sx_event.h` (enum + struct `sx_event_t`)
- `components/sx_core/sx_event_string_pool.c` + `include/sx_event_string_pool.h`

**Ownership contract thực tế (theo code):**
- Producer (WS/MQTT) tạo string payload bằng `sx_event_alloc_string()`.
  - Evidence: `sx_protocol_ws.c::websocket_event_handler()`
    - Trích: `.ptr = sx_event_alloc_string(text->valuestring)`
- Consumer (orchestrator) giải phóng bằng `sx_event_free_string()`.
  - Evidence: `sx_orchestrator.c` (đã đọc trước đó): `sx_event_free_string((char *)evt.ptr);`

**String pool thiết kế:**
- Pool tĩnh 8 slot, mỗi slot 128 bytes.
  - Evidence: `sx_event_string_pool.h`:
    - `#define SX_EVENT_STRING_POOL_SIZE 8`
    - `#define SX_EVENT_STRING_MAX_LEN 128`
- Nếu string dài hơn 127 → fallback `strdup()`.
  - Evidence: `sx_event_string_pool.c::sx_event_alloc_string()`
    - Trích: `if (src_len >= SX_EVENT_STRING_MAX_LEN) ... return strdup(src);`
- Nếu pool đầy → fallback `strdup()`.
  - Evidence: `sx_event_string_pool.c::sx_event_alloc_string()`
    - Trích: `ESP_LOGW(TAG, "Event string pool full, using malloc fallback"); return strdup(src);`
- Free: nếu pointer thuộc vùng pool thì mark unused, else `free()`.
  - Evidence: `sx_event_string_pool.c::sx_event_free_string()`
    - Trích: `if (str >= s_event_string_pool[0] && str < ...) { ... } else { free(str); }`

**Kết luận CORE về payload contract:**
- Contract có tồn tại và có helper, nhưng **dễ sai** nếu có nơi gửi `evt.ptr` không phải từ pool/malloc (ví dụ pointer static) rồi bị `free()`.

### 4.2 Dispatcher

**Key files:** `components/sx_core/sx_dispatcher.c`.

- Queue được tạo kích thước 64.
  - Evidence: `sx_dispatcher.c::sx_dispatcher_init()`
    - Trích: `s_evt_q = xQueueCreate(64, sizeof(sx_event_t));`
- Post event **không block**.
  - Evidence: `sx_dispatcher.c::sx_dispatcher_post_event()`
    - Trích: `return xQueueSend(s_evt_q, evt, 0) == pdTRUE;`

**Kết luận: có mất event không?**
- Có thể mất event khi queue đầy vì post không block và không có retry.
- Hiện **không có metric/log** cho drop event trong dispatcher.

**State snapshot:**
- Copy toàn bộ `sx_state_t` dưới mutex.
  - Evidence: `sx_dispatcher.c::sx_dispatcher_set_state()`
    - Trích: `s_state = *state;`

### 4.3 State

**Key file:** `components/sx_core/include/sx_state.h`.

- UI state giữ pointer `const char *status_text`, `const char *emotion_id`.
  - Evidence: `sx_state.h::sx_ui_state_t`.

**Rủi ro:** nếu sau này status/emotion chuyển sang string động, cần versioning hoặc buffer cố định.

### 4.4 Orchestrator

**Key file:** `components/sx_core/sx_orchestrator.c`.

- Poll event batch trong loop.
- Free payload bằng `sx_event_free_string()` ở nhiều event.

**Double-handle:**
- `SX_EVT_CHATBOT_AUDIO_CHANNEL_OPENED` xuất hiện nhiều nhánh trong file.
  - Evidence: `sx_orchestrator.c` (đã đọc trước đó; cần fix).

---

## 5) Deep dive: UI (`sx_ui`)

### 5.1 Screen lifecycle thực tế (ví dụ Boot screen)

**Key file:** `components/sx_ui/screens/screen_boot.c`

- `on_create()` tự `lvgl_port_lock(0)` trước khi gọi LVGL.
  - Evidence: `screen_boot.c::on_create()`
    - Trích: `if (!lvgl_port_lock(0)) { ... return; }`
- `on_show()` tạo `lv_timer_create(boot_timer_cb, 3000, NULL)`.
  - Evidence: `screen_boot.c::on_show()`
- Timer callback điều hướng screen:
  - Evidence: `screen_boot.c::boot_timer_cb()`
    - Trích: `ui_router_navigate_to(SCREEN_ID_FLASH);`

**Kết luận UI về thread-safety:**
- Nhiều screen tự lock LVGL trong callback.
- Router cũng tự lock.
- Nếu UI task/Router cũng lock ở ngoài, có nguy cơ nested lock hoặc lock discipline không nhất quán.

### 5.2 Screen Flash (screensaver)

**Key file:** `components/sx_ui/screens/screen_flash.c`

- Nhiều hàm helper tự lock để update label/time/weather.
  - Evidence: `screen_flash.c::update_clock()`
    - Trích: `if (!lvgl_port_lock(0)) { return; } ... lv_label_set_text(...);`
  - Evidence: `screen_flash.c::update_weather()`

**Quan sát:** cách làm này có thể ổn nếu luôn chạy từ UI task, nhưng nếu router callback gọi ngoài lock, mỗi hàm tự lock tạo overhead và nguy cơ deadlock nếu mutex không recursive.

### 5.3 Router

**Key file:** `components/sx_ui/ui_router.c`

- Router tự `lvgl_port_lock(0)`.
  - Evidence: `ui_router.c::ui_router_navigate_to()`
    - Trích: `if (lvgl_port_lock(0)) { ... lv_obj_clean(...); ... lvgl_port_unlock(); }`
- `on_hide()` được gọi 2 lần.
  - Evidence: `ui_router.c::ui_router_navigate_to()` có 1 block gọi on_hide trước, và 1 block gọi on_hide trong lock.

---

## 6) Deep dive: PLATFORM (`sx_platform`)

### 6.1 Display init

**Key file:** `components/sx_platform/sx_platform.c::sx_platform_display_init()`

- Pinmap hardcode.
  - Evidence: macro `#define LCD_PIN_NUM_MOSI 47` ... `LCD_PIN_NUM_BK_LIGHT 42`.
- Backlight PWM dùng LEDC.
  - Evidence: `sx_platform_display_init()` gọi `ledc_timer_config`, `ledc_channel_config`.
- SPI bus init dùng `spi_bus_initialize(LCD_HOST_ID, &buscfg, SPI_DMA_CH_AUTO)`.
  - Evidence: `sx_platform_display_init()`.
- Panel IO: `esp_lcd_new_panel_io_spi(...)`.
- Panel driver: `esp_lcd_new_panel_st7796(...)` với custom init sequence `st7796u_init_cmds`.

**Init order:** PWM backlight → SPI bus → panel IO → panel driver → reset/init → disp_on.

**Cleanup:**
- Nếu create panel IO fail: return empty handles (không cleanup PWM/SPI bus).
- Nếu create panel fail: return handles có io_handle nhưng panel_handle NULL (không delete io_handle).

=> Rủi ro leak resource trong init fail path.

### 6.2 Touch init

**Key file:** `components/sx_platform/sx_platform.c::sx_platform_touch_init(sx_touch_handles_t *touch_handles)`

- Pinmap hardcode (I2C SDA=8, SCL=11, INT=13, RST=9).
- Dùng I2C master bus `i2c_new_master_bus` lưu static `s_touch_i2c_bus_handle`.
- IO config dùng macro `ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG()` và override `scl_speed_hz`.
- Tạo IO: `esp_lcd_new_panel_io_i2c()`.
- Tạo driver: `esp_lcd_touch_new_i2c_ft5x06()`.

**Cleanup có tồn tại trên fail path:**
- Nếu tạo panel IO i2c fail: delete I2C bus.
- Nếu tạo touch driver fail: delete panel io + delete I2C bus.

Điểm tốt: touch init có cleanup khá đầy đủ.

---

## 7) Deep dive: SERVICES (`sx_services` + `sx_protocol`)

### 7.1 CMake graph (services)

**Evidence:** `components/sx_services/CMakeLists.txt` liệt kê SRCS rất nhiều (audio, sd, wifi, protocols, ota, stt/tts/afe...). Đây là bằng chứng dự án có scope lớn.

### 7.2 Audio service

**Key file:** `components/sx_services/sx_audio_service.c`

- I2S std mode: `i2s_new_channel()`, `i2s_channel_init_std_mode()`, `i2s_channel_enable()`.
  - Evidence: `sx_audio_service_init()`.
- Playback tạo task `sx_audio_playback_task`.
  - Evidence: `sx_audio_play_file()`.
- Hot path malloc:
  - Evidence: `sx_audio_service_feed_pcm()`
    - Trích: `int16_t *pcm_processed = (int16_t *)malloc(sample_count * sizeof(int16_t));`

**Kết luận: có đường nóng malloc/free không?**
- Có, ở `feed_pcm` mỗi lần gọi.

### 7.3 Chatbot service

**Key file:** `components/sx_services/sx_chatbot_service.c`

- Queue nội bộ `CHATBOT_QUEUE_SIZE 10`.
  - Evidence: `sx_chatbot_service.c`.
- Task `sx_chatbot_task` xử lý message, parse intent trước.
  - Evidence: `sx_chatbot_task()` gọi `sx_intent_execute(msg.text)`.

### 7.4 Protocol WS/MQTT

**Evidence WS:** `components/sx_protocol/sx_protocol_ws.c::websocket_event_handler()` parse JSON và phát event, string dùng `sx_event_alloc_string`.

**Evidence MQTT:** `components/sx_protocol/sx_protocol_mqtt.c::mqtt_event_handler()` parse JSON tương tự WS (duplication), và mở UDP audio trong hello.

---

## 8) Scorecard (0–10)

> Có kèm lý do + evidence.

- Architecture: **8/10**
  - Evidence: `docs/SIMPLEXL_ARCH.md` + dispatcher/orchestrator/ui task separation.
- Stability: **5/10**
  - Evidence: `ui_router.c` double on_hide + lock discipline không nhất quán; dispatcher drop event policy.
- Performance: **7/10**
  - Evidence: UI loop 16ms; nhưng audio hot path malloc.
- Maintainability: **6/10**
  - Evidence: module hóa tốt, nhưng duplication parser WS/MQTT và lock/lifecycle phức tạp.
- Security: **5/10**
  - Evidence: network JSON parse; cần thêm validation/limits (chưa audit đầy đủ mọi điểm).
- Release readiness: **4/10**
  - Evidence: có nhiều log/artefact; chưa đánh giá CI/release.

---

## 9) Appendices

- Module catalog: `MODULE_CATALOG.md`
- Risks P0/P1: `RISKS_P0_P1.md`
- Test plan: `TEST_PLAN.md`
- Patch plan P0: `PATCH_PLAN_P0.md`






