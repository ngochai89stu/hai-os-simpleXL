# Hoàn Thiện MQTT Audio Support

## 📋 Tổng Quan

Đã implement MQTT audio support với UDP channel và AES encryption, dựa trên phân tích repo mẫu (`xiaozhi-esp32_vietnam_ref`).

---

## ✅ Đã Hoàn Thành

### 1. UDP Audio Channel Implementation

**File:** `components/sx_protocol/sx_protocol_mqtt_udp.c`

**Features:**
- UDP socket creation và connection
- AES CTR mode encryption/decryption
- UDP packet format parsing (như repo mẫu)
- Sequence number management
- Non-blocking socket với receive task
- Thread-safe với mutex protection

**UDP Packet Format:**
```
|type 1u|flags 1u|payload_len 2u|ssrc 4u|timestamp 4u|sequence 4u|
|payload payload_len|
```

### 2. AES Encryption

**Implementation:**
- Hex string decoding (key và nonce từ server hello)
- mbedtls AES CTR mode
- Nonce update với payload size, timestamp, sequence
- Encryption/decryption cho audio packets

### 3. MQTT Protocol Integration

**File:** `components/sx_protocol/sx_protocol_mqtt.c`

**Changes:**
- Parse hello message để lấy UDP info
- Extract audio parameters (sample rate, frame duration)
- Initialize UDP channel từ hello message
- Handle goodbye message để close UDP channel
- Audio send/receive functions

**Hello Message Parsing:**
```c
// Parse "hello" message type
if (strcmp(msg_type, "hello") == 0) {
    // Get transport (must be "udp")
    // Get audio_params (sample_rate, frame_duration)
    // Get udp (server, port, key, nonce)
    // Initialize UDP channel
}
```

### 4. API Functions

**Header:** `components/sx_protocol/include/sx_protocol_mqtt.h`

**New Functions:**
- `sx_protocol_mqtt_send_audio()` - Send audio packet via UDP
- `sx_protocol_mqtt_set_audio_callback()` - Set callback for incoming audio
- `sx_protocol_mqtt_get_server_sample_rate()` - Get server sample rate
- `sx_protocol_mqtt_get_server_frame_duration()` - Get server frame duration
- `sx_protocol_mqtt_is_audio_channel_opened()` - Check if UDP channel is open

### 5. Build Integration

**File:** `components/sx_services/CMakeLists.txt`

**Changes:**
- Added `sx_protocol_mqtt_udp.c` to SRCS

---

## 🔍 Phân Tích Repo Mẫu

### UDP Channel Creation

**Repo mẫu:**
```cpp
udp_ = network->CreateUdp(2);
udp_->OnMessage([this](const std::string& data) {
    // Parse và decrypt UDP packet
    // Call on_incoming_audio_ callback
});
udp_->Connect(udp_server_, udp_port_);
```

**Repo chính:**
```c
s_udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
// Setup và connect
xTaskCreate(udp_receive_task, ...);  // Receive task
```

### AES Encryption

**Repo mẫu:**
```cpp
std::string nonce(aes_nonce_);
*(uint16_t*)&nonce[2] = htons(packet->payload.size());
*(uint32_t*)&nonce[8] = htonl(packet->timestamp);
*(uint32_t*)&nonce[12] = htonl(++local_sequence_);

mbedtls_aes_crypt_ctr(&aes_ctx_, packet->payload.size(), &nc_off,
    (uint8_t*)nonce.c_str(), stream_block,
    (uint8_t*)packet->payload.data(), (uint8_t*)&encrypted[nonce.size()]);
```

**Repo chính:**
```c
uint8_t nonce[16];
memcpy(nonce, s_aes_nonce, sizeof(nonce));
*(uint16_t *)&nonce[2] = htons(packet->payload_size);
*(uint32_t *)&nonce[8] = htonl(packet->timestamp);
*(uint32_t *)&nonce[12] = htonl(++s_local_sequence);

mbedtls_aes_crypt_ctr(&s_aes_ctx, packet->payload_size, &nc_off,
                     nonce, stream_block, packet->payload,
                     encrypted + sizeof(nonce));
```

### Hello Message Parsing

**Repo mẫu:**
```cpp
void MqttProtocol::ParseServerHello(const cJSON* root) {
    // Get transport, session_id, audio_params, udp
    aes_nonce_ = DecodeHexString(nonce);
    mbedtls_aes_init(&aes_ctx_);
    mbedtls_aes_setkey_enc(&aes_ctx_, DecodeHexString(key).c_str(), 128);
    // Create UDP và connect
}
```

**Repo chính:**
```c
// Parse trong mqtt_event_handler
if (strcmp(msg_type, "hello") == 0) {
    // Get transport, audio_params, udp
    sx_protocol_mqtt_udp_init(server, port, key, nonce);
}
```

---

## ⚠️ Cần Hoàn Thiện

### 1. Bootstrap Integration

**Cần:**
- Set MQTT audio callback trong bootstrap
- Register callback với audio bridge service
- Handle audio channel opened event

**Priority:** High

### 2. Event Support

**Cần:**
- Add `SX_EVT_CHATBOT_AUDIO_CHANNEL_OPENED` event
- Handle event trong orchestrator
- Enable audio receiving khi channel opened

**Priority:** High

### 3. Error Handling

**Cần:**
- Retry logic khi UDP send fail
- Timeout handling
- Connection recovery

**Priority:** Medium

### 4. Testing

**Cần:**
- Test UDP channel creation
- Test AES encryption/decryption
- Test audio send/receive
- Test với real MQTT server

**Priority:** High

---

## 📊 So Sánh với Repo Mẫu

| Tính Năng | Repo Mẫu | Repo Chính | Status |
|-----------|----------|------------|--------|
| **UDP Channel** | ✅ Network->CreateUdp() | ✅ socket() API | ✅ OK |
| **AES Encryption** | ✅ mbedtls AES CTR | ✅ mbedtls AES CTR | ✅ OK |
| **Packet Format** | ✅ Custom format | ✅ Same format | ✅ OK |
| **Sequence Management** | ✅ local/remote sequence | ✅ local/remote sequence | ✅ OK |
| **Hello Parsing** | ✅ ParseServerHello() | ✅ In event handler | ✅ OK |
| **Receive Task** | ✅ UDP OnMessage | ✅ Receive task | ✅ OK |

---

## 🎯 Testing Checklist

### UDP Channel

- [ ] Test UDP socket creation
- [ ] Test hostname resolution
- [ ] Test UDP connection
- [ ] Test socket close

### AES Encryption

- [ ] Test hex string decoding
- [ ] Test AES key setup
- [ ] Test encryption
- [ ] Test decryption
- [ ] Test nonce update

### Audio Send/Receive

- [ ] Test audio packet sending
- [ ] Test audio packet receiving
- [ ] Test sequence validation
- [ ] Test callback invocation

### Integration

- [ ] Test hello message parsing
- [ ] Test UDP channel initialization
- [ ] Test goodbye message handling
- [ ] Test với real MQTT server

---

## ✅ Kết Luận

**Đã hoàn thành:**
- ✅ UDP audio channel implementation
- ✅ AES encryption/decryption
- ✅ MQTT hello message parsing
- ✅ Audio send/receive functions
- ✅ Build integration

**Cần hoàn thiện:**
- ⚠️ Bootstrap integration (set callback)
- ⚠️ Event support (audio channel opened)
- ⚠️ Error handling improvements
- ⚠️ Testing với real server

**Trạng thái:** MQTT audio support đã implement xong, cần integrate vào bootstrap và test.

**Ưu tiên tiếp theo:**
1. **High:** Integrate vào bootstrap (set callback)
2. **High:** Add event support
3. **High:** Testing
4. **Medium:** Error handling improvements










