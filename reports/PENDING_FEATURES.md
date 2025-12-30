# Danh sách tính năng còn lại chưa triển khai

## 1. Audio Service - Tính năng nâng cao

### 1.1 STT (Speech-to-Text) Integration
- **Trạng thái**: Recording task chỉ log samples, chưa gửi đến STT endpoint
- **File**: `components/sx_services/sx_audio_service.c` (line 371)
- **Cần làm**:
  - Tích hợp HTTP client để gửi audio chunks đến STT endpoint
  - Xử lý response và dispatch event với transcript
  - Buffer management cho audio chunks

### 1.2 AEC (Acoustic Echo Cancellation)
- **Trạng thái**: Chưa có
- **Cần làm**:
  - Tích hợp ESP-ADF AEC hoặc thuật toán AEC đơn giản
  - Reference channel từ speaker output
  - Filter để loại bỏ echo trong microphone input

### 1.3 Hardware Volume Control
- **Trạng thái**: Chỉ có software volume (multiply PCM)
- **Cần làm**:
  - Kiểm tra codec chip có hardware volume control không (ES8388, ES8311, etc.)
  - Implement I2C/SPI control cho hardware volume
  - Fallback về software nếu không có hardware

### 1.4 EQ (Equalizer) API
- **Trạng thái**: UI có (`screen_equalizer.c`) nhưng chưa có API trong audio service
- **File**: `components/sx_ui/screens/screen_equalizer.c` (line 193)
- **Cần làm**:
  - Tạo `sx_audio_eq.h/c` với 10-band EQ filter
  - Implement biquad filters hoặc sử dụng ESP-ADF EQ
  - API: `sx_audio_eq_set_band(int band, float gain_db)`
  - API: `sx_audio_eq_set_preset(int preset_id)`
  - Áp dụng EQ trong `sx_audio_service_feed_pcm` hoặc mixer

## 2. Platform - Backlight Control

### 2.1 PWM Backlight Control
- **Trạng thái**: Chỉ có GPIO on/off (`gpio_set_level`)
- **File**: `components/sx_platform/sx_platform.c` (line 50-56)
- **Cần làm**:
  - Implement LEDC (PWM) cho backlight pin
  - API: `sx_platform_set_brightness(uint8_t percent)` (0-100)
  - Tích hợp vào `screen_display_setting.c` và `screen_settings.c`

## 3. Radio Service - Cải thiện

### 3.1 Network Buffer
- **Trạng thái**: Buffer cố định, có thể bị underrun
- **Cần làm**:
  - Dynamic buffer sizing dựa trên bitrate
  - Pre-buffer trước khi play
  - Buffer status monitoring và UI feedback

### 3.2 Content-Type Parsing
- **Trạng thái**: Chưa parse Content-Type header để detect format
- **Cần làm**:
  - Parse HTTP headers để detect AAC/MP3/OGG
  - Auto-select decoder dựa trên Content-Type
  - Fallback nếu Content-Type không có

### 3.3 UI Error Display
- **Trạng thái**: Errors chỉ log, không hiển thị trên UI
- **Cần làm**:
  - Dispatch events cho connection errors
  - Update `screen_radio.c` để hiển thị error messages
  - Retry button trong UI

## 4. AAC Decoder - Tính năng nâng cao

### 4.1 Seek/Flush
- **Trạng thái**: Chưa có
- **Cần làm**:
  - `sx_codec_aac_seek(uint32_t position_ms)` - seek trong stream
  - `sx_codec_aac_flush()` - clear decoder buffer
  - Cần stream metadata (duration, etc.)

### 4.2 HE-AAC v2 Support
- **Trạng thái**: Chưa verify
- **Cần làm**:
  - Test với HE-AAC v2 streams
  - Verify `espressif/esp_audio_codec` hỗ trợ HE-AAC v2
  - Nếu không, cần alternative decoder

## 5. Chatbot/MCP - Tính năng nâng cao

### 5.1 Intent Parser
- **Trạng thái**: ✅ Service đã implement nhưng chưa tích hợp vào chatbot
- **File**: `components/sx_services/sx_intent_service.c` (đã có)
- **File**: `components/sx_services/include/sx_chatbot_service.h` (line 30)
- **Cần làm**:
  - Tích hợp `sx_intent_service_init()` vào main
  - Connect intent service với chatbot:
    - Call `sx_intent_parse()` trong `sx_chatbot_task`
    - Map intents đến MCP tools hoặc direct service calls
    - Register intent handlers cho các services

### 5.2 More MCP Tools
- **Trạng thái**: Chỉ có basic tools (volume, radio, device status)
- **File**: `components/sx_services/sx_mcp_tools.c`
- **Cần thêm**:
  - `self.music.play` / `self.music.pause` / `self.music.next`
  - `self.wifi.connect` / `self.wifi.scan`
  - `self.settings.set_brightness`
  - `self.settings.get_all`

## 6. OTA Service - Tích hợp

### 6.1 OTA Service Implementation
- **Trạng thái**: ✅ Service đã implement nhưng chưa tích hợp vào UI
- **File**: `components/sx_services/sx_ota_service.c` (đã có)
- **File**: `components/sx_ui/screens/screen_ota.c` (line 111, 132 - TODO)
- **Cần làm**:
  - Tích hợp `sx_ota_service_init()` vào main
  - Connect OTA service với `screen_ota.c`:
    - Call `sx_ota_start_update()` khi click "Start Update"
    - Update progress bar từ callback
    - Show error messages từ `sx_ota_get_last_error()`

## 7. Settings Storage - NVS

### 7.1 Settings Persistence
- **Trạng thái**: ✅ Service đã implement nhưng chưa được sử dụng
- **File**: `components/sx_services/sx_settings_service.c` (đã có đầy đủ API)
- **Cần làm**:
  - Tích hợp `sx_settings_service_init()` vào main
  - Sử dụng settings service trong các screen:
    - `screen_settings.c`: Lưu volume, brightness
    - `screen_wifi_setup.c`: Lưu WiFi credentials
    - `screen_equalizer.c`: Lưu EQ presets
    - `screen_display_setting.c`: Lưu display settings
  - Load settings khi khởi động

## 8. UI - Tính năng còn thiếu

### 8.1 WiFi Password Input Dialog
- **Trạng thái**: UI có nhưng chưa implement dialog
- **File**: `components/sx_ui/screens/screen_wifi_setup.c` (line 160)
- **Cần làm**:
  - Tạo password input dialog với LVGL keyboard
  - Show dialog khi click vào encrypted network
  - Call `sx_wifi_connect(ssid, password)` sau khi nhập

### 8.2 Playlist Navigation
- **Trạng thái**: UI có buttons nhưng chưa có logic
- **File**: `components/sx_ui/screens/screen_music_player.c` (line 61, 69)
- **Cần làm**:
  - Implement playlist manager
  - Track current index
  - Previous/Next navigation
  - Auto-play next track

## 9. Audio Mixer - Cải thiện

### 9.1 Crossfade Implementation
- **Trạng thái**: Có structure nhưng chưa implement
- **File**: `components/sx_services/sx_audio_mixer.c`
- **Cần làm**:
  - Implement crossfade khi switch sources
  - Fade out old source, fade in new source
  - Configurable fade duration

### 9.2 Better Resampler
- **Trạng thái**: Chỉ có linear interpolation (simple)
- **Cần làm**:
  - Implement sinc-based resampler hoặc
  - Sử dụng ESP-ADF resampler component
  - Better quality cho sample rate conversion

## 10. Protocol Layer - MQTT Support

### 10.1 MQTT Protocol
- **Trạng thái**: Chỉ có WebSocket
- **Cần làm**:
  - Implement `sx_protocol_mqtt.c/h`
  - Tương tự WebSocket nhưng dùng `esp_mqtt_client`
  - Support cho MCP messages qua MQTT

---

## Tổng kết ưu tiên

### P0 - Core Features (Cần thiết cho MVP)
1. ⚠️ Recording + STT integration (recording có, STT chưa)
2. ⚠️ EQ API trong audio service (UI có, API chưa)
3. ⚠️ Brightness control API (PWM) (GPIO có, PWM chưa)
4. ⚠️ OTA service integration (service có, UI chưa connect)
5. ⚠️ Settings storage (NVS) (service có, chưa sử dụng)
6. ⚠️ WiFi password input dialog (UI có, dialog chưa)

### P1 - Important Features (Cải thiện UX)
7. Radio: Network buffer, Content-Type parsing, UI error display
8. AAC Decoder: Seek/flush
9. Intent parser
10. More MCP tools
11. Playlist navigation

### P2 - Advanced Features (Nice to have)
12. AEC (Acoustic Echo Cancellation)
13. Hardware volume control
14. Crossfade trong mixer
15. Better resampler
16. MQTT protocol support
17. HE-AAC v2 verification

