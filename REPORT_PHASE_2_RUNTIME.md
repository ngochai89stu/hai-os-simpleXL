# PHASE 2 — Runtime Architecture (Tasks/Threads/Events/State)
## Báo cáo phân tích runtime: FreeRTOS tasks, queues, events, và concurrency risks

**Ngày tạo:** 2025-01-02  
**Dự án:** hai-os-simplexl  
**Mục tiêu:** Làm rõ hệ thống chạy như thế nào: FreeRTOS tasks, queue, event loop, timer, ISR

---

## 1. FREERTOS TASKS INVENTORY

### 1.1 Bảng Tổng Hợp Tất Cả Tasks

| # | Task Name | Stack Size | Priority | Core | Nơi Tạo | Mục Đích |
|---|-----------|------------|----------|------|---------|----------|
| 1 | `sx_ui` | 8192 (8KB) | 5 | tskNO_AFFINITY | `sx_ui_task.c:L308` | UI owner task, LVGL rendering |
| 2 | `sx_orch` | 3072 (3KB) | 8 | tskNO_AFFINITY | `sx_orchestrator.c:L180` | State orchestrator, event processor |
| 3 | `taskLVGL` | Configurable | Configurable | Configurable | `esp_lvgl_port.c` | LVGL internal task (from esp_lvgl_port) |
| 4 | `sx_audio_file` | 3072 (3KB) | 4 | Core 0 | `sx_audio_service.c:L536` | Audio file playback |
| 5 | `sx_audio_rec` | 4096 (4KB) | 5 | Core 1 | `sx_audio_service.c:L618` | Audio recording |
| 6 | `sx_chatbot` | 3072 (3KB) | 5 | tskNO_AFFINITY | `sx_chatbot_service.c:L84` | Chatbot message processing |
| 7 | `sx_radio_http` | 8192 (8KB) | 3 | Core 0 | `sx_radio_service.c:L278, L329` | Radio HTTP streaming |
| 8 | `sx_wake_word` | 4096 (4KB) | 5 | tskNO_AFFINITY | `sx_wake_word_service.c:L176` | Wake word detection |
| 9 | `audio_send` | 4096 (4KB) | 5 | tskNO_AFFINITY | `sx_audio_protocol_bridge.c:L434` | Audio protocol send |
| 10 | `audio_recv` | 4096 (4KB) | 5 | tskNO_AFFINITY | `sx_audio_protocol_bridge.c:L473` | Audio protocol receive |
| 11 | `nav_queue` | 3072 (3KB) | 5 | tskNO_AFFINITY | `sx_navigation_ble.c:L454` | Navigation BLE queue processing |
| 12 | `sx_stt` | 4096 (4KB) | 5 | tskNO_AFFINITY | `sx_stt_service.c:L214` | Speech-to-Text processing |
| 13 | `sx_tts` | 4096 (4KB) | 5 | tskNO_AFFINITY | `sx_tts_service.c:L89` | Text-to-Speech processing |
| 14 | `audio_recovery` | 2048 (2KB) | 5 | tskNO_AFFINITY | `sx_audio_recovery.c:L142` | Audio error recovery |
| 15 | `led_state` | 2048 (2KB) | 3 | tskNO_AFFINITY | `sx_led_service.c:L371` | LED state-based control |
| 16 | `power_idle` | 2048 (2KB) | 3 | tskNO_AFFINITY | `sx_power_service.c:L138` | Power idle monitoring |
| 17 | `mqtt_udp_rx` | 4096 (4KB) | 5 | tskNO_AFFINITY | `sx_protocol_mqtt_udp.c:L267` | MQTT UDP receive |
| 18 | `sx_act_snd` | 4096 (4KB) | tskIDLE_PRIORITY+1 | tskNO_AFFINITY | `event_handlers_ota.c:L105` | Activation sound playback |
| 19 | `screen_walk` | 4096 (4KB) | 5 | tskNO_AFFINITY | `sx_ui_verify.c:L207` | Screen walk verification (optional) |
| 20 | `hid_task` | 4096 (4KB) | 2 | tskNO_AFFINITY | `esp_lvgl_port_usbhid.c` | USB HID task (optional) |

**Tổng số tasks:** ~20 tasks (không tính ESP-IDF system tasks)

**Tổng stack memory:** ~80KB+ (ước lượng, không tính ESP-IDF tasks)

### 1.2 Task Priority Analysis

**Priority Distribution:**
- **Priority 8:** `sx_orch` (orchestrator) — **Highest priority**
- **Priority 5:** UI, Audio recording, Chatbot, Wake word, STT, TTS, Protocol tasks — **High priority**
- **Priority 4:** Audio playback — **Medium-high priority**
- **Priority 3:** Radio HTTP, LED, Power — **Medium priority**
- **Priority 2:** USB HID — **Low priority**
- **Priority 1:** Activation sound — **Very low priority**

**Phân tích:**
- Orchestrator có priority cao nhất (8) — đúng vì cần xử lý events nhanh
- UI task priority 5 — hợp lý, cần responsive
- Audio playback priority 4 — thấp hơn recording (5), có thể gây underrun nếu system busy
- Radio HTTP priority 3 — thấp, có thể gây buffer underrun khi system busy

### 1.3 Core Affinity Analysis

**Core Distribution:**
- **Core 0 (pinned):**
  - `sx_audio_file` (playback)
  - `sx_radio_http` (streaming)
- **Core 1 (pinned):**
  - `sx_audio_rec` (recording)
- **tskNO_AFFINITY (no pinning):**
  - Hầu hết tasks khác (UI, orchestrator, services)

**Phân tích:**
- Audio playback và recording được pin vào core khác nhau — **tốt**, tránh conflict
- Radio HTTP pinned Core 0 — có thể conflict với audio playback
- Hầu hết tasks không pin — **tốt**, cho phép scheduler tự động balance

### 1.4 Stack Size Analysis

**Stack Size Distribution:**
- **8KB:** UI task, Radio HTTP task — **Lớn nhất**
- **4KB:** Audio recording, Wake word, STT, TTS, Protocol tasks — **Trung bình**
- **3KB:** Orchestrator, Chatbot, Navigation, Audio playback — **Nhỏ**
- **2KB:** Recovery, LED, Power — **Rất nhỏ**

**Rủi ro:**
- **⚠️ Audio playback (3KB):** Có thể stack overflow nếu codec phức tạp
- **⚠️ Orchestrator (3KB):** Có thể stack overflow nếu event handlers phức tạp
- **⚠️ Radio HTTP (8KB):** Stack lớn nhưng cần thiết cho HTTP buffer

---

## 2. QUEUES INVENTORY

### 2.1 Event Dispatcher Queues

**Nguồn:** `components/sx_core/sx_dispatcher.c:L14-L18, L56-L66`

| Queue | Size | Element Size | Priority | Mục Đích |
|-------|------|--------------|----------|----------|
| `s_evt_q_low` | 16 | `sizeof(sx_event_t)` | LOW | Low priority events |
| `s_evt_q_normal` | 32 | `sizeof(sx_event_t)` | NORMAL | Normal priority events (default) |
| `s_evt_q_high` | 16 | `sizeof(sx_event_t)` | HIGH | High priority events |
| `s_evt_q_critical` | 8 | `sizeof(sx_event_t)` | CRITICAL | Critical events |

**Phân tích:**
- **Priority-based queues:** 4 queues riêng biệt cho từng priority level
- **Queue sizes:** Normal queue lớn nhất (32), Critical nhỏ nhất (8)
- **Backpressure policy:** DROP (default), COALESCE, BLOCK (CRITICAL only)

### 2.2 Service-Specific Queues

| Queue | Size | Element Size | Service | Nơi Tạo | Mục Đích |
|-------|------|--------------|---------|---------|----------|
| `s_message_queue` | `CHATBOT_QUEUE_SIZE` | `sizeof(chatbot_message_t)` | Chatbot | `sx_chatbot_service.c:L68` | Chatbot messages |
| `s_audio_queue` | 10 | `320 * sizeof(int16_t)` | Wake Word | `sx_wake_word_service.c:L138` | Audio buffers for wake word |
| `s_audio_send_queue` | `AUDIO_SEND_QUEUE_SIZE` | `sizeof(sx_audio_stream_packet_t)` | Audio Protocol | `sx_audio_protocol_bridge.c:L373` | Audio send packets |
| `s_audio_receive_queue` | `AUDIO_RECEIVE_QUEUE_SIZE` | `sizeof(sx_audio_stream_packet_t)` | Audio Protocol | `sx_audio_protocol_bridge.c:L374` | Audio receive packets |
| `s_tts_queue` | 10 | `sizeof(sx_tts_request_t)` | TTS | `sx_tts_service.c:L82` | TTS requests |
| `s_chunk_queue` | `STT_CHUNK_QUEUE_SIZE` | `sizeof(stt_audio_chunk_t)` | STT | `sx_stt_service.c:L148` | STT audio chunks |
| `s_rx_queue` | (unknown) | (unknown) | IR | `sx_ir_service.c` | IR receive data |
| `lvgl_hid_ctx.queue` | 10 | `sizeof(lvgl_port_usb_hid_event_t)` | USB HID | `esp_lvgl_port_usbhid.c` | USB HID events |

**Phân tích:**
- Hầu hết queues có size 10 — **có thể không đủ** nếu producer nhanh hơn consumer
- Audio queues có size lớn hơn — **tốt**, audio cần buffer lớn
- IR queue size không rõ — **cần verify**

### 2.3 Queue Usage Patterns

**Producer-Consumer Patterns:**

1. **Event Dispatcher (Multi-producer, Single-consumer):**
   - **Producers:** Tất cả services, UI
   - **Consumer:** Orchestrator task (`sx_orch`)
   - **Pattern:** `xQueueSend()` → `sx_dispatcher_poll_event()` → `xQueueReceive()`

2. **Audio Protocol Bridge:**
   - **Producer:** Audio service → `s_audio_send_queue`
   - **Consumer:** `audio_send` task
   - **Producer:** Protocol → `s_audio_receive_queue`
   - **Consumer:** `audio_recv` task

3. **Wake Word:**
   - **Producer:** Audio service → `s_audio_queue`
   - **Consumer:** `sx_wake_word` task

4. **Chatbot:**
   - **Producer:** Chatbot service → `s_message_queue`
   - **Consumer:** `sx_chatbot` task

5. **TTS:**
   - **Producer:** Services → `s_tts_queue`
   - **Consumer:** `sx_tts` task

6. **STT:**
   - **Producer:** Audio service → `s_chunk_queue`
   - **Consumer:** `sx_stt` task

---

## 3. MUTEXES & SEMAPHORES INVENTORY

### 3.1 Bảng Tổng Hợp Mutexes

| Mutex | Type | Nơi Tạo | Mục Đích | Bảo Vệ |
|-------|------|---------|----------|--------|
| `s_state_write_mutex` | Static Mutex | `sx_dispatcher.c:L70` | State write protection | State double-buffer write |
| `s_feed_mutex` | Mutex | `sx_audio_service.c:L187` | Audio feed protection | Audio feed operations |
| `s_position_mutex` | Mutex | `sx_audio_service.c:L188` | Position protection | Audio position tracking |
| `s_spectrum_mutex` | Mutex | `sx_audio_service.c:L189` | Spectrum protection | Audio spectrum data |
| `s_pcm_mutex` | Mutex | `sx_audio_protocol_bridge.c:L382` | PCM protection | PCM buffer access |
| `s_playlist_mutex` | Mutex | `sx_playlist_manager.c:L88` | Playlist protection | Playlist operations |
| `s_mutex` (nav_ble) | Mutex | `sx_navigation_ble.c:L421` | Navigation BLE protection | Navigation BLE state |
| `s_state_mutex` | Mutex | `sx_state_manager.c:L36` | State manager protection | State persistence |
| `s_registry_mutex` (service) | Static Mutex | `sx_service_if.c:L25` | Service registry | Service registration |
| `s_registry_mutex` (screen) | Static Mutex | `sx_screen_if.c:L26` | Screen registry | Screen registration |
| `s_theme_mutex` | Mutex | `sx_theme_service.c:L50` | Theme protection | Theme data |
| `s_ws_audio_buffer_mutex` | Mutex | `sx_protocol_ws.c:L401, L441` | WebSocket audio buffer | WS audio buffer |
| `s_reconnect_mutex` | Mutex | `sx_protocol_ws.c:L528` | Reconnect protection | WS reconnection |
| `s_udp_mutex` | Mutex | `sx_protocol_mqtt_udp.c:L195` | UDP protection | MQTT UDP operations |
| `s_mqtt_mutex` | Mutex | `sx_protocol_mqtt.c:L291` | MQTT protection | MQTT operations |
| `s_mutex` (lazy_loader) | Mutex | `sx_lazy_loader.c:L43` | Lazy loader protection | Lazy service loading |
| `s_mutex` (nav_icon) | Mutex | `sx_navigation_icon_cache.c:L63` | Icon cache protection | Navigation icon cache |
| `s_mutex` (nav) | Mutex | `sx_navigation_service.c:L61` | Navigation protection | Navigation state |
| `s_recovery_mutex` | Mutex | `sx_audio_recovery.c:L78` | Recovery protection | Audio recovery state |
| `s_spi_bus_mutex` | Mutex | `sx_spi_bus_manager.c:L17` | SPI bus protection | SPI bus sharing |
| `s_crossfade_mutex` | Mutex | `sx_audio_crossfade.c:L36` | Crossfade protection | Crossfade state |
| `s_mutex` (router) | Mutex | `sx_audio_router.c:L39` | Router protection | Audio router state |
| `s_mutex` (profiler) | Mutex | `sx_audio_profiler.c:L33` | Profiler protection | Profiler data |
| `s_tts_mutex` | Mutex | `sx_tts_service.c:L76` | TTS protection | TTS state |
| `s_mutex` (bluetooth) | Mutex | `sx_bluetooth_service.c:L31` | Bluetooth protection | Bluetooth state |
| `s_stt_mutex` | Mutex | `sx_stt_service.c:L143` | STT protection | STT state |
| `s_duck_mutex` | Mutex | `sx_audio_ducking.c:L41` | Ducking protection | Audio ducking state |
| `lvgl_port_ctx.lvgl_mux` | Recursive Mutex | `esp_lvgl_port.c` | LVGL protection | LVGL API calls |
| `lvgl_port_ctx.timer_mux` | Mutex | `esp_lvgl_port.c` | LVGL timer protection | LVGL timer operations |

**Tổng số mutexes:** ~30+ mutexes

### 3.2 Mutex Usage Patterns

**Critical Sections:**

1. **State Write (Double-Buffer Pattern):**
   ```c
   xSemaphoreTake(s_state_write_mutex, portMAX_DELAY);
   // Write to back buffer
   // Atomic swap pointers
   xSemaphoreGive(s_state_write_mutex);
   ```
   - **Nguồn:** `components/sx_core/sx_dispatcher.c:L321-L352`
   - **Reader:** Lock-free (atomic pointer read)
   - **Writer:** Protected by mutex

2. **LVGL API Calls:**
   ```c
   if (lvgl_port_lock(0)) {
       // LVGL API calls
       lvgl_port_unlock();
   }
   ```
   - **Nguồn:** `components/sx_ui/sx_ui_task.c:L54-L175`
   - **Pattern:** Tất cả LVGL calls phải trong lock

3. **Audio Operations:**
   - Multiple mutexes cho feed, position, spectrum
   - **Rủi ro:** Có thể deadlock nếu lock order không đúng

### 3.3 Event Groups

| Event Group | Nơi Tạo | Mục Đích |
|-------------|---------|----------|
| `s_wifi_event_group` | `sx_wifi_service.c:L113` | WiFi connection events |
| `lvgl_port_ctx.lvgl_events` | `esp_lvgl_port.c:L78` | LVGL internal events |

---

## 4. DATAFLOW ANALYSIS

### 4.1 UI Input → Event → Action → UI Update

**Flow Diagram:**
```
Touch Input (ISR/Driver)
    ↓
LVGL Touch Handler (esp_lvgl_port)
    ↓
UI Event (LVGL event callback)
    ↓
sx_dispatcher_post_event(SX_EVT_UI_INPUT)
    ↓
Event Queue (s_evt_q_normal)
    ↓
Orchestrator Task (sx_orch, priority 8)
    ↓
sx_event_handler_process()
    ↓
sx_event_handler_ui_input()
    ↓
State Update (sx_dispatcher_set_state())
    ↓
UI Task (sx_ui, priority 5)
    ↓
sx_dispatcher_get_state()
    ↓
Screen on_state_change()
    ↓
LVGL UI Update
```

**Nguồn:**
- Touch input: `esp_lvgl_port` → LVGL event
- UI event post: `components/sx_ui/sx_ui_task.c:L166-L172`
- Event processing: `components/sx_core/sx_orchestrator.c:L104-L166`
- State update: `components/sx_ui/sx_ui_task.c:L201-L264`

### 4.2 Audio Pipeline Flow

**Playback Flow:**
```
File/Stream Source
    ↓
Audio Service (sx_audio_service)
    ↓
Codec Decoder (MP3/AAC/FLAC/Opus)
    ↓
Audio Buffer Pool
    ↓
I2S Driver (DMA)
    ↓
Audio Output (PCM5102A)
```

**Recording Flow:**
```
Microphone Input
    ↓
I2S Driver (DMA, Core 1)
    ↓
Audio Recording Task (sx_audio_rec, priority 5)
    ↓
Audio Front-End (AFE: AEC, VAD, NS, AGC)
    ↓
Wake Word / STT Queue
    ↓
Wake Word Task / STT Task
```

**Nguồn:**
- Playback: `components/sx_services/sx_audio_service.c:L536`
- Recording: `components/sx_services/sx_audio_service.c:L618-L626`

### 4.3 Network → Event → State → UI

**WiFi Connection Flow:**
```
WiFi Service
    ↓
ESP-IDF WiFi Event
    ↓
sx_dispatcher_post_event(SX_EVT_WIFI_CONNECTED)
    ↓
Event Queue (s_evt_q_high)
    ↓
Orchestrator Task
    ↓
sx_event_handler_wifi_connected()
    ↓
State Update (SX_STATE_DIRTY_WIFI)
    ↓
UI Task
    ↓
Screen on_state_change() with dirty_mask
    ↓
UI Update (WiFi status)
```

**Nguồn:**
- WiFi event: `components/sx_services/sx_wifi_service.c`
- Event handler: `components/sx_core/sx_event_handlers/` (cần verify)

### 4.4 Chatbot Flow

**Chatbot Message Flow:**
```
User Input (UI/STT)
    ↓
sx_dispatcher_post_event(SX_EVT_UI_INPUT)
    ↓
Orchestrator
    ↓
Chatbot Service
    ↓
xQueueSend(s_message_queue)
    ↓
Chatbot Task (sx_chatbot, priority 5)
    ↓
Protocol (WebSocket/MQTT)
    ↓
Server Response
    ↓
sx_dispatcher_post_event(SX_EVT_CHATBOT_STT)
    ↓
Orchestrator
    ↓
State Update
    ↓
UI Update
```

**Nguồn:**
- Chatbot queue: `components/sx_services/sx_chatbot_service.c:L68, L209, L283`

---

## 5. CONCURRENCY RISKS

### 5.1 Deadlock Risks

#### **Risk 1: Audio Mutex Deadlock**
**Vị trí:** `components/sx_services/sx_audio_service.c:L187-L189`

**Mutexes:**
- `s_feed_mutex`
- `s_position_mutex`
- `s_spectrum_mutex`

**Rủi ro:** Nếu code lock nhiều mutexes cùng lúc, có thể deadlock nếu lock order không đúng

**Điều kiện kích hoạt:**
- Task A lock `s_feed_mutex` → wait `s_position_mutex`
- Task B lock `s_position_mutex` → wait `s_feed_mutex`

**Cách tái hiện:**
- Tạo 2 tasks cùng gọi audio functions lock nhiều mutexes
- Không có lock order convention

**Cách sửa:**
- Định nghĩa lock order: `s_feed_mutex` → `s_position_mutex` → `s_spectrum_mutex`
- Luôn lock theo thứ tự này

#### **Risk 2: LVGL + State Mutex Deadlock**
**Vị trí:** `components/sx_ui/sx_ui_task.c`, `components/sx_core/sx_dispatcher.c`

**Mutexes:**
- `lvgl_port_ctx.lvgl_mux` (LVGL lock)
- `s_state_write_mutex` (State write)

**Rủi ro:** Nếu UI task lock LVGL → wait state write, và orchestrator lock state write → post event → wait LVGL

**Điều kiện kích hoạt:**
- UI task: `lvgl_port_lock()` → `sx_dispatcher_get_state()` (không lock, nhưng có thể có race)
- Orchestrator: `xSemaphoreTake(s_state_write_mutex)` → post event → UI task cần LVGL lock

**Cách tái hiện:**
- Khó xảy ra vì state read là lock-free
- Nhưng nếu có nested locks, có thể deadlock

**Cách sửa:**
- Đảm bảo không có nested locks giữa LVGL và state mutex
- State read là lock-free (double-buffer pattern)

### 5.2 Race Condition Risks

#### **Risk 1: State Read-Write Race**
**Vị trí:** `components/sx_core/sx_dispatcher.c:L321-L365`

**Pattern:** Double-buffer với atomic pointer swap

**Phân tích:**
- **Writer:** Protected by `s_state_write_mutex`
- **Reader:** Lock-free (atomic pointer read)
- **Rủi ro:** Nếu pointer swap không atomic trên architecture này

**Điều kiện kích hoạt:**
- Writer đang swap pointer
- Reader đọc pointer giữa chừng (nếu swap không atomic)

**Cách tái hiện:**
- Concurrent read/write với high frequency
- Verify pointer swap atomicity

**Cách sửa:**
- Verify pointer assignment là atomic trên ESP32-S3
- Có thể cần memory barrier

#### **Risk 2: Queue Overflow**
**Vị trí:** Tất cả queues

**Rủi ro:** Producer nhanh hơn consumer → queue full → events dropped

**Điều kiện kích hoạt:**
- High event rate (ví dụ: audio chunks)
- Consumer chậm (ví dụ: orchestrator busy)

**Cách tái hiện:**
- Flood system với events
- Monitor queue depth

**Cách sửa:**
- Tăng queue size nếu cần
- Implement backpressure (đã có: DROP, COALESCE, BLOCK)
- Monitor drop count

#### **Risk 3: Audio Buffer Underrun**
**Vị trí:** `components/sx_services/sx_audio_service.c`

**Rủi ro:** Audio playback task chậm → I2S DMA underrun → audio glitch

**Điều kiện kích hoạt:**
- System busy (high CPU load)
- Audio task priority thấp (4) → có thể bị preempt
- Buffer pool empty

**Cách tái hiện:**
- Load system với nhiều tasks
- Play audio và monitor I2S underrun

**Cách sửa:**
- Tăng audio playback priority (4 → 5)
- Tăng buffer pool size
- Monitor I2S underrun events

### 5.3 Priority Inversion Risks

#### **Risk 1: Audio Playback Priority Inversion**
**Vị trí:** `components/sx_services/sx_audio_service.c:L536`

**Vấn đề:**
- Audio playback task priority 4
- Audio recording task priority 5
- Nếu recording task hold mutex mà playback cần → priority inversion

**Điều kiện kích hoạt:**
- Recording task (priority 5) lock mutex
- Playback task (priority 4) wait mutex
- Medium priority task (priority 3-4) preempt playback → inversion

**Cách sửa:**
- Sử dụng priority inheritance mutex (nếu FreeRTOS support)
- Hoặc tăng playback priority lên 5

#### **Risk 2: Radio HTTP Priority Inversion**
**Vị trí:** `components/sx_services/sx_radio_service.c:L278, L329`

**Vấn đề:**
- Radio HTTP task priority 3 (thấp)
- Nếu hold mutex mà high priority task cần → inversion

**Cách sửa:**
- Tăng radio priority lên 4 hoặc 5
- Hoặc sử dụng priority inheritance

### 5.4 Stack Overflow Risks

#### **Risk 1: Audio Playback Stack**
**Vị trí:** `components/sx_services/sx_audio_service.c:L536`

**Stack size:** 3072 bytes

**Rủi ro:** Codec decoding có thể cần nhiều stack (đặc biệt MP3/AAC)

**Cách tái hiện:**
- Play complex audio file
- Monitor stack high water mark

**Cách sửa:**
- Tăng stack size lên 4096 hoặc 6144
- Hoặc move codec decoding to separate task với stack lớn hơn

#### **Risk 2: Orchestrator Stack**
**Vị trí:** `components/sx_core/sx_orchestrator.c:L180`

**Stack size:** 3072 bytes

**Rủi ro:** Event handlers có thể phức tạp, cần nhiều stack

**Cách sửa:**
- Monitor stack high water mark
- Tăng stack size nếu cần

### 5.5 Queue Overflow Risks

#### **Risk 1: Event Queue Overflow**
**Vị trí:** `components/sx_core/sx_dispatcher.c:L56-L66`

**Queue sizes:**
- Normal: 32 events
- High: 16 events
- Critical: 8 events

**Rủi ro:** High event rate → queue full → events dropped

**Cách tái hiện:**
- Flood system với events
- Monitor drop count

**Cách sửa:**
- Tăng queue sizes nếu cần
- Implement backpressure (đã có)
- Monitor metrics

#### **Risk 2: Audio Queue Overflow**
**Vị trí:** `components/sx_services/sx_wake_word_service.c:L138`

**Queue size:** 10 buffers (320 samples each)

**Rủi ro:** Audio producer nhanh hơn consumer → queue full → audio dropped

**Cách sửa:**
- Tăng queue size
- Hoặc implement backpressure (drop oldest)

---

## 6. TOP CONCURRENCY RISKS SUMMARY

### 6.1 P0 (Critical) Risks

1. **Audio Buffer Underrun**
   - **Mức độ:** Critical
   - **Hậu quả:** Audio glitch, poor user experience
   - **Cách sửa:** Tăng audio playback priority, tăng buffer size

2. **Event Queue Overflow**
   - **Mức độ:** Critical
   - **Hậu quả:** Events dropped, system state inconsistent
   - **Cách sửa:** Tăng queue sizes, monitor drop count

### 6.2 P1 (High) Risks

1. **Audio Mutex Deadlock**
   - **Mức độ:** High
   - **Hậu quả:** System hang
   - **Cách sửa:** Định nghĩa lock order

2. **Stack Overflow (Audio Playback)**
   - **Mức độ:** High
   - **Hậu quả:** Task crash, system unstable
   - **Cách sửa:** Tăng stack size, monitor high water mark

3. **Priority Inversion (Audio)**
   - **Mức độ:** High
   - **Hậu quả:** Audio glitch
   - **Cách sửa:** Tăng playback priority, priority inheritance

### 6.3 P2 (Medium) Risks

1. **State Read-Write Race**
   - **Mức độ:** Medium (mitigated by double-buffer)
   - **Hậu quả:** Inconsistent state read
   - **Cách sửa:** Verify atomicity, memory barriers

2. **Queue Overflow (Service Queues)**
   - **Mức độ:** Medium
   - **Hậu quả:** Service-specific data loss
   - **Cách sửa:** Tăng queue sizes, backpressure

---

## 7. KẾT LUẬN PHASE 2

### 7.1 Điểm Mạnh

1. ✅ **Priority-based event queues:** 4 queues riêng biệt, backpressure policy
2. ✅ **Double-buffer state pattern:** Lock-free read, atomic pointer swap
3. ✅ **Task separation:** Audio playback/recording trên core khác nhau
4. ✅ **Orchestrator high priority:** Priority 8, đảm bảo event processing nhanh
5. ✅ **LVGL thread safety:** Tất cả LVGL calls protected by mutex

### 7.2 Điểm Yếu

1. ⚠️ **Audio playback priority thấp:** Priority 4, có thể bị preempt
2. ⚠️ **Stack sizes nhỏ:** Một số tasks có thể stack overflow
3. ⚠️ **Queue sizes nhỏ:** Một số queues có thể overflow
4. ⚠️ **Mutex lock order:** Không có convention rõ ràng
5. ⚠️ **Priority inversion:** Có thể xảy ra với audio tasks

### 7.3 Hành Động Tiếp Theo

**PHASE 3-7:** Phân tích từng module chi tiết theo line-range

---

## 8. CHECKLIST HOÀN THÀNH PHASE 2

- [x] Liệt kê toàn bộ FreeRTOS tasks (tên, stack, priority, core)
- [x] Liệt kê tất cả queues (size, element size, mục đích)
- [x] Liệt kê tất cả mutexes/semaphores
- [x] Phân tích dataflow (UI input → Event → Action → UI update)
- [x] Phân tích audio pipeline flow
- [x] Phân tích network → event → state → UI flow
- [x] Xác định concurrency risks (deadlock, race condition, queue overflow)
- [x] Tạo REPORT_PHASE_2_RUNTIME.md

---

## 9. THỐNG KÊ FILE ĐÃ ĐỌC

**Tổng số file đã đọc trong Phase 2:** ~20 files

**Danh sách:**
1. `components/sx_ui/sx_ui_task.c`
2. `components/sx_core/sx_dispatcher.c`
3. `components/sx_core/sx_orchestrator.c`
4. `components/sx_services/sx_audio_service.c` (partial)
5. `components/sx_services/sx_chatbot_service.c` (partial)
6. `components/sx_services/sx_radio_service.c` (partial)
7. `components/sx_services/sx_wake_word_service.c` (partial)
8. Các file khác qua grep/search

**Ước lượng % file đã đọc:** ~8-10% (đọc runtime-critical files)

---

**Kết thúc PHASE 2**

