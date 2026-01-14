# Phân Tích Sâu Repo Mẫu 3: IRremoteESP8266

## 📋 Tổng Quan

**Repo:** [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266)  
**Mục đích:** Thư viện IR cho ESP8266/ESP32 - gửi và nhận tín hiệu hồng ngoại với nhiều protocol  
**Version:** v2.8.6 (Latest: Jul 28, 2023)  
**License:** LGPL-2.1  
**Stars:** 3.3k ⭐ | **Forks:** 898

---

## 🔍 Cấu Trúc Repo

### Thư Mục Chính

```
repo_mau_3_IRremoteESP8266/
├── src/                    # Source code chính
│   ├── ir_Sharp.cpp/h     # Sharp AC protocol
│   ├── ir_Toshiba.cpp/h   # Toshiba AC protocol
│   ├── IRsend.cpp/h       # IR transmitter
│   ├── IRrecv.cpp/h       # IR receiver
│   ├── IRac.cpp/h         # Universal AC control API
│   └── ... (100+ protocols)
├── examples/              # Ví dụ sử dụng
│   ├── TurnOnToshibaAC/   # Ví dụ Toshiba AC
│   └── ...
├── test/                  # Unit tests
└── docs/                  # Tài liệu
```

### Số Lượng Protocols Hỗ Trợ

- **100+ IR protocols** được hỗ trợ
- **50+ Air Conditioner brands** với hỗ trợ chi tiết
- **Full send & receive** cho hầu hết protocols

---

## 🎯 Phân Tích Tính Năng Sharp AC

### 1. Cấu Trúc Protocol

**File:** `src/ir_Sharp.h` và `src/ir_Sharp.cpp`

#### Protocol Structure

```cpp
union SharpProtocol {
    uint8_t raw[kSharpAcStateLength];  // 13 bytes
    struct {
        uint8_t Temp  :4;      // Byte 4: Nhiệt độ (15-30°C)
        uint8_t Model :1;      // Model type
        uint8_t PowerSpecial :4;  // Byte 5: Power state
        uint8_t Mode  :2;      // Byte 6: Mode (Auto/Cool/Heat/Dry)
        uint8_t Clean :1;
        uint8_t Fan   :3;      // Fan speed
        uint8_t TimerHours :4; // Byte 7: Timer
        uint8_t Swing :3;      // Byte 8: Swing vertical
        uint8_t Special :8;    // Byte 10: Special functions
        uint8_t Ion    :1;     // Byte 11: Ion filter
        uint8_t Sum    :4;     // Byte 12: Checksum
    };
};
```

#### Timing Constants

```cpp
const uint16_t kSharpAcHdrMark = 3800;   // Header mark: 3.8ms
const uint16_t kSharpAcHdrSpace = 1900; // Header space: 1.9ms
const uint16_t kSharpAcBitMark = 470;   // Bit mark: 470us
const uint16_t kSharpAcZeroSpace = 500;  // Zero space: 500us
const uint16_t kSharpAcOneSpace = 1400; // One space: 1400us
```

#### Models Hỗ Trợ

- **A907:** Model mới nhất (default)
- **A903:** Model cũ hơn
- **A705:** Model cũ nhất

#### Default State (Reset)

```cpp
static const uint8_t reset[kSharpAcStateLength] = {
    0xAA, 0x5A, 0xCF, 0x10, 0x00, 0x01, 0x00, 0x00, 
    0x08, 0x80, 0x00, 0xE0, 0x01
};
```

### 2. Tính Năng Chi Tiết

#### Power Control
- `on()` / `off()` - Bật/tắt
- `setPower(bool on, bool prev_on)` - Set power với trạng thái trước
- Hỗ trợ `PowerOnFromOff` - Bật từ trạng thái tắt

#### Temperature
- Range: **15-30°C**
- `setTemp(uint8_t temp)` - Set nhiệt độ
- Auto/Dry mode không cho phép thay đổi nhiệt độ

#### Mode
- `kSharpAcAuto` (0b00) - Tự động
- `kSharpAcCool` (0b10) - Làm mát
- `kSharpAcHeat` (0b01) - Sưởi (A907 only)
- `kSharpAcDry` (0b11) - Khô
- `kSharpAcFan` (0b00) - Quạt (A705 only)

#### Fan Speed
- `kSharpAcFanAuto` (0b010 = 2) - Tự động
- `kSharpAcFanMin` (0b100 = 4) - Thấp (FAN1)
- `kSharpAcFanMed` (0b011 = 3) - Trung (FAN2)
- `kSharpAcFanHigh` (0b101 = 5) - Cao (FAN3)
- `kSharpAcFanMax` (0b111 = 7) - Tối đa (FAN4)

#### Swing Vertical
- `kSharpAcSwingVIgnore` (0b000) - Không thay đổi
- `kSharpAcSwingVHigh` (0b001) - 0° down
- `kSharpAcSwingVMid` (0b011) - 30° down
- `kSharpAcSwingVLow` (0b100) - 45° down
- `kSharpAcSwingVLowest` (0b110) - 75° down (Heat mode only)
- `kSharpAcSwingVToggle` (0b111) - Toggle swing

#### Special Functions
- `kSharpAcSpecialTurbo` (0x01) - Turbo mode
- `kSharpAcSpecialTempEcono` (0x04) - Economy mode
- `kSharpAcSpecialFan` (0x05) - Fan control
- `kSharpAcSpecialSwing` (0x06) - Swing control
- `kSharpAcSpecialTimer` (0xC0) - Timer

#### Timer
- Increment: **30 phút**
- Max: **12 giờ** (720 phút)
- Hỗ trợ On Timer và Off Timer

### 3. Checksum Algorithm

```cpp
uint8_t IRSharpAc::calcChecksum(uint8_t state[], const uint16_t length) {
    uint8_t xorsum = xorBytes(state, length - 1);
    xorsum ^= GETBITS8(state[length - 1], kLowNibble, kNibbleSize);
    xorsum ^= GETBITS8(xorsum, kHighNibble, kNibbleSize);
    return GETBITS8(xorsum, kLowNibble, kNibbleSize);
}
```

**Checksum:** XOR tất cả bytes (trừ byte cuối), sau đó XOR với nibble thấp của byte cuối, rồi XOR với nibble cao của kết quả.

---

## 🎯 Phân Tích Tính Năng Toshiba AC

### 1. Cấu Trúc Protocol

**File:** `src/ir_Toshiba.h` và `src/ir_Toshiba.cpp`

#### Protocol Structure

```cpp
union ToshibaProtocol {
    uint8_t raw[kToshibaACStateLengthLong];  // 9 bytes (long) hoặc 5 bytes (short)
    struct {
        // Byte[0-1]: Header (0xF2, 0x0D)
        uint8_t Length   :4;  // Byte[2]: Payload length
        uint8_t Model    :4;  // Remote type (A or B)
        // Byte[3]: Inverted length
        uint8_t Temp     :4;  // Byte[5]: Nhiệt độ (17-30°C)
        uint8_t Swing    :3;  // Byte[5]: Swing
        uint8_t Mode     :3;  // Byte[6]: Mode
        uint8_t Fan      :3;  // Byte[6]: Fan speed
        uint8_t Filter   :1;  // Byte[7]: Filter
        uint8_t EcoTurbo :8;  // Byte[8]: Eco/Turbo (long message only)
    };
};
```

#### Timing Constants

```cpp
const uint16_t kToshibaAcHdrMark = 4400;      // Header mark: 4.4ms
const uint16_t kToshibaAcHdrSpace = 4300;     // Header space: 4.3ms
const uint16_t kToshibaAcBitMark = 580;       // Bit mark: 580us
const uint16_t kToshibaAcOneSpace = 1600;     // One space: 1600us
const uint16_t kToshibaAcZeroSpace = 490;     // Zero space: 490us
const uint16_t kToshibaAcUsualGap = 7400;     // Gap: 7.4ms
```

#### Message Lengths

- **Short (5 bytes):** Swing commands
- **Normal (6 bytes):** Basic control
- **Long (9 bytes):** Full control với Eco/Turbo

#### Default State (Reset)

```cpp
static const uint8_t kReset[kToshibaACStateLength] = {
    0xF2, 0x0D, 0x03, 0xFC, 0x01
};
```

**Giải thích:**
- `0xF2, 0x0D`: Header (0x0D = ~0xF2)
- `0x03`: Length = 3 (payload sau byte 4)
- `0xFC`: Inverted length (~0x03)
- `0x01`: Initial state
- Default temp: **22°C**

### 2. Tính Năng Chi Tiết

#### Power Control
- `on()` / `off()` - Bật/tắt
- `setPower(bool on)` - Set power
- Power OFF = Mode = `kToshibaAcOff` (0b111)

#### Temperature
- Range: **17-30°C**
- `setTemp(uint8_t degrees)` - Set nhiệt độ
- Stored as offset từ min temp: `_.Temp = temp - kToshibaAcMinTemp`

#### Mode
- `kToshibaAcAuto` (0b000) - Tự động
- `kToshibaAcCool` (0b001) - Làm mát
- `kToshibaAcDry` (0b010) - Khô
- `kToshibaAcHeat` (0b011) - Sưởi
- `kToshibaAcFan` (0b100) - Quạt
- `kToshibaAcOff` (0b111) - Tắt

#### Fan Speed
- `kToshibaAcFanAuto` (0b000 = 0) - Tự động
- `kToshibaAcFanMin` (0b001 = 1) - Thấp
- `kToshibaAcFanMed` (0b011 = 3) - Trung
- `kToshibaAcFanMax` (0b101 = 5) - Cao

**Lưu ý:** Internal encoding khác với external API:
- External: 0=Auto, 1-5=speed
- Internal: 0=Auto, 2-6=speed (Auto được skip)

#### Swing
- `kToshibaAcSwingStep` (0b000) - Step
- `kToshibaAcSwingOn` (0b001) - Bật
- `kToshibaAcSwingOff` (0b010) - Tắt
- `kToshibaAcSwingToggle` (0b100) - Toggle

**Đặc biệt:** Swing commands sử dụng **short message** (5 bytes) với temp = min (17°C)

#### Turbo & Economy
- Chỉ có trong **long message** (9 bytes)
- `kToshibaAcTurboOn` (0b01) - Turbo
- `kToshibaAcEconoOn` (0b11) - Economy
- Mutually exclusive

#### Filter
- `setFilter(bool on)` - Ion/Pure filter
- Chỉ có trong normal/long message

### 3. Checksum Algorithm

```cpp
uint8_t IRToshibaAC::calcChecksum(const uint8_t state[], const uint16_t length) {
    return length ? xorBytes(state, length - 1) : 0;
}
```

**Checksum:** XOR tất cả bytes trừ byte cuối (checksum byte).

### 4. Inverted Byte Pairs

Toshiba protocol sử dụng **inverted byte pairs** cho 4 bytes đầu:
- Byte[0] và Byte[1] là inverted pair
- Byte[2] và Byte[3] là inverted pair

---

## 📊 So Sánh Với Repo Gốc (hai-os-simplexl)

### 1. Kiến Trúc

| Khía Cạnh | IRremoteESP8266 | Repo Gốc (hai-os-simplexl) |
|-----------|-----------------|----------------------------|
| **Framework** | Arduino | ESP-IDF |
| **Hardware** | ESP8266/ESP32 | ESP32-S3 |
| **IR Transmit** | IRsend class (software PWM) | RMT peripheral (hardware) |
| **IR Receive** | IRrecv class (GPIO interrupt) | Chưa implement |
| **Carrier Modulation** | Software PWM với 38kHz | RMT carrier encoder (hardware) |
| **Protocol Support** | 100+ protocols | NEC only (basic) |
| **AC Support** | 50+ brands với full state | Toshiba/Sharp (basic codes) |

### 2. Tính Năng IR Service

#### IRremoteESP8266

**Ưu điểm:**
- ✅ **Full protocol support:** 100+ protocols
- ✅ **Complete AC control:** State management, checksum, validation
- ✅ **IR receive:** Capture và decode signals
- ✅ **Universal API:** IRac class cho tất cả AC brands
- ✅ **Model detection:** Tự động detect model từ raw code
- ✅ **State persistence:** Lưu và restore state
- ✅ **Checksum validation:** Verify message integrity
- ✅ **Extensive documentation:** Doxygen docs, examples

**Nhược điểm:**
- ❌ **Arduino framework:** Không tương thích trực tiếp với ESP-IDF
- ❌ **Software PWM:** Có thể không chính xác bằng hardware
- ❌ **Memory usage:** Lớn hơn do nhiều protocols

#### Repo Gốc (hai-os-simplexl)

**Ưu điểm:**
- ✅ **ESP-IDF native:** Tích hợp tốt với hệ thống
- ✅ **RMT hardware:** Sử dụng hardware peripheral (chính xác hơn)
- ✅ **Carrier modulation:** Hardware 38kHz modulation
- ✅ **Lightweight:** Chỉ implement cần thiết
- ✅ **MCP integration:** Tích hợp với chatbot

**Nhược điểm:**
- ❌ **Limited protocols:** Chỉ NEC (basic)
- ❌ **No IR receive:** Không thể capture mã
- ❌ **Basic AC control:** Chỉ có mã lệnh đơn giản
- ❌ **No state management:** Không quản lý state
- ❌ **No checksum:** Không validate message
- ❌ **Hardcoded codes:** Mã lệnh hardcode, không linh hoạt

### 3. So Sánh Chi Tiết

#### A. Protocol Implementation

**IRremoteESP8266:**
```cpp
// Sharp AC - Full state management
IRSharpAc ac(14);
ac.on();
ac.setTemp(26);
ac.setMode(kSharpAcCool);
ac.setFan(kSharpAcFanMed);
ac.setSwingV(kSharpAcSwingVHigh);
ac.setTurbo(true);
ac.send();
```

**Repo Gốc:**
```c
// Basic NEC command
sx_ir_send_nec(0x5F, 0x20);  // Sharp, Mode Cool
```

#### B. State Management

**IRremoteESP8266:**
- Quản lý full state (13 bytes cho Sharp, 9 bytes cho Toshiba)
- Lưu previous state để toggle
- Validate state trước khi send
- Checksum tự động

**Repo Gốc:**
- Chỉ gửi command đơn lẻ
- Không quản lý state
- Không validate

#### C. Model Support

**IRremoteESP8266:**
- Sharp: A907, A903, A705
- Toshiba: Remote A, Remote B
- Tự động detect model từ raw code

**Repo Gốc:**
- Hardcoded models trong database
- Không có model detection

---

## 🔍 Tìm Tập Lệnh Điều Khiển Sharp và Toshiba

### 1. Sharp AC - Mã Lệnh Chi Tiết

#### Default State (Reset)
```cpp
0xAA, 0x5A, 0xCF, 0x10, 0x00, 0x01, 0x00, 0x00, 0x08, 0x80, 0x00, 0xE0, 0x01
```

#### Power Commands
- **Power On (from Off):** `PowerSpecial = 0x01` (0b0001)
- **Power On (Normal):** `PowerSpecial = 0x03` (0b0011)
- **Power Off:** `PowerSpecial = 0x02` (0b0010)

#### Mode Commands
- **Auto:** `Mode = 0b00` (0x00)
- **Cool:** `Mode = 0b10` (0x02)
- **Heat:** `Mode = 0b01` (0x01) - A907 only
- **Dry:** `Mode = 0b11` (0x03)
- **Fan:** `Mode = 0b00` (0x00) - A705 only

#### Fan Speed Commands
- **Auto:** `Fan = 0b010` (0x02)
- **Min (FAN1):** `Fan = 0b100` (0x04)
- **Med (FAN2):** `Fan = 0b011` (0x03)
- **High (FAN3):** `Fan = 0b101` (0x05)
- **Max (FAN4):** `Fan = 0b111` (0x07)

#### Temperature Encoding
- Range: 15-30°C
- Encoding: `Temp = degrees - 15`
- Example: 26°C = `0x0B` (26 - 15 = 11)

#### Special Functions
- **Turbo:** `Special = 0x01`, `PowerSpecial = 0x06`
- **Econo:** `Special = 0x04`, `PowerSpecial = 0x06`
- **Swing:** `Special = 0x06`
- **Timer:** `Special = 0xC0` hoặc `0xDE`

### 2. Toshiba AC - Mã Lệnh Chi Tiết

#### Default State (Reset)
```cpp
0xF2, 0x0D, 0x03, 0xFC, 0x01
```

**Giải thích:**
- `0xF2, 0x0D`: Header (inverted pair)
- `0x03`: Length = 3 (payload = 3 bytes sau byte 4)
- `0xFC`: Inverted length (~0x03)
- `0x01`: Initial state (temp=22°C, mode=auto, fan=auto)

#### Power Commands
- **Power On:** `Mode != kToshibaAcOff` (0b111)
- **Power Off:** `Mode = kToshibaAcOff` (0b111)

#### Mode Commands
- **Auto:** `Mode = 0b000` (0x00)
- **Cool:** `Mode = 0b001` (0x01)
- **Dry:** `Mode = 0b010` (0x02)
- **Heat:** `Mode = 0b011` (0x03)
- **Fan:** `Mode = 0b100` (0x04)
- **Off:** `Mode = 0b111` (0x07)

#### Fan Speed Commands
- **Auto:** `Fan = 0b000` (0x00)
- **Min:** `Fan = 0b001` (0x01)
- **Med:** `Fan = 0b011` (0x03)
- **Max:** `Fan = 0b101` (0x05)

#### Temperature Encoding
- Range: 17-30°C
- Encoding: `Temp = degrees - 17`
- Example: 26°C = `0x09` (26 - 17 = 9)

#### Swing Commands (Short Message)
- **Step:** `Swing = 0b000` (0x00)
- **On:** `Swing = 0b001` (0x01)
- **Off:** `Swing = 0b010` (0x02)
- **Toggle:** `Swing = 0b100` (0x04)

**Lưu ý:** Swing commands sử dụng **short message** (5 bytes) với temp = 17°C

#### Turbo & Economy (Long Message Only)
- **Turbo On:** `EcoTurbo = 0x01`
- **Econo On:** `EcoTurbo = 0x03`
- **Both Off:** `EcoTurbo = 0x00` (sử dụng normal message)

### 3. Ví Dụ Mã Lệnh Cụ Thể

#### Sharp AC - Power On, Cool Mode, 26°C, Fan Medium

```cpp
IRSharpAc ac(14);
ac.on();
ac.setMode(kSharpAcCool);
ac.setTemp(26);
ac.setFan(kSharpAcFanMed);
ac.send();

// Raw code sẽ là:
// [0xAA, 0x5A, 0xCF, 0x10, 0x0B, 0x03, 0x13, 0x00, 0x08, 0x80, 0x05, 0xE0, 0x??]
//                                 ^     ^     ^
//                              Temp=26 Mode  Fan=Med
```

#### Toshiba AC - Power On, Cool Mode, 26°C, Fan Medium

```cpp
IRToshibaAC ac(14);
ac.on();
ac.setMode(kToshibaAcCool);
ac.setTemp(26);
ac.setFan(kToshibaAcFanMed);
ac.send();

// Raw code sẽ là:
// [0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x09, 0x03, 0x00, 0x??]
//                          ^     ^     ^
//                       Temp=26 Mode  Fan=Med
```

---

## 🚀 Đề Xuất Cải Thiện Cho Repo Gốc

### 1. Cải Thiện Protocol Support (Ưu Tiên CAO) 🔴

#### A. Implement Full Sharp AC Protocol

**Hiện tại:**
- Chỉ có basic NEC commands
- Không có state management
- Không có checksum

**Đề xuất:**
```c
// Thêm Sharp AC state structure
typedef struct {
    uint8_t raw[13];  // kSharpAcStateLength
    struct {
        uint8_t temp : 4;      // 15-30°C
        uint8_t mode : 2;      // Auto/Cool/Heat/Dry
        uint8_t fan : 3;       // Auto/Min/Med/High/Max
        uint8_t power : 4;     // Power state
        uint8_t swing : 3;     // Swing vertical
        uint8_t special : 8;   // Special functions
        uint8_t checksum : 4;  // Checksum
    };
} sharp_ac_state_t;

// API mới
esp_err_t sx_ir_sharp_ac_set_state(const sharp_ac_state_t *state);
esp_err_t sx_ir_sharp_ac_get_state(sharp_ac_state_t *state);
esp_err_t sx_ir_sharp_ac_set_temp(uint8_t temp);
esp_err_t sx_ir_sharp_ac_set_mode(sharp_ac_mode_t mode);
esp_err_t sx_ir_sharp_ac_set_fan(sharp_ac_fan_t fan);
```

**Lợi ích:**
- Quản lý state đầy đủ
- Hỗ trợ tất cả tính năng (Turbo, Swing, Timer)
- Checksum validation

#### B. Implement Full Toshiba AC Protocol

**Tương tự Sharp:**
```c
typedef struct {
    uint8_t raw[9];  // Long message
    struct {
        uint8_t temp : 4;      // 17-30°C
        uint8_t mode : 3;      // Auto/Cool/Heat/Dry/Fan/Off
        uint8_t fan : 3;       // Auto/Min/Med/Max
        uint8_t swing : 3;     // Swing
        uint8_t turbo : 1;     // Turbo mode
        uint8_t econo : 1;     // Economy mode
        uint8_t filter : 1;    // Filter
    };
} toshiba_ac_state_t;
```

### 2. Thêm IR Receive Support (Ưu Tiên CAO) 🔴

**Hiện tại:**
- Chỉ có TX (phát)
- Không có RX (nhận)

**Đề xuất:**
```c
// IR Receiver API
esp_err_t sx_ir_service_init_rx(int rx_gpio);
esp_err_t sx_ir_start_capture(void);
esp_err_t sx_ir_stop_capture(void);
esp_err_t sx_ir_get_captured_code(uint16_t *pulses, size_t *count);
esp_err_t sx_ir_decode_nec(uint16_t *pulses, size_t count, uint8_t *address, uint8_t *command);
```

**Lợi ích:**
- Capture mã từ remote thật
- Học mã lệnh mới
- Debug và test

### 3. Cải Thiện State Management (Ưu Tiên TRUNG BÌNH) 🟡

**Đề xuất:**
```c
// State management
typedef struct {
    bool power;
    uint8_t temp;
    ir_ac_mode_t mode;
    ir_ac_fan_t fan;
    // ... other settings
} ir_ac_state_t;

esp_err_t sx_ir_ac_save_state(const char *device_id, const ir_ac_state_t *state);
esp_err_t sx_ir_ac_load_state(const char *device_id, ir_ac_state_t *state);
esp_err_t sx_ir_ac_send_state(const char *brand, const char *model, const ir_ac_state_t *state);
```

**Lợi ích:**
- Lưu trạng thái hiện tại
- Restore state sau khi restart
- Toggle functions (on/off)

### 4. Thêm Checksum Validation (Ưu Tiên TRUNG BÌNH) 🟡

**Đề xuất:**
```c
// Checksum functions
uint8_t sx_ir_sharp_calc_checksum(const uint8_t *state, size_t length);
bool sx_ir_sharp_validate_checksum(const uint8_t *state, size_t length);
uint8_t sx_ir_toshiba_calc_checksum(const uint8_t *state, size_t length);
bool sx_ir_toshiba_validate_checksum(const uint8_t *state, size_t length);
```

**Lợi ích:**
- Đảm bảo message integrity
- Phát hiện lỗi transmission
- Tương thích với remote thật

### 5. Cải Thiện Database Mã Lệnh (Ưu Tiên TRUNG BÌNH) 🟡

**Hiện tại:**
- Hardcoded trong code
- Chỉ có address và command đơn giản

**Đề xuất:**
```c
// Enhanced code database
typedef struct {
    const char *model_name;
    ir_protocol_t protocol;
    union {
        struct {
            uint8_t address;
            uint8_t codes[12];  // Simple commands
        } nec;
        struct {
            uint8_t default_state[13];  // Full Sharp state
            sharp_ac_remote_model_t model_type;
        } sharp;
        struct {
            uint8_t default_state[9];  // Full Toshiba state
            toshiba_ac_remote_model_t model_type;
        } toshiba;
    };
} ir_device_code_t;
```

**Lợi ích:**
- Hỗ trợ cả simple và full state
- Linh hoạt hơn
- Dễ mở rộng

### 6. Thêm Model Detection (Ưu Tiên THẤP) 🟢

**Đề xuất:**
```c
// Model detection từ raw code
ir_protocol_t sx_ir_detect_protocol(const uint16_t *pulses, size_t count);
sharp_ac_remote_model_t sx_ir_sharp_detect_model(const uint8_t *raw_code);
toshiba_ac_remote_model_t sx_ir_toshiba_detect_model(const uint8_t *raw_code);
```

**Lợi ích:**
- Tự động detect model từ remote
- Không cần cấu hình thủ công

### 7. Thêm Universal AC API (Ưu Tiên THẤP) 🟢

**Đề xuất:**
```c
// Universal AC control (tương tự IRac trong IRremoteESP8266)
typedef struct {
    bool power;
    ir_ac_mode_t mode;
    uint8_t temp;
    ir_ac_fan_t fan;
    ir_ac_swing_t swing;
    bool turbo;
    bool econo;
    // ... other common settings
} ir_ac_common_state_t;

esp_err_t sx_ir_ac_send_common(const char *brand, const char *model, 
                                const ir_ac_common_state_t *state);
```

**Lợi ích:**
- API thống nhất cho tất cả brands
- Dễ sử dụng
- Tương thích với MCP chatbot

---

## 📝 Tập Lệnh Điều Khiển Chi Tiết

### Sharp AC - AH-X10ZEW

Dựa trên IRremoteESP8266, Sharp AC sử dụng **13-byte state** với các mã lệnh:

#### Power On (from Off)
```c
uint8_t state[13] = {
    0xAA, 0x5A, 0xCF, 0x10,  // Header
    0x00,                    // Temp = 0 (sẽ set sau)
    0x01,                    // PowerSpecial = PowerOnFromOff
    0x00,                    // Mode = Auto (sẽ set sau)
    0x00, 0x08, 0x80, 0x00, 0xE0, 0x01  // Defaults
};
```

#### Cool Mode, 26°C, Fan Medium
```c
// Byte 4: Temp = 26 - 15 = 11 = 0x0B
// Byte 5: PowerSpecial = 0x03 (Normal On)
// Byte 6: Mode = 0x02 (Cool), Fan = 0x03 (Med)
// Byte 10: Special = 0x04 (TempEcono)
```

#### Complete Example
```c
IRSharpAc ac(14);
ac.setModel(sharp_ac_remote_model_t::A907);  // Hoặc A903
ac.on();
ac.setMode(kSharpAcCool);
ac.setTemp(26);
ac.setFan(kSharpAcFanMed);
ac.send();
```

### Toshiba AC - RAS-H10C4KCVG-V

Dựa trên IRremoteESP8266, Toshiba AC sử dụng **variable-length message**:

#### Power On, Cool Mode, 26°C, Fan Medium
```c
uint8_t state[6] = {
    0xF2, 0x0D,  // Header (inverted pair)
    0x03, 0xFC,  // Length = 3, inverted
    0x01,        // Initial
    0x09,        // Temp = 26 - 17 = 9
    0x03,        // Mode = Cool (0x01), Fan = Med (0x03)
    0x00         // Filter = Off
};
// Checksum = XOR of bytes 0-5
```

#### Complete Example
```c
IRToshibaAC ac(14);
ac.setModel(toshiba_ac_remote_model_t::kToshibaGenericRemote_A);
ac.on();
ac.setMode(kToshibaAcCool);
ac.setTemp(26);
ac.setFan(kToshibaAcFanMed);
ac.send();
```

---

## 🎯 Kế Hoạch Triển Khai

### Phase 1: Cải Thiện Protocol Support (2-3 tuần)

1. **Implement Sharp AC Full Protocol**
   - Thêm Sharp AC state structure
   - Implement checksum
   - Thêm các hàm set/get state
   - Test với model AH-X10ZEW

2. **Implement Toshiba AC Full Protocol**
   - Thêm Toshiba AC state structure
   - Implement checksum và inverted pairs
   - Thêm support cho short/long messages
   - Test với model RAS-H10C4KCVG-V

### Phase 2: IR Receive Support (1-2 tuần)

1. **RMT RX Channel**
   - Setup RMT RX với GPIO 15
   - Capture IR signals
   - Decode NEC protocol

2. **Learning Feature**
   - UI để học mã
   - Lưu mã vào NVS
   - Test với remote thật

### Phase 3: State Management (1 tuần)

1. **State Storage**
   - Lưu state vào NVS
   - Restore state on boot
   - State synchronization

### Phase 4: Enhanced Features (1-2 tuần)

1. **Checksum Validation**
2. **Model Detection**
3. **Universal AC API**

---

## 📚 Tài Liệu Tham Khảo

1. **IRremoteESP8266 GitHub:** https://github.com/crankyoldgit/IRremoteESP8266
2. **Sharp AC Protocol:** `src/ir_Sharp.cpp` và `src/ir_Sharp.h`
3. **Toshiba AC Protocol:** `src/ir_Toshiba.cpp` và `src/ir_Toshiba.h`
4. **Universal AC API:** `src/IRac.cpp` và `src/IRac.h`
5. **Supported Protocols:** `SupportedProtocols.md`

---

## ✅ Tổng Kết

### Điểm Mạnh IRremoteESP8266

1. **Comprehensive:** 100+ protocols, 50+ AC brands
2. **Mature:** Đã được test kỹ với nhiều thiết bị
3. **Well-documented:** Doxygen docs, examples
4. **Active:** Đang được maintain và update

### Điểm Mạnh Repo Gốc

1. **ESP-IDF Native:** Tích hợp tốt với hệ thống
2. **Hardware RMT:** Chính xác và hiệu quả
3. **MCP Integration:** Tích hợp với chatbot
4. **Lightweight:** Chỉ implement cần thiết

### Đề Xuất Ưu Tiên

1. **CAO:** Implement full Sharp/Toshiba AC protocol
2. **CAO:** Thêm IR receive support
3. **TRUNG BÌNH:** State management và checksum
4. **THẤP:** Universal AC API và model detection

---

**Tác giả:** AI Assistant  
**Ngày:** 2024  
**Phiên bản:** 1.0  
**Repo Mẫu:** https://github.com/crankyoldgit/IRremoteESP8266




















