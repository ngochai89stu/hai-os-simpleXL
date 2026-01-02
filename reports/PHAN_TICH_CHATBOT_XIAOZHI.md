# Phân Tích Cách Repo Mẫu Gọi Chatbot Xiaozhi

## 📋 Tổng Quan

Phân tích cách repo mẫu (`xiaozhi-esp32_vietnam_ref`) kết nối và gọi chatbot xiaozhi, và đảm bảo repo chính (`hai-os-simplexl`) cũng có thể gọi chatbot tương tự.

---

## 🔍 Cách Repo Mẫu Gọi Chatbot

### 1. Protocol Layer (WebSocket hoặc MQTT)

**File:** `D:\xiaozhi-esp32_vietnam_ref\main\application.cc`

Repo mẫu sử dụng **Protocol Pattern** để kết nối với chatbot server:

```cpp
// Chọn protocol dựa trên OTA config
if (ota.HasMqttConfig()) {
    protocol_ = std::make_unique<MqttProtocol>();
} else if (ota.HasWebsocketConfig()) {
    protocol_ = std::make_unique<WebsocketProtocol>();
} else {
    protocol_ = std::make_unique<MqttProtocol>(); // Default
}
```

### 2. WebSocket Protocol

**File:** `D:\xiaozhi-esp32_vietnam_ref\main\protocols\websocket_protocol.cc`

**Cấu hình:**
- Settings namespace: `"websocket"`
- Keys:
  - `url` - WebSocket server URL (wss://...)
  - `token` - Authorization token (Bearer token)
  - `version` - Protocol version (2, 3, hoặc default)

**Kết nối:**
```cpp
bool WebsocketProtocol::OpenAudioChannel() {
    Settings settings("websocket", false);
    std::string url = settings.GetString("url");
    std::string token = settings.GetString("token");
    
    websocket_ = network->CreateWebSocket(1);
    websocket_->SetHeader("Authorization", token.c_str());
    websocket_->SetHeader("Protocol-Version", std::to_string(version_).c_str());
    websocket_->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    websocket_->SetHeader("Client-Id", Board::GetInstance().GetUuid().c_str());
    
    websocket_->Connect(url.c_str());
    // Send hello message
    SendText(GetHelloMessage());
    // Wait for server hello
}
```

**Gửi Audio:**
```cpp
bool WebsocketProtocol::SendAudio(std::unique_ptr<AudioStreamPacket> packet) {
    // Binary protocol v2 hoặc v3
    if (version_ == 2) {
        BinaryProtocol2* bp2 = ...;
        bp2->version = htons(version_);
        bp2->type = 0;
        bp2->timestamp = htonl(packet->timestamp);
        bp2->payload_size = htonl(packet->payload.size());
        memcpy(bp2->payload, packet->payload.data(), ...);
        return websocket_->Send(serialized.data(), serialized.size(), true);
    }
    // ...
}
```

**Nhận Audio:**
```cpp
websocket_->OnData([this](const char* data, size_t len, bool binary) {
    if (binary) {
        // Parse binary protocol
        BinaryProtocol2* bp2 = (BinaryProtocol2*)data;
        // ... parse và gọi on_incoming_audio_
        on_incoming_audio_(std::make_unique<AudioStreamPacket>(...));
    } else {
        // Parse JSON
        cJSON* root = cJSON_Parse(data);
        if (on_incoming_json_ != nullptr) {
            on_incoming_json_(root);
        }
    }
});
```

### 3. MQTT Protocol

**File:** `D:\xiaozhi-esp32_vietnam_ref\main\protocols\mqtt_protocol.cc`

**Cấu hình:**
- Settings namespace: `"mqtt"`
- Keys:
  - `endpoint` - MQTT broker (host:port)
  - `client_id` - MQTT client ID
  - `username` - MQTT username
  - `password` - MQTT password
  - `publish_topic` - Topic để publish messages
  - `keepalive` - Keepalive interval (default 240)

**Kết nối:**
```cpp
bool MqttProtocol::OpenAudioChannel() {
    Settings settings("mqtt", false);
    auto endpoint = settings.GetString("endpoint");
    auto client_id = settings.GetString("client_id");
    auto username = settings.GetString("username");
    auto password = settings.GetString("password");
    
    mqtt_ = network->CreateMqtt(0);
    mqtt_->Connect(broker_address, broker_port, client_id, username, password);
    // Subscribe to topics
}
```

**Gửi Text:**
```cpp
bool MqttProtocol::SendText(const std::string& text) {
    return mqtt_->Publish(publish_topic_, text);
}
```

### 4. Application Layer - Xử Lý Messages

**File:** `D:\xiaozhi-esp32_vietnam_ref\main\application.cc`

**Callbacks:**
```cpp
protocol_->OnIncomingAudio([this](std::unique_ptr<AudioStreamPacket> packet) {
    if (device_state_ == kDeviceStateSpeaking) {
        audio_service_.PushPacketToDecodeQueue(std::move(packet));
    }
});

protocol_->OnIncomingJson([this, display](const cJSON* root) {
    auto type = cJSON_GetObjectItem(root, "type");
    
    if (strcmp(type->valuestring, "tts") == 0) {
        // TTS response từ server
        auto state = cJSON_GetObjectItem(root, "state");
        if (strcmp(state->valuestring, "start") == 0) {
            SetDeviceState(kDeviceStateSpeaking);
        } else if (strcmp(state->valuestring, "sentence_start") == 0) {
            auto text = cJSON_GetObjectItem(root, "text");
            display->SetChatMessage("assistant", text->valuestring);
        }
    } else if (strcmp(type->valuestring, "stt") == 0) {
        // STT result từ server
        auto text = cJSON_GetObjectItem(root, "text");
        display->SetChatMessage("user", text->valuestring);
    } else if (strcmp(type->valuestring, "llm") == 0) {
        // LLM emotion
        auto emotion = cJSON_GetObjectItem(root, "emotion");
        display->SetEmotion(emotion->valuestring);
    } else if (strcmp(type->valuestring, "mcp") == 0) {
        // MCP message
        auto payload = cJSON_GetObjectItem(root, "payload");
        McpServer::GetInstance().ParseMessage(payload);
    }
});
```

**Gửi Audio:**
```cpp
void Application::MainEventLoop() {
    if (bits & MAIN_EVENT_SEND_AUDIO) {
        auto packet = audio_service_.PopPacketFromSendQueue();
        if (packet != nullptr && protocol_->IsAudioChannelOpened()) {
            protocol_->SendAudio(std::move(packet));
        }
    }
}
```

**Mở Audio Channel:**
```cpp
void Application::ToggleChatState() {
    if (device_state_ == kDeviceStateIdle) {
        if (!protocol_->IsAudioChannelOpened()) {
            SetDeviceState(kDeviceStateConnecting);
            protocol_->OpenAudioChannel();
        }
        SetListeningMode(...);
    }
}
```

---

## ✅ Repo Chính (SimpleXL) - Hiện Trạng

### 1. Chatbot Service

**File:** `components/sx_services/sx_chatbot_service.c`

**Đã có:**
- ✅ `sx_chatbot_service_init()` - Khởi tạo service
- ✅ `sx_chatbot_service_start()` - Start service
- ✅ `sx_chatbot_send_message()` - Gửi text message
- ✅ `sx_chatbot_handle_mcp_message()` - Xử lý MCP messages
- ✅ MCP server integration
- ✅ Intent parsing integration

**Chưa có:**
- ❌ Audio streaming (gửi/nhận audio)
- ❌ Protocol integration (WebSocket/MQTT)
- ❌ JSON message parsing (stt, tts, llm)

### 2. Protocol Layer

**WebSocket:** `components/sx_protocol/sx_protocol_ws.c`
- ✅ `sx_protocol_ws_start()` - Kết nối WebSocket
- ✅ `sx_protocol_ws_send_text()` - Gửi text
- ✅ `sx_protocol_ws_is_connected()` - Check connection
- ✅ Nhận messages và gọi `sx_chatbot_handle_mcp_message()`

**MQTT:** `components/sx_protocol/sx_protocol_mqtt.c`
- ✅ `sx_protocol_mqtt_init()` - Khởi tạo MQTT
- ✅ `sx_protocol_mqtt_start()` - Kết nối MQTT
- ✅ `sx_protocol_mqtt_publish()` - Publish messages
- ✅ `sx_protocol_mqtt_subscribe()` - Subscribe topics
- ✅ Message callback

**Chưa có:**
- ❌ Audio streaming (binary protocol)
- ❌ Integration với chatbot service
- ❌ JSON message parsing

### 3. Orchestrator

**File:** `components/sx_core/sx_orchestrator.c`

**Đã có:**
- ✅ Route text messages đến chatbot service
- ✅ `sx_chatbot_send_message()` được gọi

**Chưa có:**
- ❌ Audio streaming integration
- ❌ Protocol layer integration

---

## 🔧 So Sánh và Gaps

### Repo Mẫu vs Repo Chính

| Tính Năng | Repo Mẫu | Repo Chính | Status |
|-----------|----------|------------|--------|
| WebSocket Protocol | ✅ Full (audio + text) | ✅ Text only | ⚠️ Thiếu audio |
| MQTT Protocol | ✅ Full (audio + text) | ✅ Text only | ⚠️ Thiếu audio |
| Audio Streaming | ✅ SendAudio() + OnIncomingAudio() | ❌ | ❌ Chưa có |
| JSON Message Parsing | ✅ stt, tts, llm, mcp | ⚠️ MCP only | ⚠️ Thiếu stt/tts/llm |
| Chatbot Service | ✅ Integrated | ✅ Có nhưng chưa tích hợp | ⚠️ Cần tích hợp |
| Settings Integration | ✅ websocket/mqtt namespace | ⚠️ Chưa có | ⚠️ Cần thêm |

---

## 🎯 Đề Xuất Triển Khai

### Phase 1: Protocol Integration với Chatbot

**1. Thêm Audio Streaming vào Protocol Layer**

**WebSocket (`sx_protocol_ws.c`):**
```c
// Thêm binary frame support
esp_err_t sx_protocol_ws_send_audio(const uint8_t *data, size_t len, uint32_t timestamp) {
    // Implement binary protocol v2/v3
    // Similar to repo mẫu
}

// Thêm audio callback
typedef void (*sx_protocol_ws_audio_cb_t)(const uint8_t *data, size_t len, uint32_t timestamp);
esp_err_t sx_protocol_ws_set_audio_callback(sx_protocol_ws_audio_cb_t callback);
```

**MQTT (`sx_protocol_mqtt.c`):**
```c
// Tương tự cho MQTT
esp_err_t sx_protocol_mqtt_send_audio(const uint8_t *data, size_t len, uint32_t timestamp);
```

**2. Tích Hợp Protocol với Chatbot Service**

**Trong `sx_chatbot_service.c`:**
```c
// Thêm protocol callbacks
static sx_protocol_ws_audio_cb_t s_audio_callback = NULL;

void sx_chatbot_set_protocol_callbacks(
    sx_protocol_ws_audio_cb_t audio_cb,
    sx_protocol_ws_text_cb_t text_cb
) {
    s_audio_callback = audio_cb;
    // ...
}
```

**3. JSON Message Parsing**

**Thêm vào `sx_protocol_ws.c` hoặc `sx_chatbot_service.c`:**
```c
static void parse_json_message(const char *json_str) {
    cJSON *root = cJSON_Parse(json_str);
    cJSON *type = cJSON_GetObjectItem(root, "type");
    
    if (strcmp(type->valuestring, "stt") == 0) {
        // Route to STT handler
        cJSON *text = cJSON_GetObjectItem(root, "text");
        // Emit event hoặc update UI
    } else if (strcmp(type->valuestring, "tts") == 0) {
        // Route to TTS handler
        cJSON *state = cJSON_GetObjectItem(root, "state");
        if (strcmp(state->valuestring, "sentence_start") == 0) {
            cJSON *text = cJSON_GetObjectItem(root, "text");
            // Update UI
        }
    } else if (strcmp(type->valuestring, "llm") == 0) {
        // Route to emotion handler
        cJSON *emotion = cJSON_GetObjectItem(root, "emotion");
        // Update display emotion
    } else if (strcmp(type->valuestring, "mcp") == 0) {
        // Route to MCP handler (đã có)
        sx_chatbot_handle_mcp_message(json_str);
    }
}
```

### Phase 2: Settings Integration

**Thêm Settings Keys:**

**WebSocket:**
- Namespace: `"websocket"`
- Keys: `url`, `token`, `version`

**MQTT:**
- Namespace: `"mqtt"`
- Keys: `endpoint`, `client_id`, `username`, `password`, `publish_topic`, `subscribe_topic`

**Trong `sx_bootstrap.c`:**
```c
// Load WebSocket config
char ws_url[256] = {0};
char ws_token[128] = {0};
sx_settings_get_string_default("websocket_url", ws_url, sizeof(ws_url), NULL);
sx_settings_get_string_default("websocket_token", ws_token, sizeof(ws_token), NULL);

if (ws_url[0] != '\0') {
    sx_protocol_ws_config_t ws_cfg = {
        .url = ws_url,
        .auth_token = (ws_token[0] != '\0') ? ws_token : NULL,
        .reconnect_ms = 5000,
    };
    sx_protocol_ws_start(&ws_cfg);
}

// Tương tự cho MQTT
```

### Phase 3: Audio Service Integration

**Kết nối Audio Service với Protocol:**

**Trong `sx_orchestrator.c` hoặc tạo `sx_audio_protocol_bridge.c`:**
```c
// Bridge audio service với protocol
void sx_audio_protocol_bridge_init(void) {
    // Subscribe to audio service events
    // When audio data available -> send via protocol
    // When protocol receives audio -> push to audio service
}
```

---

## 📝 Tóm Tắt

### Repo Mẫu
- ✅ Full-featured protocol layer (WebSocket + MQTT)
- ✅ Audio streaming (binary protocol v2/v3)
- ✅ JSON message parsing (stt, tts, llm, mcp)
- ✅ Settings integration
- ✅ Application layer integration

### Repo Chính
- ✅ Chatbot service (stub, có MCP support)
- ✅ Protocol layer (WebSocket + MQTT, text only)
- ⚠️ Thiếu audio streaming
- ⚠️ Thiếu JSON message parsing (stt, tts, llm)
- ⚠️ Chưa tích hợp protocol với chatbot
- ⚠️ Chưa có settings integration

### Kết Luận
**Repo chính CÓ THỂ gọi chatbot**, nhưng cần:
1. Thêm audio streaming vào protocol layer
2. Thêm JSON message parsing
3. Tích hợp protocol với chatbot service
4. Thêm settings integration
5. Kết nối audio service với protocol

**Ưu tiên:**
1. **High:** JSON message parsing (stt, tts, llm)
2. **High:** Protocol integration với chatbot
3. **Medium:** Audio streaming
4. **Medium:** Settings integration
5. **Low:** Audio service bridge


















