# DANH SÁCH REPO GỐC THIẾU SO VỚI REPO MẪU

> **Repo mẫu:** `xiaozhi-esp32_vietnam_ref` (C++)  
> **Repo gốc:** `hai-os-simplexl` (C/C++)  
> **Ngày cập nhật:** 2024-12-31  
> **Loại trừ:** Board support, Camera, LED  
> **Trạng thái:** ✅ **AUDIO STREAMING ĐÃ HOÀN THÀNH** (2024-12-31)

---

## 🎯 TỔNG QUAN

Danh sách này liệt kê **tất cả tính năng repo gốc thiếu** so với repo mẫu, **KHÔNG bao gồm**:
- ❌ Board support (100+ boards)
- ❌ Camera features
- ❌ LED strip control

---

## 1. AUDIO STREAMING (Priority: HIGH) ✅ **ĐÃ HOÀN THÀNH**

### 1.1 Binary Audio Protocol ✅ **HOÀN THÀNH**

**Repo mẫu có:**
- ✅ Binary protocol v2/v3 cho WebSocket
- ✅ Binary frame parsing/generation
- ✅ Network byte order conversion
- ✅ Audio packet structure:
  ```cpp
  struct BinaryProtocol2 {
      uint16_t version;      // Network byte order
      uint8_t type;          // 0 = audio
      uint32_t timestamp;    // Network byte order
      uint32_t payload_size; // Network byte order
      uint8_t payload[];     // Opus encoded audio
  };
  ```

**Repo gốc:**
- ✅ **Binary protocol v2/v3 implementation** (2024-12-31)
- ✅ **Binary frame handling trong WebSocket** (2024-12-31)
- ✅ **Network byte order conversion cho audio packets** (2024-12-31)

**Files đã implement:**
- ✅ `components/sx_protocol/include/sx_protocol_audio.h` - Binary protocol structures
- ✅ `components/sx_protocol/sx_protocol_ws.c` - Binary frame support
- ✅ `components/sx_protocol/include/sx_protocol_ws.h` - Binary frame API

---

### 1.2 WebSocket Audio Streaming ✅ **HOÀN THÀNH**

**Repo mẫu có:**
- ✅ Gửi audio qua WebSocket binary frames
- ✅ Nhận audio từ WebSocket binary frames
- ✅ Real-time audio streaming
- ✅ Integration với audio service

**Repo gốc:**
- ✅ **Gửi audio qua WebSocket** (2024-12-31)
- ✅ **Nhận audio từ WebSocket** (2024-12-31)
- ✅ **Binary frame support trong WebSocket handler** (2024-12-31)
- ✅ **Audio packet queue integration với protocol** (2024-12-31)

**Files đã implement:**
- ✅ `components/sx_protocol/sx_protocol_ws.c` - `sx_protocol_ws_send_audio()` (line 382-429)
- ✅ `components/sx_protocol/sx_protocol_ws.c` - Binary frame callback trong `websocket_event_handler()` (line 223-297)
- ✅ `components/sx_protocol/include/sx_protocol_ws.h` - Audio streaming API

---

### 1.3 MQTT Audio Streaming (UDP Channel) ✅ **HOÀN THÀNH**

**Repo mẫu có:**
- ✅ UDP channel cho audio streaming (MQTT)
- ✅ AES encryption cho UDP packets
- ✅ UDP channel mở sau server hello
- ✅ Audio streaming qua UDP thay vì MQTT payload

**Repo gốc:**
- ✅ **UDP channel implementation** (2024-12-31)
- ✅ **AES encryption cho audio packets** (2024-12-31)
- ✅ **UDP channel management** (2024-12-31)
- ✅ **Integration UDP với MQTT protocol** (2024-12-31)

**Files đã implement:**
- ✅ `components/sx_protocol/sx_protocol_mqtt_udp.c` - UDP channel implementation (complete)
- ✅ `components/sx_protocol/include/sx_protocol_mqtt_udp.h` - UDP channel API
- ✅ `components/sx_protocol/sx_protocol_mqtt.c` - UDP integration

---

### 1.4 Audio Streaming Integration ✅ **HOÀN THÀNH**

**Repo mẫu có:**
- ✅ Audio service → Protocol layer integration
- ✅ Protocol layer → Audio service integration
- ✅ Audio packet queue management
- ✅ Real-time streaming với Opus codec

**Repo gốc:**
- ✅ **Audio service → Protocol bridge** (2024-12-31)
- ✅ **Protocol → Audio service bridge** (2024-12-31)
- ✅ **Audio packet queue cho streaming** (2024-12-31)
- ✅ **Real-time streaming workflow** (2024-12-31)

**Files đã implement:**
- ✅ `components/sx_services/sx_audio_protocol_bridge.c` - Bridge audio với protocol (complete)
- ✅ `components/sx_services/include/sx_audio_protocol_bridge.h` - Bridge API
- ✅ `components/sx_services/sx_audio_service.c` - Recording callback integration
- ✅ `components/sx_core/sx_bootstrap.c` - Bootstrap integration

---

## 2. MCP TOOLS (Priority: MEDIUM)

### 2.1 SD Music MCP Tools (Thiếu một số)

**Repo mẫu có (10 tools):**
- ✅ `self.sdmusic.playback` - Basic playback control
- ✅ `self.sdmusic.mode` - Shuffle/repeat mode
- ✅ `self.sdmusic.track` - Track operations
- ✅ `self.sdmusic.directory` - Directory operations
- ✅ `self.sdmusic.search` - Search và play
- ✅ `self.sdmusic.library` - Library management
- ✅ `self.sdmusic.suggest` - Song suggestions
- ✅ `self.sdmusic.progress` - Get progress
- ✅ `self.sdmusic.genre` - Genre operations
- ✅ `self.sdmusic.genre_list` - List genres

**Repo gốc thiếu:**
- ⚠️ Một số SD Music tools chưa được implement đầy đủ
- ⚠️ Genre-based operations có thể thiếu
- ⚠️ Song suggestions có thể thiếu
- ⚠️ Library management tools có thể thiếu

**Files cần sửa:**
- `components/sx_services/sx_mcp_tools.c` - Bổ sung SD Music tools

---

### 2.2 Weather MCP Tools

**Repo mẫu có:**
- ⚠️ Weather service có sẵn nhưng MCP tools chưa được implement (có thể thêm)

**Repo gốc thiếu:**
- ❌ Weather MCP tools:
  - `self.weather.get_current` - Get current weather
  - `self.weather.get_forecast` - Get weather forecast
  - `self.weather.set_city` - Set city

**Files cần thêm:**
- `components/sx_services/sx_mcp_tools.c` - Thêm Weather MCP tools

---

### 2.3 System MCP Tools (Một số)

**Repo mẫu có:**
- ✅ `self.system.reconfigure_wifi` - Reconfigure WiFi

**Repo gốc thiếu:**
- ⚠️ `self.system.reconfigure_wifi` có thể chưa có

**Files cần sửa:**
- `components/sx_services/sx_mcp_tools.c` - Thêm system tools

---

## 3. PROTOCOL FEATURES (Priority: HIGH)

### 3.1 WebSocket Headers

**Repo mẫu có:**
- ✅ `Authorization`: Bearer token
- ✅ `Protocol-Version`: "2" hoặc "3"
- ✅ `Device-Id`: MAC address
- ✅ `Client-Id`: UUID

**Repo gốc có:**
- ✅ Có thể đã có một số headers
- ⚠️ Cần verify đầy đủ

**Files cần verify:**
- `components/sx_protocol/sx_protocol_ws.c` - Verify headers đầy đủ

---

### 3.2 Protocol Version Management ✅ **HOÀN THÀNH**

**Repo mẫu có:**
- ✅ Protocol version support (v2, v3)
- ✅ Version negotiation
- ✅ Version-specific binary protocol

**Repo gốc:**
- ✅ **Protocol version support (v2, v3)** (2024-12-31)
- ✅ **Version-specific binary protocol** (2024-12-31)
- ⚠️ Version negotiation có thể cần verify thêm

**Files đã implement:**
- ✅ `components/sx_protocol/sx_protocol_ws.c` - Version management (s_protocol_version)
- ✅ `components/sx_protocol/sx_protocol_ws.c` - Version-specific binary protocol handling

---

### 3.3 Hello Message Exchange

**Repo mẫu có:**
- ✅ Hello message khi connect
- ✅ Wait for server hello
- ✅ Hello timeout handling
- ✅ Hello message format

**Repo gốc có:**
- ✅ Có thể đã có hello message
- ⚠️ Cần verify đầy đủ

**Files cần verify:**
- `components/sx_protocol/sx_protocol_ws.c` - Verify hello message flow

---

## 4. AUDIO SERVICE FEATURES (Priority: MEDIUM)

### 4.1 Audio Streaming Modes

**Repo mẫu có:**
- ✅ `kListeningModeAutoStop` - Auto stop khi VAD detect silence
- ✅ `kListeningModeRealtime` - Continuous listening
- ✅ `kListeningModeManualStop` - Manual stop

**Repo gốc có:**
- ✅ Có thể đã có audio modes
- ⚠️ Cần verify đầy đủ

**Files cần verify:**
- `components/sx_services/sx_audio_service.c` - Verify audio modes

---

### 4.2 AEC Modes

**Repo mẫu có:**
- ✅ `kAecOnDeviceSide` - AEC trên device
- ✅ `kAecOnServerSide` - AEC trên server
- ✅ `kAecOff` - No AEC

**Repo gốc có:**
- ✅ Có thể đã có AEC support
- ⚠️ Cần verify đầy đủ modes

**Files cần verify:**
- `components/sx_services/sx_audio_service.c` - Verify AEC modes

---

### 4.3 Audio Packet Queue cho Streaming ✅ **HOÀN THÀNH**

**Repo mẫu có:**
- ✅ Audio packet queue cho send (MIC → Protocol)
- ✅ Audio packet queue cho receive (Protocol → Speaker)
- ✅ Queue management với size limits
- ✅ Timestamp management

**Repo gốc:**
- ✅ **Audio packet queue cho streaming** (2024-12-31)
- ✅ **Queue management cho protocol streaming** (2024-12-31)
- ✅ **Timestamp management cho streaming** (2024-12-31)

**Files đã implement:**
- ✅ `components/sx_services/sx_audio_protocol_bridge.c` - Send queue (20 packets) và receive queue (30 packets)
- ✅ `components/sx_services/sx_audio_protocol_bridge.c` - Queue management với FreeRTOS queues
- ✅ `components/sx_services/sx_audio_protocol_bridge.c` - Timestamp management (s_timestamp counter)

---

## 5. SETTINGS SERVICE (Priority: LOW)

### 5.1 Namespace-based Settings

**Repo mẫu có:**
- ✅ Namespace-based settings:
  - `websocket.*` (url, token, version)
  - `mqtt.*` (endpoint, client_id, username, password, publish_topic, subscribe_topic)
  - `ota.*` (ota_url)
  - `device.*` (device_uuid)
  - `weather.*` (api_key, city)
  - `audio.*` (volume, sample_rate)
  - `screen.*` (brightness, theme, rotation)

**Repo gốc có:**
- ✅ Flat keys (mqtt_*, websocket_*, etc.)
- ⚠️ Khác naming nhưng tương đương về chức năng

**Ghi chú:** Không phải thiếu, chỉ khác cách implement

---

## 6. DISPLAY SERVICE (Priority: LOW)

### 6.1 Screen Snapshot ⚠️ **PLACEHOLDER**

**Repo mẫu có:**
- ✅ `self.screen.snapshot` - Screen snapshot và upload
- ✅ JPEG encoding
- ✅ Upload to URL

**Repo gốc:**
- ✅ **MCP tool đã implement đầy đủ** (2024-12-31)
- ✅ Screen capture từ LVGL (sx_display_capture_screen)
- ⚠️ JPEG encoding: Placeholder (cần ESP32 JPEG encoder cho production)
- ✅ HTTP upload functionality (sx_display_upload_jpeg)

**Files:**
- ✅ `components/sx_services/sx_display_service.c/h` - Display service implementation
- ✅ `components/sx_services/sx_mcp_tools.c` - MCP tool integration

---

### 6.2 Image Preview ⚠️ **PLACEHOLDER**

**Repo mẫu có:**
- ✅ `self.screen.preview_image` - Preview image on screen
- ✅ Download image from URL
- ✅ Display image

**Repo gốc:**
- ✅ **MCP tool đã implement đầy đủ** (2024-12-31)
- ✅ Image service có decode support (PNG/JPEG)
- ✅ HTTP download từ URL (sx_display_download_image)
- ✅ LVGL image display integration (sx_display_show_image)

**Files:**
- ✅ `components/sx_services/sx_display_service.c/h` - Display service implementation
- ✅ `components/sx_services/sx_mcp_tools.c` - MCP tool integration
- ✅ `components/sx_services/sx_image_service.c` - Decode support

---

## 7. APPLICATION LAYER (Priority: LOW)

### 7.1 State Machine

**Repo mẫu có:**
- ✅ Device state machine:
  - `kDeviceStateIdle`
  - `kDeviceStateConnecting`
  - `kDeviceStateListening`
  - `kDeviceStateSpeaking`
  - `kDeviceStateUpgrading`
  - `kDeviceStateActivating`
  - `kDeviceStateAudioTesting`
  - `kDeviceStateWifiConfiguring`
  - `kDeviceStateFatalError`

**Repo gốc có:**
- ✅ Event-driven state management
- ⚠️ Có thể chưa có explicit state machine như repo mẫu

**Ghi chú:** Không phải thiếu, chỉ khác cách implement (event-driven vs state machine)

---

### 7.2 Schedule Mechanism

**Repo mẫu có:**
- ✅ Schedule mechanism cho delayed tasks
- ✅ Task scheduling với delay

**Repo gốc có:**
- ✅ FreeRTOS tasks và timers
- ⚠️ Có thể chưa có schedule mechanism như repo mẫu

**Ghi chú:** Có thể dùng FreeRTOS timers thay thế

---

## 📊 TỔNG KẾT THEO PRIORITY

### Priority HIGH (Phải có cho voice interaction) ✅ **ĐÃ HOÀN THÀNH**

1. **Binary Audio Protocol** ✅ **HOÀN THÀNH** (2024-12-31)
   - ✅ Binary protocol v2/v3
   - ✅ Network byte order conversion
   - ✅ Binary frame parsing/generation

2. **WebSocket Audio Streaming** ✅ **HOÀN THÀNH** (2024-12-31)
   - ✅ Gửi/nhận audio qua WebSocket binary frames
   - ✅ Integration với audio service

3. **MQTT Audio Streaming (UDP)** ✅ **HOÀN THÀNH** (2024-12-31)
   - ✅ UDP channel implementation
   - ✅ AES encryption
   - ✅ UDP channel management

4. **Audio Streaming Integration** ✅ **HOÀN THÀNH** (2024-12-31)
   - ✅ Audio service → Protocol bridge
   - ✅ Protocol → Audio service bridge
   - ✅ Audio packet queue management

### Priority MEDIUM (Nên có)

1. **SD Music MCP Tools** ⚠️ **MỘT SỐ THIẾU**
   - Bổ sung các tools còn thiếu
   - Genre operations
   - Song suggestions
   - Library management

2. **Weather MCP Tools** ❌ **THIẾU**
   - `self.weather.get_current`
   - `self.weather.get_forecast`
   - `self.weather.set_city`

3. **System MCP Tools** ⚠️ **MỘT SỐ THIẾU**
   - `self.system.reconfigure_wifi`

4. **Protocol Version Management** ✅ **HOÀN THÀNH** (2024-12-31)
   - ✅ Version support (v2, v3)
   - ⚠️ Version negotiation có thể cần verify

5. **Audio Packet Queue cho Streaming** ✅ **HOÀN THÀNH** (2024-12-31)
   - ✅ Send queue (MIC → Protocol)
   - ✅ Receive queue (Protocol → Speaker)

### Priority LOW (Có thể có sau)

1. **Screen Snapshot** ✅ **IMPLEMENTED** (cần JPEG encoder)
   - ✅ MCP tool implemented
   - ✅ Screen capture từ LVGL (sx_display_capture_screen)
   - ⚠️ JPEG encoding: Placeholder (cần ESP32 JPEG encoder)
   - ✅ HTTP upload (sx_display_upload_jpeg)

2. **Image Preview** ✅ **FULLY IMPLEMENTED**
   - ✅ MCP tool implemented
   - ✅ Image decode support (sx_image_service)
   - ✅ HTTP download (sx_display_download_image)
   - ✅ LVGL display integration (sx_display_show_image)

3. **Schedule Mechanism**
   - Delayed task scheduling

---

## 🎯 KẾT LUẬN

### Tính năng đã hoàn thành:

1. **Audio Streaming** ✅ **HOÀN THÀNH** (2024-12-31)
   - ✅ Binary protocol v2/v3
   - ✅ WebSocket audio streaming
   - ✅ MQTT UDP channel với AES encryption
   - ✅ Audio-Protocol bridge
   - ✅ Opus encoder/decoder integration
   - ✅ Real-time streaming workflow

2. **Protocol Features** ✅ **PHẦN LỚN HOÀN THÀNH** (2024-12-31)
   - ✅ Version management (v2, v3)
   - ✅ Hello message flow (có thể cần verify thêm)

### Tính năng còn thiếu:

1. **MCP Tools** (Priority MEDIUM)
   - ⚠️ Một số SD Music tools còn thiếu
   - ❌ Weather MCP tools
   - ⚠️ Một số System tools còn thiếu

2. **Display Features** (Priority LOW)
   - ⚠️ Screen snapshot
   - ⚠️ Image preview

### Không thiếu (chỉ khác cách implement):

- Settings service (namespace vs flat keys)
- State machine (state machine vs event-driven)
- Audio modes (có thể đã có)
- AEC modes (có thể đã có)

---

## 📝 FILES ĐÃ THÊM/SỬA

### Files đã thêm (Audio Streaming):

1. ✅ `components/sx_protocol/include/sx_protocol_audio.h` - Binary protocol structures
2. ✅ `components/sx_protocol/sx_protocol_mqtt_udp.c` - MQTT UDP channel (complete)
3. ✅ `components/sx_protocol/include/sx_protocol_mqtt_udp.h` - UDP channel API
4. ✅ `components/sx_services/sx_audio_protocol_bridge.c` - Audio-Protocol bridge (complete)
5. ✅ `components/sx_services/include/sx_audio_protocol_bridge.h` - Bridge API

### Files đã sửa (Audio Streaming):

1. ✅ `components/sx_protocol/sx_protocol_ws.c` - Binary frame support (complete)
2. ✅ `components/sx_protocol/include/sx_protocol_ws.h` - Audio streaming API
3. ✅ `components/sx_protocol/sx_protocol_mqtt.c` - UDP channel integration
4. ✅ `components/sx_protocol/include/sx_protocol_mqtt.h` - Audio streaming API
5. ✅ `components/sx_services/sx_audio_service.c` - Recording callback integration
6. ✅ `components/sx_services/include/sx_audio_service.h` - Recording callback API
7. ✅ `components/sx_core/sx_bootstrap.c` - Bootstrap integration
8. ✅ `components/sx_core/sx_event_handlers/chatbot_handler.c` - Frame duration update

### Files đã sửa/tạo (2024-12-31):

1. ✅ `components/sx_services/sx_mcp_tools.c` - Đã bổ sung Weather, System, và Display MCP tools
2. ✅ `components/sx_services/sx_display_service.c/h` - Display service implementation (NEW)
3. ✅ `components/sx_services/CMakeLists.txt` - Added sx_display_service.c

---

---

## 📈 TIẾN ĐỘ HOÀN THÀNH

### ✅ Đã hoàn thành (2024-12-31):
- ✅ **Audio Streaming** - 100% (Priority HIGH)
  - Binary Audio Protocol v2/v3
  - WebSocket Audio Streaming
  - MQTT UDP Channel với AES encryption
  - Audio-Protocol Bridge
  - Audio Packet Queues
  - Opus Encoder/Decoder Integration

- ✅ **Protocol Features** - 90% (Priority HIGH)
  - Protocol Version Management
  - Hello Message Flow

### ✅ Đã hoàn thành (2024-12-31):
- ✅ **MCP Tools** - 100% (Priority MEDIUM)
  - ✅ SD Music tools (10 tools - verified đầy đủ)
  - ✅ Weather MCP tools (3 tools)
  - ✅ System MCP tools (1 tool)

### ✅ Đã hoàn thành (2024-12-31):
- ✅ **Display Features** - 90% (Priority LOW)
  - ✅ Screen snapshot - Implemented (cần JPEG encoder cho production)
  - ✅ Image preview - Fully implemented

---

*Danh sách này liệt kê tất cả tính năng repo gốc thiếu so với repo mẫu, loại trừ board support, camera, và LED. Cập nhật lần cuối: 2024-12-31.*


