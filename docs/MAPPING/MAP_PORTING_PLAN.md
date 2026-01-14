# Porting Plan - XIAOZHI → SIMPLEXL

**Ngày tạo:** 2025-01-02  
**Mục đích:** Kế hoạch porting chi tiết theo batch, với patch adapter và test protocol

## Tổng Quan

**Mục tiêu:** Tích hợp tối thiểu "voice loop end-to-end" từ XIAOZHI vào SIMPLEXL

**Ràng buộc:**
- Giữ kiến trúc SIMPLEXL (event-driven dispatcher/orchestrator + service/screen lifecycle)
- Tuyệt đối không để LVGL leak ra ngoài sx_ui
- Mọi giao tiếp với SIMPLEXL phải qua dispatcher events hoặc service interface

## Batch 0: Repo Discovery & Build Baseline ✅

**Status:** In Progress

**Tasks:**
- [x] Đọc entrypoint, CMakeLists, sdkconfig, partitions của cả 2 repo
- [x] Tạo API_CATALOG_SIMPLEXL.md
- [x] Tạo API_CATALOG_XIAOZHI.md
- [x] Tạo MAP_INDEX.md
- [x] Tạo MAP_MODULE_MATRIX.md
- [x] Tạo MAP_EVENTS_STATE.md
- [ ] Tạo CALL_GRAPH_CROSS_REPO.md
- [ ] Tạo OWNERSHIP_THREADING_RULES_CROSS_REPO.md

**Output:**
- ✅ `docs/API_CATALOG_SIMPLEXL.md`
- ✅ `docs/API_CATALOG_XIAOZHI.md`
- ✅ `docs/MAPPING/MAP_INDEX.md`
- ✅ `docs/MAPPING/MAP_MODULE_MATRIX.md`
- ✅ `docs/MAPPING/MAP_EVENTS_STATE.md`

## Batch 1: SIMPLEXL Core Contracts

**Status:** Pending

**Mục tiêu:** Phân tích sâu sx_core để hiểu contracts mà XIAOZHI phải tuân theo

**Tasks:**
1. Phân tích `sx_dispatcher`:
   - Event posting/polling mechanism
   - Priority queues, backpressure policy
   - Event schema, taxonomy
   - Line-range evidence: `components/sx_core/sx_dispatcher.c:L14-L18, L56-L66`

2. Phân tích `sx_orchestrator`:
   - State transitions, single-writer rule
   - Event handler registry
   - Line-range evidence: `components/sx_core/sx_orchestrator.c:L104-L166`

3. Phân tích service lifecycle:
   - Service interface (vtable)
   - Lazy loading mechanism
   - Line-range evidence: `components/sx_core/sx_service_if.c:L29-L66`

4. Phân tích state management:
   - Double-buffer pattern
   - Version + dirty mask
   - Line-range evidence: `components/sx_core/sx_dispatcher.c:L321-L365`

**Output:**
- `docs/MAPPING/BATCH1_SIMPLEXL_CORE_CONTRACTS.md`
- Cập nhật `MAP_MODULE_MATRIX.md` với SIMPLEXL contracts

## Batch 2: XIAOZHI Core Runtime

**Status:** Pending

**Mục tiêu:** Xác định tasks/queues/events của XIAOZHI và ownership

**Tasks:**
1. Phân tích `Application::Start()`:
   - Initialization order
   - Task creation
   - Line-range evidence: `main/application.cc:357-615`

2. Phân tích `Application::MainEventLoop()`:
   - Event processing
   - Schedule pattern
   - Line-range evidence: `main/application.cc:629-711`

3. Phân tích audio pipeline:
   - Audio input/output tasks
   - Opus codec task
   - Queue ownership
   - Line-range evidence: `main/audio/audio_service.cc`

4. Phân tích protocol:
   - MQTT/WebSocket callbacks
   - Audio send/receive
   - Line-range evidence: `main/protocols/mqtt_protocol.cc`, `websocket_protocol.cc`

**Output:**
- `docs/MAPPING/BATCH2_XIAOZHI_CORE_RUNTIME.md`
- Cập nhật `MAP_MODULE_MATRIX.md` với XIAOZHI runtime details

## Batch 3: Mapping Voice Loop End-to-End (Minimum Viable)

**Status:** Pending

**Mục tiêu:** Từ SIMPLEXL phát event "start conversation" → XIAOZHI xử lý → trả PCM/TTS → SIMPLEXL playback

**Tasks:**
1. Quyết định "ai làm audio owner":
   - Option A: SIMPLEXL audio_service làm owner, XIAOZHI chỉ cung cấp voice processing
   - Option B: XIAOZHI audio pipeline làm owner, SIMPLEXL chỉ playback
   - **Khuyến nghị:** Option A (giữ SIMPLEXL architecture)

2. Tạo adapter facade:
   - `components/sx_xiaozhi_adapter/sx_xiaozhi_adapter.h`
   - `components/sx_xiaozhi_adapter/sx_xiaozhi_adapter.c`
   - API tối thiểu (3-5 functions):
     - `sx_xiaozhi_adapter_init()`
     - `sx_xiaozhi_adapter_start_conversation()`
     - `sx_xiaozhi_adapter_stop_conversation()`
     - `sx_xiaozhi_adapter_feed_audio(pcm_data, samples)`
     - `sx_xiaozhi_adapter_get_tts_audio(pcm_data, max_samples)`

3. Wiring qua dispatcher:
   - SIMPLEXL event: `SX_EVT_UI_INPUT_START_CONVERSATION`
   - → Adapter: `sx_xiaozhi_adapter_start_conversation()`
   - → XIAOZHI: `Application::StartListening()`
   - → XIAOZHI: Audio processing → TTS
   - → Adapter: `sx_xiaozhi_adapter_get_tts_audio()`
   - → SIMPLEXL: `sx_audio_service_feed_pcm()`

4. Thread safety:
   - Ownership của audio buffers
   - Lock order (nếu có nested locks)
   - Queue ownership

**Output:**
- `components/sx_xiaozhi_adapter/sx_xiaozhi_adapter.h`
- `components/sx_xiaozhi_adapter/sx_xiaozhi_adapter.c`
- `docs/MAPPING/BATCH3_VOICE_LOOP_MAPPING.md`
- Patch diff (unified diff format)

**Test Protocol:**
1. Build SIMPLEXL với adapter
2. Flash firmware
3. Trigger conversation (UI button hoặc wake word)
4. Verify logs:
   - Event posted: `SX_EVT_UI_INPUT_START_CONVERSATION`
   - Adapter called: `sx_xiaozhi_adapter_start_conversation()`
   - XIAOZHI started: `Application::StartListening()`
   - TTS audio received: `sx_xiaozhi_adapter_get_tts_audio()`
   - SIMPLEXL playback: `sx_audio_service_feed_pcm()`
5. Check metrics:
   - Heap free: >= X KB
   - Stack watermark: >= Y bytes
   - Audio underrun: 0
   - Event drop count: 0 (hoặc <= N)

## Batch 4: Mapping Protocol & Device Control

**Status:** Pending

**Mục tiêu:** Map command/control từ XIAOZHI vào event taxonomy của SIMPLEXL

**Tasks:**
1. Map MCP commands:
   - XIAOZHI: `MCP::HandleCommand()`
   - SIMPLEXL: `SX_EVT_MCP_*` events
   - Line-range evidence: `main/mcp_server.cc`

2. Map device control:
   - Volume control
   - LED control
   - GPIO control
   - Line-range evidence: `main/mcp_server.cc`

3. Đảm bảo không phá threading/ownership:
   - Protocol callbacks schedule vào orchestrator
   - State updates qua dispatcher
   - No direct UI calls

**Output:**
- `docs/MAPPING/BATCH4_PROTOCOL_MAPPING.md`
- Cập nhật adapter với protocol integration

## Batch 5: UI Binding

**Status:** Pending

**Mục tiêu:** Map status/emoji/text từ XIAOZHI → SIMPLEXL state → sx_ui update

**Tasks:**
1. Map display status:
   - XIAOZHI: `Display::SetStatus()`
   - SIMPLEXL: `state.ui.status_text` → UI screen update
   - Line-range evidence: `main/display/display.cc`

2. Map emoji/emotion:
   - XIAOZHI: `Display::SetEmotion()`
   - SIMPLEXL: `state.ui.emotion` → UI screen update
   - Line-range evidence: `main/display/emote_display.cc`

3. Map chat message:
   - XIAOZHI: `Display::SetChatMessage()`
   - SIMPLEXL: `state.ui.chat_message` → UI screen update
   - Line-range evidence: `main/display/lcd_display.cc`

4. Đảm bảo LVGL chỉ trong sx_ui:
   - Adapter không được include LVGL
   - State updates qua dispatcher
   - UI screen callbacks render LVGL

**Output:**
- `docs/MAPPING/BATCH5_UI_BINDING.md`
- Cập nhật adapter với UI state integration

## Adapter Architecture

### Directory Structure

```
components/sx_xiaozhi_adapter/
├── include/
│   ├── sx_xiaozhi_adapter.h          # Public facade API
│   └── sx_xiaozhi_port_config.h      # Compile-time switches
├── src/
│   ├── sx_xiaozhi_adapter.c          # Glue + ownership + thread-safety
│   ├── sx_xiaozhi_event_adapter.c    # Event conversion
│   ├── sx_xiaozhi_state_adapter.c    # State conversion
│   └── sx_xiaozhi_audio_adapter.c    # Audio pipeline integration
└── vendor/
    └── (XIAOZHI code - symlink hoặc copy)
```

### Public API (Facade)

**File:** `components/sx_xiaozhi_adapter/include/sx_xiaozhi_adapter.h`

```c
// Initialization
esp_err_t sx_xiaozhi_adapter_init(void);
esp_err_t sx_xiaozhi_adapter_deinit(void);

// Voice loop
esp_err_t sx_xiaozhi_adapter_start_conversation(void);
esp_err_t sx_xiaozhi_adapter_stop_conversation(void);
esp_err_t sx_xiaozhi_adapter_feed_audio(const int16_t *pcm, size_t samples);

// TTS
esp_err_t sx_xiaozhi_adapter_get_tts_audio(int16_t *pcm, size_t max_samples, size_t *actual_samples);

// Protocol
esp_err_t sx_xiaozhi_adapter_send_message(const char *message);
esp_err_t sx_xiaozhi_adapter_set_protocol_callback(sx_xiaozhi_protocol_callback_t callback);
```

## Test Protocol (Minimum Viable)

### Test 1: Voice Loop End-to-End

**Setup:**
1. Build SIMPLEXL với adapter
2. Flash firmware
3. Connect WiFi
4. Configure protocol (WebSocket/MQTT)

**Steps:**
1. Trigger conversation (UI button hoặc wake word)
2. Speak: "Xin chào"
3. Wait for TTS response
4. Verify audio playback

**Expected Logs:**
```
I (1234) sx_xiaozhi_adapter: Starting conversation
I (1235) Application: StartListening()
I (2345) sx_xiaozhi_adapter: TTS audio received, 16000 samples
I (2346) sx_audio_service: Feeding PCM, 16000 samples
I (3456) sx_audio_service: Playback started
```

**Metrics:**
- Heap free: >= 50 KB
- Stack watermark (sx_orch): >= 500 bytes
- Stack watermark (sx_ui): >= 1000 bytes
- Audio underrun: 0
- Event drop count: 0

### Test 2: Protocol Integration

**Steps:**
1. Send MCP command via protocol
2. Verify command processed
3. Verify state update
4. Verify UI update

**Expected Logs:**
```
I (1234) sx_protocol_ws: Received message
I (1235) sx_xiaozhi_adapter: MCP command received
I (1236) sx_orch: Processing SX_EVT_MCP_VOLUME_SET
I (1237) sx_ui: State update (dirty_mask=0x02)
```

## Issues & Risks

### P0 Issues

1. **Audio Ownership Conflict**
   - Risk: XIAOZHI và SIMPLEXL đều muốn control audio pipeline
   - Mitigation: SIMPLEXL audio_service làm owner, XIAOZHI chỉ cung cấp voice processing

2. **Event System Mismatch**
   - Risk: XIAOZHI dùng Event Group, SIMPLEXL dùng Priority Queues
   - Mitigation: Adapter convert Event Group bits → SIMPLEXL events

3. **State Management Mismatch**
   - Risk: XIAOZHI dùng DeviceState enum, SIMPLEXL dùng structured state
   - Mitigation: Adapter convert DeviceState → SIMPLEXL state fields

### P1 Issues

1. **Thread Safety**
   - Risk: XIAOZHI callbacks có thể chạy từ network task
   - Mitigation: Schedule callbacks vào orchestrator

2. **Memory Ownership**
   - Risk: Audio buffers ownership không rõ
   - Mitigation: Document ownership, dùng RAII pattern

3. **LVGL Leak**
   - Risk: XIAOZHI code có thể gọi LVGL trực tiếp
   - Mitigation: Compile-time guard, runtime check

## Next Steps

1. **Hoàn thành BATCH 0:** Tạo CALL_GRAPH_CROSS_REPO.md và OWNERSHIP_THREADING_RULES_CROSS_REPO.md
2. **Bắt đầu BATCH 1:** Phân tích SIMPLEXL core contracts
3. **Bắt đầu BATCH 2:** Phân tích XIAOZHI core runtime
4. **Bắt đầu BATCH 3:** Implement voice loop adapter (minimum viable)

