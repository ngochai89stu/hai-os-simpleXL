# Call Graph - Luồng Runtime và Module Dependencies

Tài liệu này mô tả call graph theo module và luồng runtime của hệ thống **hai-os-simplexl**.

## Boot Chain

### Entry Point

```
app_main() [app/app_main.c:8]
  └─> sx_bootstrap_start() [components/sx_core/sx_bootstrap.c:55]
```

### Bootstrap Sequence

```
sx_bootstrap_start()
  ├─> nvs_flash_init() [ESP-IDF]
  ├─> sx_error_handler_init() [components/sx_core/sx_error_handler.c:28]
  ├─> sx_settings_service_init() [Service]
  ├─> sx_theme_service_init() [Service]
  ├─> sx_ota_service_init() [Service]
  ├─> sx_mcp_server_init() [Service]
  ├─> sx_dispatcher_init() [components/sx_core/sx_dispatcher.c:47]
  │   ├─> sx_metrics_init() [components/sx_core/sx_metrics.c:22]
  │   └─> sx_event_string_pool_init() [String pool]
  ├─> sx_orchestrator_start() [components/sx_core/sx_orchestrator.c:178]
  │   └─> xTaskCreatePinnedToCore(sx_orchestrator_task) [FreeRTOS]
  ├─> sx_platform_display_init() [components/sx_platform/sx_platform.c:133]
  │   ├─> ledc_timer_config() [ESP-IDF LEDC]
  │   ├─> ledc_channel_config() [ESP-IDF LEDC]
  │   ├─> spi_bus_initialize(SPI3_HOST) [ESP-IDF SPI]
  │   ├─> esp_lcd_new_panel_io_spi() [ESP-IDF LCD]
  │   └─> esp_lcd_new_panel_st7796/st7789/ili9341() [ESP-IDF LCD]
  ├─> sx_platform_touch_init() [components/sx_platform/sx_platform.c:371]
  │   ├─> i2c_new_master_bus(I2C_NUM_1) [ESP-IDF I2C]
  │   ├─> esp_lcd_new_panel_io_i2c() [ESP-IDF LCD]
  │   └─> esp_lcd_touch_new_i2c_ft5x06() [ESP-IDF LCD Touch]
  ├─> sx_spi_bus_manager_init() [components/sx_platform/sx_spi_bus_manager.c:12]
  │   └─> xSemaphoreCreateMutex() [FreeRTOS]
  ├─> sx_sd_service_init() [Service]
  ├─> sx_assets_init() [Assets]
  ├─> sx_ui_start() [UI]
  │   └─> xTaskCreatePinnedToCore(sx_ui_task) [FreeRTOS]
  ├─> sx_audio_ducking_init() [Service]
  ├─> sx_audio_power_init() [Service]
  ├─> sx_audio_router_init() [Service]
  └─> sx_audio_service_init() + start() [Service]
```

**Thứ tự quan trọng:**
1. NVS phải init trước (cần cho settings)
2. Dispatcher phải init trước orchestrator
3. Orchestrator phải start trước services (để xử lý events)
4. Platform (display/touch) phải init trước UI
5. UI phải start trước services (để hiển thị state)

## Event Flow

### UI Input → Event → State → UI Update

```
User Input (Touch/Button)
  └─> sx_ui_task() [UI Task]
      └─> sx_dispatcher_post_event(SX_EVT_UI_INPUT) [components/sx_core/sx_dispatcher.c:158]
          └─> xQueueSend(s_evt_q_normal, evt) [FreeRTOS Queue]

sx_orchestrator_task() [Orchestrator Task]
  └─> sx_dispatcher_poll_event() [components/sx_core/sx_dispatcher.c:300]
      └─> xQueueReceive(s_evt_q_critical/high/normal/low) [FreeRTOS Queue]
  └─> sx_event_handler_process() [components/sx_core/sx_event_handler.c:42]
      └─> sx_event_handler_ui_input() [Event handler]
          └─> Update state [sx_state_t]
  └─> sx_dispatcher_set_state() [components/sx_core/sx_dispatcher.c:321]
      └─> Double-buffer swap [Atomic pointer update]

sx_ui_task() [UI Task]
  └─> sx_dispatcher_get_state() [components/sx_core/sx_dispatcher.c:354]
      └─> Copy state snapshot [Lock-free read]
  └─> lv_timer_handler() [LVGL]
  └─> Update UI widgets based on state
```

### Network & AI Flow

#### WiFi Connection Flow
```
sx_wifi_service_init()
  ├─> esp_netif_init()
  ├─> esp_event_loop_create_default()
  ├─> esp_netif_create_default_wifi_sta()
  ├─> esp_wifi_init()
  └─> sx_network_optimizer_init()

sx_wifi_service_start()
  └─> esp_wifi_start()

WiFi Event Handler [ESP-IDF Event Loop]
  ├─> WIFI_EVENT_STA_CONNECTED
  │   └─> xEventGroupSetBits(WIFI_CONNECTED_BIT)
  ├─> WIFI_EVENT_STA_DISCONNECTED
  │   └─> Auto-reconnect logic (if enabled)
  └─> IP_EVENT_STA_GOT_IP
      └─> Update s_ip_address, post SX_EVT_WIFI_CONNECTED
```

#### STT Flow (Speech-to-Text)
```
sx_audio_start_recording_with_stt()
  ├─> sx_stt_start_session(callback)
  │   └─> xTaskCreate(sx_stt_task)
  └─> sx_audio_start_recording()

sx_audio_recording_task() [Recording Task]
  └─> i2s_channel_read() [I2S RX]
      └─> sx_stt_send_audio_chunk(pcm, samples)
          └─> xQueueSend(s_chunk_queue) [Queue to STT task]

sx_stt_task() [STT Task]
  └─> xQueueReceive(s_chunk_queue)
      └─> esp_http_client_perform() [HTTP POST to STT endpoint]
          └─> Parse JSON response (transcript, is_final)
              ├─> Call callback(transcript, is_final)
              └─> If final: sx_dispatcher_post_event(SX_EVT_UI_INPUT)
```

#### TTS Flow (Text-to-Speech)
```
sx_tts_speak(text, priority)
  └─> xQueueSend(s_tts_queue) [Priority queue]

sx_tts_task() [TTS Task]
  └─> xQueueReceive(s_tts_queue)
      └─> sx_tts_http_request(text)
          ├─> esp_http_client_perform() [HTTP GET/POST]
          └─> Parse response (audio data)
              └─> sx_audio_service_feed_pcm(audio_data)
                  └─> I2S pipeline
```

#### Wake Word Detection Flow
```
sx_wake_word_start(callback)
  ├─> sx_wake_word_register_callback_esp_sr()
  └─> xTaskCreate(sx_wake_word_task)

sx_audio_recording_task() [Recording Task]
  └─> sx_wake_word_feed_audio(pcm, samples)
      └─> xQueueSend(s_audio_queue) [Queue to wake word task]

sx_wake_word_task() [Wake Word Task]
  └─> xQueueReceive(s_audio_queue)
      └─> sx_wake_word_feed_audio_esp_sr() [ESP-SR Engine]
          └─> If detected: callback()
              └─> sx_dispatcher_post_event(SX_EVT_WAKE_WORD_DETECTED)
```

#### Chatbot Flow
```
sx_chatbot_handle_json_message(cJSON *root)
  ├─> Parse message type (stt, tts, mcp, system, alert)
  ├─> If MCP: sx_chatbot_handle_mcp_message()
  │   └─> sx_mcp_server_parse_message()
  │       └─> SX_PROTOCOL_SEND_MCP_MESSAGE()
  ├─> If STT: sx_stt_start_session()
  ├─> If TTS: sx_tts_speak()
  └─> If system: sx_dispatcher_post_event(SX_EVT_SYSTEM_*)

sx_chatbot_task() [Chatbot Task]
  └─> xQueueReceive(s_message_queue)
      └─> Process message (intent parsing, MCP, etc.)
```

#### Intent Parsing Flow
```
sx_intent_execute(text)
  └─> sx_intent_parse(text, &intent)
      └─> Keyword matching (contains_keyword, extract_entity)
          └─> Return intent type + entity

  └─> If handler registered: handler(&intent)
      Else: Default handler
          ├─> SX_INTENT_MUSIC_PLAY: sx_radio_play_station()
          ├─> SX_INTENT_VOLUME_UP/DOWN: sx_audio_set_volume()
          └─> SX_INTENT_WIFI_CONNECT: sx_wifi_connect()
```

### Audio Play → Decode → Feed → I2S

```
Service/UI
  └─> sx_audio_service_play() [Audio Service]
      └─> sx_audio_codec_open() [Codec]
      └─> sx_audio_router_set_source() [Router]
      └─> sx_dispatcher_post_event(SX_EVT_AUDIO_PLAYBACK_STARTED) [Dispatcher]

sx_audio_service_task() [Audio Task]
  └─> sx_audio_codec_read() [Codec]
      └─> Decode audio data
  └─> sx_audio_router_feed() [Router]
      └─> Apply EQ, ducking, volume
  └─> i2s_write() [I2S Driver]
      └─> Send to DAC/Amplifier

sx_orchestrator_task() [Orchestrator Task]
  └─> Process SX_EVT_AUDIO_PLAYBACK_STARTED
      └─> Update state.audio.playing = true
      └─> sx_dispatcher_set_state() [Dispatcher]

sx_ui_task() [UI Task]
  └─> Read state.audio.playing
      └─> Update UI (play/pause button, progress bar)
```

### Network Event → Dispatcher → Orchestrator → State

```
WiFi Service Task
  └─> esp_wifi_event_handler() [ESP-IDF]
      └─> sx_dispatcher_post_event(SX_EVT_WIFI_CONNECTED) [Dispatcher]

sx_orchestrator_task() [Orchestrator Task]
  └─> sx_dispatcher_poll_event() [Dispatcher]
      └─> Receive SX_EVT_WIFI_CONNECTED
  └─> sx_event_handler_process() [Event Handler]
      └─> sx_event_handler_wifi_connected() [Handler]
          └─> Update state.wifi.connected = true
          └─> Update state.wifi.rssi
          └─> Update state.wifi.ssid
  └─> sx_dispatcher_set_state() [Dispatcher]
      └─> Double-buffer swap

sx_ui_task() [UI Task]
  └─> sx_dispatcher_get_state() [Dispatcher]
      └─> Read state.wifi.connected
      └─> Update UI (WiFi icon, SSID display)
```

## Module Dependencies

### sx_core Dependencies

```
sx_core
  ├─> FreeRTOS (queues, tasks, mutexes)
  ├─> ESP-IDF (esp_log, esp_err)
  ├─> NVS (nvs_flash)
  └─> (No dependencies on other sx_* components)
```

### sx_platform Dependencies

```
sx_platform
  ├─> ESP-IDF (LCD driver, SPI, I2C, GPIO, LEDC)
  ├─> LVGL (display integration - indirect, qua UI)
  └─> (No dependencies on sx_core)
  
sx_platform Internal:
  ├─> sx_platform.c: Display + Touch initialization
  ├─> sx_spi_bus_manager.c: SPI bus mutex management
  └─> sx_platform_volume.c: Hardware volume control (I2C codec chips)
```

### sx_ui Dependencies

```
sx_ui
  ├─> sx_core (dispatcher, state, events)
  ├─> sx_platform (display, touch)
  ├─> sx_assets (images)
  ├─> LVGL (UI framework)
  └─> FreeRTOS (task)
  
sx_ui Internal:
  ├─> sx_ui_task.c: Main UI loop (state polling, LVGL rendering)
  ├─> ui_router.c: Navigation system (screen lifecycle)
  ├─> sx_screen_if.c: Screen interface registry (vtable pattern)
  ├─> ui_screen_registry.c: Screen callbacks registry
  ├─> sx_lvgl.h: LVGL wrapper (compile-time guard)
  ├─> sx_lvgl_lock.c: LVGL lock guard (RAII-style)
  └─> screens/: 29 screen implementations
```

### sx_services Dependencies

```
sx_services
  ├─> sx_core (dispatcher, events, state)
  ├─> sx_platform (SPI for SD, I2C for audio)
  ├─> ESP-IDF (WiFi, HTTP, MQTT, I2S, NVS, FAT)
  └─> Third-party (codecs, ESP-SR)
  
sx_services Storage:
  ├─> sx_settings_service: NVS wrapper (persistent config)
  ├─> sx_sd_service: SD card + FAT filesystem (SPI shared với LCD)
  ├─> sx_assets: Asset loader (RGB565 từ SD, embedded images)
  ├─> sx_playlist_manager: Playlist với shuffle/repeat/gapless
  └─> sx_media_metadata: ID3v2 (MP3) + Vorbis Comments (OGG/FLAC)
```

## Task Architecture

### FreeRTOS Tasks

```
Priority 8: sx_orchestrator_task
  └─> Single consumer của event queue
  └─> Single writer của state
  └─> Core: tskNO_AFFINITY

Priority 7: sx_ui_task
  └─> Multi-reader của state
  └─> LVGL rendering
  └─> Core: tskNO_AFFINITY

Priority 6: sx_audio_service_task
  └─> Audio processing
  └─> I2S feeding
  └─> Core: tskNO_AFFINITY

Priority 5: Service tasks (WiFi, Radio, Chatbot, etc.)
  └─> Various service-specific tasks
  └─> Core: tskNO_AFFINITY
```

### Task Communication

```
┌─────────────────┐
│  Service Tasks  │ (Multi-producer)
│  (WiFi, Audio,  │
│   Radio, etc.)  │
└────────┬────────┘
         │ sx_dispatcher_post_event()
         ▼
┌─────────────────┐
│  Event Queues    │ (4 priority queues)
│  (LOW, NORMAL,   │
│   HIGH, CRITICAL)│
└────────┬────────┘
         │ sx_dispatcher_poll_event()
         ▼
┌─────────────────┐
│ Orchestrator    │ (Single-consumer)
│ Task            │
└────────┬────────┘
         │ sx_dispatcher_set_state()
         ▼
┌─────────────────┐
│ State Buffers   │ (Double-buffer)
│ (Front/Back)    │
└────────┬────────┘
         │ sx_dispatcher_get_state()
         ▼
┌─────────────────┐
│  UI Task        │ (Multi-reader)
│  (LVGL render)  │
└─────────────────┘
```

## Platform Initialization Flow

### Display Initialization

```
sx_platform_display_init()
  ├─> LEDC PWM init (~1ms)
  │   └─> Backlight control (GPIO 42)
  ├─> SPI3 bus init (~10ms)
  │   └─> Shared với SD card (MISO pin 12)
  ├─> LCD panel IO init (~50ms)
  │   └─> SPI communication setup
  ├─> LCD panel driver init (~200ms) ⚠️ SLOW
  │   └─> Reset + init commands với delays
  └─> Display on (~20ms)
```

**Bottlenecks:**
- Panel init commands với delays: ~200ms
- Total display init: ~500ms

### Touch Initialization

```
sx_platform_touch_init()
  ├─> I2C bus 1 init (~10ms)
  │   └─> GPIO 8 (SDA), 11 (SCL), 400kHz
  ├─> Touch panel IO init (~50ms)
  │   └─> I2C communication setup
  └─> FT5x06 touch driver init (~50ms)
      └─> Reset + calibration
```

**Bottlenecks:**
- I2C bus init: ~10ms
- Touch driver init: ~50ms
- Total touch init: ~100ms

### SPI Bus Sharing

```
SPI3 Bus (Shared)
  ├─> LCD (Primary user)
  │   └─> Continuous display updates
  └─> SD Card (Secondary user)
      └─> File I/O operations
      
Access Control:
  ├─> sx_spi_bus_lock() [Mutex]
  ├─> SPI operation
  └─> sx_spi_bus_unlock() [Mutex]
```

**⚠️ RISK**: Nếu LCD và SD card access đồng thời, cần mutex để sync.

## Critical Paths

### Boot Time Critical Path

```
app_main()
  └─> sx_bootstrap_start()
      ├─> nvs_flash_init() [~100ms]
      ├─> sx_dispatcher_init() [~1ms]
      ├─> sx_orchestrator_start() [~1ms]
      ├─> sx_platform_display_init() [~500ms] ⚠️ SLOW
      ├─> sx_platform_touch_init() [~100ms]
      ├─> sx_sd_service_init() [~2000ms] ⚠️ VERY SLOW (if SD card slow)
      └─> sx_ui_start() [~50ms]
```

**Bottlenecks:**
- Display init: ~500ms
- SD card mount: ~2000ms (nếu SD card chậm)

### Event Processing Critical Path

```
Event Posted
  └─> sx_dispatcher_post_event() [~1μs]
      └─> xQueueSend() [~10μs]
          └─> sx_orchestrator_task() wakes up [~100μs]
              └─> sx_dispatcher_poll_event() [~1μs]
                  └─> sx_event_handler_process() [~1-10ms] ⚠️ VARIES
                      └─> Handler function [~1-100ms] ⚠️ VARIES
                          └─> sx_dispatcher_set_state() [~10μs]
                              └─> UI task reads state [~1ms]
```

**Bottlenecks:**
- Event handler execution time: 1-100ms (tùy handler)
- State update frequency: 10ms polling interval

### Audio Playback Critical Path

```
Audio Service Play
  └─> Codec open [~10ms]
      └─> Audio task starts [~1ms]
          └─> Codec read [~10ms per chunk]
              └─> Router feed [~1ms]
                  └─> I2S write [~5ms]
                      └─> DAC output [Real-time]
```

**Bottlenecks:**
- Codec decode time: ~10ms per chunk
- I2S buffer size: Phải đủ lớn để tránh underrun

## Audio Pipeline Flow

### Playback Pipeline
```
App -> sx_audio_router_route_audio(SD_MUSIC, pcm)  # source = SD_MUSIC
  ├─> Router consults route table (default sink = I2S)
  └─> sx_audio_service_feed_pcm(pcm)
        ├─> EQ: sx_audio_eq_process()
        ├─> Crossfade: sx_audio_crossfade_process()
        ├─> Volume: logarithmic scaling (software) / HW volume
        └─> I2S Driver write (DMA)
```

### Ducking Interaction
```
TTS starts
  └─> sx_audio_duck()            # fade volume -30 dB (duck_level=0.3)
TTS finishes
  └─> sx_audio_restore()         # fade back to original volume
```

### Storage Flow

#### SD Card Mount Flow
```
sx_sd_service_init(cfg)
  └─> Store config (mount_point, SPI host, GPIO pins)

sx_sd_service_start()
  ├─> sx_spi_bus_lock() [Acquire SPI bus]
  ├─> gpio_set_direction(CS, OUTPUT) [Configure CS pin]
  ├─> esp_vfs_fat_sdspi_mount() [Mount FAT filesystem]
  │   └─> SDSPI host init + card detection
  ├─> sx_spi_bus_unlock() [Release SPI bus]
  └─> sx_assets_set_sd_ready(true) [Notify assets service]
```

#### Settings Read/Write Flow
```
sx_settings_set_string(key, value)
  └─> nvs_set_str(s_nvs_handle, key, value) [NVS write]

sx_settings_commit()
  └─> nvs_commit(s_nvs_handle) [Flush to flash]

sx_settings_get_string(key, value, max_len)
  └─> nvs_get_str(s_nvs_handle, key, value, &size) [NVS read]
```

#### Playlist Navigation Flow
```
sx_playlist_next()
  ├─> xSemaphoreTake(s_playlist_mutex) [Lock]
  ├─> get_next_index() [Calculate next index]
  │   ├─> If shuffle: rand() % track_count
  │   ├─> If repeat_one: current_index
  │   └─> Else: current_index + 1 (with repeat_all check)
  ├─> Update current_index
  ├─> xSemaphoreGive(s_playlist_mutex) [Unlock]
  └─> sx_audio_play_file(track_path) [Play track]

sx_playlist_preload_next()
  ├─> Calculate next track index
  ├─> malloc(preloaded_track_path) [Allocate path]
  └─> Store preloaded track path [For gapless playback]
```

#### Metadata Parsing Flow
```
sx_meta_parse_file(file_path, &meta)
  ├─> fopen(file_path, "rb") [Open file]
  ├─> Try parse_id3v2() [MP3]
  │   ├─> Read ID3v2 header ("ID3")
  │   ├─> Read tag data (sync-safe size)
  │   └─> Parse frames (TIT2, TPE1, TCON, TLEN)
  ├─> If failed: Try parse_vorbis_comment() [OGG/FLAC]
  │   ├─> Check "fLaC" or "OggS" header
  │   ├─> Find Vorbis comment block (FLAC type 4, OGG page)
  │   └─> Parse "TAG=value" comments
  └─> fclose() [Close file]
```

### Sample Rate Change Flow
```
New PCM chunk with sample_rate != current
  └─> sx_audio_service_feed_pcm()
        ├─> Reconfigure I2S (blocking)
        ├─> sx_audio_eq_set_sample_rate()
        └─> sx_audio_crossfade_set_sample_rate()
```

## UI Rendering Flow

### UI Task Main Loop

```
sx_ui_task() [UI Task]
  ├─> sx_dispatcher_get_state() [Lock-free read]
  │   └─> Copy state snapshot
  ├─> ui_router_get_current_screen() [Get active screen]
  ├─> ui_screen_registry_get() [Get screen callbacks]
  ├─> callbacks->on_update() [Update UI from state]
  │   └─> lvgl_port_lock() [Protect LVGL operations]
  ├─> sx_dispatcher_poll_event() [Poll display events]
  │   └─> screen_display_helper_handle_event() [Handle display events]
  ├─> lv_timer_handler() [LVGL timer processing]
  │   └─> lvgl_port_lock() [Protect LVGL operations]
  └─> vTaskDelayUntil() [16ms interval, ~60 FPS]
```

**Frame Rate**: 60 FPS (16ms interval)
**State Polling**: Mỗi frame (lock-free read)
**LVGL Rendering**: Protected bởi lvgl_port_lock()

### Screen Navigation Flow

```
User Action / Timer / Event
  └─> ui_router_navigate_to(screen_id)
      └─> lvgl_port_lock() [Acquire LVGL lock]
          ├─> old_callbacks->on_hide() [Hide old screen]
          ├─> lv_obj_clean(container) [Clear container]
          ├─> old_callbacks->on_destroy() [Destroy old screen]
          ├─> new_callbacks->on_create() [Create new screen]
          └─> new_callbacks->on_show() [Show new screen]
      └─> lvgl_port_unlock() [Release LVGL lock]
```

**Navigation Sequence**: hide → destroy → create → show
**Thread Safety**: Navigation phải được gọi từ UI task hoặc với LVGL lock

### UI Input → Event → State → UI Update

```
User Input (Touch/Button)
  └─> LVGL Event Callback [UI Task]
      └─> sx_dispatcher_post_event(SX_EVT_UI_INPUT) [Dispatcher]
          └─> xQueueSend(s_evt_q_normal, evt) [FreeRTOS Queue]

sx_orchestrator_task() [Orchestrator Task]
  └─> sx_dispatcher_poll_event() [Dispatcher]
      └─> sx_event_handler_process() [Event Handler]
          └─> sx_event_handler_ui_input() [Handler]
              └─> Update state.ui.last_user_message
              └─> sx_dispatcher_set_state() [Dispatcher]

sx_ui_task() [UI Task]
  └─> sx_dispatcher_get_state() [Lock-free read]
      └─> callbacks->on_update() [Screen update callback]
          └─> Update UI widgets based on state
```

## Data Flow

### State Update Flow

```
Orchestrator Task
  └─> Process event
      └─> Modify local state copy
          └─> sx_dispatcher_set_state()
              ├─> Take mutex [s_state_write_mutex]
              ├─> Write to back buffer
              ├─> Atomic swap pointer [s_state_read_ptr]
              └─> Release mutex

UI Task (or any reader)
  └─> sx_dispatcher_get_state()
      └─> Read atomic pointer [s_state_read_ptr]
          └─> Copy state snapshot [Lock-free]
              └─> Use state for rendering
```

### Event Payload Flow

```
Service (Producer)
  └─> Allocate payload (string pool or malloc)
      └─> Create event with ptr = payload
          └─> sx_dispatcher_post_event()
              └─> Copy event struct to queue
                  └─> (Payload pointer copied, not payload itself)

Orchestrator Task (Consumer)
  └─> sx_dispatcher_poll_event()
      └─> Receive event struct
          └─> Call event handler with event.ptr
              └─> Handler uses payload
                  └─> (Payload still owned by producer)

Producer
  └─> Free payload after event posted
      └─> (⚠️ RISK: Payload may still be in queue)
```

**⚠️ RISK**: Nếu producer free payload ngay sau post, consumer có thể đọc invalid pointer.

**Giải pháp**: Dùng string pool hoặc đảm bảo payload lifetime đủ dài.

## Circular Dependencies

### Không Có Circular Dependencies

```
sx_core
  └─> (No dependencies on other sx_*)

sx_platform
  └─> (No dependencies on sx_core)

sx_ui
  ├─> sx_core (dispatcher, state)
  └─> sx_platform (display, touch)

sx_services
  ├─> sx_core (dispatcher, events)
  └─> sx_platform (SPI, I2C)
```

**Kiến trúc tốt**: Không có circular dependencies, dependencies chỉ đi một chiều từ services → core → platform.

## Performance Metrics

### Event Processing

- **Event post latency**: ~1-10μs (tùy queue depth)
- **Event processing latency**: ~1-100ms (tùy handler)
- **State update latency**: ~10μs
- **State read latency**: ~1μs (lock-free)

### Memory Usage

- **Event queues**: ~4KB (4 queues × ~1KB each)
- **State buffers**: ~1KB (2 buffers × ~500B each)
- **Coalesce table**: ~2KB (32 entries × ~64B each)
- **Service registry**: ~1KB (32 services × ~32B each)

### CPU Usage

- **Orchestrator task**: ~1-5% (10ms polling, ~1ms processing)
- **UI task**: ~10-20% (LVGL rendering)
- **Audio task**: ~5-10% (codec decode + I2S feed)

---

**Lưu ý**: Call graph này dựa trên Batch 1 (sx_core). Sẽ được cập nhật khi hoàn thành các batches khác.


