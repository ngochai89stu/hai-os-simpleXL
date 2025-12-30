# Cấu Hình Phần Cứng IR Control

## 📋 Tổng Quan

Tài liệu này mô tả cấu hình phần cứng cho tính năng điều khiển IR trên ESP32.

---

## 🔌 Kết Nối Phần Cứng

### IR Transmitter (Phát Tín Hiệu IR)

**GPIO:** **14** (đã được cấu hình trong hệ thống)

**Kết Nối:**
```
ESP32 GPIO 14  ->  Base của Transistor (NPN, ví dụ: 2N2222 hoặc BC547)
                    |
                    Resistor 1KΩ
                    |
                    GND

IR LED Anode    ->  Collector của Transistor
                    |
                    Resistor 100Ω (để giới hạn dòng)
                    |
                    VCC (3.3V hoặc 5V)

IR LED Cathode  ->  Emitter của Transistor
                    |
                    GND
```

**Sơ Đồ Kết Nối:**
```
                    VCC (3.3V/5V)
                      |
                      |
                   [100Ω]
                      |
                      |
              IR LED (Anode)
                      |
                      |
              Transistor (Collector)
                      |
              Transistor (Base) <- GPIO 14 qua [1KΩ]
                      |
              Transistor (Emitter)
                      |
                     GND
```

**Lưu Ý:**
- IR LED thường cần dòng 20-50mA
- Transistor NPN cần để khuếch đại tín hiệu từ ESP32
- Resistor 1KΩ bảo vệ GPIO của ESP32
- Resistor 100Ω giới hạn dòng qua IR LED

### IR Receiver (Nhận Tín Hiệu IR - Để Capture Mã)

**GPIO:** **15** (hoặc GPIO khác, tùy chọn)

**Kết Nối:**
```
IR Receiver Module (VS1838B)    ESP32
VCC                      ->      3.3V
GND                      ->      GND
OUT                      ->      GPIO 15 (hoặc GPIO khác)
```

**Lưu Ý:**
- IR Receiver chỉ cần khi muốn capture mã từ remote thật
- Không bắt buộc cho chức năng phát IR

---

## ⚙️ Cấu Hình Trong Code

### Bootstrap Configuration

File: `components/sx_core/sx_bootstrap.c`

```c
sx_ir_config_t ir_cfg = {
    .tx_gpio = 14,      // IR TX pin (confirmed)
    .rx_gpio = -1,      // IR RX not used for now
    .carrier_hz = 38000, // Carrier frequency 38kHz
};
```

### IR Service Initialization

File: `components/sx_services/sx_ir_service.c`

- RMT TX channel được khởi tạo với GPIO 14
- Carrier modulation: 38kHz, duty cycle 33%
- Resolution: 1MHz (1 microsecond per tick)

---

## 🔧 Thông Số Kỹ Thuật

### IR Transmitter
- **GPIO:** 14
- **Carrier Frequency:** 38kHz
- **Duty Cycle:** 33%
- **Protocol:** NEC (với carrier modulation)
- **Voltage:** 3.3V logic
- **Current:** 20-50mA (qua IR LED)

### IR Receiver (nếu sử dụng)
- **GPIO:** 15 (hoặc tùy chọn)
- **Module:** VS1838B hoặc tương tự
- **Voltage:** 3.3V
- **Frequency:** 38kHz

---

## 📝 Checklist Kết Nối

### IR Transmitter
- [ ] Kết nối GPIO 14 với base của transistor qua resistor 1KΩ
- [ ] Kết nối collector của transistor với IR LED anode
- [ ] Kết nối IR LED cathode với emitter của transistor
- [ ] Thêm resistor 100Ω giữa VCC và IR LED anode
- [ ] Kết nối emitter của transistor với GND
- [ ] Kiểm tra cực tính IR LED (Anode/Cathode)
- [ ] Kiểm tra cực tính Transistor (Base/Collector/Emitter)

### IR Receiver (Tùy Chọn)
- [ ] Kết nối VCC của IR receiver với 3.3V
- [ ] Kết nối GND của IR receiver với GND
- [ ] Kết nối OUT của IR receiver với GPIO 15 (hoặc GPIO khác)

---

## 🧪 Test Phần Cứng

### Test IR Transmitter

1. **Kiểm tra GPIO:**
   ```c
   // Test GPIO 14 output
   gpio_set_direction(14, GPIO_MODE_OUTPUT);
   gpio_set_level(14, 1);
   vTaskDelay(pdMS_TO_TICKS(100));
   gpio_set_level(14, 0);
   ```

2. **Test IR LED:**
   - Sử dụng camera điện thoại (camera thường không lọc IR)
   - Nhìn vào IR LED khi gửi lệnh
   - Nếu thấy ánh sáng tím/hồng → IR LED hoạt động

3. **Test với thiết bị:**
   - Gửi lệnh IR qua chatbot
   - Kiểm tra thiết bị có nhận được lệnh không

### Test IR Receiver (nếu có)

1. **Kiểm tra kết nối:**
   - Đảm bảo VCC, GND, OUT đã kết nối đúng
   - Kiểm tra GPIO input

2. **Test capture:**
   - Sử dụng code capture IR
   - Nhấn nút trên remote
   - Kiểm tra serial monitor có nhận được tín hiệu không

---

## ⚠️ Lưu Ý Quan Trọng

1. **Cực Tính:**
   - IR LED có cực tính (Anode/Cathode) - kết nối sai sẽ không hoạt động
   - Transistor có cực tính (Base/Collector/Emitter) - cần kết nối đúng

2. **Dòng Điện:**
   - GPIO ESP32 chỉ cung cấp tối đa ~40mA
   - Cần transistor để khuếch đại dòng cho IR LED
   - Không kết nối IR LED trực tiếp với GPIO

3. **Khoảng Cách:**
   - IR LED có tầm hoạt động hạn chế (thường 5-10m)
   - Đảm bảo IR LED hướng về phía thiết bị cần điều khiển
   - Tránh vật cản giữa IR LED và thiết bị

4. **Nhiễu:**
   - Ánh sáng mặt trời có thể gây nhiễu
   - Đèn huỳnh quang có thể gây nhiễu
   - Nên test trong điều kiện ánh sáng bình thường

---

## 🔍 Troubleshooting

### IR LED Không Phát Sáng
- Kiểm tra cực tính IR LED
- Kiểm tra cực tính Transistor
- Kiểm tra kết nối GPIO 14
- Kiểm tra nguồn điện (VCC)

### Thiết Bị Không Nhận Lệnh
- Kiểm tra khoảng cách (quá xa?)
- Kiểm tra hướng IR LED (có hướng đúng không?)
- Kiểm tra carrier frequency (có đúng 38kHz không?)
- Kiểm tra mã lệnh (có đúng không?)

### Tín Hiệu Yếu
- Tăng dòng qua IR LED (giảm resistor 100Ω)
- Sử dụng nhiều IR LED song song
- Kiểm tra transistor có đủ công suất không

---

## 📚 Tài Liệu Tham Khảo

1. **ESP32 GPIO:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/gpio.html
2. **ESP32 RMT:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/rmt.html
3. **IR LED Datasheet:** Tham khảo datasheet của IR LED bạn sử dụng
4. **Transistor Datasheet:** Tham khảo datasheet của transistor (2N2222, BC547, v.v.)

---

**Cập nhật:** 2024  
**GPIO TX:** 14 (confirmed)  
**GPIO RX:** 15 (optional, for capture)



