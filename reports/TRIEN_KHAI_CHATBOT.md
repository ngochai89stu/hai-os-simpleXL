# Triển Khai Chatbot - Tóm Tắt

## ✅ Đã Hoàn Thành

### 1. JSON Message Parsing trong Protocol Layer

**WebSocket Protocol (`sx_protocol_ws.c`):**
- ✅ Parse JSON messages từ server
- ✅ Xử lý các message types:
  - `type: "stt"` - STT result từ server
  - `type: "tts"` - TTS response (start, stop, sentence_start)
  - `type: "llm"` - LLM emotion
  - `type: "mcp"` - MCP messages
- ✅ Emit events qua dispatcher cho UI updates

**MQTT Protocol (`sx_protocol_mqtt.c`):**
- ✅ Parse JSON messages tương tự WebSocket
- ✅ Xử lý tất cả message types
- ✅ Emit events qua dispatcher

### 2. Event Types Mới

**File:** `components/sx_core/include/sx_event.h`

Thêm các event types:
- `SX_EVT_CHATBOT_STT` - STT result (ptr: text string)
- `SX_EVT_CHATBOT_TTS_START` - TTS started
- `SX_EVT_CHATBOT_TTS_STOP` - TTS stopped
- `SX_EVT_CHATBOT_TTS_SENTENCE` - TTS sentence (ptr: text string)
- `SX_EVT_CHATBOT_EMOTION` - LLM emotion (ptr: emotion string)
- `SX_EVT_CHATBOT_CONNECTED` - Chatbot connected
- `SX_EVT_CHATBOT_DISCONNECTED` - Chatbot disconnected

### 3. Tích Hợp Protocol với Chatbot Service

**File:** `components/sx_services/sx_chatbot_service.c`

- ✅ Thêm protocol availability flags (`s_protocol_ws_available`, `s_protocol_mqtt_available`)
- ✅ Gửi messages qua WebSocket hoặc MQTT khi có
- ✅ Build JSON messages với format: `{"type": "user_message", "text": "..."}`
- ✅ Functions mới:
  - `sx_chatbot_set_protocol_ws_available()` - Set WebSocket availability
  - `sx_chatbot_set_protocol_mqtt_available()` - Set MQTT availability

**File:** `components/sx_services/include/sx_chatbot_service.h`

- ✅ Thêm `publish_topic` vào `sx_chatbot_config_t` cho MQTT

### 4. Event Handlers trong Orchestrator

**File:** `components/sx_core/sx_orchestrator.c`

- ✅ Xử lý `SX_EVT_CHATBOT_STT` - Update UI với user message
- ✅ Xử lý `SX_EVT_CHATBOT_TTS_SENTENCE` - Update UI với assistant message
- ✅ Xử lý `SX_EVT_CHATBOT_EMOTION` - Update UI emotion
- ✅ Xử lý `SX_EVT_CHATBOT_TTS_START/STOP` - Update speaking indicator
- ✅ Xử lý `SX_EVT_CHATBOT_CONNECTED/DISCONNECTED` - Update protocol availability

### 5. Settings Integration trong Bootstrap

**File:** `components/sx_core/sx_bootstrap.c`

- ✅ Load WebSocket config từ Settings:
  - `websocket_url` - WebSocket server URL
  - `websocket_token` - Authorization token
- ✅ Load MQTT config từ Settings:
  - `mqtt_broker` - MQTT broker URI
  - `mqtt_client_id` - Client ID
  - `mqtt_username` - Username
  - `mqtt_password` - Password
  - `mqtt_publish_topic` - Topic để publish messages
  - `mqtt_subscribe_topic` - Topic để subscribe
- ✅ Initialize protocol (WebSocket hoặc MQTT) dựa trên config
- ✅ Tích hợp với chatbot service initialization
- ✅ Set protocol availability flags

### 6. Protocol Headers

**File:** `components/sx_protocol/include/sx_protocol_ws.h`

- ✅ Thêm `sx_protocol_ws_message_cb_t` callback type
- ✅ Thêm `sx_protocol_ws_set_message_callback()` function

---

## 📋 Cách Sử Dụng

### 1. Cấu Hình WebSocket

**Qua Settings Service:**
```c
sx_settings_set_string("websocket_url", "wss://your-server.com/ws");
sx_settings_set_string("websocket_token", "your-auth-token");
```

**Hoặc qua Kconfig:**
```
CONFIG_SX_WEBSOCKET_URL="wss://your-server.com/ws"
CONFIG_SX_WEBSOCKET_TOKEN="your-auth-token"
```

### 2. Cấu Hình MQTT

**Qua Settings Service:**
```c
sx_settings_set_string("mqtt_broker", "mqtt://broker.example.com:1883");
sx_settings_set_string("mqtt_client_id", "simplexl_device_001");
sx_settings_set_string("mqtt_username", "user");
sx_settings_set_string("mqtt_password", "pass");
sx_settings_set_string("mqtt_publish_topic", "chatbot/message");
sx_settings_set_string("mqtt_subscribe_topic", "chatbot/response");
```

### 3. Gửi Message đến Chatbot

**Từ UI hoặc code:**
```c
// Gửi text message
esp_err_t ret = sx_chatbot_send_message("Hello chatbot!");
```

**Message sẽ được:**
1. Queue vào chatbot service
2. Process qua intent parsing (nếu enabled)
3. Gửi qua WebSocket hoặc MQTT nếu protocol available

### 4. Nhận Messages từ Server

**Server gửi JSON messages:**
```json
{
  "type": "stt",
  "text": "User said this"
}
```

```json
{
  "type": "tts",
  "state": "sentence_start",
  "text": "Assistant response"
}
```

```json
{
  "type": "llm",
  "emotion": "happy"
}
```

**Events sẽ được emit và UI có thể handle.**

---

## 🔄 Flow Hoạt Động

### Gửi Message (User → Server)

1. User nhập text trong UI → `SX_EVT_UI_INPUT`
2. Orchestrator handle → `sx_chatbot_send_message()`
3. Chatbot service queue message
4. Chatbot task process:
   - Try intent parsing first
   - If not intent, build JSON: `{"type": "user_message", "text": "..."}`
   - Send via WebSocket hoặc MQTT
5. Server nhận và xử lý

### Nhận Message (Server → Device)

1. Server gửi JSON message qua WebSocket/MQTT
2. Protocol layer parse JSON
3. Emit event dựa trên `type`:
   - `stt` → `SX_EVT_CHATBOT_STT`
   - `tts` → `SX_EVT_CHATBOT_TTS_*`
   - `llm` → `SX_EVT_CHATBOT_EMOTION`
   - `mcp` → `sx_chatbot_handle_mcp_message()`
4. Orchestrator handle events
5. UI update (cần implement trong screen handlers)

---

## ⚠️ Cần Hoàn Thiện

### 1. UI Event Handlers

**Cần thêm vào các screen handlers:**
- `screen_chat.c` (nếu có) - Handle STT/TTS events để hiển thị messages
- `screen_home.c` - Handle emotion events để update emotion display

**Ví dụ:**
```c
// Trong screen handler
if (evt.type == SX_EVT_CHATBOT_STT) {
    const char *text = (const char *)evt.ptr;
    // Update UI với user message
    lv_label_set_text(s_user_message_label, text);
    free((void *)evt.ptr);
}
```

### 2. Audio Streaming

**Chưa implement:**
- Binary audio protocol (v2/v3) như repo mẫu
- Gửi audio từ mic → server
- Nhận audio response → speaker

**Cần thêm:**
- `sx_protocol_ws_send_audio()` - Gửi audio qua WebSocket
- `sx_protocol_mqtt_send_audio()` - Gửi audio qua MQTT
- Audio callbacks trong protocol layer

### 3. Connection State Management

**Cần thêm:**
- Emit `SX_EVT_CHATBOT_CONNECTED` khi protocol connected
- Emit `SX_EVT_CHATBOT_DISCONNECTED` khi protocol disconnected
- Update protocol availability flags

### 4. Error Handling

**Cần thêm:**
- Retry logic khi send message fails
- Error events cho UI
- Timeout handling

---

## 📝 Settings Keys

### WebSocket
- `websocket_url` - WebSocket server URL
- `websocket_token` - Authorization token

### MQTT
- `mqtt_broker` - MQTT broker URI
- `mqtt_client_id` - Client ID
- `mqtt_username` - Username
- `mqtt_password` - Password
- `mqtt_publish_topic` - Publish topic
- `mqtt_subscribe_topic` - Subscribe topic

---

## ✅ Kết Luận

**Đã triển khai:**
- ✅ JSON message parsing (stt, tts, llm, mcp)
- ✅ Protocol integration với chatbot service
- ✅ Settings integration
- ✅ Event system cho UI updates
- ✅ WebSocket và MQTT support

**Cần hoàn thiện:**
- ⚠️ UI event handlers trong screens
- ⚠️ Audio streaming support
- ⚠️ Connection state management
- ⚠️ Error handling

**Hệ thống chatbot đã sẵn sàng để sử dụng với text messages qua WebSocket hoặc MQTT!**



