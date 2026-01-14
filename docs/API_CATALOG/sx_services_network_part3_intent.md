# API Catalog: sx_services – Network & AI Part 3: Intent Service

## Tổng quan

`sx_intent_service` cung cấp parsing text-based voice-command thành cấu trúc `sx_intent_t` và dispatch tới handler.

---

## 1. Header / Source
* `sx_intent_service.h`
* `sx_intent_service.c`

### A) Vai trò
* Parse chuỗi text đa ngôn ngữ (EN + VN) thành intent.
* Hỗ trợ một số intent built-in: Music, Radio, Volume, Wi-Fi, IR, AC…
* Cho phép module khác đăng ký handler theo intent type.
* Nếu không có handler custom ➔ dùng handler mặc định (call vào audio, radio, wifi APIs).

### B) API
```
esp_err_t sx_intent_service_init(void);
esp_err_t sx_intent_register_handler(sx_intent_type_t type, sx_intent_handler_t cb);
esp_err_t sx_intent_parse(const char *text, sx_intent_t *out);
esp_err_t sx_intent_execute(const char *text);
```
Structs:
```
typedef enum { UNKNOWN=0, MUSIC_PLAY, MUSIC_STOP, MUSIC_PAUSE, RADIO_PLAY, RADIO_STOP,
               VOLUME_UP, VOLUME_DOWN, VOLUME_SET, WIFI_CONNECT, WIFI_DISCONNECT,
               IR_SEND, AC_CONTROL } sx_intent_type_t;

typedef struct { sx_intent_type_t type; char entity[128]; int value; } sx_intent_t;
```

### C) Data model
* `s_handlers[16]` – bảng function ptr cho mỗi intent.
* `s_initialized` flag.

Invariants:
* Counters chỉ tăng; reset bằng `sx_network_optimizer_reset_stats()`.

### D) Concurrency
* Không mutex; functions có thể gọi từ nhiều task (race khi ghi `s_handlers`).  
  – Risk thấp vì register chỉ tại boot.
* `sx_intent_execute` thực thi synchronous; gọi handler ngay trong caller context.

### E) Side-Effects
* Mặc định gọi: `sx_audio_*`, `sx_radio_*`, `sx_wifi_*`.
* Đăng log ESP_LOG.
* Gửi sự kiện qua dispatcher chưa thấy – intent parse chỉ gọi API trực tiếp.

### F) Call-sites
* Audio Service: `sx_audio_start_recording_with_stt()` starts STT session and sends chunks.
* Chatbot / Intent: receives UI_INPUT event transcript.

### G) Issues / Risks
1. P1: Bộ keyword matching thủ công → dễ false positive, không Unicode-safe.
2. P1: Không thread-safe khi nhiều task cùng gọi register sau init.
3. P2: Entity extraction rất đơn giản (từ pattern tới cuối chuỗi) ➔ cần NLP.
4. P2: Không hỗ trợ intent context / slot filling.

### H) Đề xuất
* Dùng thư viện NLP hoặc regex đầy đủ.
* Thêm mutex cho bảng handler.
* Hỗ trợ callback async (post event) thay vì gọi trực tiếp API.

---
