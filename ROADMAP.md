# ROADMAP hoàn thiện dự án `hai-os-simplexl`

> Mục tiêu: đưa firmware từ trạng thái “chạy được nhiều tính năng nhưng còn rủi ro crash/khó bảo trì” lên trạng thái **ổn định – dễ mở rộng – dễ release**.
>
> Phạm vi roadmap này ưu tiên theo: **ổn định (P0)** → **đúng kiến trúc & đúng dữ liệu (P1)** → **hiệu năng & tối ưu (P2)** → **release & quy trình (P3)**.

---

## 0) Hiện trạng nhanh (baseline)

### Điểm mạnh
- Kiến trúc event/state rõ ràng (theo `docs/SIMPLEXL_ARCH.md`):
  - Event queue: multi-producer / single-consumer (orchestrator)
  - State snapshot: single-writer (orchestrator) / multi-reader (UI)
- UI dùng LVGL v9 + `esp_lvgl_port`.
- Service phong phú: audio (play/rec, EQ/crossfade), SD, WiFi, OTA, chatbot + MCP, WS/MQTT (MQTT có UDP audio).

### Điểm yếu/rủi ro lớn (cần sửa sớm)
- **Kỷ luật LVGL lock / vòng đời screen chưa chuẩn** (nguy cơ crash/treo ngẫu nhiên).
- Một số event handling bị lặp logic (ví dụ `SX_EVT_CHATBOT_AUDIO_CHANNEL_OPENED` trong orchestrator).
- State chưa đủ để UI hiển thị đầy đủ nội dung chat (payload STT/TTS bị free sau event).
- `build/` và artefact nằm trong cây dự án, repo hiện không có `.git` → khó release/bảo trì.

---

## 1) Roadmap theo giai đoạn & ưu tiên

### P0 — Ổn định hệ thống (1–3 ngày)
Mục tiêu: loại bỏ crash/treo hiếm, làm cho UI và event flow **đúng thread-safety**.

#### 1.1 Chuẩn hóa LVGL lock (BẮT BUỘC)
**Vấn đề:** hiện tại có dấu hiệu:
- Lock lồng nhau (`sx_ui_task` gọi `ui_router_navigate_to()` trong khi đang lock, còn router lại lock).
- `on_update/on_create/on_show/on_destroy` có thể gọi LVGL nhưng không lock.
- `ui_router_navigate_to` gọi `on_hide` 2 lần.

**Việc cần làm (đề xuất chuẩn hóa 1 trong 2 mô hình):**

**Mô hình A (khuyến nghị): “UI task giữ lock, router/screen KHÔNG tự lock”**
- Quy ước: *Tất cả* hàm router/screen callback chỉ được gọi khi UI task đã giữ LVGL mutex.
- Xóa toàn bộ `lvgl_port_lock/unlock` bên trong `ui_router.c`.
- Trong `sx_ui_task.c`, bao quanh các đoạn:
  - `ui_router_navigate_to(...)`
  - `callbacks->on_update(...)`
  - và bất kỳ thao tác LVGL nào
  bằng `lvgl_port_lock(0)`.

**Mô hình B: “router tự lock, UI task không lock khi gọi router/callback”**
- Cần đảm bảo mọi callback đều chạy trong lock nội bộ router.
- Nhược điểm: lock/unlock nhiều lần, khó tối ưu.

**Checklist sửa file:**
- `components/sx_ui/sx_ui_task.c`
  - Đảm bảo `callbacks->on_update(&state)` chạy trong lock.
  - Đảm bảo không gọi router khi đang lock nếu router còn lock.
- `components/sx_ui/ui_router.c`
  - Chỉ gọi `on_hide` 1 lần.
  - Đảm bảo `on_create/on_show/on_destroy` không chạy “ngoài lock” nếu chúng đụng LVGL.

**Tiêu chí DONE:**
- Không còn lock lồng nhau.
- Tất cả LVGL calls nằm trong lock.
- Điều hướng màn hình không gọi lifecycle trùng.

#### 1.2 Sửa lỗi logic orchestrator (event xử lý trùng)
- File: `components/sx_core/sx_orchestrator.c`
- Sửa: bỏ xử lý trùng `SX_EVT_CHATBOT_AUDIO_CHANNEL_OPENED`.

#### 1.3 Thêm thống kê drop event (giám sát)
- File: `components/sx_core/sx_dispatcher.c`
- Hiện `sx_dispatcher_post_event()` dùng `xQueueSend(..., 0)` → dễ drop.
- Thêm:
  - counter `s_evt_drop_count`
  - log rate-limit khi drop
  - (tùy chọn) timeout nhỏ 1–5ms cho event “quan trọng”.

---

### P1 — Hoàn thiện “dữ liệu UI” & đúng kiến trúc event/state (3–7 ngày)
Mục tiêu: UI hiển thị đúng nội dung, state/event contract rõ ràng, giảm hack kiểu `status_text = "tts_sentence"`.

#### 2.1 Thiết kế state cho Chat (UI cần hiển thị)
**Vấn đề:** orchestrator nhận event STT/TTS rồi free `evt.ptr`, nhưng state không lưu text.

**Giải pháp đề xuất:**
- Mở rộng `sx_state_t` (file: `components/sx_core/include/sx_state.h`) để có khối chat:
  - `char last_user_text[...];`
  - `char last_assistant_text[...];`
  - (tùy chọn) ring buffer N tin nhắn (phức tạp hơn)
- Orchestrator copy text vào state trước khi free.

**Tiêu chí DONE:**
- UI chat screen chỉ cần đọc state snapshot để render.

#### 2.2 Chuẩn hóa ownership của `sx_event_t.ptr`
- Viết rõ trong `sx_event.h` (comment):
  - ptr có thể là string từ pool (sx_event_alloc_string) → consumer phải free bằng `sx_event_free_string`.
  - ptr không bao giờ là stack pointer.
- Audit các producer chính: WS/MQTT/chat screen/radio/audio.

#### 2.3 Refactor parser JSON dùng chung cho WS & MQTT
**Vấn đề:** `sx_protocol_ws.c` và `sx_protocol_mqtt.c` lặp logic parse JSON.

**Giải pháp:**
- Tạo module chung: `sx_protocol_message_parser.[ch]` (hoặc đặt trong `sx_protocol_audio.h` tuỳ cấu trúc)
- Hàm kiểu:
  - `sx_protocol_handle_json_message(const char* payload, size_t len)`
  - parse type: stt/tts/llm/hello/mcp
  - phát event qua dispatcher

**Tiêu chí DONE:**
- 1 chỗ sửa parser thay vì 2.

---

### P2 — Hiệu năng & độ bền (1–3 tuần)
Mục tiêu: giảm phân mảnh heap, giảm jitter audio/UI, giảm log spam.

#### 3.1 Audio: bỏ malloc/free trong đường nóng
- File: `components/sx_services/sx_audio_service.c`
- Hiện `sx_audio_service_feed_pcm()` malloc buffer mỗi lần để EQ/crossfade.

**Giải pháp:**
- Dùng buffer pool (đã có dấu hiệu `sx_audio_buffer_pool.*` trong repo):
  - cấp phát buffer cố định theo block
  - tái sử dụng, giảm fragmentation
- Hoặc xử lý in-place (nếu caller cho phép mutable) để tránh copy.

#### 3.2 Giảm log ở đường nóng
- Chuyển các log loop (UI frame/audio record) sang `ESP_LOGD` và compile-time enable.
- Thêm rate-limit cho log event spam.

#### 3.3 Backpressure / ưu tiên event
- Xem xét tách 2 queue: UI input vs service events, hoặc event priority.
- Hoặc tăng queue size + timeout có chọn lọc.

---

### P3 — Release & quy trình (song song, 1–2 ngày setup + duy trì)
Mục tiêu: project “đúng chuẩn repo”, build reproducible.

#### 4.1 Đưa vào Git + chuẩn hóa `.gitignore`
- Không commit `build/`, `*.log`, artefact.
- Giữ `managed_components/` tùy chiến lược (vendor vs managed) nhưng phải nhất quán.

#### 4.2 Chuẩn hóa cấu hình board
- Đưa pin mapping (I2S GPIO, SD SPI GPIO, LCD config) vào:
  - Kconfig / sdkconfig.defaults
  - hoặc `board.h` theo profile

#### 4.3 Tài liệu hóa “cách build/flash/monitor”
- Không cần dài, chỉ cần:
  - version ESP-IDF
  - lệnh build
  - partition table
  - yêu cầu PSRAM

---

## 2) Milestone đề xuất (có tiêu chí nghiệm thu)

### Milestone M0 — UI ổn định (P0)
**Done khi:**
- Không crash do LVGL lock trong 30–60 phút test thao tác UI.
- Router không gọi lifecycle trùng.

### Milestone M1 — Chat hiển thị đúng (P1)
**Done khi:**
- STT/TTS/emotion cập nhật UI bằng state snapshot, không phụ thuộc “status_text = stt_result”.

### Milestone M2 — Audio mượt & bền (P2)
**Done khi:**
- Playback/record không bị drop/jitter do heap fragmentation.

### Milestone M3 — Có thể release (P3)
**Done khi:**
- Repo sạch, reproducible build, có tag version.

---

## 3) Phân công công việc (gợi ý)
- 1 người tập trung UI lock/router (P0)
- 1 người tập trung orchestrator/state/chat (P1)
- 1 người tập trung audio performance/buffer pool (P2)
- 1 người setup Git/release hygiene (P3)

---

## 4) Phụ lục: các file trọng yếu cần ưu tiên đọc/sửa
- `components/sx_ui/sx_ui_task.c`
- `components/sx_ui/ui_router.c`
- `components/sx_core/sx_orchestrator.c`
- `components/sx_core/sx_dispatcher.c`
- `components/sx_core/include/sx_state.h`
- `components/sx_services/sx_audio_service.c`
- `components/sx_protocol/sx_protocol_ws.c`
- `components/sx_protocol/sx_protocol_mqtt.c`


