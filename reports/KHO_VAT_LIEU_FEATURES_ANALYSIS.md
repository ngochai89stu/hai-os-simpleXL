# Phân tích sâu toàn diện tính năng trong Kho vật liệu (xiaozhi-esp32)

## Tổng quan

**Kho vật liệu** (`D:\NEWESP32\xiaozhi-esp32`): Hệ thống AI voice assistant với kiến trúc service-based phức tạp, modular và nhiều tính năng nâng cao.

**Mục đích phân tích**: Liệt kê và phân tích tất cả các tính năng, bao gồm cả các tính năng chưa hoàn thành, để làm tài liệu tham khảo cho việc tích hợp vào SimpleXL.

---

## 1. KIẾN TRÚC TỔNG THỂ

### 1.1 Service-Based Architecture ✅
- **ServiceBase**: Abstract base class cho tất cả services
  - Lifecycle: `Initialize()`, `Start()`, `Stop()`, `Cleanup()`
  - State management: `UNINITIALIZED`, `INITIALIZING`, `INITIALIZED`, `STARTING`, `RUNNING`, `STOPPING`, `STOPPED`, `ERROR`
  - Dependencies: `GetDependencies()` - declare service dependencies
  - Event-driven: Publish events qua EventBus

### 1.2 Service Manager ✅
- **ServiceManager**: Quản lý lifecycle của tất cả services
  - Dependency resolution
  - Startup sequence
  - Shutdown sequence
  - Service registry

### 1.3 Event Bus System ✅
- **EventBus**: Central event system
  - Publish/Subscribe pattern
  - Event types: Service events, UI events, Audio events, etc.
  - Thread-safe event delivery

### 1.4 Service Ready Gate ✅
- **ServiceReadyGate**: Đảm bảo services sẵn sàng trước khi sử dụng
  - Wait for service ready
  - Service health checks

---

## 2. AUDIO SERVICES - Hệ thống Audio hoàn chỉnh

### 2.1 AudioService (Core) ✅
**File**: `main/audio/audio_service.h`, `audio_service.cc`

**Tính năng:**
- ✅ Core audio service kế thừa từ `ServiceBase`
- ✅ Lifecycle management
- ✅ Event publishing
- ✅ Integration với các audio sub-services

**Trạng thái**: Hoàn chỉnh

---

### 2.2 AudioEncoderService ✅
**File**: `main/audio/audio_encoder_service.h`, `audio_encoder_service.cc`

**Tính năng:**
- ✅ Opus encoder wrapper
- ✅ Queue management (BASE_MAX_ENCODE_TASKS_IN_QUEUE = 12)
- ✅ Encoding task management
- ✅ Adaptive buffer sizing
- ✅ Queue overflow warning (QUEUE_OVERFLOW_WARNING_THRESHOLD = 0.8)

**Trạng thái**: Hoàn chỉnh

---

### 2.3 AudioDecoderService ✅
**File**: `main/audio/audio_decoder_service.h`, `audio_decoder_service.cc`

**Tính năng:**
- ✅ Opus decoder wrapper
- ✅ Multi-codec support (Opus, AAC, FLAC)
- ✅ Smart codec engine (auto-detect codec type)
- ✅ Queue management (BASE_MAX_DECODE_PACKETS_IN_QUEUE = 45)
- ✅ Fallback mechanism
- ✅ Buffer underrun protection (BUFFER_UNDERRUN_THRESHOLD = 3)

**Trạng thái**: Hoàn chỉnh

---

### 2.4 AudioInputService ✅
**File**: `main/audio/audio_input_service.h`, `audio_input_service.cc`

**Tính năng:**
- ✅ I2S input channel management
- ✅ Audio recording
- ✅ VAD (Voice Activity Detection) integration
- ✅ AEC (Acoustic Echo Cancellation) support
- ✅ Audio processor integration

**Trạng thái**: Hoàn chỉnh

---

### 2.5 AudioOutputService ✅
**File**: `main/audio/audio_output_service.h`, `audio_output_service.cc`

**Tính năng:**
- ✅ I2S output channel management
- ✅ Playback queue management (BASE_MAX_PLAYBACK_TASKS_IN_QUEUE = 15)
- ✅ Audio output routing
- ✅ Duplex I2S support

**Trạng thái**: Hoàn chỉnh

---

### 2.6 AudioPlaybackService ✅
**File**: `main/audio/audio_playback_service.h`, `audio_playback_service.cc`

**Tính năng:**
- ✅ Playback control (play, pause, stop)
- ✅ Track management
- ✅ Playlist support
- ✅ Gapless playback
- ✅ Crossfade support

**Trạng thái**: Hoàn chỉnh

---

### 2.7 AudioVolumeService ✅
**File**: `main/audio/audio_volume_service.h`, `audio_volume_service.cc`

**Tính năng:**
- ✅ Smooth volume control (logarithmic scaling)
- ✅ Volume ramp (configurable duration, default 200ms)
- ✅ Hardware volume support
- ✅ Software volume fallback

**Trạng thái**: Hoàn chỉnh

---

### 2.8 AudioEffectsService ✅
**File**: `main/audio/audio_effects_service.h`, `audio_effects_service.cc`

**Tính năng:**
- ✅ 10-band equalizer (-12dB to +12dB)
- ✅ EQ presets (Flat, Pop, Rock, Jazz, Classical)
- ✅ `SetEQPreset()` và `GetCurrentEQPreset()`
- ✅ `SetEqualizer()` - Set 10-band gains
- ✅ `GetEqualizer()` - Get current EQ gains
- ✅ EQ Preset Manager (`eq_preset_manager.h/c`)

**Trạng thái**: Hoàn chỉnh

---

### 2.9 WakeWordService ✅
**File**: `main/audio/wake_word_service.h`, `wake_word_service.cc`

**Tính năng:**
- ✅ Multiple wake word types:
  - `AfeWakeWord` (ESP-SR)
  - `CustomWakeWord`
  - `EspWakeWord` (fallback)
- ✅ Wake word encoding (Opus packets)
- ✅ `GetWakeWordOpus()` - Lấy wake word Opus packets
- ✅ `IsAfeWakeWord()` - Check wake word type
- ✅ Static task buffer cho wake word encode task

**Trạng thái**: Hoàn chỉnh

---

### 2.10 RadioService ✅
**File**: `main/audio/radio_service.h`, `radio_service.cc`

**Tính năng:**
- ✅ Radio streaming service
- ✅ ICY metadata processing (`radio_service_metadata.cc`)
- ✅ Event-driven radio service (`radio_service_events.cc`)
- ✅ Auto-reconnect logic (`radio_service_reconnect.cc`)
- ✅ Stream management (`radio_service_stream.cc`)
- ✅ AAC decoder support

**Trạng thái**: Hoàn chỉnh

---

### 2.11 SDPlaybackService ✅
**File**: `main/audio/sd_playback_service.h`, `sd_playback_service.cc`

**Tính năng:**
- ✅ SD card audio playback
- ✅ Audio format detection (MP3, AAC, FLAC, WAV, Opus)
- ✅ Multi-codec support
- ✅ File-based playback

**Trạng thái**: Hoàn chỉnh (nhưng có thể thiếu ID3 tags, genre playlist như Esp32SdMusic)

---

### 2.12 Audio Recovery & Monitoring ⚠️
**File**: `main/audio/audio_service_recovery.cc`

**Tính năng:**
- ✅ AudioRecoveryManager: Quản lý recovery từ underflow/overflow
- ✅ AudioStreamMonitor: Monitor audio stream health
- ✅ Buffer underrun detection và recovery
- ✅ `PausePlaybackForRecovery()` - Pause để recovery
- ✅ `ResumePlaybackAfterRecovery()` - Resume sau recovery
- ✅ `RefillPlaybackBuffer()` - Refill buffer khi cần
- ✅ `DetectAndRecoverUnderflow()` - Auto-detect và recover
- ✅ `HandleBufferUnderrun()` - Handle underrun events

**Trạng thái**: Hoàn chỉnh

---

### 2.13 Audio Ducking Manager ✅
**Tính năng:**
- ✅ AudioDuckingManager: Service riêng để quản lý ducking
- ✅ Duck Audio: Lower volume khi Assistant nói
- ✅ Restore Audio: Restore volume sau khi Assistant nói xong
- ✅ Duck Level: Configurable duck level (0.0 = mute, 1.0 = no ducking)
- ✅ Deprecated methods trong AudioService (dùng AudioDuckingManager)

**Trạng thái**: Hoàn chỉnh

---

### 2.14 Crossfade Engine ✅
**Tính năng:**
- ✅ AudioCrossfadeEngine: Service riêng để quản lý crossfade
- ✅ `StartCrossfade()` - Crossfade giữa các tracks (default 500ms)
- ✅ Gapless Playback:
  - `SetGaplessPlayback()` - Enable/disable gapless
  - `PreloadNextTrack()` - Preload track tiếp theo
  - `GaplessState` - State machine cho gapless transitions
- ✅ `ProcessCrossfade()` - Xử lý crossfade trong output task
- ✅ `ProcessGaplessTransition()` - Xử lý gapless transition

**Trạng thái**: Hoàn chỉnh

---

### 2.15 Audio Power Management ✅
**Tính năng:**
- ✅ AudioPowerManager: Service riêng để quản lý power
- ✅ Power Save Mode: Auto power save khi idle
- ✅ Power Timer: ESP timer để check power state
- ✅ `CheckAndUpdateAudioPowerState()` - Check và update power state
- ✅ `AUDIO_POWER_TIMEOUT_MS`: 15000ms timeout
- ✅ `AUDIO_POWER_CHECK_INTERVAL_MS`: 1000ms check interval

**Trạng thái**: Hoàn chỉnh

---

### 2.16 Audio Buffer Management ✅
**Tính năng:**
- ✅ AudioBufferPool: Thread-safe buffer pool
- ✅ AudioBufferAllocator: Buffer allocation management
- ✅ AudioChunkManager: Quản lý audio chunks
- ✅ PSRAM Buffer Helper: Helper để allocate buffers trong PSRAM
- ✅ Buffer Monitoring: Monitor buffer usage

**Trạng thái**: Hoàn chỉnh

---

### 2.17 Audio Pipeline Profiler ✅
**Tính năng:**
- ✅ AudioPipelineProfiler: Profile audio pipeline performance
- ✅ Performance Metrics: Track encoding/decoding time
- ✅ Bottleneck Detection: Detect bottlenecks trong pipeline

**Trạng thái**: Hoàn chỉnh

---

### 2.18 Audio Test Framework ✅
**Tính năng:**
- ✅ AudioTestFramework: Framework để test audio features
- ✅ `TestCrossfadeOpus()` - Test crossfade với Opus
- ✅ Test Results: Structured test results

**Trạng thái**: Hoàn chỉnh

---

### 2.19 TTS & Voice ✅
**Tính năng:**
- ✅ TTS Double Buffer: Double buffer cho TTS playback
- ✅ `SpeakText()` - Text-to-speech với priority
- ✅ Voice State Machine: State machine cho voice interactions
- ✅ Navigation Voice Guidance: Support navigation voice với priority

**Trạng thái**: Hoàn chỉnh

---

### 2.20 OGG Parser ✅
**Tính năng:**
- ✅ OggParser: Parse OGG/Opus files
- ✅ `ParseOpusHead()` - Parse OpusHead packet
- ✅ `ParseOpusTags()` - Parse OpusTags packet
- ✅ Extract Opus Packets: Extract Opus packets từ OGG stream

**Trạng thái**: Hoàn chỉnh

---

### 2.21 Audio Router & External Bridge ✅
**Tính năng:**
- ✅ AudioRouter: Route audio giữa các sources
- ✅ ExternalAudioBridge: Bridge cho external audio sources

**Trạng thái**: Hoàn chỉnh

---

### 2.22 Frequency Analyzer ✅
**Tính năng:**
- ✅ FrequencyAnalyzer: Analyze audio frequency spectrum
- ✅ FFT Support: FFT cho spectrum analysis

**Trạng thái**: Hoàn chỉnh

---

### 2.23 Music Library & Playlist ✅
**Tính năng:**
- ✅ MusicLibrary: Quản lý music library
- ✅ PlaylistManager: Quản lý playlists
- ✅ Playlist: Playlist data structure

**Trạng thái**: Hoàn chỉnh (nhưng có thể thiếu ID3 tags, genre như Esp32SdMusic)

---

## 3. SYSTEM SERVICES

### 3.1 ServiceManager ✅
**File**: `main/system/service_manager.h`, `service_manager.cc`

**Tính năng:**
- ✅ Service registry
- ✅ Dependency resolution
- ✅ Startup/shutdown sequence
- ✅ Service lifecycle management

**Trạng thái**: Hoàn chỉnh

---

### 3.2 DiagnosticsService ✅
**File**: `main/system/diagnostics_service.h`, `diagnostics_service.cc`

**Tính năng:**
- ✅ System diagnostics aggregation
- ✅ CPU usage monitoring (`diagnostics_service_monitor.cc`)
- ✅ Memory usage tracking
- ✅ Audio underrun detection
- ✅ WiFi status monitoring
- ✅ System health reporting
- ✅ Event publishing (`diagnostics_service_events.cc`)
- ✅ History tracking (`diagnostics_service_history.cc`)
- ✅ Health checks (`diagnostics_service_health.cc`)

**Trạng thái**: Hoàn chỉnh

---

### 3.3 PermissionService ✅
**File**: `main/system/permission_service.h`, `permission_service.cc`

**Tính năng:**
- ✅ Service capabilities management
- ✅ Permission enforcement
- ✅ Capability checks

**Trạng thái**: Hoàn chỉnh

---

### 3.4 PluginService ✅
**File**: `main/system/plugin_service.h`, `plugin_service.cc`

**Tính năng:**
- ✅ Dynamic skill runtime
- ✅ Load skill packages (JSON/configuration)
- ✅ Register new intents → action graph
- ✅ Backward compatible với static skill set

**Trạng thái**: Hoàn chỉnh

---

### 3.5 SnapshotService ✅
**File**: `main/system/snapshot_service.h`, `snapshot_service.cc`

**Tính năng:**
- ✅ Persistent snapshot và resume
- ✅ UI state snapshot (khi legacy UI stack built)
- ✅ System state serialization
- ✅ Service states serialization

**Trạng thái**: Hoàn chỉnh

---

## 4. NETWORK SERVICES

### 4.1 WiFiService ✅
**File**: `main/services/wifi_service.h`, `wifi_service.cc`

**Tính năng:**
- ✅ WiFi connection management
- ✅ Network scanning
- ✅ Connection status
- ✅ Auto-reconnect

**Trạng thái**: Hoàn chỉnh

---

### 4.2 NetworkService ✅
**File**: `main/services/network_service.h`, `network_service.cc`

**Tính năng:**
- ✅ Network management
- ✅ Network status
- ✅ Network configuration

**Trạng thái**: Hoàn chỉnh

---

### 4.3 ProvisioningService ✅
**File**: `main/services/provisioning_service.h`, `provisioning_service.cc`

**Tính năng:**
- ✅ WiFi provisioning với retry logic
- ✅ Exponential backoff
- ✅ Timeout handling
- ✅ Recovery mechanism
- ✅ EventBus integration
- ✅ Fail-safe provisioning
- ✅ AP mode support
- ✅ MCP command support
- ✅ Acoustic provisioning (nếu enabled)

**Trạng thái**: Hoàn chỉnh

---

## 5. PROTOCOL SERVICES

### 5.1 MCPService ✅
**File**: `main/services/mcp_service.h`, `mcp_service.cc`

**Tính năng:**
- ✅ MCP (Model Context Protocol) server
- ✅ JSON-RPC 2.0 parsing
- ✅ Tool registration
- ✅ Response generation
- ✅ WebSocket/MQTT integration

**Trạng thái**: Hoàn chỉnh

---

## 6. UI SERVICES

### 6.1 UIService ✅
**File**: `main/services/ui_service.h`, `ui_service.cc`

**Tính năng:**
- ✅ UI state management
- ✅ Screen management
- ✅ UI events

**Trạng thái**: Hoàn chỉnh

---

### 6.2 UIControlService ✅
**File**: `main/services/ui_control/ui_control_service.h`, `ui_control_service.cc`

**Tính năng:**
- ✅ UI control commands
- ✅ UI state synchronization

**Trạng thái**: Hoàn chỉnh

---

## 7. NAVIGATION SERVICES

### 7.1 NavigationService ✅
**File**: `main/navigation/navigation_service.h`, `navigation_service.cc`

**Tính năng:**
- ✅ Navigation route management (`navigation_service_route.cc`)
- ✅ Navigation state (`navigation_service_state.cc`)
- ✅ Navigation events (`navigation_service_events.cc`)
- ✅ Voice guidance
- ✅ Turn-by-turn directions

**Trạng thái**: Hoàn chỉnh

---

## 8. OTHER SERVICES

### 8.1 ChatbotService ✅
**File**: `main/services/chatbot_service.h`, `chatbot_service.cc`

**Tính năng:**
- ✅ Chatbot request/response handling
- ✅ MCP integration
- ✅ Intent parsing integration

**Trạng thái**: Hoàn chỉnh

---

### 8.2 OTAService ✅
**File**: `main/services/ota_service.h`, `ota_service.cc`

**Tính năng:**
- ✅ OTA firmware updates
- ✅ Progress tracking
- ✅ Error handling

**Trạng thái**: Hoàn chỉnh

---

### 8.3 AssetsService ✅
**File**: `main/services/assets_service.h`, `assets_service.cc`

**Tính năng:**
- ✅ Assets management
- ✅ Assets download
- ✅ Assets versioning

**Trạng thái**: Hoàn chỉnh

---

### 8.4 SDService ✅
**File**: `main/services/sd_service.h`, `sd_service.cc`

**Tính năng:**
- ✅ SD card management
- ✅ File operations
- ✅ Mount/unmount

**Trạng thái**: Hoàn chỉnh

---

### 8.5 BluetoothService ✅
**File**: `main/services/bluetooth_service.h`, `bluetooth_service.cc`

**Tính năng:**
- ✅ Bluetooth management
- ✅ Bluetooth pairing
- ✅ Bluetooth audio

**Trạng thái**: Hoàn chỉnh (cần verify)

---

### 8.6 TelegramService ✅
**File**: `main/services/telegram_service.h`, `telegram_service.cc`

**Tính năng:**
- ✅ Telegram bot integration
- ✅ Send messages to Telegram
- ✅ Configuration management

**Trạng thái**: Hoàn chỉnh

---

### 8.7 StateSyncService ✅
**File**: `main/services/state_sync_service.h`, `state_sync_service.cc`

**Tính năng:**
- ✅ UI state synchronization
- ✅ External consumer registration
- ✅ State snapshot publishing
- ✅ Remote state push/pull
- ✅ State diff merging

**Trạng thái**: Hoàn chỉnh

---

### 8.8 BLEHubService ✅
**File**: `main/ble/ble_hub_service.h`, `ble_hub_service.cc`

**Tính năng:**
- ✅ BLE hub management
- ✅ BLE device connection
- ✅ BLE data transfer

**Trạng thái**: Hoàn chỉnh (cần verify)

---

### 8.9 MainLoopService ✅
**File**: `main/services/main_loop_service.h`, `main_loop_service.cc`

**Tính năng:**
- ✅ Main event loop management
- ✅ Task scheduling
- ✅ Event processing

**Trạng thái**: Hoàn chỉnh

---

## 9. TÍNH NĂNG CHƯA HOÀN THÀNH / CẦN CẢI THIỆN

### 9.1 SD Music - ID3 Tags & Genre Playlist ⚠️
**Vấn đề**: SDPlaybackService có thể thiếu các tính năng nâng cao như:
- ❌ ID3v1 + ID3v2 tag parsing
- ❌ Genre-based playlist
- ❌ Track suggestions (based on history)
- ❌ Pagination (list tracks by page)
- ❌ Case-insensitive search
- ❌ UTF-8 support
- ❌ Cover art metadata

**Ghi chú**: Esp32SdMusic trong repo mẫu có đầy đủ các tính năng này.

**Trạng thái**: Cần cải thiện

---

### 9.2 Audio Effects - Advanced Presets ⚠️
**Vấn đề**: Có thể thiếu:
- ❌ Custom EQ presets (user-defined)
- ❌ EQ preset import/export
- ❌ Real-time EQ adjustment

**Trạng thái**: Cần verify

---

### 9.3 Radio Service - Advanced Features ⚠️
**Vấn đề**: Có thể thiếu:
- ❌ Display modes (spectrum/info)
- ❌ Per-station volume amplification
- ❌ Better buffer management (dynamic sizing)

**Trạng thái**: Cần verify

---

### 9.4 Multi-codec Support - HE-AAC v2 ⚠️
**Vấn đề**: 
- ❌ HE-AAC v2 support chưa verify
- ❌ Cần test với HE-AAC v2 streams

**Trạng thái**: Cần verify

---

### 9.5 Audio Ducking - Advanced ⚠️
**Vấn đề**: Có thể thiếu:
- ❌ Per-source ducking levels
- ❌ Ducking profiles
- ❌ Smooth ducking transitions

**Trạng thái**: Cần verify

---

### 9.6 Crossfade - Advanced ⚠️
**Vấn đề**: Có thể thiếu:
- ❌ Configurable crossfade curves
- ❌ Per-track crossfade settings
- ❌ Crossfade preview

**Trạng thái**: Cần verify

---

### 9.7 TTS - Advanced ⚠️
**Vấn đề**: Có thể thiếu:
- ❌ Multiple TTS voices
- ❌ TTS language selection
- ❌ TTS speed/pitch control

**Trạng thái**: Cần verify

---

### 9.8 Navigation - Advanced ⚠️
**Vấn đề**: Có thể thiếu:
- ❌ Offline maps support
- ❌ Route alternatives
- ❌ Traffic information
- ❌ POI (Points of Interest) search

**Trạng thái**: Cần verify

---

## 10. TỔNG KẾT

### 10.1 Tính năng đã hoàn thành ✅
- **Audio Services**: 23 services/tính năng
- **System Services**: 5 services
- **Network Services**: 3 services
- **Protocol Services**: 1 service
- **UI Services**: 2 services
- **Navigation Services**: 1 service
- **Other Services**: 9 services

**Tổng**: ~44 services/tính năng chính

### 10.2 Tính năng cần cải thiện/verify ⚠️
- SD Music ID3 tags & genre playlist
- Audio Effects advanced presets
- Radio Service advanced features
- Multi-codec HE-AAC v2
- Audio Ducking advanced
- Crossfade advanced
- TTS advanced
- Navigation advanced

**Tổng**: ~8 tính năng cần cải thiện/verify

---

## 11. KHUYẾN NGHỊ CHO SIMPLEXL

### 11.1 Tính năng nên tích hợp từ Kho vật liệu:
1. ✅ **Audio Effects & EQ** (10-band EQ với presets)
2. ✅ **Audio Ducking Manager**
3. ✅ **Crossfade Engine**
4. ✅ **Gapless Playback**
5. ✅ **Audio Recovery Manager**
6. ✅ **Audio Stream Monitor**
7. ✅ **Smooth Volume Control**
8. ✅ **Multi-codec Support** (AAC, FLAC ngoài Opus)
9. ✅ **Smart Codec Engine** (auto-detect codec)
10. ✅ **Audio Buffer Pool**
11. ✅ **Frequency Analyzer** (FFT)
12. ✅ **Audio Pipeline Profiler** (optional, cho debugging)
13. ✅ **Service-Based Architecture** (nếu phù hợp với SimpleXL)

### 11.2 Tính năng nên lấy từ Repo mẫu:
1. ✅ **Esp32SdMusic features** (ID3 tags, genre playlist, suggestions, pagination)
   - Nhưng convert sang C và tích hợp vào SimpleXL architecture

### 11.3 Tính năng cần phát triển riêng:
1. ✅ **SimpleXL-specific integrations**
2. ✅ **C API wrappers** cho các tính năng từ kho vật liệu
3. ✅ **Component structure** phù hợp với SimpleXL

---

## 12. LƯU Ý QUAN TRỌNG

1. **Kiến trúc SimpleXL**: Tuyệt đối không phá vỡ kiến trúc hiện tại
2. **C API**: Kho vật liệu dùng C++, SimpleXL dùng C - cần convert
3. **Component Structure**: Giữ nguyên cấu trúc component của SimpleXL
4. **Event System**: Sử dụng sx_dispatcher và sx_event hiện có
5. **State Management**: Sử dụng sx_state_t hiện có
6. **UI System**: Sử dụng ui_router và ui_screen_registry hiện có

---

## 13. KẾ HOẠCH TÍCH HỢP

### Phase 1: Core Audio Features (P0)
1. Audio Effects & EQ
2. Audio Ducking Manager
3. Smooth Volume Control
4. Multi-codec Support (AAC, FLAC)

### Phase 2: Advanced Audio Features (P1)
5. Crossfade Engine
6. Gapless Playback
7. Audio Recovery Manager
8. Frequency Analyzer (FFT)

### Phase 3: System Features (P2)
9. Service-Based Architecture (nếu cần)
10. Audio Buffer Pool
11. Audio Pipeline Profiler (optional)

---

**Cập nhật lần cuối**: 2024-12-19
**Người phân tích**: AI Assistant
**Trạng thái**: Hoàn chỉnh
