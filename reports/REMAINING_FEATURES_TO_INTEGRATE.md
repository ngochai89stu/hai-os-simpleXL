# Danh sách tính năng còn lại cần tích hợp

## Tổng quan
Tài liệu này tổng hợp tất cả các tính năng từ:
- **Repo mẫu** (`xiaozhi-esp32_vietnam_ref`): C++ implementation với nhiều tính năng
- **Kho vật liệu** (`xiaozhi-esp32`): Service-based architecture với 44+ services
- **SimpleXL hiện tại**: Đã có một số tính năng cơ bản

---

## ✅ ĐÃ TÍCH HỢP (Từ các session trước)

1. ✅ **EQ API** - 10-band equalizer với presets và persistence
2. ✅ **Weather Service** - OpenWeatherMap API integration
3. ✅ **PWM Brightness Control** - LEDC PWM cho backlight
4. ✅ **Weather UI** - Widget trên home screen
5. ✅ **SD Music Improvements** - ID3v1/v2 parsing, genre playlists, search
6. ✅ **Music Online** - Lyrics download, display modes
7. ✅ **Radio Improvements** - Display modes, buffer management

---

## 🚧 P0 - CORE FEATURES (Ưu tiên cao - Cần thiết cho MVP)

### 1. Audio Service - Tính năng nâng cao

#### 1.1 STT (Speech-to-Text) Integration ⚠️
**Trạng thái**: Recording task có nhưng chưa gửi đến STT endpoint
**Files**: `components/sx_services/sx_audio_service.c`
**Cần làm**:
- Tích hợp HTTP client để gửi audio chunks đến STT endpoint
- Xử lý response và dispatch event với transcript
- Buffer management cho audio chunks
- Error handling và retry logic

#### 1.2 Opus Encoder/Decoder ⚠️
**Trạng thái**: Chưa có
**Từ kho vật liệu**: `AudioEncoderService`, `AudioDecoderService`
**Cần làm**:
- Opus encoder wrapper cho audio input
- Opus decoder wrapper cho audio output
- Queue management (encode/decode tasks)
- Multi-codec support (Opus, AAC, FLAC)

#### 1.3 Audio Processor (AFE) ⚠️
**Trạng thái**: Chưa có
**Từ kho vật liệu**: Audio processor với AEC, VAD
**Cần làm**:
- AEC (Acoustic Echo Cancellation) integration
- VAD (Voice Activity Detection) integration
- Audio processing pipeline

#### 1.4 Wake Word Detection ⚠️
**Trạng thái**: Chưa có
**Từ kho vật liệu**: `WakeWordService` với AFE/Custom/ESP wake words
**Cần làm**:
- AFE Wake Word (ESP-SR)
- Custom Wake Word support
- Wake word encoding (Opus packets)

#### 1.5 Hardware Volume Control ⚠️
**Trạng thái**: Chỉ có software volume
**Cần làm**:
- Kiểm tra codec chip (ES8388, ES8311, etc.)
- I2C/SPI control cho hardware volume
- Fallback về software nếu không có hardware

---

### 2. Audio Advanced Features

#### 2.1 Audio Ducking Manager ⚠️
**Trạng thái**: Chưa có
**Từ kho vật liệu**: `AudioDuckingManager`
**Cần làm**:
- Lower volume khi Assistant nói
- Restore volume sau khi Assistant nói xong
- Configurable duck level (0.0 = mute, 1.0 = no ducking)
- Per-source ducking levels

#### 2.2 Crossfade Engine ⚠️
**Trạng thái**: Có structure nhưng chưa implement
**Từ kho vật liệu**: `AudioCrossfadeEngine`
**Files**: `components/sx_services/sx_audio_mixer.c`
**Cần làm**:
- Crossfade giữa các tracks (default 500ms)
- Fade out old source, fade in new source
- Configurable fade duration và curves

#### 2.3 Gapless Playback ⚠️
**Trạng thái**: Chưa có
**Từ kho vật liệu**: Gapless playback với preload
**Cần làm**:
- Preload next track
- State machine cho gapless transitions
- Seamless track switching

#### 2.4 Audio Recovery Manager ⚠️
**Trạng thái**: Chưa có
**Từ kho vật liệu**: `AudioRecoveryManager`, `AudioStreamMonitor`
**Cần làm**:
- Buffer underrun detection và recovery
- Pause playback for recovery
- Resume playback after recovery
- Refill playback buffer

#### 2.5 Smooth Volume Control ⚠️
**Trạng thái**: Chưa có
**Từ kho vật liệu**: Logarithmic volume scaling với ramp
**Cần làm**:
- Logarithmic volume scaling
- Volume ramp (configurable duration, default 200ms)
- Smooth transitions

---

### 3. MCP Tools - Mở rộng

#### 3.1 SD Music MCP Tools ⚠️
**Trạng thái**: Chưa có
**Từ repo mẫu**: 10 tools gộp thành action-based
**Cần làm**:
- `self.sdmusic.playback`: play/pause/stop/next/prev
- `self.sdmusic.mode`: shuffle/repeat mode control
- `self.sdmusic.track`: set/info/list/current track operations
- `self.sdmusic.directory`: play/list directories
- `self.sdmusic.search`: search/play by keyword
- `self.sdmusic.library`: count_dir/count_current/page tracks
- `self.sdmusic.suggest`: suggest next/similar tracks
- `self.sdmusic.progress`: get playback progress
- `self.sdmusic.genre`: genre search/play/play_index/next
- `self.sdmusic.genre_list`: list all genres

#### 3.2 Music Online MCP Tools ⚠️
**Trạng thái**: Chưa có
**Cần làm**:
- `self.music.play_song`: Play online music (song_name, artist_name)
- `self.music.set_display_mode`: Set display mode (spectrum/lyrics)
- `self.music.pause` / `self.music.resume`

#### 3.3 User-Only Tools ⚠️
**Trạng thái**: Chưa có
**Cần làm**:
- `self.get_system_info`: System information
- `self.reboot`: Reboot system
- `self.upgrade_firmware`: OTA upgrade from URL
- `self.screen.get_info`: Screen information
- `self.screen.snapshot`: Snapshot screen và upload
- `self.screen.preview_image`: Preview image on screen

#### 3.4 Intent Parser Integration ⚠️
**Trạng thái**: Service có nhưng chưa tích hợp vào chatbot
**Files**: `components/sx_services/sx_intent_service.c`
**Cần làm**:
- Tích hợp `sx_intent_service_init()` vào main
- Connect intent service với chatbot
- Map intents đến MCP tools hoặc direct service calls
- Register intent handlers cho các services

---

### 4. UI Integration - Các tính năng còn thiếu

#### 4.1 WiFi Password Input Dialog ⚠️
**Trạng thái**: UI có nhưng chưa implement dialog
**Files**: `components/sx_ui/screens/screen_wifi_setup.c`
**Cần làm**:
- Tạo password input dialog với LVGL keyboard
- Show dialog khi click vào encrypted network
- Call `sx_wifi_connect(ssid, password)` sau khi nhập

#### 4.2 OTA Service UI Integration ⚠️
**Trạng thái**: Service có nhưng chưa tích hợp vào UI
**Files**: `components/sx_ui/screens/screen_ota.c`
**Cần làm**:
- Tích hợp `sx_ota_service_init()` vào main
- Connect OTA service với `screen_ota.c`
- Update progress bar từ callback
- Show error messages từ `sx_ota_get_last_error()`

#### 4.3 Settings Persistence ⚠️
**Trạng thái**: Service có nhưng chưa được sử dụng đầy đủ
**Files**: `components/sx_services/sx_settings_service.c`
**Cần làm**:
- Sử dụng settings service trong các screen:
  - `screen_settings.c`: Lưu volume, brightness
  - `screen_wifi_setup.c`: Lưu WiFi credentials
  - `screen_equalizer.c`: Lưu EQ presets (đã có)
  - `screen_display_setting.c`: Lưu display settings (đã có)
- Load settings khi khởi động (đã có cho brightness)

#### 4.4 Playlist Navigation ⚠️
**Trạng thái**: UI có buttons nhưng chưa có logic
**Files**: `components/sx_ui/screens/screen_music_player.c`
**Cần làm**:
- Implement playlist manager
- Track current index
- Previous/Next navigation
- Auto-play next track

---

## 🚧 P1 - IMPORTANT FEATURES (Cải thiện UX)

### 5. Radio Service - Cải thiện nâng cao

#### 5.1 Content-Type Parsing ⚠️
**Trạng thái**: Chưa parse Content-Type header
**Cần làm**:
- Parse HTTP headers để detect AAC/MP3/OGG
- Auto-select decoder dựa trên Content-Type
- Fallback nếu Content-Type không có

#### 5.2 UI Error Display ⚠️
**Trạng thái**: Errors chỉ log, không hiển thị trên UI
**Cần làm**:
- Dispatch events cho connection errors
- Update `screen_radio.c` để hiển thị error messages
- Retry button trong UI

#### 5.3 Per-Station Volume Amplification ⚠️
**Trạng thái**: Chưa có
**Cần làm**:
- Store volume boost per station
- Apply volume boost khi play station
- UI để configure volume boost

---

### 6. AAC Decoder - Tính năng nâng cao

#### 6.1 Seek/Flush ⚠️
**Trạng thái**: Chưa có
**Cần làm**:
- `sx_codec_aac_seek(uint32_t position_ms)` - seek trong stream
- `sx_codec_aac_flush()` - clear decoder buffer
- Stream metadata (duration, etc.)

#### 6.2 HE-AAC v2 Support ⚠️
**Trạng thái**: Chưa verify
**Cần làm**:
- Test với HE-AAC v2 streams
- Verify `espressif/esp_audio_codec` hỗ trợ HE-AAC v2
- Alternative decoder nếu không hỗ trợ

---

### 7. Audio Mixer - Cải thiện

#### 7.1 Better Resampler ⚠️
**Trạng thái**: Chỉ có linear interpolation (simple)
**Cần làm**:
- Implement sinc-based resampler hoặc
- Sử dụng ESP-ADF resampler component
- Better quality cho sample rate conversion

---

### 8. Protocol Layer - MQTT Support

#### 8.1 MQTT Protocol ⚠️
**Trạng thái**: Chỉ có WebSocket
**Cần làm**:
- Implement `sx_protocol_mqtt.c/h`
- Tương tự WebSocket nhưng dùng `esp_mqtt_client`
- Support cho MCP messages qua MQTT
- UDP support (nếu cần)
- AES encryption (nếu cần)

---

## 🚧 P2 - ADVANCED FEATURES (Nice to have)

### 9. Display & UI - Tính năng nâng cao

#### 9.1 Theme System ⚠️
**Trạng thái**: Chưa có
**Từ repo mẫu**: Light/Dark themes, custom colors
**Cần làm**:
- Light/Dark theme support
- Custom color schemes
- Theme persistence (NVS)
- UI để switch themes

#### 9.2 Image/GIF Support ⚠️
**Trạng thái**: Chưa có
**Từ repo mẫu**: GIF animation, JPEG, CBin images
**Cần làm**:
- GIF animation (lvgl_gif component)
- JPEG image support
- CBin image support
- Image preview với timeout

#### 9.3 QR Code Display ⚠️
**Trạng thái**: Chưa có
**Từ repo mẫu**: Generate QR code cho IP address
**Cần làm**:
- QR code generation library
- Display QR code trên screen
- MCP tool: `self.network.ip2qrcode`

#### 9.4 FFT Spectrum Visualization ⚠️
**Trạng thái**: Đã hủy theo yêu cầu
**Note**: Không tích hợp

---

### 10. LED Control

#### 10.1 LED Types ⚠️
**Trạng thái**: Chưa có
**Từ repo mẫu**: GPIO LED, Single LED, WS2812, State-based
**Cần làm**:
- GPIO LED control
- Single LED control
- Circular Strip (WS2812) control
- State-based LED (theo device state)

---

### 11. Power Management

#### 11.1 Power Features ⚠️
**Trạng thái**: Chưa có
**Từ repo mẫu**: Power save mode, battery monitoring, sleep timer
**Cần làm**:
- Power save mode (auto power save khi idle)
- Battery monitoring (level, charging status)
- Sleep timer (auto sleep after idle)
- Power state management

---

### 12. Device State Management

#### 12.1 State Machine Improvements ⚠️
**Trạng thái**: Cần cải thiện
**Từ repo mẫu**: State machine với nhiều states
**Cần làm**:
- State machine improvements:
  - Unknown, Starting, Configuring, Idle
  - Connecting, Listening, Speaking
  - Upgrading, Activating, AudioTesting
  - FatalError
- State events (event-driven state changes)
- State persistence

#### 12.2 Application Event Loop ⚠️
**Trạng thái**: Cần cải thiện
**Từ repo mẫu**: Main event loop với scheduling
**Cần làm**:
- Main event loop với scheduling
- Task scheduling trong main loop
- Alert system (show alerts với emotion và sound)
- Listening mode control (auto-stop, continuous modes)

---

### 13. Audio Buffer Management

#### 13.1 Audio Buffer Pool ⚠️
**Trạng thái**: Chưa có
**Từ kho vật liệu**: `AudioBufferPool`, `AudioBufferAllocator`
**Cần làm**:
- Thread-safe buffer pool
- Buffer allocation management
- PSRAM Buffer Helper
- Buffer monitoring

---

### 14. Audio Pipeline Profiler (Optional)

#### 14.1 Performance Profiling ⚠️
**Trạng thái**: Chưa có
**Từ kho vật liệu**: `AudioPipelineProfiler`
**Cần làm**:
- Profile audio pipeline performance
- Performance metrics (encoding/decoding time)
- Bottleneck detection
- **Note**: Optional, chỉ cho debugging

---

### 15. Navigation Service

#### 15.1 Navigation Features ⚠️
**Trạng thái**: Chưa có
**Từ repo mẫu**: Navigation route management, voice guidance
**Cần làm**:
- Navigation route management
- Navigation state management
- Voice guidance
- Turn-by-turn directions
- MCP tools: `self.navigation.*`

---

### 16. Other Services

#### 16.1 Bluetooth Service ⚠️
**Trạng thái**: Chưa có
**Từ kho vật liệu**: `BluetoothService`
**Cần làm**:
- Bluetooth management
- Bluetooth pairing
- Bluetooth audio

#### 16.2 Telegram Service ⚠️
**Trạng thái**: Chưa có
**Từ kho vật liệu**: `TelegramService`
**Cần làm**:
- Telegram bot integration
- Send messages to Telegram
- Configuration management

#### 16.3 State Sync Service ⚠️
**Trạng thái**: Chưa có
**Từ kho vật liệu**: `StateSyncService`
**Cần làm**:
- UI state synchronization
- External consumer registration
- State snapshot publishing
- Remote state push/pull

---

## 📊 TỔNG KẾT

### Đã tích hợp: 7 tính năng
1. ✅ EQ API
2. ✅ Weather Service
3. ✅ PWM Brightness Control
4. ✅ Weather UI
5. ✅ SD Music Improvements
6. ✅ Music Online (lyrics, display modes)
7. ✅ Radio Improvements (display modes, buffer)

### Cần tích hợp: ~50+ tính năng

**P0 - Core Features**: ~20 tính năng
**P1 - Important Features**: ~10 tính năng
**P2 - Advanced Features**: ~20+ tính năng

---

## 🎯 KẾ HOẠCH TÍCH HỢP

### Phase 1: Core Audio Features (P0)
1. STT Integration
2. Opus Encoder/Decoder
3. Audio Processor (AFE)
4. Wake Word Detection
5. Hardware Volume Control
6. Audio Ducking Manager
7. Crossfade Engine
8. Gapless Playback

### Phase 2: MCP & UI Integration (P0)
9. SD Music MCP Tools
10. Music Online MCP Tools
11. User-Only Tools
12. Intent Parser Integration
13. WiFi Password Dialog
14. OTA UI Integration
15. Settings Persistence (hoàn thiện)
16. Playlist Navigation

### Phase 3: Advanced Audio (P1)
17. Audio Recovery Manager
18. Smooth Volume Control
19. Better Resampler
20. Content-Type Parsing
21. UI Error Display
22. AAC Seek/Flush

### Phase 4: System Features (P1/P2)
23. MQTT Protocol
24. Theme System
25. Image/GIF Support
26. QR Code Display
27. LED Control
28. Power Management
29. Device State Management
30. Application Event Loop

### Phase 5: Optional Features (P2)
31. Audio Buffer Pool
32. Audio Pipeline Profiler
33. Navigation Service
34. Bluetooth Service
35. Other services (nếu cần)

---

## 📝 LƯU Ý QUAN TRỌNG

1. **Kiến trúc SimpleXL**: Tuyệt đối không phá vỡ kiến trúc hiện tại
2. **C API**: Repo mẫu và kho vật liệu dùng C++, SimpleXL dùng C - cần convert
3. **Component Structure**: Giữ nguyên cấu trúc component của SimpleXL
4. **Event System**: Sử dụng sx_dispatcher và sx_event hiện có
5. **State Management**: Sử dụng sx_state_t hiện có
6. **UI System**: Sử dụng ui_router và ui_screen_registry hiện có
7. **Priority**: Ưu tiên P0 trước, sau đó P1, cuối cùng P2

---

**Cập nhật lần cuối**: 2024-12-19
**Trạng thái**: Tổng hợp đầy đủ từ 4 nguồn tài liệu












