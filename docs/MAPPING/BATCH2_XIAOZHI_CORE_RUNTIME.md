# BATCH 2: XIAOZHI Core Runtime

**Ngày tạo:** 2025-01-02  
**Mục đích:** Phân tích sâu XIAOZHI core runtime để hiểu cách XIAOZHI hoạt động và mapping vào SIMPLEXL

## Tổng Quan

XIAOZHI core runtime gồm 4 thành phần chính:
1. **Application::Start():** Initialization order, task creation
2. **Application::MainEventLoop():** Event processing, schedule pattern
3. **AudioService:** Audio pipeline với 3 tasks (input, output, opus codec)
4. **Protocol:** MQTT/WebSocket callbacks, audio send/receive

## 1. Application::Start() - Initialization Order

### 1.1 Initialization Sequence

**File:** `main/application.cc:357-615`

**Thứ tự khởi tạo (line-by-line):**

1. **L357-359:** Setup display, set state = `kDeviceStateStarting`
   ```cpp
   auto& board = Board::GetInstance();
   SetDeviceState(kDeviceStateStarting);
   auto display = board.GetDisplay();
   ```

2. **L389-405:** Initialize audio service với codec, set callbacks
   ```cpp
   auto codec = board.GetAudioCodec();
   audio_service_.Initialize(codec);
   audio_service_.Start();
   AudioServiceCallbacks callbacks;
   callbacks.on_send_queue_available = [this]() {
       xEventGroupSetBits(event_group_, MAIN_EVENT_SEND_AUDIO);
   };
   // ... other callbacks ...
   audio_service_.SetCallbacks(callbacks);
   ```

3. **L407-411:** Tạo main event loop task (priority 3, stack 3.5KB)
   ```cpp
   xTaskCreate([](void* arg) {
       ((Application*)arg)->MainEventLoop();
       vTaskDelete(NULL);
   }, "main_event_loop", 1024 * 3 + 512, this, 3, &main_event_loop_task_handle_);
   ```

4. **L413-414:** Start clock timer (1 second periodic)
   ```cpp
   esp_timer_start_periodic(clock_timer_handle_, 1000000);
   ```

5. **L416-417:** Start network (WiFi/4G)
   ```cpp
   board.StartNetwork();
   ```

6. **L419-427:** Initialize music và radio services
   ```cpp
   music_ = new Esp32Music();
   music_->Initialize();
   radio_ = new Esp32Radio();
   radio_->Initialize();
   ```

7. **L429-441:** Mount SD card (nếu enable) và init SD music
   ```cpp
   auto sd_card = board.GetSdCard();
   if (sd_card->Initialize() == ESP_OK) {
       sd_music_ = new Esp32SdMusic();
       sd_music_->Initialize(sd_card);
   }
   ```

8. **L443-444:** Update status bar
   ```cpp
   display->UpdateStatusBar(true);
   ```

9. **L446-447:** Check assets version
   ```cpp
   CheckAssetsVersion();
   ```

10. **L449-451:** Check new firmware version (blocking) ⚠️ P0-002
    ```cpp
    Ota ota;
    CheckNewVersion(ota);  // BLOCKING - có thể block vài giây
    ```

11. **L453-459:** Start OTA server
    ```cpp
    auto& ota_server = ota::OtaServer::GetInstance();
    ota_server.Start();
    ```

12. **L461-483:** Initialize protocol (MQTT hoặc WebSocket)
    ```cpp
    if (ota.HasMqttConfig()) {
        protocol_ = std::make_unique<MqttProtocol>();
    } else if (ota.HasWebsocketConfig()) {
        protocol_ = std::make_unique<WebsocketProtocol>();
    }
    ```

13. **L485-601:** Set protocol callbacks
    ```cpp
    protocol_->OnConnected([this]() { ... });
    protocol_->OnNetworkError([this](const std::string& message) { ... });
    protocol_->OnIncomingAudio([this](std::unique_ptr<AudioStreamPacket> packet) { ... });
    protocol_->OnIncomingJson([this, display](const cJSON* root) { ... });
    ```

14. **L602:** Start protocol
    ```cpp
    bool protocol_started = protocol_->Start();
    ```

15. **L604-614:** Print heap stats, set state = `kDeviceStateIdle`
    ```cpp
    SystemInfo::PrintHeapStats();
    SetDeviceState(kDeviceStateIdle);
    ```

**Evidence:**
- Initialization sequence: `application.cc:357-615`
- Blocking operation: `application.cc:449-451` (P0-002)

**⚠️ ISSUE P0-002:** Blocking `CheckNewVersion()` trong `Start()`
- **Vị trí:** `application.cc:449-451`
- **Hậu quả:** UI không responsive trong lúc check version (có thể vài giây)
- **Cách sửa:** Chuyển `CheckNewVersion()` sang background task

### 1.2 Task Creation

**Tasks Created trong Start():**
1. `main_event_loop` (priority 3, stack 3.5KB) - L407-411
2. `audio_input` (priority 8, core 0) - `audio_service.cc:84-88`
3. `audio_output` (priority 4) - `audio_service.cc:91-95`
4. `opus_codec` (priority 2, stack 25KB) - `audio_service.cc:113-117`

**Evidence:**
- Main event loop: `application.cc:407-411`
- Audio tasks: `audio_service.cc:76-118`

## 2. Application::MainEventLoop() - Event Processing

### 2.1 Event Group Pattern

**File:** `main/application.cc:629-711`

**Event Bits:**
- `MAIN_EVENT_SCHEDULE` (bit 0): Async tasks pending
- `MAIN_EVENT_SEND_AUDIO` (bit 1): Audio data ready to send
- `MAIN_EVENT_WAKE_WORD_DETECTED` (bit 2): Wake word detected
- `MAIN_EVENT_VAD_CHANGE` (bit 3): Voice activity change
- `MAIN_EVENT_CLOCK_TICK` (bit 4): 1-second timer tick
- `MAIN_EVENT_ERROR` (bit 5): Network/protocol error

**Wait Pattern:**
```cpp
auto bits = xEventGroupWaitBits(event_group_, 
    MAIN_EVENT_SCHEDULE | MAIN_EVENT_SEND_AUDIO | 
    MAIN_EVENT_WAKE_WORD_DETECTED | MAIN_EVENT_VAD_CHANGE | 
    MAIN_EVENT_CLOCK_TICK | MAIN_EVENT_ERROR, 
    pdTRUE, pdFALSE, portMAX_DELAY);
```

**Evidence:**
- Event group wait: `application.cc:631-636`
- Event processing: `application.cc:638-709`

### 2.2 Schedule Pattern

**File:** `main/application.cc:618-624`

**API:**
```cpp
void Application::Schedule(std::function<void()> callback) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        main_tasks_.push_back(std::move(callback));
    }
    xEventGroupSetBits(event_group_, MAIN_EVENT_SCHEDULE);
}
```

**Processing:**
```cpp
if (bits & MAIN_EVENT_SCHEDULE) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto tasks = std::move(main_tasks_);
    lock.unlock();
    for (auto& task : tasks) {
        task();
    }
}
```

**Evidence:**
- Schedule API: `application.cc:618-624`
- Schedule processing: `application.cc:662-669`

**Call Sites (>=2):**
1. `application.cc:507` - Audio channel closed callback
2. `application.cc:519` - TTS start callback
3. `application.cc:526` - TTS stop callback
4. `application.cc:539` - TTS sentence callback
5. `application.cc:548` - STT callback
6. `application.cc:555` - LLM emotion callback
7. `application.cc:570` - System reboot callback
8. `mqtt_protocol.cc:23` - Reconnect callback
9. `mqtt_protocol.cc:112` - Goodbye callback

**Pattern:**
- Protocol callbacks → `Application::Schedule()` → Main event loop
- UI updates → `Application::Schedule()` → Main event loop
- State changes → `Application::Schedule()` → Main event loop

**Contract cho SIMPLEXL:**
- XIAOZHI `Schedule()` pattern tương đương SIMPLEXL `sx_dispatcher_post_event()`
- Adapter phải convert `Schedule()` calls → SIMPLEXL events
- Callbacks từ protocol phải schedule vào orchestrator (không chạy trực tiếp)

### 2.3 Event Processing Logic

**File:** `main/application.cc:638-709`

**Processing Order:**
1. **MAIN_EVENT_ERROR** (L638-641): Set state idle, show alert
2. **MAIN_EVENT_SEND_AUDIO** (L643-649): Pop packets từ send queue, send via protocol
3. **MAIN_EVENT_WAKE_WORD_DETECTED** (L651-653): Call `OnWakeWordDetected()`
4. **MAIN_EVENT_VAD_CHANGE** (L655-660): Update LED state
5. **MAIN_EVENT_SCHEDULE** (L662-669): Execute scheduled callbacks
6. **MAIN_EVENT_CLOCK_TICK** (L671-709): Update status bar, weather fetch

**Evidence:**
- Error handling: `application.cc:638-641`
- Audio send: `application.cc:643-649`
- Wake word: `application.cc:651-653`
- Schedule: `application.cc:662-669`
- Clock tick: `application.cc:671-709`

## 3. AudioService - Audio Pipeline

### 3.1 Audio Tasks

**File:** `main/audio/audio_service.cc:76-118`

**Tasks:**
1. **audio_input** (priority 8, core 0, stack 3KB)
   - Read mic → Feed wake word/processor → Encode queue
   - Evidence: `audio_service.cc:84-88` (với AFE) hoặc `98-102` (không AFE)

2. **audio_output** (priority 4, stack 3KB)
   - Playback queue → Codec output → Speaker
   - Evidence: `audio_service.cc:91-95` hoặc `105-109`

3. **opus_codec** (priority 2, stack 25KB)
   - Encode queue → Opus encode → Send queue
   - Decode queue → Opus decode → Playback queue
   - Evidence: `audio_service.cc:113-117`

**Evidence:**
- Task creation: `audio_service.cc:76-118`
- Task implementations: `audio_service.cc:190-372`

### 3.2 Audio Queues

**File:** `main/audio/audio_service.h:136-142`

**Queues (std::deque với mutex):**
- `audio_encode_queue_`: Max 2 tasks (L139)
- `audio_playback_queue_`: Max 2 tasks (L140)
- `audio_decode_queue_`: Max 40 packets (L136)
- `audio_send_queue_`: Max 40 packets (L137)
- `audio_testing_queue_`: Max ~167 packets (L138)
- `timestamp_queue_`: Max 3 timestamps (L142)

**Thread Safety:**
- Tất cả queues dùng `audio_queue_mutex_` (L134)
- Condition variable `audio_queue_cv_` cho blocking wait (L135)

**Evidence:**
- Queue definitions: `audio_service.h:136-142`
- Mutex: `audio_service.h:134`
- Condition variable: `audio_service.h:135`

**⚠️ ISSUE P1-015:** Queue backpressure không có timeout
- **Vị trí:** `audio_service.cc:407` - `audio_queue_cv_.wait()` với no timeout
- **Hậu quả:** Nếu consumer chậm, producer có thể block forever
- **Cách sửa:** Thêm timeout và drop packets nếu queue full

### 3.3 Audio Input Flow

**File:** `main/audio/audio_service.cc:190-257`

**Flow:**
```
AudioInputTask() [priority 8, core 0]
  └─> xEventGroupWaitBits() [wait for AS_EVENT_*]
      ├─> AS_EVENT_AUDIO_TESTING_RUNNING: ReadAudioData() → EncodeToTestingQueue
      ├─> AS_EVENT_WAKE_WORD_RUNNING: ReadAudioData() → WakeWord::Feed()
      └─> AS_EVENT_AUDIO_PROCESSOR_RUNNING: ReadAudioData() → AudioProcessor::Feed()
          └─> AudioProcessor::OnOutput() → PushTaskToEncodeQueue()
              └─> audio_encode_queue_ → OpusCodecTask()
```

**Evidence:**
- Input task: `audio_service.cc:190-257`
- Read audio: `audio_service.cc:135-188`
- Push to encode: `audio_service.cc:389-410`

### 3.4 Audio Output Flow

**File:** `main/audio/audio_service.cc:259-293`

**Flow:**
```
AudioOutputTask() [priority 4]
  └─> audio_queue_cv_.wait() [wait for playback queue not empty]
      └─> Pop từ audio_playback_queue_
          └─> Codec::OutputData() → Speaker
```

**Evidence:**
- Output task: `audio_service.cc:259-293`
- Codec output: `audio_service.cc:277`

### 3.5 Opus Codec Task

**File:** `main/audio/audio_service.cc:295-372`

**Flow:**
```
OpusCodecTask() [priority 2, stack 25KB]
  └─> Wait condition: encode queue not empty OR decode queue not empty
      ├─> Decode path:
      │   └─> Pop từ audio_decode_queue_
      │       └─> OpusDecoder::Decode()
      │           └─> Resample (nếu cần)
      │               └─> Push vào audio_playback_queue_
      └─> Encode path:
          └─> Pop từ audio_encode_queue_
              └─> OpusEncoder::Encode()
                  └─> Push vào audio_send_queue_
                      └─> Callback: on_send_queue_available()
```

**Evidence:**
- Opus codec task: `audio_service.cc:295-372`
- Decode: `audio_service.cc:307-336`
- Encode: `audio_service.cc:338-368`

**⚠️ ISSUE P0-008:** Audio queue race condition
- **Vị trí:** `audio_service.cc:298-302` - Check condition và pop có thể race
- **Hậu quả:** Packet có thể bị drop hoặc queue overflow
- **Cách sửa:** Atomic check-and-pop hoặc re-check sau lock

## 4. Protocol - MQTT/WebSocket Callbacks

### 4.1 Protocol Base Class

**File:** `main/protocols/protocol.h:L44-95`

**Callbacks:**
- `OnIncomingAudio()`: Audio packets từ server
- `OnIncomingJson()`: JSON messages từ server
- `OnAudioChannelOpened()`: Audio channel opened
- `OnAudioChannelClosed()`: Audio channel closed
- `OnNetworkError()`: Network error
- `OnConnected()`: Protocol connected
- `OnDisconnected()`: Protocol disconnected

**Evidence:**
- Callback definitions: `protocol.h:L58-64`
- Callback storage: `protocol.h:L78-84`

### 4.2 MQTT Protocol

**File:** `main/protocols/mqtt_protocol.cc:L93-121`

**OnMessage Callback:**
```cpp
mqtt_->OnMessage([this](const std::string& topic, const std::string& payload) {
    cJSON* root = cJSON_Parse(payload.c_str());
    // Parse message type
    if (strcmp(type->valuestring, "hello") == 0) {
        ParseServerHello(root);
    } else if (on_incoming_json_ != nullptr) {
        on_incoming_json_(root);  // Call Application callback
    }
    cJSON_Delete(root);
});
```

**Thread Context:**
- Callback chạy trong network task (ESP-IDF MQTT library)
- Application callback (`on_incoming_json_`) được gọi trực tiếp (không schedule)

**Evidence:**
- OnMessage: `mqtt_protocol.cc:L93-121`
- Application callback: `application.cc:513-601`

**⚠️ ISSUE P1-014:** Protocol callbacks chạy trong network task context
- **Vị trí:** `mqtt_protocol.cc:93` - `OnMessage()` callback
- **Rủi ro:** Network task có priority thấp, có thể delay audio processing
- **Cách sửa:** Schedule callbacks vào main_event_loop (đã làm một phần trong Application::OnIncomingJson)

### 4.3 WebSocket Protocol

**File:** `main/protocols/websocket_protocol.cc:L111-165`

**OnData Callback:**
```cpp
websocket_->OnData([this](const char* data, size_t len, bool binary) {
    if (binary) {
        if (on_incoming_audio_ != nullptr) {
            on_incoming_audio_(std::make_unique<AudioStreamPacket>(...));
        }
    } else {
        auto root = cJSON_Parse(data);
        if (on_incoming_json_ != nullptr) {
            on_incoming_json_(root);  // Call Application callback
        }
        cJSON_Delete(root);
    }
});
```

**Thread Context:**
- Callback chạy trong network task (ESP-IDF WebSocket library)
- Application callback được gọi trực tiếp (không schedule)

**Evidence:**
- OnData: `websocket_protocol.cc:L111-165`
- Application callback: `application.cc:513-601`

### 4.4 Protocol Audio Send/Receive

**Audio Send:**
```cpp
// MainEventLoop() → MAIN_EVENT_SEND_AUDIO
while (auto packet = audio_service_.PopPacketFromSendQueue()) {
    if (protocol_ && !protocol_->SendAudio(std::move(packet))) {
        break;
    }
}
```

**Audio Receive:**
```cpp
// Protocol callback → Application::OnIncomingAudio()
protocol_->OnIncomingAudio([this](std::unique_ptr<AudioStreamPacket> packet) {
    if (device_state_ == kDeviceStateSpeaking) {
        audio_service_.PushPacketToDecodeQueue(std::move(packet));
    }
});
```

**Evidence:**
- Audio send: `application.cc:643-649`
- Audio receive: `application.cc:493-497`

## 5. State Management

### 5.1 DeviceState Enum

**File:** `main/device_state.h:25-37`

**States:**
- `kDeviceStateUnknown` (0)
- `kDeviceStateStarting` (1)
- `kDeviceStateWifiConfiguring` (2)
- `kDeviceStateIdle` (3)
- `kDeviceStateConnecting` (4)
- `kDeviceStateListening` (5)
- `kDeviceStateSpeaking` (6)
- `kDeviceStateUpgrading` (7)
- `kDeviceStateActivating` (8)
- `kDeviceStateAudioTesting` (9)
- `kDeviceStateFatalError` (10)

**Evidence:**
- State enum: `device_state.h:25-37`

### 5.2 SetDeviceState()

**File:** `main/application.cc:764-810`

**Implementation:**
```cpp
void Application::SetDeviceState(DeviceState state) {
    if (device_state_ == state) {
        return;
    }
    auto previous_state = device_state_;
    device_state_ = state;  // ⚠️ No mutex protection
    ESP_LOGI(TAG, "STATE: %s", STATE_STRINGS[device_state_]);
    
    // Send state change event
    DeviceStateEventManager::GetInstance().PostStateChangeEvent(previous_state, state);
    
    // Update display, LED, stop music/radio
    // ...
}
```

**Thread Safety:**
- ❌ NOT THREAD-SAFE (volatile DeviceState, no mutex)
- Có thể gọi từ bất kỳ task nào
- Race condition risk nếu set từ multiple tasks

**Evidence:**
- SetDeviceState: `application.cc:764-810`
- State variable: `application.h:89` (volatile DeviceState)

**Call Sites (>=2):**
1. `application.cc:359` - Start()
2. `application.cc:452` - CheckNewVersion()
3. `application.cc:605` - Start() end
4. `application.cc:639` - MainEventLoop() error
5. `application.cc:722` - OnWakeWordDetected()
6. `application.cc:761` - SetListeningMode()
7. ... (31 total calls)

### 5.3 DeviceStateEventManager

**File:** `main/device_state_event.cc:15-21`

**PostStateChangeEvent:**
```cpp
void DeviceStateEventManager::PostStateChangeEvent(DeviceState prev, DeviceState curr) {
    device_state_event_data_t data = {prev, curr};
    esp_event_post(XIAOZHI_STATE_EVENTS, XIAOZHI_STATE_CHANGED_EVENT, 
                   &data, sizeof(data), portMAX_DELAY);
}
```

**Callbacks:**
- Vector of callbacks (no unregister mechanism - P1 risk)
- Thread-safe registration (mutex protected)

**Evidence:**
- Post event: `device_state_event.cc:15-21`
- Callbacks: `device_state_event.cc:10-13`

## 6. Call Sites Analysis

### 6.1 Application::Schedule() Call Sites

**Top 10 Call Sites:**
1. `application.cc:507` - Audio channel closed
2. `application.cc:519` - TTS start
3. `application.cc:526` - TTS stop
4. `application.cc:539` - TTS sentence
5. `application.cc:548` - STT
6. `application.cc:555` - LLM emotion
7. `application.cc:570` - System reboot
8. `mqtt_protocol.cc:23` - Reconnect
9. `mqtt_protocol.cc:112` - Goodbye
10. `websocket_protocol.cc:165` - JSON message

**Pattern:**
- Protocol callbacks → Schedule → Main event loop
- UI updates → Schedule → Main event loop
- State changes → Schedule → Main event loop

### 6.2 SetDeviceState() Call Sites

**Top 10 Call Sites:**
1. `application.cc:359` - Start() initial state
2. `application.cc:452` - CheckNewVersion() state
3. `application.cc:605` - Start() end state
4. `application.cc:639` - MainEventLoop() error
5. `application.cc:722` - OnWakeWordDetected()
6. `application.cc:761` - SetListeningMode()
7. `application.cc:352` - ToggleChatState()
8. `application.cc:100` - CheckAssetsVersion()
9. `application.cc:135` - CheckNewVersion() retry
10. `application.cc:182` - CheckNewVersion() activation

**Pattern:**
- State transitions từ nhiều contexts (protocol, audio, UI, system)
- Không có mutex protection → Race condition risk

## 7. Mapping Summary cho SIMPLEXL Adapter

### 7.1 Application::Start() → sx_bootstrap_start()

**Mapping:**
- XIAOZHI: `Application::Start()` → Display → Audio → Main event loop → Network → Protocol
- SIMPLEXL: `sx_bootstrap_start()` → Platform → UI → Audio → Services (lazy-loaded)

**Adapter Strategy:**
- Adapter init trong `sx_bootstrap_start()` sau orchestrator
- Adapter start XIAOZHI `Application::Start()` trong `sx_service_if_t::start()`

### 7.2 Application::MainEventLoop() → sx_orchestrator_task()

**Mapping:**
- XIAOZHI: Event Group wait → Process events → Execute scheduled callbacks
- SIMPLEXL: Priority queue poll → Process events → Update state

**Adapter Strategy:**
- Convert Event Group bits → SIMPLEXL events
- Convert scheduled callbacks → SIMPLEXL events với priority NORMAL

### 7.3 AudioService → sx_audio_service

**Mapping:**
- XIAOZHI: 3 tasks (input priority 8, output priority 4, opus priority 2)
- SIMPLEXL: 2 tasks (recording priority 5, playback priority 4)

**Adapter Strategy:**
- SIMPLEXL audio_service làm owner
- XIAOZHI audio pipeline chỉ cung cấp voice processing
- Adapter feed PCM vào SIMPLEXL audio_service

### 7.4 Protocol Callbacks → SIMPLEXL Events

**Mapping:**
- XIAOZHI: Protocol callbacks → `Application::Schedule()` → Main event loop
- SIMPLEXL: Protocol callbacks → `sx_dispatcher_post_event()` → Orchestrator

**Adapter Strategy:**
- Adapter intercept protocol callbacks
- Convert callbacks → SIMPLEXL events
- Post events với priority phù hợp (HIGH cho audio, NORMAL cho JSON)

## 8. Issues & Risks

### P0 Issues

1. **P0-002:** Blocking `CheckNewVersion()` trong `Start()`
   - **Vị trí:** `application.cc:449-451`
   - **Hậu quả:** UI freeze trong lúc check version
   - **Cách sửa:** Chuyển sang background task

2. **P0-008:** Audio queue race condition
   - **Vị trí:** `audio_service.cc:298-302`
   - **Hậu quả:** Packet drop hoặc queue overflow
   - **Cách sửa:** Atomic check-and-pop

### P1 Issues

1. **P1-003:** Main event loop stack có thể thiếu
   - **Vị trí:** `application.cc:411` - stack = 3.5KB
   - **Rủi ro:** Stack overflow nếu có nhiều nested callbacks
   - **Cách sửa:** Tăng lên 4KB hoặc monitor stack watermark

2. **P1-014:** Protocol callbacks chạy trong network task
   - **Vị trí:** `mqtt_protocol.cc:93`, `websocket_protocol.cc:111`
   - **Rủi ro:** Delay audio processing
   - **Cách sửa:** Schedule callbacks vào orchestrator (đã làm một phần)

3. **P1-015:** Queue backpressure không có timeout
   - **Vị trí:** `audio_service.cc:407`
   - **Hậu quả:** Producer có thể block forever
   - **Cách sửa:** Thêm timeout và drop packets

## 9. Next Steps

1. **BATCH 3:** Implement adapter với mappings trên
2. **BATCH 4:** Map protocol callbacks chi tiết
3. **BATCH 5:** Map UI state updates

## 10. File Đã Đọc

- ✅ `main/application.h` (133 lines)
- ✅ `main/application.cc` (650+ lines)
- ✅ `main/audio/audio_service.h` (162 lines)
- ✅ `main/audio/audio_service.cc` (690+ lines)
- ✅ `main/protocols/protocol.h` (98 lines)
- ✅ `main/protocols/mqtt_protocol.h` (61 lines)
- ✅ `main/protocols/websocket_protocol.h` (35 lines)
- ✅ `main/protocols/mqtt_protocol.cc` (150+ lines)
- ✅ `main/protocols/websocket_protocol.cc` (150+ lines)
- ✅ `main/device_state.h` (18 lines)
- ✅ `main/device_state_event.cc` (46 lines)

**Tổng:** ~2,200+ lines đã đọc trong BATCH 2

