# Phân Tích Repo Mẫu - Audio Streaming Implementation

## 📋 Tổng Quan

Phân tích chi tiết cách repo mẫu (`xiaozhi-esp32_vietnam_ref`) implement audio streaming để hoàn thiện repo chính.

---

## 🔍 Architecture Repo Mẫu

### 1. Audio Service Architecture

**File:** `main/audio/audio_service.h`, `audio_service.cc`

**Flow:**
```
1. MIC → [Audio Processor] → {Encode Queue} → [Opus Encoder] → {Send Queue} → Protocol
2. Protocol → {Decode Queue} → [Opus Decoder] → {Playback Queue} → Speaker
```

**Queues:**
- `audio_encode_queue_` - PCM data để encode
- `audio_send_queue_` - Opus packets để gửi
- `audio_decode_queue_` - Opus packets từ server
- `audio_playback_queue_` - PCM data để phát

**Tasks:**
- `AudioInputTask` - Đọc từ MIC, process, push vào encode queue
- `AudioOutputTask` - Đọc từ playback queue, output đến speaker
- `OpusCodecTask` - Encode/Decode Opus packets

### 2. Opus Codec

**Encoder:**
```cpp
opus_encoder_ = std::make_unique<OpusEncoderWrapper>(16000, 1, OPUS_FRAME_DURATION_MS);
opus_encoder_->SetComplexity(0);
```

**Decoder:**
```cpp
opus_decoder_ = std::make_unique<OpusDecoderWrapper>(codec->output_sample_rate(), 1, OPUS_FRAME_DURATION_MS);
```

**OpusCodecTask:**
```cpp
void AudioService::OpusCodecTask() {
    while (!service_stopped_) {
        // Decode từ decode queue
        if (!audio_decode_queue_.empty()) {
            auto packet = std::move(audio_decode_queue_.front());
            audio_decode_queue_.pop_front();
            
            SetDecodeSampleRate(packet->sample_rate, packet->frame_duration);
            if (opus_decoder_->Decode(std::move(packet->payload), task->pcm)) {
                // Resample nếu cần
                if (opus_decoder_->sample_rate() != codec_->output_sample_rate()) {
                    output_resampler_.Process(...);
                }
                audio_playback_queue_.push_back(std::move(task));
            }
        }
        
        // Encode từ encode queue
        if (!audio_encode_queue_.empty()) {
            auto task = std::move(audio_encode_queue_.front());
            audio_encode_queue_.pop_front();
            
            auto packet = std::make_unique<AudioStreamPacket>();
            packet->frame_duration = OPUS_FRAME_DURATION_MS;
            packet->sample_rate = 16000;
            packet->timestamp = task->timestamp;
            if (opus_encoder_->Encode(std::move(task->pcm), packet->payload)) {
                audio_send_queue_.push_back(std::move(packet));
            }
        }
    }
}
```

### 3. Protocol Integration

**Application Layer:**
```cpp
protocol_->OnIncomingAudio([this](std::unique_ptr<AudioStreamPacket> packet) {
    if (device_state_ == kDeviceStateSpeaking) {
        audio_service_.PushPacketToDecodeQueue(std::move(packet));
    }
});
```

**Main Event Loop:**
```cpp
if (bits & MAIN_EVENT_SEND_AUDIO) {
    while (auto packet = audio_service_.PopPacketFromSendQueue()) {
        if (protocol_ && !protocol_->SendAudio(std::move(packet))) {
            break;
        }
    }
}
```

### 4. WebSocket Audio

**Send Audio:**
```cpp
bool WebsocketProtocol::SendAudio(std::unique_ptr<AudioStreamPacket> packet) {
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

**Receive Audio:**
```cpp
websocket_->OnData([this](const char* data, size_t len, bool binary) {
    if (binary) {
        if (version_ == 2) {
            BinaryProtocol2* bp2 = (BinaryProtocol2*)data;
            // Parse và tạo AudioStreamPacket
            on_incoming_audio_(std::make_unique<AudioStreamPacket>(...));
        }
    }
});
```

### 5. MQTT Audio (UDP)

**Send Audio:**
```cpp
bool MqttProtocol::SendAudio(std::unique_ptr<AudioStreamPacket> packet) {
    // AES encryption
    std::string nonce(aes_nonce_);
    *(uint16_t*)&nonce[2] = htons(packet->payload.size());
    *(uint32_t*)&nonce[8] = htonl(packet->timestamp);
    *(uint32_t*)&nonce[12] = htonl(++local_sequence_);
    
    // Encrypt
    mbedtls_aes_crypt_ctr(&aes_ctx_, packet->payload.size(), ...);
    
    // Send via UDP
    return udp_->Send(encrypted) > 0;
}
```

**Receive Audio:**
```cpp
udp_->OnMessage([this](const std::string& data) {
    // Parse UDP packet format:
    // |type 1u|flags 1u|payload_len 2u|ssrc 4u|timestamp 4u|sequence 4u|
    // |payload payload_len|
    
    // Decrypt
    mbedtls_aes_crypt_ctr(&aes_ctx_, decrypted_size, ...);
    
    // Create AudioStreamPacket
    auto packet = std::make_unique<AudioStreamPacket>();
    packet->payload = decrypted_data;
    
    // Callback
    on_incoming_audio_(std::move(packet));
});
```

**UDP Packet Format:**
```
|type 1u|flags 1u|payload_len 2u|ssrc 4u|timestamp 4u|sequence 4u|
|payload payload_len|
```

### 6. State Management

**Device States:**
- `kDeviceStateIdle` - Standby
- `kDeviceStateConnecting` - Đang kết nối protocol
- `kDeviceStateListening` - Đang nghe (MIC active)
- `kDeviceStateSpeaking` - Đang phát (Speaker active)

**Audio Flow Control:**
- Listening mode: Enable MIC → Encode → Send
- Speaking mode: Receive → Decode → Playback

---

## 📊 So Sánh với Repo Chính

| Tính Năng | Repo Mẫu | Repo Chính | Action |
|-----------|----------|------------|--------|
| **Opus Encoder** | ✅ OpusEncoderWrapper | ✅ sx_codec_opus_encode | ✅ OK |
| **Opus Decoder** | ✅ OpusDecoderWrapper | ❌ Chưa có | ⚠️ Cần thêm |
| **Audio Queues** | ✅ Encode/Decode/Send/Playback | ⚠️ Bridge queues | ⚠️ Cần cải thiện |
| **OpusCodecTask** | ✅ Single task encode/decode | ⚠️ Separate tasks | ⚠️ Có thể tối ưu |
| **WebSocket Audio** | ✅ Full support | ✅ Implemented | ✅ OK |
| **MQTT Audio (UDP)** | ✅ Full support | ❌ Chưa có | ⚠️ Cần implement |
| **State Management** | ✅ Device states | ⚠️ Cần integration | ⚠️ Cần thêm |
| **Resampling** | ✅ Input/Output resamplers | ❌ Chưa có | ⚠️ Cần thêm |

---

## 🎯 Cần Hoàn Thiện cho Repo Chính

### 1. Opus Decoder

**Vấn đề:** `esp-opus-encoder` component chỉ có encoder.

**Giải pháp từ repo mẫu:**
- Repo mẫu dùng `OpusDecoderWrapper` từ component `78/esp-opus-encoder`
- Component này có thể có cả encoder và decoder, hoặc có component riêng

**Cần kiểm tra:**
- Component `78/esp-opus-encoder` có decoder không?
- Hoặc cần thêm component decoder riêng

**Implementation:**
```c
// Cần implement tương tự OpusEncoderWrapper
esp_err_t sx_codec_opus_decode(const uint8_t *opus_data, size_t opus_size,
                                int16_t *pcm, size_t pcm_capacity, size_t *pcm_samples) {
    // Decode Opus to PCM
    // Sử dụng libopus decoder hoặc component decoder
}
```

### 2. MQTT Audio Support (UDP)

**Cần implement:**
- UDP channel creation
- AES encryption/decryption
- UDP packet format parsing
- Sequence number management

**UDP Packet Format:**
```c
typedef struct __attribute__((packed)) {
    uint8_t type;           // 0x01 = audio
    uint8_t flags;          // Reserved
    uint16_t payload_len;   // Payload size (network byte order)
    uint32_t ssrc;          // SSRC (network byte order)
    uint32_t timestamp;     // Timestamp (network byte order)
    uint32_t sequence;      // Sequence number (network byte order)
    uint8_t payload[];      // Encrypted Opus data
} mqtt_udp_audio_packet_t;
```

**AES Encryption:**
- Sử dụng mbedtls AES CTR mode
- Nonce từ server hello message
- Key từ server hello message

### 3. Audio Queue Management

**Repo mẫu dùng:**
- Separate queues cho encode/decode/send/playback
- Single OpusCodecTask xử lý cả encode và decode
- Mutex và condition variable cho synchronization

**Repo chính hiện tại:**
- Audio bridge có queues riêng
- Separate tasks cho send/receive
- Có thể tối ưu bằng cách combine tasks

### 4. Resampling

**Repo mẫu có:**
- Input resampler (MIC sample rate → 16kHz)
- Output resampler (decoded sample rate → codec output rate)

**Repo chính cần:**
- Input resampler nếu MIC rate khác 16kHz
- Output resampler nếu server rate khác codec rate

### 5. State Management Integration

**Repo mẫu:**
- Audio sending chỉ khi `kDeviceStateListening`
- Audio receiving chỉ khi `kDeviceStateSpeaking`
- State transitions trigger audio enable/disable

**Repo chính cần:**
- Integration với device state machine
- Enable audio send khi listening
- Enable audio receive khi speaking

---

## 🔧 Implementation Plan

### Phase 1: Opus Decoder

1. **Kiểm tra component:**
   - Check `78/esp-opus-encoder` có decoder không
   - Hoặc tìm decoder component khác

2. **Implement decoder wrapper:**
   - Tạo C wrapper cho Opus decoder
   - Tương tự encoder wrapper

3. **Integration:**
   - Update `sx_codec_opus.c` với decoder
   - Test với audio packets

### Phase 2: MQTT Audio (UDP)

1. **UDP Channel:**
   - Tạo UDP client trong MQTT protocol
   - Connect đến UDP server từ hello message

2. **AES Encryption:**
   - Parse key/nonce từ hello message
   - Implement encryption/decryption

3. **Packet Format:**
   - Implement UDP packet serialization
   - Implement UDP packet parsing

4. **Integration:**
   - `sx_protocol_mqtt_send_audio()`
   - `sx_protocol_mqtt_set_audio_callback()`

### Phase 3: Audio Queue Optimization

1. **Combine Tasks:**
   - Tạo single OpusCodecTask
   - Xử lý cả encode và decode

2. **Queue Management:**
   - Separate queues như repo mẫu
   - Better synchronization

### Phase 4: Resampling

1. **Input Resampler:**
   - Resample MIC data to 16kHz

2. **Output Resampler:**
   - Resample decoded data to codec rate

### Phase 5: State Integration

1. **State Machine:**
   - Integration với device states
   - Enable/disable audio based on state

---

## 📝 Chi Tiết Kỹ Thuật

### Opus Frame Duration

**Repo mẫu:** `OPUS_FRAME_DURATION_MS = 60` (60ms frames)

**Repo chính:** `20ms` frames

**Lưu ý:** Có thể điều chỉnh, nhưng cần match với server.

### Sample Rates

**Encoder:** 16000 Hz (fixed)
**Decoder:** Dynamic (từ server hello message)

### Queue Sizes

**Repo mẫu:**
- `MAX_DECODE_PACKETS_IN_QUEUE = 2400 / OPUS_FRAME_DURATION_MS` (~40 packets)
- `MAX_SEND_PACKETS_IN_QUEUE = 2400 / OPUS_FRAME_DURATION_MS` (~40 packets)

**Repo chính:**
- `AUDIO_SEND_QUEUE_SIZE = 10`
- `AUDIO_RECEIVE_QUEUE_SIZE = 10`

**Khuyến nghị:** Tăng queue sizes để buffer tốt hơn.

---

## ✅ Kết Luận

**Repo mẫu có:**
- ✅ Opus encoder/decoder (OpusEncoderWrapper/OpusDecoderWrapper)
- ✅ Queue-based audio processing
- ✅ Single OpusCodecTask cho encode/decode
- ✅ WebSocket và MQTT audio support
- ✅ Resampling support
- ✅ State-based audio control

**Repo chính cần:**
- ⚠️ Opus decoder implementation
- ⚠️ MQTT audio với UDP
- ⚠️ Queue optimization
- ⚠️ Resampling support
- ⚠️ State integration

**Ưu tiên:**
1. **High:** Opus decoder (cần để audio receiving hoạt động)
2. **High:** MQTT audio support
3. **Medium:** Queue optimization
4. **Medium:** Resampling
5. **Low:** State integration (có thể làm sau)

