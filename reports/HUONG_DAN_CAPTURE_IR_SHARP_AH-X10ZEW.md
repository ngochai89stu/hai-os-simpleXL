# Hướng Dẫn Capture Mã Lệnh IR Cho Sharp AH-X10ZEW

## 📋 Tổng Quan

Model **Sharp AH-X10ZEW 1 HP** đã được thêm vào database với mã lệnh mặc định của Sharp. Tuy nhiên, **cần test và điều chỉnh** để đảm bảo hoạt động chính xác với model cụ thể của bạn.

### ⚙️ Cấu Hình Phần Cứng

**IR Transmitter (Output):**
- **GPIO 14** - Chân phát tín hiệu IR (đã được cấu hình trong hệ thống)
- Kết nối với IR LED qua transistor (khuyến nghị)

**IR Receiver (Input - để capture mã):**
- **GPIO 15** (hoặc GPIO khác) - Chân nhận tín hiệu IR từ remote
- Kết nối với IR receiver module (VS1838B hoặc tương tự)

## 🔧 Cách Capture Mã Lệnh IR Từ Remote Thật

### Phương Pháp 1: Sử Dụng ESP32 Với IR Receiver (Khuyến Nghị)

#### Bước 1: Chuẩn Bị Phần Cứng

**Cần:**
- ESP32 development board
- IR Receiver module (VS1838B hoặc tương tự)
- Remote điều khiển Sharp AH-X10ZEW
- Dây nối

**Kết Nối:**
```
IR Receiver    ESP32
VCC      ->    3.3V
GND      ->    GND
OUT      ->    GPIO 15 (hoặc GPIO khác)
```

#### Bước 2: Code Capture IR

Tạo file test `test_ir_capture.c`:

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_rx.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "ir_capture";

void app_main(void) {
    // Configure RMT RX
    rmt_rx_channel_config_t rx_cfg = {
        .gpio_num = 15,  // IR receiver pin
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000,  // 1MHz = 1us per tick
        .mem_block_symbols = 64,
    };
    
    rmt_channel_handle_t rx_chan = NULL;
    esp_err_t ret = rmt_new_rx_channel(&rx_cfg, &rx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT RX channel");
        return;
    }
    
    // Enable RX channel
    ret = rmt_enable(rx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable RMT RX channel");
        return;
    }
    
    ESP_LOGI(TAG, "IR Receiver ready. Press buttons on remote...");
    
    // Receive buffer
    rmt_symbol_word_t symbols[128];
    
    while (1) {
        rmt_receive_config_t receive_cfg = {
            .signal_range_min_ns = 1000,
            .signal_range_max_ns = 100000000,
        };
        
        // Wait for IR signal
        ret = rmt_receive(rx_chan, symbols, sizeof(symbols), &receive_cfg);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Received IR signal:");
            
            // Decode NEC protocol
            // Header: symbols[0] = mark (~9000us), symbols[1] = space (~4500us)
            if (symbols[0].duration0 > 8000 && symbols[0].duration0 < 10000 &&
                symbols[1].duration0 > 4000 && symbols[1].duration0 < 5000) {
                
                ESP_LOGI(TAG, "NEC Protocol detected");
                
                // Decode address and command
                uint8_t address = 0;
                uint8_t command = 0;
                
                // Decode address (bits 2-9)
                for (int i = 0; i < 8; i++) {
                    int bit_idx = 2 + i * 2;
                    if (symbols[bit_idx + 1].duration0 > 1500) {
                        address |= (1 << i);
                    }
                }
                
                // Decode command (bits 18-25)
                for (int i = 0; i < 8; i++) {
                    int bit_idx = 18 + i * 2;
                    if (symbols[bit_idx + 1].duration0 > 1500) {
                        command |= (1 << i);
                    }
                }
                
                ESP_LOGI(TAG, "Address: 0x%02X, Command: 0x%02X", address, command);
                
                // Print raw symbols for analysis
                ESP_LOGI(TAG, "Raw symbols (first 20):");
                for (int i = 0; i < 20 && i < 64; i++) {
                    ESP_LOGI(TAG, "  [%d] level=%d, duration=%d us", 
                             i, symbols[i].level0, symbols[i].duration0);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

#### Bước 3: Capture Các Lệnh

1. Build và flash code vào ESP32
2. Mở serial monitor (115200 baud)
3. Nhấn từng nút trên remote:
   - Power On
   - Power Off
   - Temp Up
   - Temp Down
   - Mode Cool
   - Mode Heat
   - Mode Auto
   - Fan Low
   - Fan Medium
   - Fan High

4. Ghi lại **Address** và **Command** cho mỗi lệnh

#### Bước 4: Cập Nhật Mã Trong Database

Sau khi có mã, cập nhật trong `sx_ir_codes.c`:

```c
{
    .model_name = "Sharp_AH-X10ZEW",
    .protocol = IR_PROTOCOL_SHARP_AC,
    .address = 0xXX,  // Address từ remote thật
    .codes = {
        [IR_AC_POWER_ON] = 0xXX,      // Command từ remote thật
        [IR_AC_POWER_OFF] = 0xXX,
        // ... các lệnh khác
    }
}
```

### Phương Pháp 2: Sử Dụng Arduino IRremote Library

#### Bước 1: Code Arduino

```cpp
#include <IRremote.h>

const int RECV_PIN = 11;
IRrecv irrecv(RECV_PIN);
decode_results results;

void setup() {
    Serial.begin(9600);
    irrecv.enableIRIn();
    Serial.println("IR Receiver ready");
}

void loop() {
    if (irrecv.decode(&results)) {
        Serial.print("Protocol: ");
        Serial.println(results.decode_type);
        
        if (results.decode_type == NEC) {
            Serial.print("Address: 0x");
            Serial.println(results.address, HEX);
            Serial.print("Command: 0x");
            Serial.println(results.value, HEX);
        }
        
        irrecv.resume();
    }
}
```

#### Bước 2: Capture và Ghi Lại

Tương tự phương pháp 1, nhấn từng nút và ghi lại mã.

### Phương Pháp 3: Sử Dụng IRremoteESP8266 Database

Tham khảo thư viện IRremoteESP8266 để tìm mã tương tự:

1. Truy cập: https://github.com/crankyoldgit/IRremoteESP8266
2. Tìm file `ir_Sharp.cpp` hoặc `ir_Sharp.h`
3. Tìm mã lệnh cho model tương tự
4. Test và điều chỉnh nếu cần

## 🧪 Test Mã Lệnh

### Test Bằng Code

```c
// Test Power On
esp_err_t ret = sx_ir_send_ac_command_by_model("sharp", "AH-X10ZEW", IR_AC_POWER_ON, 0);
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Power On command sent");
} else {
    ESP_LOGE(TAG, "Failed: %s", esp_err_to_name(ret));
}
```

### Test Bằng Chatbot

```
"Bật điều hòa Sharp AH-X10ZEW"
"Tăng nhiệt độ điều hòa Sharp AH-X10ZEW"
"Chuyển sang chế độ làm mát"
```

## ⚠️ Lưu Ý Quan Trọng

1. **Mã Hiện Tại Là Mặc Định**: Mã lệnh trong database hiện tại là mã mặc định của Sharp, có thể không hoạt động với model AH-X10ZEW
2. **Cần Test**: Bắt buộc phải test với remote thật để xác nhận mã chính xác
3. **Có Thể Khác Protocol**: Một số model Sharp mới có thể dùng protocol khác NEC
4. **Address Có Thể Khác**: Mỗi model có thể có address khác nhau

## 📝 Checklist Capture Mã Lệnh

- [ ] Kết nối IR receiver với ESP32
- [ ] Flash code capture IR
- [ ] Capture mã Power On
- [ ] Capture mã Power Off
- [ ] Capture mã Temp Up
- [ ] Capture mã Temp Down
- [ ] Capture mã Mode Cool
- [ ] Capture mã Mode Heat
- [ ] Capture mã Mode Auto
- [ ] Capture mã Fan Low
- [ ] Capture mã Fan Medium
- [ ] Capture mã Fan High
- [ ] Cập nhật mã vào `sx_ir_codes.c`
- [ ] Test lại với thiết bị thật

## 🔍 Troubleshooting

### Không Nhận Được Tín Hiệu
- Kiểm tra kết nối IR receiver
- Kiểm tra pin GPIO
- Thử remote khác để test IR receiver

### Mã Không Hoạt Động
- Kiểm tra address có đúng không
- Kiểm tra command code có đúng không
- Thử thay đổi protocol (có thể không phải NEC)
- Kiểm tra carrier frequency (có thể không phải 38kHz)

### Tín Hiệu Yếu
- Kiểm tra khoảng cách giữa IR LED và điều hòa
- Kiểm tra IR LED có hoạt động không
- Tăng công suất IR LED nếu cần

## 📚 Tài Liệu Tham Khảo

1. **ESP-IDF RMT RX**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/rmt.html#receive
2. **IRremoteESP8266**: https://github.com/crankyoldgit/IRremoteESP8266
3. **NEC Protocol**: https://www.sbprojects.net/knowledge/ir/nec.php

---

**Lưu ý:** Sau khi capture được mã lệnh chính xác, hãy cập nhật lại file `sx_ir_codes.c` với mã thật để đảm bảo hoạt động ổn định.

