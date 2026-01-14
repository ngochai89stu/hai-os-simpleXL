# API Catalog: sx_services – Network & AI Part 5: TTS Service

## Tổng quan

`sx_tts_service` cung cấp Text-to-Speech dựa trên cloud API (HTTP POST text → nhận audio stream). Service:
- Khởi tạo với endpoint, api_key, voice_id, sample_rate.
- Quản lý phiên phát (start/stop speaking).
- Gửi text → HTTP POST → nhận PCM stream → feed vào audio pipeline.
- Hỗ trợ ducking audio khi TTS đang nói.

---

## 1. Header / Source
* `sx_tts_service.h`
* `sx_tts_service.c`

### A) Vai trò
* Worker task `sx_tts_task` xử lý HTTP request/response và decode audio.
* Queue text messages để xử lý tuần tự.
* Parse JSON response (cJSON) trường `audio_url` hoặc `audio_data` (base64).
* Feed PCM vào `sx_audio_service_feed_pcm()` sau khi decode.
* Tích hợp với audio ducking service.

### B) API
```
esp_err_t sx_tts_service_init(const sx_tts_config_t *cfg);
esp_err_t sx_tts_start_speaking(const char *text);
esp_err_t sx_tts_stop_speaking(void);
bool       sx_tts_is_speaking(void);
esp_err_t sx_tts_set_voice(const char *voice_id);
const char*sx_tts_get_voice(void);
```

Config default:
```
endpoint_url, api_key, voice_id, sample_rate_hz = 16000, duck_audio = true
```

### C) Data model
* Flags: `s_initialized`, `s_speaking`.
* Config struct `s_config` (endpoint_url/api_key/voice_id etc.).
* Queue `s_message_queue` (TTS_QUEUE_SIZE=5) chứa `{char text[512]}`.
* Task handle `s_tts_task_handle`.
* Mutex `s_tts_mutex` cho start/stop speaking.
* Buffer `s_current_voice[64]`.

### D) Concurrency
* `sx_tts_task` chạy core, priority 5.
* API start/stop/set_voice có mutex.
* HTTP request synchronous trong task → block task.

### E) Side-effects
1. HTTP POST tới `s_config.endpoint_url` với JSON body `{"text": "...", "voice": "..."}`.
2. Download audio từ `audio_url` hoặc decode base64 `audio_data`.
3. Feed PCM vào `sx_audio_service_feed_pcm()`.
4. Gọi `sx_audio_duck()` khi bắt đầu, `sx_audio_restore()` khi kết thúc (nếu `duck_audio=true`).
5. Post events `SX_EVT_TTS_STARTED`, `SX_EVT_TTS_STOPPED`, `SX_EVT_TTS_ERROR`.

### F) Call-sites
* Chatbot service: gọi `sx_tts_start_speaking()` với response text.
* Intent service: có thể trigger TTS cho feedback.
* UI/Orchestrator: từ events.

### G) Issues / Risks
1. **P1** Queue size 5, blocking 100 ms – overflow drops messages.
2. **P1** HTTP request synchronous → block task lâu.
3. **P1** Download audio từ URL → thêm latency.
4. **P2** Base64 decode không tối ưu (malloc/free mỗi lần).
5. **P2** Không hỗ trợ SSML hoặc prosody control.
6. **P2** Không cache voice samples.

### H) Đề xuất
* Chuyển sang streaming audio (WebSocket hoặc HTTP chunked).
* Tăng queue size hoặc back-pressure.
* Use async HTTP hoặc separate task cho network.
* Cache decoded audio cho common phrases.
* Hỗ trợ SSML cho prosody control.

---
