# Phân Tích Sâu Chatbot - Tính Năng và Khả Năng

## 📋 Tổng Quan

Phân tích chi tiết các tính năng và khả năng của chatbot trong repo mẫu (`xiaozhi-esp32_vietnam_ref`), dựa trên MCP (Model Context Protocol) tools và message types.

---

## 🎯 Message Types

### 1. STT (Speech-to-Text)
**Type:** `"stt"`

**Mô tả:** Server gửi text từ speech recognition

**Format:**
```json
{
  "type": "stt",
  "text": "user message text"
}
```

**Xử lý:**
- Hiển thị message trên display với role "user"
- Log message với prefix ">>"

### 2. TTS (Text-to-Speech)
**Type:** `"tts"`

**States:**
- `"start"` - TTS bắt đầu, chuyển sang `kDeviceStateSpeaking`
- `"stop"` - TTS kết thúc, chuyển về `kDeviceStateListening` hoặc `kDeviceStateIdle`
- `"sentence_start"` - Câu mới bắt đầu, hiển thị text với role "assistant"

**Format:**
```json
{
  "type": "tts",
  "state": "start|stop|sentence_start",
  "text": "sentence text"  // chỉ có khi state = "sentence_start"
}
```

### 3. LLM Emotion
**Type:** `"llm"`

**Mô tả:** Server gửi emotion từ LLM

**Format:**
```json
{
  "type": "llm",
  "emotion": "happy|sad|neutral|..."
}
```

**Xử lý:**
- Update emotion trên display

### 4. MCP (Model Context Protocol)
**Type:** `"mcp"`

**Mô tả:** Server gửi MCP message để gọi tools

**Format:**
```json
{
  "type": "mcp",
  "payload": {
    // MCP message format
  }
}
```

**Xử lý:**
- Parse và execute tool calls
- Return results về server

### 5. System Commands
**Type:** `"system"`

**Commands:**
- `"reboot"` - Reboot device

**Format:**
```json
{
  "type": "system",
  "command": "reboot"
}
```

### 6. Alert
**Type:** `"alert"`

**Mô tả:** Hiển thị alert message

**Format:**
```json
{
  "type": "alert",
  "status": "status text",
  "message": "message text",
  "emotion": "emotion icon"
}
```

### 7. Custom Message
**Type:** `"custom"` (optional, CONFIG_RECEIVE_CUSTOM_MESSAGE)

**Mô tả:** Custom message format

**Format:**
```json
{
  "type": "custom",
  "payload": {
    // custom payload
  }
}
```

---

## 🛠️ MCP Tools (Common Tools)

### Device Status

#### `self.get_device_status`
**Mô tả:** Lấy thông tin real-time của device

**Returns:**
- Audio speaker status
- Screen status
- Battery status
- Network status
- etc.

**Use cases:**
- Trả lời câu hỏi về trạng thái hiện tại
- Bước đầu tiên để control device

### Network

#### `self.network.ip2qrcode`
**Mô tả:** Hiển thị QR code của IP address

**Returns:**
- IP address
- SSID
- Connection status
- RSSI, channel, MAC address
- QR code displayed status

**Use cases:**
- Khi user hỏi về network connection
- Khi user muốn xem IP address
- Khi user muốn print QR code

### Audio

#### `self.audio_speaker.set_volume`
**Mô tả:** Set volume của audio speaker (0-100)

**Parameters:**
- `volume` (integer, 0-100)

**Use cases:**
- Tăng/giảm volume
- Set volume cụ thể

### Screen

#### `self.screen.set_brightness`
**Mô tả:** Set brightness của screen (0-100)

**Parameters:**
- `brightness` (integer, 0-100)

#### `self.screen.set_theme`
**Mô tả:** Set theme của screen

**Parameters:**
- `theme` (string): "light" hoặc "dark"

#### `self.screen.set_rotation`
**Mô tả:** Set rotation của screen

**Parameters:**
- `rotation_degree` (integer, 0|90|180|270)

### Camera

#### `self.camera.take_photo`
**Mô tả:** Chụp ảnh và giải thích

**Parameters:**
- `question` (string): Câu hỏi về ảnh

**Returns:**
- JSON object với thông tin ảnh
- Image content (base64 encoded)

**Use cases:**
- Khi user yêu cầu "xem gì đó"
- Vision analysis

### Music

#### `self.music.play_song`
**Mô tả:** Phát nhạc từ online service

**Parameters:**
- `song_name` (string): Tên bài hát
- `artist_name` (string, optional): Tên nghệ sĩ

**Use cases:**
- Phát nhạc theo yêu cầu
- Tìm và phát bài hát

#### `self.music.set_display_mode`
**Mô tả:** Set display mode cho music player

**Parameters:**
- `mode` (string): Display mode

### Radio

#### `self.radio.play_station`
**Mô tả:** Phát radio station

**Parameters:**
- `station_name` (string): Tên station

#### `self.radio.play_url`
**Mô tả:** Phát radio từ URL

**Parameters:**
- `url` (string): Radio stream URL

#### `self.radio.stop`
**Mô tả:** Dừng radio

#### `self.radio.get_stations`
**Mô tả:** Lấy danh sách radio stations

**Returns:**
- List of stations

#### `self.radio.set_display_mode`
**Mô tả:** Set display mode cho radio

### SD Music

#### `self.sdmusic.playback`
**Mô tả:** Control SD music playback cơ bản

**Parameters:**
- `action` (string): "play", "pause", "stop", "next", "prev"

**Use cases:**
- Phát nhạc từ SD card
- Control playback (play, pause, stop, next, prev)

**Lưu ý:** Tool này chỉ dùng khi user nói rõ "nhạc trong thẻ nhớ", "nhạc offline", "SD card", etc.

#### `self.sdmusic.mode`
**Mô tả:** Control playback mode (shuffle và repeat)

**Parameters:**
- `action` (string): "shuffle" hoặc "repeat"
- `enabled` (boolean): Cho shuffle
- `mode` (string): "none", "one", "all" cho repeat

**Use cases:**
- Enable/disable shuffle
- Set repeat mode

#### `self.sdmusic.track`
**Mô tả:** Track-level operations

**Parameters:**
- `action` (string): "set", "info", "list", "current"
- `index` (integer): Track index (cho set và info)

**Use cases:**
- Set track by index
- Get track info
- List all tracks
- Get current track

#### `self.sdmusic.directory`
**Mô tả:** Directory-level operations

**Parameters:**
- `action` (string): "play" hoặc "list"
- `directory` (string): Directory path (cho play)

**Use cases:**
- Play all tracks in directory
- List directories

#### `self.sdmusic.search`
**Mô tả:** Search và play tracks by name

**Parameters:**
- `action` (string): "search" hoặc "play"
- `keyword` (string): Search keyword

**Use cases:**
- Search tracks by name
- Play track by name

#### `self.sdmusic.library`
**Mô tả:** Library management operations

**Parameters:**
- `action` (string): "reload", "count", "page"
- `page` (integer): Page number (cho page)
- `page_size` (integer): Page size (cho page)

**Use cases:**
- Reload track list
- Get track count
- Get paginated track list

#### `self.sdmusic.suggest`
**Mô tả:** Song suggestion based on history/similarity

**Parameters:**
- `action` (string): "next" hoặc "similar"
- `keyword` (string): Keyword cho similar
- `max_results` (integer, 1-50): Max results

**Use cases:**
- Suggest next tracks
- Suggest similar tracks

#### `self.sdmusic.progress`
**Mô tả:** Get current playback progress

**Returns:**
- `position_ms`: Current position
- `duration_ms`: Track duration
- `state`: "playing", "paused", "stopped", etc.
- `bitrate_kbps`: Bitrate
- `track_name`: Current track name
- `track_path`: Current track path

#### `self.sdmusic.genre`
**Mô tả:** Genre-based music operations

**Parameters:**
- `action` (string): "search", "play", "play_index", "next"
- `genre` (string): Genre name
- `index` (integer): Track index trong genre playlist

**Use cases:**
- Search tracks by genre
- Play genre playlist
- Play track by index trong genre
- Play next track trong genre

#### `self.sdmusic.genre_list`
**Mô tả:** List all unique genres

**Returns:**
- Array of genre names

**Use cases:**
- Get available genres

### Weather

**Note:** Weather service có sẵn nhưng chưa có MCP tools được implement trong code. Có thể thêm tools:
- `self.weather.get_current` - Get current weather
- `self.weather.get_forecast` - Get weather forecast
- `self.weather.set_city` - Set city

**Use cases:**
- Lấy thông tin thời tiết
- Hiển thị weather forecast
- Update weather data

### Alarm Clock

**Note:** Alarm clock feature chưa được implement (file `alarm_clock.h` chỉ có comment "not implemented"). Có thể thêm tools trong tương lai:
- `self.alarm.set` - Set alarm
- `self.alarm.get` - Get alarm status
- `self.alarm.cancel` - Cancel alarm

---

## 🎮 Board-Specific Tools

### Robot Control (Otto, Electron Bot, ESP-HI, etc.)

#### `self.otto.action`
**Mô tả:** Control robot actions

**Parameters:**
- `action_name` (string): Tên action
- `speed` (integer, optional): Tốc độ

**Actions:**
- Walk, dance, wave, etc.

#### `self.otto.stop`
**Mô tả:** Dừng tất cả actions

#### `self.otto.get_status`
**Mô tả:** Lấy robot status

**Returns:**
- "moving" hoặc "idle"

#### `self.otto.get_trims`
**Mô tả:** Lấy servo trim settings

#### `self.battery.get_level`
**Mô tả:** Lấy battery level và charging status

### LED Strip Control

#### `self.led_strip.get_brightness`
**Mô tả:** Lấy brightness của LED strip

#### `self.led_strip.set_brightness`
**Mô tả:** Set brightness

**Parameters:**
- `brightness` (integer, 0-100)

#### `self.led_strip.set_single_color`
**Mô tả:** Set màu cho single LED

**Parameters:**
- `index` (integer): LED index
- `r`, `g`, `b` (integer, 0-255): RGB values

#### `self.led_strip.set_all_color`
**Mô tả:** Set màu cho tất cả LEDs

**Parameters:**
- `r`, `g`, `b` (integer, 0-255): RGB values

#### `self.led_strip.blink`
**Mô tả:** Blink LEDs

**Parameters:**
- `r`, `g`, `b` (integer, 0-255): RGB values
- `duration_ms` (integer): Duration

#### `self.led_strip.scroll`
**Mô tả:** Scroll effect

**Parameters:**
- `r`, `g`, `b` (integer, 0-255): RGB values
- `speed` (integer): Scroll speed

### Camera Control

#### `self.camera.get_ir_filter_state`
**Mô tả:** Lấy IR filter state

#### `self.camera.enable_ir_filter`
**Mô tả:** Enable IR filter

#### `self.camera.disable_ir_filter`
**Mô tả:** Disable IR filter

#### `self.camera.set_camera_flipped`
**Mô tả:** Flip camera image

### WiFi Reconfiguration

#### `self.system.reconfigure_wifi`
**Mô tả:** Reconfigure WiFi

**Use cases:**
- Khi user muốn đổi WiFi
- Khi WiFi connection fail

### Press-to-Talk

#### `self.set_press_to_talk`
**Mô tả:** Enable/disable press-to-talk mode

**Parameters:**
- `enabled` (boolean): Enable/disable

**Use cases:**
- Switch giữa voice activation và press-to-talk
- Control input mode

### User-Only Tools (Không hiển thị cho AI)

#### `self.get_system_info`
**Mô tả:** Get system information (chỉ user có thể gọi)

**Returns:**
- System info JSON

#### `self.reboot`
**Mô tả:** Reboot device (chỉ user có thể gọi)

#### `self.upgrade_firmware`
**Mô tả:** Upgrade firmware from URL (chỉ user có thể gọi)

**Parameters:**
- `url` (string): Firmware URL

#### `self.screen.get_info`
**Mô tả:** Get screen information (chỉ user có thể gọi)

**Returns:**
- Width, height, monochrome status

#### `self.screen.snapshot`
**Mô tả:** Snapshot screen và upload (chỉ user có thể gọi)

**Parameters:**
- `url` (string): Upload URL
- `quality` (integer, 1-100): JPEG quality

#### `self.screen.preview_image`
**Mô tả:** Preview image on screen (chỉ user có thể gọi)

**Parameters:**
- `url` (string): Image URL

#### `self.assets.set_download_url`
**Mô tả:** Set assets download URL (chỉ user có thể gọi)

**Parameters:**
- `url` (string): Download URL

---

## 🔄 Device States

### State Machine

**States:**
- `kDeviceStateIdle` - Standby
- `kDeviceStateConnecting` - Đang kết nối protocol
- `kDeviceStateListening` - Đang nghe (MIC active)
- `kDeviceStateSpeaking` - Đang phát (Speaker active)
- `kDeviceStateUpgrading` - Đang upgrade
- `kDeviceStateActivating` - Activating
- `kDeviceStateAudioTesting` - Testing audio
- `kDeviceStateWifiConfiguring` - Configuring WiFi
- `kDeviceStateFatalError` - Fatal error

### State Transitions

**Idle → Listening:**
- User bấm button hoặc wake word detected
- Open audio channel
- Start listening

**Listening → Speaking:**
- TTS start event
- Stop listening, start speaking

**Speaking → Listening/Idle:**
- TTS stop event
- Return to listening (auto mode) hoặc idle (manual mode)

---

## 🎤 Audio Modes

### Listening Modes

1. **Auto Stop** (`kListeningModeAutoStop`)
   - Tự động stop khi VAD detect silence
   - Phù hợp cho short commands

2. **Realtime** (`kListeningModeRealtime`)
   - Continuous listening
   - Phù hợp cho conversation

3. **Manual Stop** (`kListeningModeManualStop`)
   - User phải stop manually
   - Phù hợp cho long input

### AEC Modes

1. **Device AEC** (`kAecOnDeviceSide`)
   - AEC xử lý trên device
   - Giảm echo từ speaker

2. **Server AEC** (`kAecOnServerSide`)
   - AEC xử lý trên server
   - Cần timestamp trong audio packets

3. **No AEC** (`kAecOff`)
   - Không có AEC
   - Đơn giản nhất

---

## 📊 Features Summary

### Core Features

1. **Voice Interaction**
   - STT (Speech-to-Text)
   - TTS (Text-to-Speech)
   - Wake word detection
   - VAD (Voice Activity Detection)

2. **Audio Streaming**
   - Opus encoding/decoding
   - Real-time audio streaming
   - AEC support

3. **Display Control**
   - Chat messages
   - Emotion display
   - QR code display
   - Status bar

4. **Device Control**
   - Volume control
   - Brightness control
   - Theme control
   - Rotation control

### Extended Features

1. **Media Playback**
   - Online music
   - Radio streaming
   - SD card music
   - Playback control

2. **Network**
   - WiFi management
   - IP address display
   - QR code for connection

3. **Camera**
   - Photo capture
   - Vision analysis
   - IR filter control

4. **Robot Control** (board-specific)
   - Action control
   - Status monitoring
   - Battery monitoring

5. **LED Control** (board-specific)
   - Brightness control
   - Color control
   - Effects (blink, scroll)

6. **Weather** (optional)
   - Weather data fetching
   - Weather display

7. **Alarm Clock** (optional)
   - Alarm setting
   - Alarm management

---

## 🎯 Use Cases

### 1. Basic Conversation
- User: "Xin chào"
- Bot: "Xin chào! Tôi có thể giúp gì cho bạn?"
- Flow: STT → LLM → TTS

### 2. Device Control
- User: "Tăng volume lên 50"
- Bot: Calls `self.audio_speaker.set_volume(50)`
- Bot: "Đã tăng volume lên 50"

### 3. Media Control
- User: "Phát nhạc [tên bài hát]"
- Bot: Calls `self.music.play_song("song_name")`
- Bot: "Đang phát [tên bài hát]"

### 4. Vision Analysis
- User: "Xem cái gì đó"
- Bot: Calls `self.camera.take_photo("What is this?")`
- Bot: Returns image analysis

### 5. Network Info
- User: "IP address là gì?"
- Bot: Calls `self.network.ip2qrcode()`
- Bot: Displays IP và QR code

### 6. Robot Control
- User: "Đi về phía trước"
- Bot: Calls `self.otto.action("walk_forward")`
- Bot: "Đang đi về phía trước"

---

## 🔧 MCP Protocol Flow

### 1. Tool Discovery
```
Server → Device: "tools/list"
Device → Server: List of available tools
```

### 2. Tool Call
```
Server → Device: {
  "type": "mcp",
  "payload": {
    "method": "tools/call",
    "params": {
      "name": "self.audio_speaker.set_volume",
      "arguments": {"volume": 50}
    }
  }
}

Device → Server: {
  "type": "mcp",
  "payload": {
    "result": {
      "content": [{"type": "text", "text": "true"}],
      "isError": false
    }
  }
}
```

### 3. Error Handling
```
Device → Server: {
  "type": "mcp",
  "payload": {
    "error": {
      "code": -1,
      "message": "Tool not found"
    }
  }
}
```

---

## 📊 Tổng Kết Tools

### Common Tools (AI có thể dùng)

**Device Control:**
- `self.get_device_status` - Get device status
- `self.audio_speaker.set_volume` - Set volume
- `self.screen.set_brightness` - Set brightness
- `self.screen.set_theme` - Set theme
- `self.screen.set_rotation` - Set rotation

**Network:**
- `self.network.ip2qrcode` - Display IP QR code

**Camera:**
- `self.camera.take_photo` - Take photo và analyze

**Music:**
- `self.music.play_song` - Play online music
- `self.music.set_display_mode` - Set display mode

**Radio:**
- `self.radio.play_station` - Play radio station
- `self.radio.play_url` - Play radio from URL
- `self.radio.stop` - Stop radio
- `self.radio.get_stations` - Get station list
- `self.radio.set_display_mode` - Set display mode

**SD Music (10 tools):**
- `self.sdmusic.playback` - Basic playback control
- `self.sdmusic.mode` - Shuffle/repeat mode
- `self.sdmusic.track` - Track operations
- `self.sdmusic.directory` - Directory operations
- `self.sdmusic.search` - Search và play
- `self.sdmusic.library` - Library management
- `self.sdmusic.suggest` - Song suggestions
- `self.sdmusic.progress` - Get progress
- `self.sdmusic.genre` - Genre operations
- `self.sdmusic.genre_list` - List genres

**Board-Specific:**
- Robot control (Otto, Electron Bot, ESP-HI, etc.)
- LED strip control
- Camera IR filter
- WiFi reconfiguration
- Press-to-talk

### User-Only Tools (Chỉ user có thể gọi)

- `self.get_system_info` - System info
- `self.reboot` - Reboot device
- `self.upgrade_firmware` - Firmware upgrade
- `self.screen.get_info` - Screen info
- `self.screen.snapshot` - Screen snapshot
- `self.screen.preview_image` - Preview image
- `self.assets.set_download_url` - Set assets URL

---

## ✅ Kết Luận

**Chatbot có thể làm:**

1. **Conversation**
   - Trò chuyện tự nhiên bằng tiếng Việt
   - Hiểu context và intent
   - Trả lời câu hỏi
   - Emotion expression

2. **Device Control**
   - Control audio (volume)
   - Control screen (brightness, theme, rotation)
   - Control camera (take photo, IR filter)
   - Get device status

3. **Media Playback**
   - Play online music (tìm và phát bài hát)
   - Play radio (stations hoặc custom URL)
   - Play SD card music (10 tools cho full control)
   - Control playback (play, pause, stop, next, prev)
   - Display modes (spectrum, lyrics, info)

4. **Vision Analysis**
   - Take photos
   - Analyze images với AI vision
   - Answer visual questions
   - Return image content (base64)

5. **Network Management**
   - WiFi configuration
   - IP address display
   - QR code generation
   - Connection status

6. **SD Music Management** (Advanced)
   - Full library management
   - Track search và selection
   - Genre-based playback
   - Shuffle và repeat modes
   - Progress tracking
   - Song suggestions

7. **Board-Specific Features**
   - Robot control (actions, status, battery)
   - LED strip control (color, effects)
   - Camera control
   - Custom hardware control

8. **System Management** (User-only)
   - System info
   - Firmware upgrade
   - Screen snapshot
   - Assets management

**Architecture:**
- **MCP (Model Context Protocol)** cho tool calling
- **Event-driven state machine** cho device states
- **Real-time audio streaming** (Opus codec)
- **Extensible tool system** (dễ thêm tools mới)
- **Board-specific tools** (customizable per board)

**Extensibility:**
- ✅ Dễ dàng thêm tools mới
- ✅ Board-specific tools support
- ✅ Custom message types
- ✅ Feature modules (weather, alarm, etc.)
- ✅ User-only tools (hidden from AI)

**Message Flow:**
```
User Voice → STT → LLM → MCP Tools → Device Actions → TTS → User
```

**Key Features:**
- 🎤 Voice interaction (STT/TTS)
- 🎵 Media playback (music, radio, SD)
- 📷 Vision analysis
- 🤖 Robot control (board-specific)
- 💡 LED control (board-specific)
- 📡 Network management
- 🎨 Display control
- 🔧 System management

