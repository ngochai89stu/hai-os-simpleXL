# API Catalog: sx_services – Network & AI Part 4: STT Service

## Tổng quan

`sx_stt_service` thêm tính năng Speech-to-Text dựa trên cloud API (HTTP POST raw PCM). Service:
- Khởi tạo với endpoint, api_key, sample_rate.
- Quản lý phiên ghi âm (start/stop session).
- Nhận PCM chunk (từ `sx_audio_service` recording callback) → queue → HTTP POST.
- Gọi callback trả transcript và post event `SX_EVT_UI_INPUT` (nếu final).

---

## 1. Header / Source
* `sx_stt_service.h`
* `sx_stt_service.c`

### A) Vai trò
* Worker task `sx_stt_task` đọc queue và gửi HTTP.
* Bảo vệ state bằng `s_stt_mutex`, queue size 5.
* Parse JSON phản hồi (cJSON) trường `transcript`, `is_final`.
* Dispatch UI_INPUT event với chuỗi transcript final.

### B) API
```
esp_err_t sx_stt_service_init(const sx_stt_config_t *cfg);
esp_err_t sx_stt_start_session(sx_stt_result_cb_t cb, void *user);
esp_err_t sx_stt_stop_session(void);
esp_err_t sx_stt_send_audio_chunk(const int16_t *pcm,size_t n);
bool       sx_stt_is_active(void);
const char*sx_stt_get_last_error(void);
```

Config default:
```
chunk_duration_ms = 1000, sample_rate_hz = 16000, auto_send = true
```

### C) Data model
* Flags: `s_initialized`, `s_active`.
* Config struct `s_config` (endpoint_url/api_key etc.).
* Queue `s_chunk_queue` (STT_CHUNK_QUEUE_SIZE=5) chứa struct `{int16_t* pcm; size_t sample_count}`.
* Task handle `s_stt_task_handle`.
* Mutex `s_stt_mutex` cho start/stop session.
* Buffers `s_last_error[256]`.

### D) Concurrency
* `sx_stt_task` chạy core, priority 5.
* API start/stop/ send chunk có mutex + queue.
* `sx_audio_recording_task` gọi `sx_stt_send_audio_chunk`.

### E) Side-effects
1. HTTP POST tới `s_config.endpoint_url` (lib esp_http_client). Header Content-Type: `audio/pcm;rate=16000;channels=1`.
2. Gọi callback user + post dispatcher event khi transcript final.
3. Load endpoint/api_key từ NVS (sx_settings_service) nếu không pass config.

### F) Call-sites
* Audio Service: `sx_audio_start_recording_with_stt()` starts STT session and sends chunks.
* Chatbot / Intent: receives UI_INPUT event transcript.

### G) Issues / Risks
1. **P1** Queue size 5, blocking 100 ms – overflow drops chunks.
2. **P1** HTTP request synchronous; long latency blocks task reading queue.
3. **P1** Sends raw PCM as one POST – no streaming/gRPC → high latency.
4. **P2** API key header Bearer only; cannot add custom header names.
5. **P2** `s_last_error` not mutex-protected.
6. **P2** Task deletion race in stop_session similar pattern.

### H) Đề xuất
* Chuyển sang HTTP chunked/WS streaming để giảm latency.
* Tăng queue size hoặc back-pressure.
* Use async HTTP or separate task for network.
* Protect `s_last_error` with mutex.
* Return event types `STT_PARTIAL`, `STT_FINAL`.

---
