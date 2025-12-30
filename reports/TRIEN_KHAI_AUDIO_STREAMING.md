# Triển Khai Audio Streaming - Báo Cáo Tiến Độ

## 📋 Tổng Quan

Triển khai tính năng audio streaming (gửi/nhận audio) cho chatbot, cho phép giao tiếp bằng giọng nói thay vì chỉ text.

---

## ✅ Đã Hoàn Thành

### 1. Định Nghĩa Cấu Trúc Dữ Liệu

**File:** `components/sx_protocol/include/sx_protocol_audio.h`

- ✅ `sx_audio_stream_packet_t` - Cấu trúc packet audio
- ✅ `sx_binary_protocol_v2_t` - Binary protocol version 2
- ✅ `sx_binary_protocol_v3_t` - Binary protocol version 3
- ✅ `sx_protocol_audio_callback_t` - Callback type cho audio packets

### 2. WebSocket Protocol - Audio Support

**File:** `components/sx_protocol/sx_protocol_ws.c`

**Đã thêm:**
- ✅ `sx_protocol_ws_send_audio()` - Gửi audio packet qua WebSocket
- ✅ Binary frame parsing (v2/v3) - Nhận audio từ server
- ✅ Protocol version management
- ✅ Server hello message parsing (sample rate, frame duration)
- ✅ Audio callback registration

**Chi tiết:**
- Hỗ trợ BinaryProtocol v2 và v3
- Network byte order conversion (htonl/ntohl)
- Memory management cho audio packets
- Error handling

### 3. Audio Protocol Bridge Service

**File:** `components/sx_services/sx_audio_protocol_bridge.c`

**Đã tạo:**
- ✅ Service kết nối audio service với protocol layer
- ✅ Opus encoder initialization
- ✅ Audio send task (ghi âm và gửi)
- ✅ Audio receive task (nhận và phát)
- ✅ Queue management cho audio packets

**Tính năng:**
- Enable/disable audio sending
- Enable/disable audio receiving
- Opus encoding cho audio gửi đi
- Queue-based audio packet handling

### 4. CMakeLists.txt Update

**File:** `components/sx_services/CMakeLists.txt`

- ✅ Thêm `sx_audio_protocol_bridge.c` vào build

---

## ⚠️ Đang Thực Hiện

### 1. Audio Send Task - I2S Integration

**Vấn đề:** Cần truy cập I2S RX channel để đọc PCM data.

**Giải pháp đề xuất:**
- Option 1: Thêm callback vào audio service để nhận PCM data
- Option 2: Sử dụng shared I2S channel (cần synchronization)
- Option 3: Tạo I2S channel riêng cho audio bridge

**Hiện tại:** Code đã có sẵn, cần test và điều chỉnh.

### 2. Opus Decoder Integration

**Vấn đề:** `esp-opus-encoder` library chỉ cung cấp encoder, không có decoder.

**Giải pháp đề xuất:**
- Option 1: Sử dụng libopus decoder (cần port cho ESP32)
- Option 2: Sử dụng ESP-ADF Opus decoder (nếu có)
- Option 3: Tạm thời skip decoder, chỉ implement encoder

**Hiện tại:** Decoder chưa implement, cần thêm library.

### 3. MQTT Protocol - Audio Support

**File:** `components/sx_protocol/sx_protocol_mqtt.c`

**Cần thêm:**
- ⚠️ `sx_protocol_mqtt_send_audio()` - Gửi audio qua UDP (như repo mẫu)
- ⚠️ UDP audio packet parsing - Nhận audio từ UDP
- ⚠️ AES encryption/decryption cho UDP packets
- ⚠️ Audio callback registration

**Lưu ý:** MQTT dùng UDP cho audio streaming (như repo mẫu).

---

## 📝 Cần Hoàn Thiện

### 1. Audio Service Integration

**Cần thêm vào `sx_audio_service.h`:**
```c
// Callback for PCM data from recording
typedef void (*sx_audio_recording_callback_t)(const int16_t *pcm, size_t samples, uint32_t sample_rate);
esp_err_t sx_audio_set_recording_callback(sx_audio_recording_callback_t callback);
```

**Hoặc:**
- Export I2S RX channel handle (không khuyến khích)
- Tạo shared queue cho PCM data

### 2. Opus Decoder Implementation

**Cần:**
- Thêm Opus decoder library
- Implement `sx_codec_opus_decode()` function
- Test với audio packets từ server

### 3. MQTT Audio Support

**Cần implement:**
- UDP channel management
- AES encryption/decryption
- Audio packet serialization/deserialization
- Integration với MQTT hello message

### 4. Bootstrap Integration

**Cần thêm vào `sx_bootstrap.c`:**
```c
// Initialize audio protocol bridge
esp_err_t bridge_ret = sx_audio_protocol_bridge_init();
if (bridge_ret == ESP_OK) {
    bridge_ret = sx_audio_protocol_bridge_start();
    // Enable when protocol connected
}
```

### 5. Orchestrator Integration

**Cần thêm:**
- Enable audio bridge khi protocol connected
- Disable audio bridge khi protocol disconnected
- Handle audio state changes

---

## 🔧 Chi Tiết Kỹ Thuật

### Binary Protocol v2

```c
typedef struct __attribute__((packed)) {
    uint16_t version;          // 2 (network byte order)
    uint16_t type;             // 0 = OPUS, 1 = JSON
    uint32_t reserved;         // Reserved
    uint32_t timestamp;        // Timestamp in ms (network byte order)
    uint32_t payload_size;     // Payload size (network byte order)
    uint8_t payload[];         // Opus data
} sx_binary_protocol_v2_t;
```

### Binary Protocol v3

```c
typedef struct __attribute__((packed)) {
    uint8_t type;              // 0 = OPUS, 1 = JSON
    uint8_t reserved;          // Reserved
    uint16_t payload_size;      // Payload size (network byte order)
    uint8_t payload[];         // Opus data
} sx_binary_protocol_v3_t;
```

### Opus Configuration

- **Sample Rate:** 16000 Hz (default)
- **Channels:** 1 (mono)
- **Application:** VOIP (0)
- **Bitrate:** 16000 bps
- **Frame Duration:** 20 ms

---

## 📊 Tiến Độ

| Tính Năng | Trạng Thái | Ghi Chú |
|-----------|------------|---------|
| Audio Structures | ✅ Hoàn thành | |
| WebSocket Audio Send | ✅ Hoàn thành | |
| WebSocket Audio Receive | ✅ Hoàn thành | |
| Opus Encoder | ✅ Hoàn thành | Cần test |
| Opus Decoder | ⚠️ Chưa có | Cần library |
| Audio Bridge Service | ✅ Hoàn thành | Cần test I2S |
| MQTT Audio Support | ⚠️ Chưa có | Cần implement |
| Bootstrap Integration | ⚠️ Chưa có | |
| Orchestrator Integration | ⚠️ Chưa có | |

---

## 🎯 Bước Tiếp Theo

### Ưu Tiên Cao

1. **Test Audio Send Task**
   - Verify I2S access
   - Test Opus encoding
   - Test WebSocket sending

2. **Implement Opus Decoder**
   - Tìm/port Opus decoder library
   - Implement decode function
   - Test với audio packets

3. **Bootstrap Integration**
   - Initialize audio bridge
   - Enable khi protocol connected

### Ưu Tiên Trung Bình

4. **MQTT Audio Support**
   - Implement UDP channel
   - AES encryption
   - Audio packet handling

5. **Orchestrator Integration**
   - Audio state management
   - Enable/disable logic

### Ưu Tiên Thấp

6. **Error Handling**
   - Retry logic
   - Error recovery
   - Timeout handling

7. **Performance Optimization**
   - Buffer management
   - Memory optimization
   - CPU usage optimization

---

## 📝 Lưu Ý

1. **I2S Access:** Audio bridge cần truy cập I2S RX channel. Hiện tại code giả định có function `sx_platform_get_i2s_rx_channel()`, cần implement hoặc sử dụng cách khác.

2. **Opus Decoder:** `esp-opus-encoder` chỉ có encoder. Cần tìm decoder library hoặc implement riêng.

3. **MQTT UDP:** MQTT protocol dùng UDP cho audio (như repo mẫu), cần implement UDP channel management.

4. **Memory Management:** Audio packets cần được allocate/free đúng cách để tránh memory leak.

5. **Synchronization:** Cần đảm bảo audio send/receive không conflict với audio service hiện có.

---

## ✅ Kết Luận

**Đã hoàn thành:**
- ✅ Cấu trúc dữ liệu và protocol definitions
- ✅ WebSocket audio send/receive
- ✅ Audio bridge service framework
- ✅ Opus encoder integration

**Cần hoàn thiện:**
- ⚠️ I2S access và PCM data collection
- ⚠️ Opus decoder implementation
- ⚠️ MQTT audio support
- ⚠️ Bootstrap và orchestrator integration

**Trạng thái:** ~60% hoàn thành, cần test và hoàn thiện các phần còn lại.

