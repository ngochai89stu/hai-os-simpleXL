# Phân tích sâu tính năng từ repo mẫu xiaozhi-esp32_vietnam_ref

## Tổng quan
Repo mẫu là một hệ thống AI voice assistant hoàn chỉnh với nhiều tính năng nâng cao. Dưới đây là danh sách đầy đủ các tính năng cần tích hợp vào SimpleXL.

---

## 1. AUDIO SERVICE - Tính năng nâng cao

### 1.1 Audio Processing Pipeline
- **Opus Encoder/Decoder**: Encode audio để gửi lên server, decode audio từ server
- **Audio Processor (AFE)**: Xử lý audio với AEC (Acoustic Echo Cancellation), VAD (Voice Activity Detection)
- **Wake Word Detection**: 
  - AFE Wake Word (ESP-SR)
  - Custom Wake Word
  - ESP Wake Word
- **Audio Codec Support**:
  - ES8388, ES8389, ES8374, ES8311
  - Box Audio Codec
  - Dummy/No Audio Codec
- **Audio Debugger**: Debug audio pipeline

### 1.2 Audio Features
- **Duplex I2S**: Input và Output đồng thời
- **Sample Rate Reconfiguration**: Runtime sample rate change
- **Volume Control**: Hardware và software volume
- **Audio Power Management**: Auto power save khi idle

---

## 2. DISPLAY & UI - Tính năng nâng cao

### 2.1 Display Types
- **LCD Display**: LVGL-based với FFT spectrum, music UI
- **OLED Display**: Monochrome OLED support
- **Emote Display**: Emoji/icon animation engine

### 2.2 LVGL Features
- **Theme System**: Light/Dark themes, custom colors
- **Image Support**: 
  - Raw images
  - GIF animation (lvgl_gif)
  - JPEG images
  - CBin images
- **Font System**: Custom fonts, icon fonts, emoji fonts
- **QR Code Display**: Hiển thị QR code cho IP address
- **Rotation Support**: 0°, 90°, 180°, 270°

### 2.3 Display Features
- **FFT Spectrum Visualization**: Real-time audio spectrum
- **Music UI**: 
  - Track info (title, artist, album)
  - Progress bar
  - Time display
  - Lyrics display (sync với playback)
- **Weather UI**: Hiển thị thông tin thời tiết
- **Idle Display**: Weather info khi idle
- **Preview Image**: Preview images với timeout
- **Status Bar**: Network, battery, time

---

## 3. MUSIC & RADIO - Tính năng đầy đủ

### 3.1 Online Music (Esp32Music)
- **Streaming**: HTTP streaming MP3
- **Authentication**: MAC, Chip-ID, Timestamp, SHA256 key
- **Display Modes**: 
  - Spectrum mode
  - Lyrics mode
- **Lyrics Sync**: Download và sync lyrics với playback
- **Buffer Management**: Dynamic buffer sizing
- **MP3 Decoder**: mini-mp3 decoder

### 3.2 Online Radio (Esp32Radio)
- **Station List**: Predefined VOV stations
- **AAC Decoder**: esp_audio_simple_dec cho AAC streams
- **Display Modes**:
  - Spectrum mode
  - Info mode (station info)
- **Volume Amplification**: Per-station volume boost
- **ICY Metadata**: Parse stream metadata
- **Buffer Management**: Dynamic buffer sizing

### 3.3 SD Card Music (Esp32SdMusic) - RẤT ĐẦY ĐỦ
- **Playlist Management**:
  - Load track list từ SD card
  - Directory navigation
  - Track search
  - Genre-based playlist
- **Playback Control**:
  - Play, Pause, Stop
  - Next, Previous
  - Shuffle mode
  - Repeat modes (None, One, All)
- **Track Info**:
  - ID3v1 + ID3v2 tags (title, artist, album, genre, year, comment, track number)
  - Cover art metadata (offset, size, MIME)
  - Duration, bitrate
- **Advanced Features**:
  - Track suggestion (based on history/similarity)
  - Genre playlist
  - Pagination (list tracks by page)
  - Directory counting
  - Case-insensitive search
  - UTF-8 support
- **Progress Tracking**: Position, duration, bitrate
- **State Machine**: Stopped, Preparing, Playing, Paused, Error

---

## 4. MCP SERVER - Tools đầy đủ

### 4.1 Common Tools
- `self.get_device_status`: Device status (audio, screen, battery, network)
- `self.network.ip2qrcode`: Generate QR code cho IP address
- `self.audio_speaker.set_volume`: Set volume (0-100)
- `self.screen.set_brightness`: Set screen brightness (0-100)
- `self.screen.set_theme`: Set theme (light/dark)
- `self.screen.set_rotation`: Set rotation (0/90/180/270)
- `self.camera.take_photo`: Take photo và explain
- `self.music.play_song`: Play online music (song_name, artist_name)
- `self.music.set_display_mode`: Set display mode (spectrum/lyrics)
- `self.radio.play_station`: Play radio station by name
- `self.radio.play_url`: Play radio from URL
- `self.radio.stop`: Stop radio
- `self.radio.get_stations`: Get station list
- `self.radio.set_display_mode`: Set display mode (spectrum/info)

### 4.2 SD Music Tools (10 tools gộp thành action-based)
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

### 4.3 User-Only Tools
- `self.get_system_info`: System information
- `self.reboot`: Reboot system
- `self.upgrade_firmware`: OTA upgrade from URL
- `self.screen.get_info`: Screen information
- `self.screen.snapshot`: Snapshot screen và upload
- `self.screen.preview_image`: Preview image on screen
- `self.assets.set_download_url`: Set assets download URL

### 4.4 Board-Specific Tools
- `self.system.reconfigure_wifi`: Reconfigure WiFi (một số boards)
- `self.disp.setbacklight`: Set backlight (một số boards)
- `self.disp.network`: Reconfigure network (một số boards)
- `self.otto.*`: Otto robot control (nhiều tools)
- `self.electron.*`: Electron bot control
- `self.chassis.*`: Chassis control (forward, back, turn, dance, etc.)
- `self.led_strip.*`: LED strip control (brightness, color, blink, scroll)
- `self.camera.get_ir_filter_state`: IR filter state
- `self.camera.enable_ir_filter`: Enable IR filter
- `self.camera.disable_ir_filter`: Disable IR filter
- `self.model.*`: Model parameter control (sensecap-watcher)

---

## 5. PROTOCOL LAYER

### 5.1 WebSocket Protocol
- **Connection Management**: Auto-reconnect
- **Audio Channel**: Open/close audio channel
- **Text Messages**: Send/receive text
- **Server Hello**: Parse server configuration

### 5.2 MQTT Protocol
- **Connection Management**: Auto-reconnect với timer
- **UDP Support**: UDP audio streaming
- **AES Encryption**: Encrypted audio packets
- **Sequence Management**: Local/remote sequence numbers
- **Ping Mechanism**: Keep-alive ping

---

## 6. SETTINGS & STORAGE

### 6.1 Settings Service
- **NVS Storage**: Namespace-based storage
- **Data Types**: String, Int, Bool
- **Operations**: Get, Set, Erase, EraseAll
- **Dirty Flag**: Auto-commit on changes

### 6.2 System Info
- **Flash Size**: Get flash size
- **Heap Info**: Free heap, minimum free heap
- **MAC Address**: Get MAC address
- **Chip Model**: Get chip model name
- **User Agent**: Generate user agent string
- **Task Monitoring**: CPU usage, task list, heap stats

---

## 7. OTA & FIRMWARE

### 7.1 OTA Service
- **Version Check**: Check for new version
- **Activation**: Device activation với code/challenge
- **Upgrade**: Download và install firmware
- **Progress Callback**: Progress và speed reporting
- **Server Time**: Sync server time
- **Config Download**: MQTT/WebSocket config từ server

### 7.2 OTA Server
- **Local OTA Server**: HTTP server cho OTA updates
- **Web Interface**: HTML interface cho OTA

---

## 8. WEATHER SERVICE

### 8.1 Weather Features
- **API Integration**: OpenWeatherMap API
- **Auto City Detection**: Get city from IP address
- **Weather Info**: Temperature, humidity, description, icon
- **Update Interval**: Configurable update interval
- **UI Component**: Weather UI với icons

---

## 9. LED CONTROL

### 9.1 LED Types
- **GPIO LED**: Simple GPIO LED
- **Single LED**: Single color LED
- **Circular Strip**: WS2812 circular strip
- **State-Based**: LED state theo device state

---

## 10. POWER MANAGEMENT

### 10.1 Power Features
- **Power Save Mode**: Enable/disable power save
- **Battery Monitoring**: Battery level, charging status
- **Sleep Timer**: Auto sleep after idle
- **Backlight Control**: PWM backlight control

---

## 11. FEATURES KHÁC

### 11.1 Alarm Clock
- **Status**: Chưa implement (placeholder)

### 11.2 Device State Management
- **State Machine**: 
  - Unknown, Starting, Configuring, Idle
  - Connecting, Listening, Speaking
  - Upgrading, Activating, AudioTesting
  - FatalError
- **State Events**: Event-driven state changes

### 11.3 Application Management
- **Main Event Loop**: Central event loop
- **Task Scheduling**: Schedule tasks trong main loop
- **Alert System**: Show alerts với emotion và sound
- **Chat State**: Toggle chat state
- **Listening Mode**: Auto-stop, continuous modes
- **AEC Mode**: Off, Device-side, Server-side

---

## 12. BOARD SUPPORT

### 12.1 Board Types
- **100+ Board Configurations**: Support cho nhiều hardware boards
- **Board-Specific Features**: 
  - Custom LCD displays
  - Custom audio codecs
  - Power managers
  - IR controllers
  - Camera support
  - Robot control (Otto, Electron Bot, Sparkbot)

---

## TỔNG KẾT TÍNH NĂNG CẦN TÍCH HỢP

### P0 - Core Features (Đã có nhưng cần cải thiện)
1. ✅ Audio Service - Cần thêm: Opus encoder/decoder, AFE processor, wake word
2. ✅ Radio Service - Cần thêm: Display modes, buffer management, station volume
3. ✅ Music Online - Cần thêm: Lyrics sync, display modes, authentication
4. ✅ SD Music - Cần thêm: ID3 tags, genre playlist, suggestions, pagination
5. ✅ MCP Tools - Cần thêm: SD music tools, user-only tools, board-specific tools
6. ✅ Display - Cần thêm: FFT spectrum, music UI, weather UI, GIF support
7. ✅ Settings - Đã có, cần tích hợp vào UI
8. ✅ OTA - Đã có, cần tích hợp vào UI
9. ✅ Weather Service - ĐÃ TÍCH HỢP (sx_weather_service)

### P1 - Important Features (Chưa có)
10. LED Control - Hoàn toàn mới
11. Power Management - Hoàn toàn mới
12. Theme System - Hoàn toàn mới
13. Image/GIF Support - Hoàn toàn mới
14. QR Code Display - Hoàn toàn mới
15. Device State Management - Cần cải thiện
16. Application Event Loop - Cần cải thiện

### P2 - Advanced Features (Nice to have)
17. Alarm Clock - Placeholder, chưa implement
18. Board-Specific Features - Tùy theo hardware
19. Camera Support - Tùy theo hardware
20. Robot Control - Tùy theo hardware

---

## KẾ HOẠCH TÍCH HỢP

### Phase 1: Core Improvements (P0)
1. Cải thiện Audio Service với Opus và AFE
2. Hoàn thiện Radio/Music với display modes
3. Implement đầy đủ SD Music features
4. Thêm MCP tools còn thiếu
5. Tích hợp Settings và OTA vào UI

### Phase 2: New Features (P1)
6. Weather Service
7. LED Control
8. Power Management
9. Theme System
10. Image/GIF Support

### Phase 3: Advanced (P2)
11. Alarm Clock
12. Board-specific features (nếu cần)

---

## LƯU Ý QUAN TRỌNG

1. **Kiến trúc SimpleXL**: Tuyệt đối không phá vỡ kiến trúc hiện tại
2. **C API**: Repo mẫu dùng C++, SimpleXL dùng C - cần convert
3. **Component Structure**: Giữ nguyên cấu trúc component của SimpleXL
4. **Event System**: Sử dụng sx_dispatcher và sx_event hiện có
5. **State Management**: Sử dụng sx_state_t hiện có
6. **UI System**: Sử dụng ui_router và ui_screen_registry hiện có
