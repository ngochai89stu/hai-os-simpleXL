# Module Mapping Matrix - XIAOZHI → SIMPLEXL

**Ngày tạo:** 2025-01-02  
**Mục đích:** Bảng ánh xạ module/component giữa XIAOZHI (source) và SIMPLEXL (target)

## Mapping Rules

- **Symbol Equivalence:** Function/Struct/Enum mapping với line-range evidence
- **Call Sites:** Mỗi mapping phải có >=2 call-sites
- **Status:** ✅ Mapped, ⚠️ Partial, ❌ Not Mapped, 🔄 In Progress

## Core System

| XIAOZHI Module | SIMPLEXL Module | Mapping Type | Status | Evidence |
|----------------|-----------------|--------------|--------|----------|
| `main/application.cc` | `components/sx_core/sx_bootstrap.c` | Boot sequence | ⚠️ Partial | `main.cc:55` → `app_main.c:8` |
| `Application::Start()` | `sx_bootstrap_start()` | Initialization | ⚠️ Partial | `application.cc:357` vs `sx_bootstrap.c:55` |
| `Application::MainEventLoop()` | `sx_orchestrator_task()` | Event processing | ⚠️ Partial | `application.cc:629` vs `sx_orchestrator.c:104` |
| `Application::Schedule()` | `sx_dispatcher_post_event()` | Async task scheduling | ⚠️ Partial | Pattern khác: Schedule queue vs Priority queues |
| `DeviceStateEventManager` | `sx_dispatcher` + `sx_orchestrator` | State events | ⚠️ Partial | ESP event vs SIMPLEXL event, cần adapter convert |

## Audio System

| XIAOZHI Module | SIMPLEXL Module | Mapping Type | Status | Evidence |
|----------------|-----------------|--------------|--------|----------|
| `main/audio/audio_service.cc` | `components/sx_services/sx_audio_service.c` | Audio pipeline | ⚠️ Partial | Cần phân tích sâu |
| `AudioService::ReadAudioData()` | `sx_audio_start_recording()` | Audio input | ❌ Not Mapped | |
| `AudioService::PlayAudioData()` | `sx_audio_service_play()` | Audio playback | ❌ Not Mapped | |
| `AudioCodec::InputData()` | `sx_audio_codec_read()` | Codec input | ❌ Not Mapped | |
| `AudioCodec::OutputData()` | `sx_audio_codec_write()` | Codec output | ❌ Not Mapped | |
| `AudioProcessor` (AFE) | `sx_audio_afe` | Audio front-end | ⚠️ Partial | ESP-SR integration |
| `opus_codec` task | `sx_audio_protocol_bridge` | Opus encode/decode | ❌ Not Mapped | |

## Display/UI System

| XIAOZHI Module | SIMPLEXL Module | Mapping Type | Status | Evidence |
|----------------|-----------------|--------------|--------|----------|
| `main/display/display.h` | `components/sx_ui/include/sx_ui.h` | Display abstraction | ⚠️ Partial | |
| `LCDDisplay` | `sx_platform_display_init()` | LCD init | ⚠️ Partial | `lcd_display.cc` vs `sx_platform.c:133` |
| `OLEDDisplay` | `sx_ui` (nếu có OLED screen) | OLED init | ❌ Not Mapped | |
| `EmoteDisplay` | `sx_ui` screens | Emoji display | ❌ Not Mapped | |
| `touch_task` | `sx_ui_task` (touch handling) | Touch input | ⚠️ Partial | `lcd_touch.cc:46` vs `sx_ui_task.c:308` |
| `lvgl_task` | `sx_ui_task` | LVGL rendering | ⚠️ Partial | Core 1, priority 5 |

## Protocol System

| XIAOZHI Module | SIMPLEXL Module | Mapping Type | Status | Evidence |
|----------------|-----------------|--------------|--------|----------|
| `main/protocols/mqtt_protocol.cc` | `components/sx_services/sx_protocol_mqtt.c` | MQTT protocol | ⚠️ Partial | |
| `main/protocols/websocket_protocol.cc` | `components/sx_services/sx_protocol_ws.c` | WebSocket protocol | ⚠️ Partial | |
| `Protocol::SendAudio()` | `sx_protocol_ws_send_audio()` / `sx_protocol_mqtt_send_audio()` | Audio send | ❌ Not Mapped | |
| `Protocol::OnIncomingAudio()` | `sx_audio_protocol_bridge` | Audio receive | ❌ Not Mapped | |
| `MqttProtocol::OnMessage()` | `sx_protocol_mqtt_on_message()` | MQTT message | ❌ Not Mapped | |
| `WebsocketProtocol::OnData()` | `sx_protocol_ws_on_data()` | WS message | ❌ Not Mapped | |

## Storage System

| XIAOZHI Module | SIMPLEXL Module | Mapping Type | Status | Evidence |
|----------------|-----------------|--------------|--------|----------|
| `main/settings.cc` | `components/sx_services/sx_settings_service.c` | NVS settings | ⚠️ Partial | Wrapper pattern |
| `main/assets.cc` | `components/sx_assets/sx_assets.c` | Assets partition | ⚠️ Partial | SPIFFS partition |
| `Settings::GetString()` | `sx_settings_get_string()` | Settings read | ⚠️ Partial | |
| `Settings::SetString()` | `sx_settings_set_string()` | Settings write | ⚠️ Partial | |
| `Assets::Apply()` | `sx_assets_apply()` | Assets apply | ❌ Not Mapped | |
| `Assets::Download()` | `sx_assets_download()` | Assets download | ❌ Not Mapped | |

## System Services

| XIAOZHI Module | SIMPLEXL Module | Mapping Type | Status | Evidence |
|----------------|-----------------|--------------|--------|----------|
| `main/ota.cc` | `components/sx_services/sx_ota_service.c` | OTA update | ⚠️ Partial | |
| `main/ota_server.cc` | `components/sx_services/sx_ota_full_service.c` | OTA web server | ⚠️ Partial | |
| `main/system_info.cc` | `components/sx_services/sx_diagnostics_service.c` | System info | ⚠️ Partial | |
| `main/mcp_server.cc` | `components/sx_services/sx_mcp_server.c` | MCP server | ⚠️ Partial | |

## Task Mapping

| XIAOZHI Task | SIMPLEXL Task | Priority | Core | Status |
|--------------|---------------|----------|------|--------|
| `main_event_loop` | `sx_orch` | 3 → 8 | Any | ⚠️ Priority khác |
| `audio_input` | `sx_audio_rec` | 8 → 5 | 0 → Any | ⚠️ Priority/Core khác |
| `audio_output` | `sx_audio_file` | 4 → 4 | Any | ✅ Match |
| `opus_codec` | `audio_send` / `audio_recv` | 2 → 5 | Any | ⚠️ Priority khác |
| `touch_task` | `sx_ui` (touch) | 5 → 7 | 1 → Any | ⚠️ Priority/Core khác |
| `lvgl_task` | `sx_ui` | Variable → 7 | 1 → Any | ⚠️ Priority/Core khác |

## Queue Mapping

| XIAOZHI Queue | SIMPLEXL Queue | Type | Size | Status |
|---------------|----------------|------|------|--------|
| `main_tasks_` (std::deque) | Event queues (FreeRTOS) | Deque → Queue | Variable → 32 | ⚠️ Type khác |
| `audio_encode_queue` | `s_audio_send_queue` | Deque → Queue | 2 → Variable | ⚠️ Type/Size khác |
| `audio_playback_queue` | Audio buffer pool | Deque → Pool | 2 → Variable | ⚠️ Type khác |
| `audio_decode_queue` | `s_audio_receive_queue` | Deque → Queue | 40 → Variable | ⚠️ Type/Size khác |

## Event Mapping

| XIAOZHI Event | SIMPLEXL Event | Priority | Status |
|---------------|----------------|----------|--------|
| `MAIN_EVENT_SCHEDULE` | `SX_EVT_*` (various) | NORMAL | ❌ Not Mapped |
| `MAIN_EVENT_SEND_AUDIO` | `SX_EVT_AUDIO_SEND_READY` | HIGH | ❌ Not Mapped |
| `MAIN_EVENT_WAKE_WORD_DETECTED` | `SX_EVT_WAKE_WORD_DETECTED` | HIGH | ⚠️ Partial |
| `MAIN_EVENT_VAD_CHANGE` | `SX_EVT_AUDIO_VAD_CHANGE` | NORMAL | ❌ Not Mapped |
| `MAIN_EVENT_CLOCK_TICK` | Timer event (internal) | LOW | ❌ Not Mapped |
| `MAIN_EVENT_ERROR` | `SX_EVT_ERROR` | CRITICAL | ⚠️ Partial |
| `XIAOZHI_STATE_CHANGED_EVENT` | State update (internal) | NORMAL | ❌ Not Mapped |

## Contract Details (từ BATCH 1)

### SIMPLEXL Dispatcher Contract
- **Priority Queues:** CRITICAL (8), HIGH (16), NORMAL (32), LOW (16)
- **Backpressure:** DROP (default), COALESCE, BLOCK (CRITICAL only)
- **Event Taxonomy:** Range reservation (0x0100 per domain)
- **Evidence:** `sx_dispatcher.c:L14-18, L56-66, L158-298`

### SIMPLEXL Orchestrator Contract
- **Single-Writer:** Orchestrator task (priority 8) là writer duy nhất
- **Event Handler Registry:** Max 64 handlers, thread-safe registration
- **State Update Pattern:** Get → Modify → Update version/dirty → Set
- **Evidence:** `sx_orchestrator.c:L15-176, L104-161`

### SIMPLEXL Service Lifecycle Contract
- **Vtable:** `init()`, `start()`, `stop()`, `deinit()`, `on_event()`
- **Registry:** Max 32 services, thread-safe
- **Lifecycle Order:** init → start → (stop) → deinit
- **Evidence:** `sx_service_if.c:L29-197`

### SIMPLEXL State Management Contract
- **Double-Buffer:** Lock-free read, mutex-protected write
- **Version + Dirty Mask:** Version increment, dirty_mask per domain
- **Read Pattern:** Atomic pointer read → Copy snapshot
- **Evidence:** `sx_dispatcher.c:L321-365, sx_state.h:L82-97`

## Next Steps

1. ✅ **BATCH 1:** Phân tích sâu SIMPLEXL core contracts (hoàn thành)
2. **BATCH 2:** Phân tích sâu XIAOZHI core runtime
3. **BATCH 3:** Mapping voice loop end-to-end (minimum viable)
4. **BATCH 4:** Mapping protocol & device control
5. **BATCH 5:** UI binding

