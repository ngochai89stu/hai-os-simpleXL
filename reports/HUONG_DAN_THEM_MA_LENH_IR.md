# Hướng Dẫn Thêm Mã Lệnh IR Cho Model Điều Hòa Cụ Thể

## 📋 Tổng Quan

Hệ thống IR control hỗ trợ lưu trữ mã lệnh cho từng model điều hòa cụ thể. Có 2 cách để thêm mã lệnh:

1. **Hardcoded trong code** (nhanh, đơn giản) - File `sx_ir_codes.c`
2. **Lưu trong NVS** (linh hoạt, có thể thay đổi runtime) - Sẽ thêm sau

---

## 🔧 Cách 1: Thêm Mã Lệnh Trong Code (Khuyến Nghị)

### Bước 1: Lấy Mã Lệnh IR Từ Remote Thật

Có nhiều cách để lấy mã lệnh IR:

#### Phương pháp A: Sử dụng IR Receiver + ESP32
1. Kết nối IR receiver vào ESP32 (GPIO khác)
2. Sử dụng RMT RX để capture tín hiệu
3. Decode và lấy mã lệnh

#### Phương pháp B: Sử dụng Arduino IRremote Library
1. Dùng Arduino với IR receiver
2. Capture và decode mã lệnh
3. Ghi lại address và command code

#### Phương pháp C: Tìm Trên Internet
- Tìm mã lệnh IR cho model cụ thể trên các trang như:
  - https://www.remotecentral.com/
  - https://github.com/crankyoldgit/IRremoteESP8266
  - Database IR codes

#### Phương pháp D: Sử Dụng Oscilloscope/Logic Analyzer
- Đo tín hiệu IR trực tiếp từ remote
- Phân tích timing và decode

### Bước 2: Thêm Mã Vào Database

Mở file `components/sx_services/sx_ir_codes.c` và thêm entry mới:

```c
{
    .model_name = "Toshiba_RAS-B10GK",  // Format: "Brand_Model"
    .protocol = IR_PROTOCOL_TOSHIBA_AC, // hoặc IR_PROTOCOL_SHARP_AC
    .address = 0xBF,                     // Device address (8-bit)
    .codes = {
        [IR_AC_POWER_ON] = 0x02,         // Mã lệnh bật
        [IR_AC_POWER_OFF] = 0x03,        // Mã lệnh tắt
        [IR_AC_TEMP_UP] = 0x10,           // Tăng nhiệt độ
        [IR_AC_TEMP_DOWN] = 0x11,         // Giảm nhiệt độ
        [IR_AC_MODE_COOL] = 0x20,        // Chế độ làm mát
        [IR_AC_MODE_HEAT] = 0x21,        // Chế độ sưởi
        [IR_AC_MODE_AUTO] = 0x22,        // Chế độ tự động
        [IR_AC_FAN_SPEED_LOW] = 0x30,    // Quạt thấp
        [IR_AC_FAN_SPEED_MEDIUM] = 0x31, // Quạt trung
        [IR_AC_FAN_SPEED_HIGH] = 0x32,   // Quạt cao
    }
},
```

### Bước 3: Ví Dụ Cụ Thể

#### Ví dụ 1: Thêm Model Toshiba Mới

```c
{
    .model_name = "Toshiba_RAS-B18GK",
    .protocol = IR_PROTOCOL_TOSHIBA_AC,
    .address = 0xC0,  // Address khác với model khác
    .codes = {
        [IR_AC_POWER_ON] = 0x04,
        [IR_AC_POWER_OFF] = 0x05,
        [IR_AC_TEMP_UP] = 0x12,
        [IR_AC_TEMP_DOWN] = 0x13,
        [IR_AC_MODE_COOL] = 0x24,
        [IR_AC_MODE_HEAT] = 0x25,
        [IR_AC_MODE_AUTO] = 0x26,
        [IR_AC_FAN_SPEED_LOW] = 0x33,
        [IR_AC_FAN_SPEED_MEDIUM] = 0x34,
        [IR_AC_FAN_SPEED_HIGH] = 0x35,
    }
},
```

#### Ví dụ 2: Thêm Model Sharp Mới

```c
{
    .model_name = "Sharp_AY-XP18FR",
    .protocol = IR_PROTOCOL_SHARP_AC,
    .address = 0x60,
    .codes = {
        [IR_AC_POWER_ON] = 0x03,
        [IR_AC_POWER_OFF] = 0x04,
        [IR_AC_TEMP_UP] = 0x12,
        [IR_AC_TEMP_DOWN] = 0x13,
        [IR_AC_MODE_COOL] = 0x24,
        [IR_AC_MODE_HEAT] = 0x25,
        [IR_AC_MODE_AUTO] = 0x26,
        [IR_AC_FAN_SPEED_LOW] = 0x43,
        [IR_AC_FAN_SPEED_MEDIUM] = 0x44,
        [IR_AC_FAN_SPEED_HIGH] = 0x45,
    }
},
```

### Bước 4: Sử Dụng Trong MCP Tool

Sau khi thêm mã, chatbot có thể gọi với model cụ thể:

```json
{
  "brand": "toshiba",
  "model": "RAS-B18GK",
  "command": "power_on"
}
```

Hoặc dùng tiếng Việt:
- "Bật điều hòa Toshiba RAS-B18GK"
- "Tăng nhiệt độ điều hòa Sharp AY-XP18FR"

---

## 📝 Cấu Trúc Mã Lệnh IR

### NEC Protocol Format

Mã lệnh IR thường theo format NEC:
- **Header**: 9ms mark + 4.5ms space
- **Address**: 8-bit (hoặc 16-bit cho extended)
- **Command**: 8-bit
- **Repeat**: Optional

### Ví Dụ Mã Lệnh

```
Power On:
- Address: 0xBF
- Command: 0x02
- Format: [Header] + [0xBF] + [~0xBF] + [0x02] + [~0x02]

Temp Up:
- Address: 0xBF
- Command: 0x10
- Format: [Header] + [0xBF] + [~0xBF] + [0x10] + [~0x10]
```

---

## 🔍 Cách Tìm Mã Lệnh IR

### 1. Sử Dụng IRremoteESP8266 Library

Thư viện này có database lớn các mã lệnh IR:

```cpp
#include <IRremoteESP8266.h>
#include <IRsend.h>

IRsend irsend(14);  // GPIO 14

// Toshiba AC codes
irsend.sendToshibaAC(0xBF, 0x02);  // Power On
```

### 2. Capture Từ Remote Thật

Sử dụng ESP32 với IR receiver:

```c
// Setup RMT RX
rmt_rx_channel_config_t rx_cfg = {
    .gpio_num = 15,  // IR RX pin
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .resolution_hz = 1000000,
    .mem_block_symbols = 64,
};
rmt_new_rx_channel(&rx_cfg, &rx_chan);

// Capture signal
rmt_receive_config_t receive_cfg = {
    .signal_range_min_ns = 1000,
    .signal_range_max_ns = 100000000,
};
rmt_receive(rx_chan, symbols, sizeof(symbols), &receive_cfg);

// Decode NEC
// Analyze symbols to extract address and command
```

### 3. Tìm Trên Database Online

- **LIRC Database**: http://lirc.sourceforge.net/remotes/
- **IRremoteESP8266**: https://github.com/crankyoldgit/IRremoteESP8266/tree/master/src/ir
- **Remote Central**: https://www.remotecentral.com/

---

## 🧪 Kiểm Tra Mã Lệnh

### Test Bằng Code

```c
// Test mã lệnh mới
esp_err_t ret = sx_ir_send_ac_command_by_model("toshiba", "RAS-B18GK", IR_AC_POWER_ON, 0);
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Command sent successfully");
} else {
    ESP_LOGE(TAG, "Failed: %s", esp_err_to_name(ret));
}
```

### Test Bằng MCP Tool

Gọi qua chatbot:
```
"Bật điều hòa Toshiba RAS-B18GK"
```

Hoặc qua JSON:
```json
{
  "method": "tools/call",
  "params": {
    "name": "self.ir_control.ac_command",
    "arguments": {
      "brand": "toshiba",
      "model": "RAS-B18GK",
      "command": "power_on"
    }
  }
}
```

---

## 📊 Danh Sách Models Đã Hỗ Trợ

Xem trong file `sx_ir_codes.c`:

- **Toshiba**:
  - RAS-B10GK
  - RAS-B13GK
  - (Thêm model khác...)

- **Sharp**:
  - AY-XP10FR
  - AY-XP13FR
  - (Thêm model khác...)

---

## ⚠️ Lưu Ý Quan Trọng

1. **Address và Command phải chính xác**: Mỗi model có thể có address và command khác nhau
2. **Protocol có thể khác**: Một số model có thể dùng protocol khác NEC
3. **Test kỹ trước khi commit**: Đảm bảo mã lệnh hoạt động với thiết bị thật
4. **Format model_name**: Phải đúng format "Brand_Model" (không có khoảng trắng)

---

## 🚀 Cách 2: Lưu Trong NVS (Tương Lai)

Sẽ thêm tính năng lưu mã lệnh trong NVS để có thể:
- Thay đổi mã lệnh runtime
- Thêm model mới không cần recompile
- Backup/restore mã lệnh

---

## 📚 Tài Liệu Tham Khảo

1. **ESP-IDF RMT Documentation**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/rmt.html
2. **NEC Protocol**: https://www.sbprojects.net/knowledge/ir/nec.php
3. **IRremoteESP8266**: https://github.com/crankyoldgit/IRremoteESP8266

---

**Tác giả:** AI Assistant  
**Ngày:** 2024  
**Phiên bản:** 1.0


















