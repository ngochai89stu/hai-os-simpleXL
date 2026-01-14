# API Catalog: sx_services – Network & AI Part 6: Wake Word Service

## Tổng quan

`sx_wake_word_service` tích hợp ESP-SR wake word detection engine. Service:
- Khởi tạo ESP-SR model (ví dụ: "Hi Lexin").
- Nhận PCM từ microphone → feed vào ESP-SR → detect wake word.
- Post event `SX_EVT_WAKE_WORD_DETECTED` khi phát hiện.
- Tích hợp với audio recording pipeline.

---

## 1. Header / Source
* `sx_wake_word_service.h`
* `sx_wake_word_service.c` (có thể có wrapper C++ cho ESP-SR)

### A) Vai trò
* Wrap ESP-SR API (`esp_sr_iface.h`, `esp_sr_models.h`).
* Quản lý model handle và audio buffer.
* Feed PCM chunks vào ESP-SR engine.
* Dispatch event khi wake word detected.

### B) API
```
esp_err_t sx_wake_word_service_init(const sx_wake_word_config_t *cfg);
esp_err_t sx_wake_word_service_start(void);
esp_err_t sx_wake_word_service_stop(void);
bool       sx_wake_word_is_active(void);
esp_err_t sx_wake_word_feed_audio(const int16_t *pcm, size_t sample_count);
```

Config default:
```
model_name = "Hi Lexin", threshold = 0.5, sample_rate_hz = 16000
```

### C) Data model
* Flags: `s_initialized`, `s_active`.
* Config struct `s_config` (model_name/threshold etc.).
* ESP-SR model handle `s_model_handle`.
* Audio buffer cho ESP-SR input.
* Mutex `s_wake_word_mutex` cho start/stop.

### D) Concurrency
* `sx_wake_word_feed_audio()` được gọi từ audio recording task.
* ESP-SR engine có thể chạy trong ISR hoặc task riêng (tùy ESP-SR implementation).
* Mutex protect state changes.

### E) Side-effects
1. Initialize ESP-SR model (load từ flash/partition).
2. Feed PCM vào ESP-SR engine.
3. Post event `SX_EVT_WAKE_WORD_DETECTED` với wake word name.
4. Trigger chatbot `sx_chatbot_send_wake_word_detected()`.

### F) Call-sites
* Audio recording task: gọi `sx_wake_word_feed_audio()` với PCM chunks.
* Bootstrap: init và start wake word service.
* Chatbot: nhận wake word event → start listening.

### G) Issues / Risks
1. **P1** ESP-SR model size lớn → tốn flash/PSRAM.
2. **P1** False positive rate phụ thuộc vào threshold.
3. **P2** Không hỗ trợ multiple wake words đồng thời.
4. **P2** Không có confidence score trong event.

### H) Đề xuất
* Tối ưu model size (quantization, pruning).
* Expose confidence score trong event.
* Hỗ trợ multiple wake words.
* Adaptive threshold dựa trên noise level.

---
