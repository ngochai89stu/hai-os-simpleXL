# RISKS_P0_P1 — Danh sách rủi ro ưu tiên (kèm bằng chứng + repro + fix)

> Mỗi item: **Symptom → Root cause → Evidence → Repro idea → Fix proposal → Files impacted**.

---

## P0 (phải xử lý trước khi coi là ổn định)

### P0-01 — Router gọi `on_hide()` 2 lần (double-callback)
- **Symptom:** màn hình bị dọn dẹp 2 lần, timer bị del 2 lần, object bị xoá sai thứ tự → crash hiếm.
- **Root cause:** `ui_router_navigate_to()` gọi `on_hide()` ở 2 chỗ khác nhau.
- **Evidence:** `components/sx_ui/ui_router.c::ui_router_navigate_to()` có 1 block gọi `old_callbacks->on_hide()` trước, và 1 block gọi lại trong block `if (lvgl_port_lock(0)) { ... }`.
- **Repro idea:** chuyển screen liên tục (BOOT→FLASH→HOME→FLASH...) + bật verify logs; quan sát on_hide log lặp.
- **Fix proposal:** chỉ gọi `on_hide()` đúng 1 lần theo lifecycle chuẩn; quyết định gọi trong lock hay ngoài lock, nhưng không được gọi 2 lần.
- **Files impacted:** `components/sx_ui/ui_router.c`.

### P0-02 — Kỷ luật LVGL lock không nhất quán (nested lock / gọi ngoài lock)
- **Symptom:** treo (deadlock) hoặc crash ngẫu nhiên trong LVGL, đặc biệt khi navigate + timer + input.
- **Root cause:** router tự lock; screen callbacks cũng tự lock; UI task cũng có đoạn lock rồi gọi router (nested).
- **Evidence:**
  - `components/sx_ui/ui_router.c::ui_router_navigate_to()` dùng `lvgl_port_lock(0)`.
  - `components/sx_ui/screens/screen_boot.c::on_create()` dùng `lvgl_port_lock(0)`.
  - `components/sx_ui/screens/screen_boot.c::boot_timer_cb()` lock rồi gọi `ui_router_navigate_to()`.
- **Repro idea:** để Boot timer tự navigate, đồng thời spam touch/gesture ở flash/home; quan sát có khi router lock fail hoặc treo.
- **Fix proposal:** chọn **1 mô hình**:
  - (A) UI task giữ lock, router/screen không tự lock; hoặc
  - (B) router giữ lock và đảm bảo callback chạy trong lock, screen không lock.
- **Files impacted:** `sx_ui_task.c`, `ui_router.c`, nhiều `screen_*.c`.

### P0-03 — Dispatcher post event không block → drop event khi queue đầy
- **Symptom:** mất event (UI input, chatbot events, audio events) khi tải cao → UI/logic lệch.
- **Root cause:** `xQueueSend(..., 0)`.
- **Evidence:** `components/sx_core/sx_dispatcher.c::sx_dispatcher_post_event()`
  - Trích: `xQueueSend(s_evt_q, evt, 0)`
- **Repro idea:** spam message/chat nhanh + spam touch gesture + streaming events → quan sát hành vi thiếu event.
- **Fix proposal:**
  - thêm metric drop + log rate-limit;
  - cho một số event critical dùng timeout nhỏ (1–10ms) hoặc `xQueueSendToBack` với timeout;
  - cân nhắc queue priority (P1/P2).
- **Files impacted:** `components/sx_core/sx_dispatcher.c`.

### P0-04 — Resource leak khi display init fail path
- **Symptom:** nếu init LCD fail (SPI/panel IO/panel driver), hệ thống có thể leak SPI bus/IO/LEDC config.
- **Root cause:** `sx_platform_display_init()` return sớm mà không cleanup.
- **Evidence:** `components/sx_platform/sx_platform.c::sx_platform_display_init()`
  - Nếu `esp_lcd_new_panel_io_spi` fail: return handles (không deinit bus/pwm)
  - Nếu `esp_lcd_new_panel_st7796` fail: return handles có `io_handle` nhưng không delete io_handle
- **Repro idea:** cố tình cấu hình sai chân CS/DC hoặc tháo LCD; boot nhiều lần.
- **Fix proposal:** bổ sung cleanup trên fail path: `esp_lcd_panel_io_del`, `spi_bus_free`, rollback LEDC nếu cần.
- **Files impacted:** `components/sx_platform/sx_platform.c`.

### P0-05 — Double-handle event trong orchestrator (chatbot audio channel)
- **Symptom:** enable/disable audio receive có thể bị gọi lặp; logic khó hiểu.
- **Root cause:** xử lý event type bị trùng nhánh.
- **Evidence:** `components/sx_core/sx_orchestrator.c` có nhiều nhánh cho `SX_EVT_CHATBOT_AUDIO_CHANNEL_OPENED`.
- **Repro idea:** MQTT hello mở UDP nhiều lần → quan sát log enable receive.
- **Fix proposal:** gom xử lý event theo switch-case hoặc if-else chain duy nhất cho mỗi event.
- **Files impacted:** `components/sx_core/sx_orchestrator.c`.

### P0-06 — String pool size nhỏ (8 slots) + fallback malloc → nguy cơ fragmentation khi burst
- **Symptom:** khi STT/TTS message burst (hoặc spam UI input), pool đầy → `strdup` nhiều → phân mảnh heap.
- **Root cause:** `SX_EVENT_STRING_POOL_SIZE 8`.
- **Evidence:** `components/sx_core/include/sx_event_string_pool.h`
  - `#define SX_EVENT_STRING_POOL_SIZE 8`
  - `sx_event_alloc_string()` fallback `strdup()` khi pool full.
- **Repro idea:** spam 20 message trong thời gian ngắn; quan sát log "pool full".
- **Fix proposal:** tăng pool size hoặc chuyển sang ring-buffer/heap caps + metrics.
- **Files impacted:** `sx_event_string_pool.[ch]`.

---

## P1 (cần xử lý để đúng tính năng & dễ mở rộng)

### P1-01 — State không lưu nội dung chat/STT/TTS (UI khó render)
- **Symptom:** UI muốn hiển thị STT/TTS text nhưng orchestrator đã free payload; state chỉ set `status_text` dạng token.
- **Root cause:** model state thiếu fields chat.
- **Evidence:**
  - `components/sx_core/include/sx_state.h::sx_ui_state_t` chỉ có `status_text`, `emotion_id`.
  - `sx_protocol_ws.c` phát event với `.ptr = sx_event_alloc_string(text)`.
  - `sx_orchestrator.c` free `evt.ptr` sau khi set `status_text`.
- **Repro idea:** nhận STT/TTS; UI không có nguồn text nếu chỉ đọc state.
- **Fix proposal:** mở rộng `sx_state_t` (thêm buffer last_user_text/last_assistant_text hoặc message ring) và copy payload trước khi free.
- **Files impacted:** `sx_state.h`, `sx_orchestrator.c`, `screen_chat.c`.

### P1-02 — Parser JSON WS/MQTT bị lặp code
- **Symptom:** sửa logic message type phải sửa 2 nơi; dễ lệch behavior.
- **Root cause:** `sx_protocol_ws.c` và `sx_protocol_mqtt.c` parse JSON tương tự.
- **Evidence:** cả hai file đều xử lý type: stt/tts/llm/mcp/hello.
- **Repro idea:** thêm type mới (ví dụ "error") dễ quên update 1 nơi.
- **Fix proposal:** trích parser chung `sx_protocol_msg_parser.[ch]`.
- **Files impacted:** `components/sx_protocol/sx_protocol_ws.c`, `sx_protocol_mqtt.c`.

### P1-03 — Audio hot path malloc trong `feed_pcm`
- **Symptom:** jitter audio hoặc fragmentation khi feed liên tục.
- **Root cause:** `sx_audio_service_feed_pcm()` malloc buffer copy mỗi call.
- **Evidence:** `components/sx_services/sx_audio_service.c::sx_audio_service_feed_pcm()`
  - `malloc(sample_count * sizeof(int16_t))`
- **Repro idea:** play nhạc dài + đổi volume/EQ liên tục; theo dõi heap.
- **Fix proposal:** dùng `sx_audio_buffer_pool` hoặc xử lý in-place.
- **Files impacted:** `sx_audio_service.c`, `sx_audio_buffer_pool.c`.

### P1-04 — Pinmap hardcode (LCD/Touch/Audio/SD)
- **Symptom:** đổi board là phải sửa code.
- **Root cause:** macro `#define LCD_PIN...` trong `sx_platform.c`; audio GPIO trong `sx_audio_service.c`.
- **Evidence:** `components/sx_platform/sx_platform.c` và `components/sx_services/sx_audio_service.c`.
- **Repro idea:** chuyển sang board khác -> init fail.
- **Fix proposal:** đưa vào Kconfig + sdkconfig.defaults hoặc `board.h` profile.
- **Files impacted:** `sx_platform.c`, `sx_audio_service.c`, bootstrap SD config.


