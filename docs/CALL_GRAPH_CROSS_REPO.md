# Call Graph Cross-Repo - XIAOZHI → SIMPLEXL

**Ngày tạo:** 2025-01-02  
**Mục đích:** Call graph xuyên repo cho boot/ui/audio/network flows với line-range evidence

## Boot Chain Comparison

### XIAOZHI Boot Chain

**File:** `main/main.cc:55-74`, `main/application.cc:357-615`

```
app_main() [main/main.cc:55]
  ├─> esp_event_loop_create_default() [ESP-IDF]
  ├─> nvs_flash_init() [ESP-IDF]
  └─> Application::GetInstance().Start() [main/application.cc:357]
      ├─> Display::Setup() [main/display/display.cc]
      ├─> AudioService::Initialize() [main/audio/audio_service.cc]
      ├─> xTaskCreate(main_event_loop) [main/application.cc:411]
      ├─> esp_timer_start(clock_timer) [main/application.cc:413]
      ├─> Network::Start() [main/network/]
      ├─> Music::Initialize() [main/features/music/]
      ├─> Radio::Initialize() [main/features/radio/]
      ├─> SDCard::Mount() [main/storage/sd_card.cc]
      ├─> Assets::CheckVersion() [main/assets.cc]
      ├─> Ota::CheckNewVersion() [main/ota.cc:449] ⚠️ BLOCKING
      ├─> OtaServer::Start() [main/ota_server.cc]
      └─> Protocol::Start() [main/protocols/]
```

**Bottlenecks:**
- `Ota::CheckNewVersion()` blocking trong `Start()` (P0-002)
- Total boot time: ~5-10 seconds (tùy network)

### SIMPLEXL Boot Chain

**File:** `app/app_main.c:8-11`, `components/sx_core/sx_bootstrap.c:55-831`

```
app_main() [app/app_main.c:8]
  └─> sx_bootstrap_start() [components/sx_core/sx_bootstrap.c:55]
      ├─> nvs_flash_init() [ESP-IDF]
      ├─> sx_error_handler_init() [sx_error_handler.c:28]
      ├─> sx_settings_service_init() [sx_settings_service.c]
      ├─> sx_theme_service_init() [sx_theme_service.c]
      ├─> sx_ota_service_init() [sx_ota_service.c]
      ├─> sx_mcp_server_init() [sx_mcp_server.c]
      ├─> sx_dispatcher_init() [sx_dispatcher.c:47]
      ├─> sx_orchestrator_start() [sx_orchestrator.c:178]
      ├─> sx_platform_display_init() [sx_platform.c:133] ⚠️ ~500ms
      ├─> sx_platform_touch_init() [sx_platform.c:371] ⚠️ ~100ms
      ├─> sx_spi_bus_manager_init() [sx_spi_bus_manager.c:12]
      ├─> sx_sd_service_init() + start() [sx_sd_service.c] ⚠️ ~2000ms (nếu SD chậm)
      ├─> sx_assets_init() [sx_assets.c]
      ├─> sx_ui_start() [sx_ui.c:308]
      ├─> sx_audio_ducking_init() [sx_audio_ducking.c]
      ├─> sx_audio_power_init() [sx_audio_power.c]
      ├─> sx_audio_router_init() [sx_audio_router.c]
      └─> sx_audio_service_init() + start() [sx_audio_service.c]
```

**Bottlenecks:**
- Display init: ~500ms
- SD mount: ~2000ms (nếu SD chậm)
- Total boot time: ~3-5 seconds (không tính lazy-loaded services)

### Boot Chain Mapping

| XIAOZHI Step | SIMPLEXL Step | Status | Notes |
|--------------|---------------|--------|-------|
| `esp_event_loop_create_default()` | (Built-in ESP-IDF) | ✅ Match | |
| `nvs_flash_init()` | `nvs_flash_init()` | ✅ Match | |
| `Application::Start()` | `sx_bootstrap_start()` | ⚠️ Partial | XIAOZHI có blocking ops |
| `Display::Setup()` | `sx_platform_display_init()` | ⚠️ Partial | API khác |
| `AudioService::Initialize()` | `sx_audio_service_init()` | ⚠️ Partial | API khác |
| `xTaskCreate(main_event_loop)` | `sx_orchestrator_start()` | ⚠️ Partial | Priority khác (3 vs 8) |
| `Ota::CheckNewVersion()` | (Lazy-loaded) | ❌ Not Mapped | XIAOZHI blocking, SIMPLEXL lazy |

## UI Chain Comparison

### XIAOZHI UI Chain

**File:** `main/display/lcd_display.cc`, `main/display/lcd_touch.cc`

```
Touch Input (ISR/Driver)
  └─> touch_event_task() [main/display/lcd_touch.cc:46]
      └─> lvgl_port_touch_read() [esp_lvgl_port]
          └─> LVGL Event
              └─> Application::Schedule() [main/application.cc:57]
                  └─> main_tasks_ queue
                      └─> MainEventLoop() [main/application.cc:629]
                          └─> Display::SetStatus() / SetChatMessage() [main/display/display.cc]
                              └─> LVGL API (trong display task)
```

**Thread Context:**
- Touch task: Core 1, priority 5
- LVGL task: Core 1, priority variable
- Main event loop: Any core, priority 3

### SIMPLEXL UI Chain

**File:** `components/sx_ui/sx_ui_task.c`, `components/sx_platform/sx_platform.c`

```
Touch Input (ISR/Driver)
  └─> esp_lvgl_port (touch handler)
      └─> LVGL Event
          └─> sx_ui_task() [components/sx_ui/sx_ui_task.c:308]
              └─> sx_dispatcher_post_event(SX_EVT_UI_INPUT) [sx_dispatcher.c:158]
                  └─> Event Queue (s_evt_q_normal)
                      └─> sx_orchestrator_task() [sx_orchestrator.c:104]
                          └─> sx_event_handler_process() [sx_event_handler.c:42]
                              └─> sx_event_handler_ui_input() [event handlers]
                                  └─> sx_dispatcher_set_state() [sx_dispatcher.c:321]
                                      └─> sx_ui_task() [sx_ui_task.c:201]
                                          └─> sx_dispatcher_get_state() [sx_dispatcher.c:354]
                                              └─> Screen on_state_change() [ui_router.c]
                                                  └─> LVGL API (trong UI task)
```

**Thread Context:**
- UI task: Any core, priority 7
- Orchestrator: Any core, priority 8
- LVGL: Chỉ trong UI task (compile-time + runtime guard)

### UI Chain Mapping

| XIAOZHI Step | SIMPLEXL Step | Status | Notes |
|--------------|---------------|--------|-------|
| `touch_event_task()` | `sx_ui_task()` (touch handling) | ⚠️ Partial | Core khác (1 vs Any) |
| `Application::Schedule()` | `sx_dispatcher_post_event()` | ❌ Not Mapped | Pattern khác |
| `MainEventLoop()` | `sx_orchestrator_task()` | ⚠️ Partial | Priority khác (3 vs 8) |
| `Display::SetStatus()` | State update → UI screen | ❌ Not Mapped | Direct call vs state |

## Audio Chain Comparison

### XIAOZHI Audio Chain

**File:** `main/audio/audio_service.cc`

**Recording Flow:**
```
MIC → Codec::InputData()
  └─> AudioInputTask() [main/audio/audio_service.cc:84] (priority 8, core 0)
      ├─> WakeWord::Feed() [nếu wake word active]
      └─> AudioProcessor::Feed() [nếu processor active]
          └─> PushTaskToEncodeQueue()
              └─> audio_encode_queue_ (std::deque, max 2)
                  └─> OpusCodecTask() [main/audio/audio_service.cc:113] (priority 2)
                      └─> OpusEncoder::Encode()
                          └─> audio_send_queue_ (std::deque, max 40)
                              └─> MainEventLoop() [main/application.cc:629]
                                  └─> Protocol::SendAudio()
```

**Playback Flow:**
```
Server → Protocol::OnIncomingAudio()
  └─> Application::OnIncomingAudio() [main/application.cc]
      └─> AudioService::PushPacketToDecodeQueue()
          └─> audio_decode_queue_ (std::deque, max 40)
              └─> OpusCodecTask() [main/audio/audio_service.cc:113]
                  └─> OpusDecoder::Decode()
                      └─> Resample (nếu cần)
                          └─> audio_playback_queue_ (std::deque, max 2)
                              └─> AudioOutputTask() [main/audio/audio_service.cc:91] (priority 4)
                                  └─> Codec::OutputData()
                                      └─> Speaker
```

**Thread Context:**
- Audio input: Core 0, priority 8
- Audio output: Any core, priority 4
- Opus codec: Any core, priority 2
- Main event loop: Any core, priority 3

### SIMPLEXL Audio Chain

**File:** `components/sx_services/sx_audio_service.c`

**Recording Flow:**
```
MIC → I2S RX
  └─> sx_audio_recording_task() [sx_audio_service.c:618] (priority 5)
      ├─> sx_wake_word_feed_audio() [nếu wake word active]
      └─> sx_stt_send_audio_chunk() [nếu STT active]
          └─> s_chunk_queue (FreeRTOS queue, size 5)
              └─> sx_stt_task() [sx_stt_service.c:214] (priority 5)
                  └─> HTTP POST to STT endpoint
```

**Playback Flow:**
```
Service/UI
  └─> sx_audio_service_play() [sx_audio_service.c]
      └─> sx_audio_codec_open() [codec]
      └─> sx_audio_router_set_source() [router]
      └─> sx_dispatcher_post_event(SX_EVT_AUDIO_PLAYBACK_STARTED)
          └─> sx_audio_file_task() [sx_audio_service.c:536] (priority 4)
              └─> sx_audio_codec_read() [codec]
                  └─> Decode audio data
              └─> sx_audio_router_feed() [router]
                  └─> Apply EQ, ducking, volume
              └─> i2s_write() [I2S Driver]
                  └─> DAC/Amplifier
```

**Thread Context:**
- Audio recording: Core 1, priority 5
- Audio playback: Core 0, priority 4
- STT: Any core, priority 5

### Audio Chain Mapping

| XIAOZHI Step | SIMPLEXL Step | Status | Notes |
|--------------|---------------|--------|-------|
| `AudioInputTask()` | `sx_audio_recording_task()` | ⚠️ Partial | Priority/Core khác (8/0 vs 5/1) |
| `OpusCodecTask()` | (Không có dedicated task) | ❌ Not Mapped | SIMPLEXL dùng codec library |
| `AudioOutputTask()` | `sx_audio_file_task()` | ⚠️ Partial | Priority match (4), core khác |
| `Protocol::SendAudio()` | `sx_protocol_ws_send_audio()` / `sx_protocol_mqtt_send_audio()` | ❌ Not Mapped | |
| `Protocol::OnIncomingAudio()` | `sx_audio_protocol_bridge` | ❌ Not Mapped | |

## Network/AI Chain Comparison

### XIAOZHI Network/AI Chain

**File:** `main/protocols/mqtt_protocol.cc`, `main/protocols/websocket_protocol.cc`

```
WiFi Connected
  └─> Network Event Handler
      └─> Protocol::Start() [main/protocols/]
          └─> Protocol::Connect()
              └─> Protocol::OnConnected()
                  └─> Application::Schedule() [main/application.cc:57]
                      └─> MainEventLoop() [main/application.cc:629]
                          └─> SetDeviceState(kDeviceStateConnecting)

Protocol Message Received
  └─> Protocol::OnMessage() / OnData() [main/protocols/]
      └─> Application::Schedule() [main/application.cc:57]
          └─> MainEventLoop() [main/application.cc:629]
              └─> Application::OnIncomingAudio() / OnMessage()
                  └─> AudioService::PushPacketToDecodeQueue() [nếu audio]
                  └─> Chatbot::HandleMessage() [nếu text]
                      └─> STT/TTS/MCP processing
```

**Thread Context:**
- Protocol callbacks: Network task (ESP-IDF)
- Main event loop: Any core, priority 3

### SIMPLEXL Network/AI Chain

**File:** `components/sx_services/sx_wifi_service.c`, `components/sx_services/sx_protocol_ws.c`

```
WiFi Connected
  └─> esp_wifi_event_handler() [ESP-IDF]
      └─> sx_dispatcher_post_event(SX_EVT_WIFI_CONNECTED) [sx_wifi_service.c]
          └─> Event Queue (s_evt_q_high)
              └─> sx_orchestrator_task() [sx_orchestrator.c:104]
                  └─> sx_event_handler_wifi_connected() [event handlers]
                      └─> sx_dispatcher_set_state() [sx_dispatcher.c:321]
                          └─> State update (wifi.connected = true)

Protocol Message Received
  └─> sx_protocol_ws_on_data() / sx_protocol_mqtt_on_message() [protocol]
      └─> sx_dispatcher_post_event(SX_EVT_CHATBOT_MESSAGE) [sx_chatbot_service.c]
          └─> Event Queue (s_evt_q_normal)
              └─> sx_orchestrator_task() [sx_orchestrator.c:104]
                  └─> sx_event_handler_chatbot_message() [event handlers]
                      └─> sx_chatbot_handle_json_message() [sx_chatbot_service.c]
                          └─> STT/TTS/MCP processing
```

**Thread Context:**
- Protocol callbacks: Network task (ESP-IDF) → Schedule vào orchestrator
- Orchestrator: Any core, priority 8

### Network/AI Chain Mapping

| XIAOZHI Step | SIMPLEXL Step | Status | Notes |
|--------------|---------------|--------|-------|
| `Protocol::Start()` | `sx_protocol_ws_start()` / `sx_protocol_mqtt_start()` | ⚠️ Partial | API khác |
| `Protocol::OnMessage()` | `sx_protocol_ws_on_data()` / `sx_protocol_mqtt_on_message()` | ⚠️ Partial | Callback pattern khác |
| `Application::Schedule()` | `sx_dispatcher_post_event()` | ❌ Not Mapped | Pattern khác |
| `Application::OnIncomingAudio()` | `sx_audio_protocol_bridge` | ❌ Not Mapped | |
| `Chatbot::HandleMessage()` | `sx_chatbot_handle_json_message()` | ⚠️ Partial | API khác |

## Adapter Integration Points

### Voice Loop Integration

```
SIMPLEXL Event: SX_EVT_UI_INPUT_START_CONVERSATION
  └─> sx_xiaozhi_adapter_start_conversation() [adapter]
      └─> Application::StartListening() [XIAOZHI]
          └─> AudioInputTask() [XIAOZHI]
              └─> AudioProcessor::Feed() [XIAOZHI]
                  └─> OpusCodecTask() [XIAOZHI]
                      └─> Protocol::SendAudio() [XIAOZHI]
                          └─> Server Response
                              └─> Protocol::OnIncomingAudio() [XIAOZHI]
                                  └─> TTS Audio
                                      └─> sx_xiaozhi_adapter_get_tts_audio() [adapter]
                                          └─> sx_audio_service_feed_pcm() [SIMPLEXL]
                                              └─> sx_audio_file_task() [SIMPLEXL]
                                                  └─> Speaker
```

## Next Steps

1. **BATCH 3:** Implement voice loop adapter với call chains trên
2. **BATCH 4:** Map protocol callbacks
3. **BATCH 5:** Map UI state updates

