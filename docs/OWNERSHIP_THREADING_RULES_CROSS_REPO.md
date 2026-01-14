# Ownership & Threading Rules Cross-Repo

**Ngày tạo:** 2025-01-02  
**Mục đích:** Ownership và threading rules xuyên repo, với invariant audit và line-range evidence

## Invariant Audit (>=10 Invariants)

### Invariant 1: UI Thread Rule

**SIMPLEXL:**
- **Rule:** LVGL API chỉ được gọi từ UI task (`sx_ui`, priority 7)
- **Enforcement:** Compile-time guard (`sx_lvgl.h`) + runtime check (`SX_ASSERT_UI_THREAD()`)
- **Evidence:** `components/sx_ui/include/sx_lvgl.h`, `components/sx_ui/sx_ui_task.c:54-175`
- **Violation Risk:** Services include LVGL → Compile fail (compile-time guard)

**XIAOZHI:**
- **Rule:** LVGL API chỉ được gọi từ LVGL task (core 1, priority variable)
- **Enforcement:** Runtime only (no compile-time guard)
- **Evidence:** `main/display/lcd_display.cc:1467`, `main/display/lcd_touch.cc:46`
- **Violation Risk:** Protocol callbacks có thể gọi LVGL → Runtime crash

**Cross-Repo:**
- **Adapter Rule:** `sx_xiaozhi_adapter` không được include LVGL
- **Mitigation:** State updates qua dispatcher, UI screen callbacks render LVGL

### Invariant 2: Lock Order

**SIMPLEXL:**
- **Rule:** Lock order: `s_state_write_mutex` → `lvgl_port_ctx.lvgl_mux` (nếu cần)
- **Evidence:** `components/sx_core/sx_dispatcher.c:321-365`, `components/sx_ui/sx_ui_task.c:54-175`
- **Violation Risk:** Deadlock nếu lock theo thứ tự ngược

**XIAOZHI:**
- **Rule:** Lock order: `audio_queue_mutex_` → `mutex_` (Application)
- **Evidence:** `main/audio/audio_service.cc:286-289`, `main/application.cc:57`
- **Violation Risk:** Deadlock nếu lock theo thứ tự ngược

**Cross-Repo:**
- **Adapter Rule:** Không được lock cả 2 mutexes cùng lúc
- **Mitigation:** Adapter chỉ lock SIMPLEXL mutexes, XIAOZHI locks trong XIAOZHI context

### Invariant 3: Init Order

**SIMPLEXL:**
- **Rule:** NVS → Dispatcher → Orchestrator → Platform → UI → Services
- **Evidence:** `components/sx_core/sx_bootstrap.c:55-831`
- **Violation Risk:** Services init trước orchestrator → Events không được process

**XIAOZHI:**
- **Rule:** NVS → Event Loop → Application → Display → Audio → Network → Protocol
- **Evidence:** `main/main.cc:55-74`, `main/application.cc:357-615`
- **Violation Risk:** Protocol init trước network → Connection fail

**Cross-Repo:**
- **Adapter Rule:** Adapter init sau SIMPLEXL core, trước XIAOZHI Application
- **Mitigation:** `sx_xiaozhi_adapter_init()` gọi trong `sx_bootstrap_start()` sau orchestrator

### Invariant 4: Memory Ownership

**SIMPLEXL:**
- **Rule:** Event payload owned by producer, valid until consumed
- **Evidence:** `components/sx_core/sx_dispatcher.c:158-182`
- **Violation Risk:** Producer free payload ngay sau post → Consumer đọc invalid pointer

**XIAOZHI:**
- **Rule:** Audio buffers owned by AudioService, valid until consumed
- **Evidence:** `main/audio/audio_service.cc:295-372`
- **Violation Risk:** Queue overflow → Buffer loss

**Cross-Repo:**
- **Adapter Rule:** Audio buffers từ XIAOZHI → SIMPLEXL: Adapter owns, copy vào SIMPLEXL buffer pool
- **Mitigation:** Adapter copy PCM data, không pass pointer

### Invariant 5: ISR Safety

**SIMPLEXL:**
- **Rule:** Event posting từ ISR: ❌ NOT ALLOWED (FreeRTOS queues không ISR-safe trong một số configs)
- **Evidence:** `components/sx_core/sx_dispatcher.c:158` (không có `xQueueSendFromISR`)
- **Violation Risk:** ISR post event → System crash

**XIAOZHI:**
- **Rule:** GPIO events từ ISR: ✅ ALLOWED (dùng `xQueueSendFromISR`)
- **Evidence:** `main/power/power_manager.cc:78` (GPIO ISR → queue)
- **Violation Risk:** None (đúng pattern)

**Cross-Repo:**
- **Adapter Rule:** Adapter không được post events từ ISR
- **Mitigation:** Defer ISR work to task (dùng queue hoặc semaphore)

### Invariant 6: Backpressure Policy

**SIMPLEXL:**
- **Rule:** CRITICAL events: BLOCK, HIGH/NORMAL: COALESCE/DROP, LOW: DROP
- **Evidence:** `components/sx_core/sx_dispatcher.c:158-182`
- **Violation Risk:** Queue overflow → Events dropped

**XIAOZHI:**
- **Rule:** No backpressure policy (Event Group không có queue limit)
- **Evidence:** `main/application.cc:629-711` (Event Group wait với `portMAX_DELAY`)
- **Violation Risk:** Event bits có thể bị overwrite nếu set nhiều lần

**Cross-Repo:**
- **Adapter Rule:** Adapter convert Event Group bits → SIMPLEXL events với priority mapping
- **Mitigation:** HIGH priority events → `SX_EVT_PRIORITY_HIGH`, NORMAL → `SX_EVT_PRIORITY_NORMAL`

### Invariant 7: State Single-Writer

**SIMPLEXL:**
- **Rule:** Orchestrator task là single-writer của state
- **Evidence:** `components/sx_core/sx_dispatcher.c:321-365` (mutex protected)
- **Violation Risk:** Multiple writers → Race condition

**XIAOZHI:**
- **Rule:** Application::SetDeviceState() có thể gọi từ bất kỳ task nào (không có mutex)
- **Evidence:** `main/application.cc:764` (volatile DeviceState, no mutex)
- **Violation Risk:** Race condition nếu set từ multiple tasks

**Cross-Repo:**
- **Adapter Rule:** Adapter không được set SIMPLEXL state trực tiếp
- **Mitigation:** Adapter post events, orchestrator update state

### Invariant 8: Audio Pipeline Ownership

**SIMPLEXL:**
- **Rule:** `sx_audio_service` owns audio pipeline (I2S, codec, router)
- **Evidence:** `components/sx_services/sx_audio_service.c:536, 618`
- **Violation Risk:** Multiple services control audio → Conflict

**XIAOZHI:**
- **Rule:** `AudioService` owns audio pipeline (I2S, codec, processor)
- **Evidence:** `main/audio/audio_service.cc:84, 91, 113`
- **Violation Risk:** Multiple services control audio → Conflict

**Cross-Repo:**
- **Adapter Rule:** SIMPLEXL audio_service làm owner, XIAOZHI chỉ cung cấp voice processing
- **Mitigation:** Adapter feed PCM vào SIMPLEXL audio_service, không control I2S trực tiếp

### Invariant 9: Protocol Callback Context

**SIMPLEXL:**
- **Rule:** Protocol callbacks schedule vào orchestrator (không chạy trực tiếp trong network task)
- **Evidence:** `components/sx_services/sx_protocol_ws.c`, `sx_protocol_mqtt.c`
- **Violation Risk:** Callbacks chạy trong network task → Block network I/O

**XIAOZHI:**
- **Rule:** Protocol callbacks schedule vào main event loop (dùng `Application::Schedule()`)
- **Evidence:** `main/protocols/mqtt_protocol.cc:93-121`, `websocket_protocol.cc:111-165`
- **Violation Risk:** Callbacks chạy trong network task → Block network I/O (mitigated by Schedule)

**Cross-Repo:**
- **Adapter Rule:** XIAOZHI protocol callbacks → Adapter → SIMPLEXL events → Orchestrator
- **Mitigation:** Adapter convert callbacks thành SIMPLEXL events

### Invariant 10: Queue Ownership

**SIMPLEXL:**
- **Rule:** Dispatcher owns event queues, services own service-specific queues
- **Evidence:** `components/sx_core/sx_dispatcher.c:14-18`, service queues
- **Violation Risk:** Queue deleted while in use → Crash

**XIAOZHI:**
- **Rule:** AudioService owns audio queues (std::deque với mutex)
- **Evidence:** `main/audio/audio_service.cc:139-142`
- **Violation Risk:** Queue destroyed while in use → Crash

**Cross-Repo:**
- **Adapter Rule:** Adapter không được delete XIAOZHI queues
- **Mitigation:** Adapter chỉ read/write queues, không manage lifetime

## Thread Context Rules

### SIMPLEXL Thread Contexts

| Task | Priority | Core | Context | Blocking Allowed |
|------|----------|------|---------|------------------|
| `sx_orch` | 8 | Any | Event processing, state write | Max 10ms per event |
| `sx_ui` | 7 | Any | LVGL rendering, state read | Max 16ms (60 FPS) |
| `sx_audio_file` | 4 | 0 | Audio playback | Real-time (I2S) |
| `sx_audio_rec` | 5 | 1 | Audio recording | Real-time (I2S) |
| Service tasks | 5 | Any | Service operations | Service-specific |

### XIAOZHI Thread Contexts

| Task | Priority | Core | Context | Blocking Allowed |
|------|----------|------|---------|------------------|
| `main_event_loop` | 3 | Any | Event processing | Variable |
| `audio_input` | 8 | 0 | Audio input, wake word | Real-time (I2S) |
| `audio_output` | 4 | Any | Audio playback | Real-time (I2S) |
| `opus_codec` | 2 | Any | Opus encode/decode | Variable |
| `touch_task` | 5 | 1 | Touch input | Max 16ms |
| `lvgl_task` | Variable | 1 | LVGL rendering | Max 16ms (60 FPS) |

### Cross-Repo Thread Context Rules

**Adapter Thread Context:**
- **Allowed:** Gọi từ orchestrator task (priority 8), service tasks (priority 5)
- **Not Allowed:** Gọi từ UI task (priority 7), ISR
- **Blocking:** Max 10ms per call (để không block orchestrator)

## Lock Order Rules

### SIMPLEXL Lock Order

```
Level 1: s_state_write_mutex (Dispatcher)
Level 2: lvgl_port_ctx.lvgl_mux (LVGL)
Level 3: Service-specific mutexes (Audio, Playlist, Protocol, etc.)
```

**Rule:** Luôn lock theo thứ tự Level 1 → Level 2 → Level 3

### XIAOZHI Lock Order

```
Level 1: audio_queue_mutex_ (AudioService)
Level 2: mutex_ (Application)
Level 3: Protocol-specific mutexes (MQTT, WebSocket, UDP)
```

**Rule:** Luôn lock theo thứ tự Level 1 → Level 2 → Level 3

### Cross-Repo Lock Order

**Adapter Rule:** Không được lock cả 2 mutexes cùng lúc
- Nếu cần lock SIMPLEXL mutex: Chỉ lock SIMPLEXL mutexes
- Nếu cần lock XIAOZHI mutex: Chỉ lock XIAOZHI mutexes (trong XIAOZHI context)

## Memory Ownership Rules

### SIMPLEXL Memory Ownership

- **Event payload:** Producer owns, valid until consumed
- **State buffers:** Dispatcher owns (persistent)
- **State snapshots:** Caller owns (temporary copy)
- **Service instances:** Service module owns (persistent)

### XIAOZHI Memory Ownership

- **Audio buffers:** AudioService owns (std::deque với mutex)
- **Event payload:** Caller owns (must be valid until consumed)
- **Assets:** Assets singleton owns (mmap partition)
- **Settings:** Settings class owns (NVS handle, RAII pattern)

### Cross-Repo Memory Ownership

**Adapter Rule:**
- **Audio buffers từ XIAOZHI → SIMPLEXL:** Adapter owns, copy vào SIMPLEXL buffer pool
- **Event payload từ XIAOZHI → SIMPLEXL:** Adapter allocates, SIMPLEXL consumes
- **State updates từ XIAOZHI → SIMPLEXL:** Adapter creates state snapshot, SIMPLEXL owns

## Next Steps

1. **BATCH 3:** Implement adapter với ownership rules trên
2. **BATCH 4:** Map protocol callbacks với thread safety
3. **BATCH 5:** Map UI state updates với LVGL guard

