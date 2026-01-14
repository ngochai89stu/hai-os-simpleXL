# Phase 3: Test Matrix & Stress Tests

**Ngày:** 2025-01-02

## 1) Mục tiêu

- Chuẩn hoá các kịch bản QA/stress-test quan trọng nhất (SD, WiFi, Audio, UI)
- Đảm bảo mỗi thay đổi lớn có checklist test rõ ràng
- Hướng đến automation dần qua Unity tests + scripts

---

## 2) Môi trường test

### 2.1 Target
- ESP32 / ESP32-S3 (tuỳ board dự án)
- SD Card gắn ngoài
- WiFi AP (2.4GHz), có thể chuẩn bị 2 AP để test roaming/reconnect

### 2.2 Build modes
- Debug build (log verbose)
- Release-ish build (log minimal)

---

## 3) KPI/Quan sát bắt buộc

- UI render time: `sx_metrics.ui_render_ms_last/avg/max`
- Heap free min/current
- PSRAM free min/current (nếu có)
- Audio underrun/recovery counters
- WiFi reconnect time (last/max)

---

## 4) Test matrix

### 4.1 Boot & Stability
| ID | Test | Steps | Expected |
|---|---|---|---|
| BOOT-01 | Cold boot | Power cycle 10 lần | Không crash, vào HOME/FLASH ổn định |
| BOOT-02 | Warm reboot | Gửi `SX_EVT_SYSTEM_REBOOT` 10 lần | Không treo, state reset đúng |

### 4.2 WiFi
| ID | Test | Steps | Expected |
|---|---|---|---|
| WIFI-01 | Save credentials | Connect WiFi → reboot | Auto-connect thành công |
| WIFI-02 | Reconnect (AP off/on) | Tắt AP 10s → bật lại | Reconnect trong < 30s |
| WIFI-03 | Wrong password | Set sai pass | Fail sau MAX_RETRY, UI báo lỗi |

### 4.3 SD Card
| ID | Test | Steps | Expected |
|---|---|---|---|
| SD-01 | Mount | Insert SD → boot | SD mount OK |
| SD-02 | Hot unplug | Đang list nhạc → rút SD | Không crash, UI báo mất SD |
| SD-03 | Playlist persistence | Save playlist → reboot → load | Playlist khôi phục đúng |

### 4.4 Audio Playback
| ID | Test | Steps | Expected |
|---|---|---|---|
| AUD-01 | Crossfade | Play track A→B | Crossfade mượt |
| AUD-02 | Gapless | Playlist nhiều track | Chuyển track không gap |
| AUD-03 | EQ flat | EQ enable nhưng all gains 0 | CPU không tăng đáng kể |
| AUD-04 | EQ active bands | Enable 3 band | Audio thay đổi đúng |
| AUD-05 | Underrun recovery | Giảm network băng thông | `audio_underrun_total` tăng và recovery chạy |

### 4.5 Network Streaming (Radio)
| ID | Test | Steps | Expected |
|---|---|---|---|
| NET-01 | Stream AAC | Play station AAC | Stable playback |
| NET-02 | ICY metadata | Stream có metadata | Title update đúng |
| NET-03 | Reconnect | Rớt mạng 10s | Auto reconnect |

### 4.6 UI
| ID | Test | Steps | Expected |
|---|---|---|---|
| UI-01 | FPS baseline | Idle screens | UI render avg < 20ms |
| UI-02 | Stress navigation | Swipe/transition 50 lần | Không leak, không crash |
| UI-03 | Chat update | STT/TTS events | UI update chỉ khi dirty_mask |

---

## 5) Stress test tags

### 5.1 Quy ước
- Test nào là stress: đặt tag `@stress` trong tên test hoặc comment gần RUN_TEST.
- Script `scripts/run_stress_tests.py` sẽ lọc theo `@stress`.

### 5.2 Danh sách stress-tests đề xuất
- `test_event_performance_* @stress`
- `test_event_queue_full @stress`
- `test_state_thread_safety @stress`

---

## 6) Automation roadmap

- Short: tag `@stress` cho 3–5 test quan trọng
- Mid: thêm integration test cho WiFi reconnect/audio gapless
- Long: hardware runner cho GitHub Actions

---

*Draft: 2025-01-02*
