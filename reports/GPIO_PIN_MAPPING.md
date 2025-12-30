# Bảng Xác Nhận GPIO Pin Mapping - SimpleXL OS

**Ngày:** 2025-01-27  
**Mục tiêu:** Xác nhận tất cả các chân GPIO đang được sử dụng trong dự án  
**Tham khảo:** Board mẫu `bread-compact-wifi-lcd` từ repo `xiaozhi-esp32_vietnam_ref`

---

## 📋 Tổng Quan

**ESP32-S3 GPIO Range:** 0-48  
**Tổng số GPIO đang sử dụng thực tế:** 13 chân (GPIO 4,5,6,7,8,9,10,12,13,14,15,16,47)  
**GPIO có conflict:** 2 chân (GPIO 7, GPIO 10)  
**GPIO đã tắt:** 4 chân (GPIO 15 IR RX, GPIO 19/23 BLE flow control, GPIO 22 I2C SCL)  

**Audio GPIO (PCM5102A) - Xác nhận:**
- **Microphone Input:** GPIO 4, 5, 6 (GPIO 4,5 cần xác nhận chức năng cụ thể)
- **Audio Output (PCM5102A):** GPIO 7 (DOUT), GPIO 15 (BCLK), GPIO 16 (WS)

---

## 🔌 Chi Tiết GPIO Pin Mapping

### 1. **LCD Display (ST7796 320x480)**

**✅ Đã cập nhật theo board mẫu `bread-compact-wifi-lcd`**

| Chức năng | GPIO (Cũ) | GPIO (Mới) | File | Ghi chú |
|-----------|-----------|------------|------|---------|
| MOSI (SPI Data) | GPIO 13 | **GPIO 47** ✅ | `sx_platform.c:13` | SPI3_HOST MOSI - **Theo board mẫu** |
| CLK (SPI Clock) | GPIO 14 | **GPIO 21** ✅ | `sx_platform.c:14` | SPI3_HOST CLK - **Theo board mẫu** |
| CS (Chip Select) | GPIO 10 | **GPIO 41** ✅ | `sx_platform.c:15` | LCD CS - **Theo board mẫu** |
| DC (Data/Command) | GPIO 9 | **GPIO 40** ✅ | `sx_platform.c:16` | LCD Data/Command - **Theo board mẫu** |
| RST (Reset) | GPIO 8 | **GPIO 45** ✅ | `sx_platform.c:17` | LCD Reset - **Theo board mẫu** |
| Backlight (PWM) | GPIO 7 | **GPIO 42** ✅ | `sx_platform.c:19` | LCD Backlight - **Theo board mẫu**, tránh conflict với Audio DOUT |

**SPI Host:** SPI3_HOST

---

### 2. **SD Card (SPI Mode)**

| Chức năng | GPIO | File | Ghi chú |
|-----------|------|------|---------|
| MISO | **GPIO 12** | `sx_bootstrap.c:123` | SPI3_HOST MISO |
| MOSI | **GPIO 47** | `sx_bootstrap.c:124` | SPI3_HOST MOSI |
| SCLK | **GPIO 21** | `sx_bootstrap.c:125` | ⚠️ **CONFLICT** với I2C SDA |
| CS | **GPIO 10** | `sx_bootstrap.c:126` | ⚠️ **CONFLICT** với LCD CS |

**SPI Host:** SPI3_HOST (shared với LCD)

---

### 3. **I2C (Codec Volume Control)**

| Chức năng | GPIO | File | Ghi chú |
|-----------|------|------|---------|
| SCL (Clock) | **GPIO 22** | `sx_platform_volume.c:21` | I2C Clock - điều khiển codec volume |
| SDA (Data) | **GPIO 21** | `sx_platform_volume.c:22` | ⚠️ **CONFLICT** với SD SCLK |

**I2C Port:** I2C_NUM_0  
**Frequency:** 100kHz  
**Devices:** ES8388 (0x10), ES8311 (0x18)

**Chức năng GPIO 22:**
- **I2C SCL (Serial Clock):** Dùng để đồng bộ giao tiếp I2C với codec chip (ES8388/ES8311)
- **Mục đích:** Điều khiển volume hardware của codec thay vì software volume
- **Hoạt động:** Gửi lệnh điều chỉnh volume qua I2C đến codec chip

**⚠️ VẤN ĐỀ:**
- GPIO 22 có thể không hợp lệ trên một số ESP32-S3 (cần kiểm tra datasheet)
- GPIO 21 conflict với SD card SCLK (nghiêm trọng)
- **CẦN XÁC NHẬN LẠI VỚI HARDWARE THỰC TẾ**

---

### 4. **Audio I2S - PCM5102A Codec**

#### **Audio Output (PCM5102A - Speaker/Headphone)**

| Chức năng | GPIO | File | Ghi chú |
|-----------|------|------|---------|
| DOUT (Data Output) | **GPIO 7** | `sx_audio_service.c:77` | **PCM5102A Data Output** - phát audio ra speaker/headphone |
| BCLK (Bit Clock) | **GPIO 15** | `sx_audio_service.c:75` | **PCM5102A Bit Clock** - đồng bộ từng bit dữ liệu |
| WS (Word Select/LRCLK) | **GPIO 16** | `sx_audio_service.c:76` | **PCM5102A Word Select** - phân biệt Left/Right channel |
| MCLK (Master Clock) | **I2S_GPIO_UNUSED** | `sx_audio_service.c:74` | PCM5102A không cần MCLK |

#### **Microphone Input**

| Chức năng | GPIO | File | Ghi chú |
|-----------|------|------|---------|
| DIN (Data Input) | **GPIO 6** | `sx_audio_service.c:78` | **Microphone Data Input** - nhận tín hiệu từ mic |
| Mic Control 1 | **GPIO 4** | - | **Cần xác nhận** - có thể là mic enable/select |
| Mic Control 2 | **GPIO 5** | - | **Cần xác nhận** - có thể là mic power/control |

**Giải thích:**
- **PCM5102A Codec (Audio Output):** Sử dụng GPIO 7, 15, 16
  - **GPIO 7 (DOUT):** Data output đến PCM5102A → Speaker/Headphone OUT
  - **GPIO 15 (BCLK):** Bit clock để đồng bộ từng bit dữ liệu
  - **GPIO 16 (WS):** Word select (LRCLK) để phân biệt kênh trái/phải
  - PCM5102A là DAC không cần MCLK, chỉ cần BCLK và WS
- **Microphone Input:** Sử dụng GPIO 4, 5, 6
  - **GPIO 6 (DIN):** Data input từ microphone qua I2S - nhận tín hiệu audio
  - **GPIO 4:** Mic Control 1 - cần xác nhận (có thể là mic enable, select, hoặc I2S clock cho mic)
  - **GPIO 5:** Mic Control 2 - cần xác nhận (có thể là mic power, gain control, hoặc I2S signal khác)

**⚠️ VẤN ĐỀ:**
- GPIO 7 conflict với LCD backlight (GPIO 7)
- **CẦN XÁC NHẬN LẠI VỚI HARDWARE THỰC TẾ**

---

### 5. **IR (Infrared)**

| Chức năng | GPIO | File | Ghi chú |
|-----------|------|------|---------|
| TX (Transmit) | **GPIO 14** | `sx_bootstrap.c:340` | ⚠️ **CONFLICT** với LCD CLK |
| RX (Receive) | **GPIO 15** | `sx_bootstrap.c:341` | ⚠️ **KHÔNG SỬ DỤNG** - chỉ dùng TX |

**Carrier Frequency:** 38kHz  
**Trạng thái:**
- **TX (GPIO 14):** ✅ Đang sử dụng để phát tín hiệu IR
- **RX (GPIO 15):** ❌ **KHÔNG SỬ DỤNG** - Code có implement nhưng không được gọi trong thực tế
- **⚠️ VẤN ĐỀ:**
  - GPIO 14 conflict với LCD CLK (nghiêm trọng)
  - GPIO 15 có thể giải phóng vì không dùng RX

---

### 6. **BLE (Bluetooth Low Energy)**

| Chức năng | GPIO | File | Ghi chú |
|-----------|------|------|---------|
| HCI UART RTS | **GPIO 19** | `sdkconfig:1051` | BLE HCI Flow Control (RTS - Request To Send) |
| HCI UART CTS | **GPIO 23** | `sdkconfig:1052` | BLE HCI Flow Control (CTS - Clear To Send) |

**Giải thích:**
- **RTS (Request To Send):** GPIO 19 - Báo hiệu BLE controller sẵn sàng nhận dữ liệu
- **CTS (Clear To Send):** GPIO 23 - Báo hiệu BLE controller sẵn sàng gửi dữ liệu
- **Lưu ý:** Flow control đã được tắt (`CONFIG_BT_NIMBLE_HCI_UART_FLOW_CTRL=0`) nên 2 GPIO này có thể không hoạt động
- **⚠️ Có thể giải phóng:** Nếu flow control không cần thiết, có thể giải phóng GPIO 19 và 23

---

### 7. **Touch Screen (Chưa implement)**

| Chức năng | GPIO | File | Ghi chú |
|-----------|------|------|---------|
| Touch I2C SCL | **Chưa định nghĩa** | - | Dự kiến: GPIO 18 (theo examples) |
| Touch I2C SDA | **Chưa định nghĩa** | - | Dự kiến: GPIO 8 (theo examples) |
| Touch INT | **Chưa định nghĩa** | - | Dự kiến: GPIO 3 (theo examples) |

**Status:** Stub implementation - chưa có hardware init

---

## ⚠️ Các Conflict GPIO

### 1. **GPIO 7 - LCD Backlight vs Audio DOUT**
- **LCD Backlight:** `sx_platform.c:19`
- **Audio DOUT:** `sx_audio_service.c:77`
- **Giải pháp:** Cần xác nhận hardware - có thể được thiết kế để share hoặc cần dùng GPIO khác

### 2. **GPIO 10 - LCD CS vs SD CS**
- **LCD CS:** `sx_platform.c:15`
- **SD CS:** `sx_bootstrap.c:126`
- **Giải pháp:** Có thể share vì cùng SPI3_HOST, nhưng cần quản lý CS riêng biệt

### 3. **GPIO 14 - LCD CLK vs IR TX**
- **LCD CLK:** `sx_platform.c:14`
- **IR TX:** `sx_bootstrap.c:340`
- **Giải pháp:** ⚠️ **CONFLICT NGHIÊM TRỌNG** - cần xác nhận hardware

### 4. **GPIO 15 - Audio BCLK vs IR RX**
- **Audio BCLK:** `sx_audio_service.c:75` ✅ Đang sử dụng
- **IR RX:** `sx_bootstrap.c:341` ❌ **KHÔNG SỬ DỤNG** - chỉ config nhưng không gọi
- **Giải pháp:** ✅ **KHÔNG CÓ CONFLICT** - IR RX không được sử dụng thực tế

### 5. **GPIO 21 - SD SCLK vs I2C SDA**
- **SD SCLK:** `sx_bootstrap.c:125`
- **I2C SDA:** `sx_platform_volume.c:22`
- **Giải pháp:** ⚠️ **CONFLICT NGHIÊM TRỌNG** - cần xác nhận hardware

---

## 📊 Tổng Hợp GPIO Sử Dụng

| GPIO | Chức năng | File | Status |
|------|-----------|------|--------|
| **4** | **Mic Control 1** | - | ⚠️ **CẦN XÁC NHẬN** - mic enable/select/I2S? |
| **5** | **Mic Control 2** | - | ⚠️ **CẦN XÁC NHẬN** - mic power/gain/I2S? |
| **6** | **Microphone Input (DIN)** | `sx_audio_service.c:78` | ✅ OK - **MIC IN (I2S DIN)** |
| **7** | LCD Backlight / **PCM5102A DOUT** | `sx_platform.c:19`, `sx_audio_service.c:77` | ⚠️ CONFLICT - **SPEAKER OUT** |
| **8** | LCD RST | `sx_platform.c:17` | ✅ OK |
| **9** | LCD DC | `sx_platform.c:16` | ✅ OK |
| **10** | LCD CS / SD CS | `sx_platform.c:15`, `sx_bootstrap.c:126` | ⚠️ CONFLICT (có thể share) |
| **12** | SD MISO | `sx_bootstrap.c:123` | ✅ OK |
| **13** | LCD MOSI | `sx_platform.c:13` | ✅ OK |
| **14** | LCD CLK / IR TX | `sx_platform.c:14`, `sx_bootstrap.c:340` | ⚠️ CONFLICT |
| **15** | **PCM5102A BCLK** | `sx_audio_service.c:75` | ✅ OK - **Audio Bit Clock** |
| **16** | **PCM5102A WS** | `sx_audio_service.c:76` | ✅ OK - **Audio Word Select** |
| **19** | BLE HCI RTS | `sdkconfig:1051` | ⚠️ ĐÃ TẮT - không sử dụng |
| **21** | SD SCLK | `sx_bootstrap.c:125` | ✅ OK (I2C SDA đã tắt) |
| **22** | I2C SCL | `sx_platform_volume.c:21` | ⚠️ ĐÃ TẮT - không sử dụng |
| **23** | BLE HCI CTS | `sdkconfig:1052` | ⚠️ ĐÃ TẮT - không sử dụng |
| **47** | SD MOSI | `sx_bootstrap.c:124` | ✅ OK |

---

## 🔍 Phân Tích Conflict

### Conflict Nghiêm Trọng (Cần xử lý ngay):

1. **GPIO 14: LCD CLK vs IR TX**
   - LCD CLK là SPI clock, cần hoạt động liên tục
   - IR TX chỉ hoạt động khi gửi IR signal
   - **Giải pháp:** Có thể share nếu IR TX chỉ dùng khi LCD không active, nhưng không khuyến nghị

2. **GPIO 15: Audio BCLK vs IR RX**
   - Audio BCLK cần hoạt động liên tục khi phát audio
   - IR RX chỉ hoạt động khi nhận IR signal
   - **Giải pháp:** Có thể share nếu IR RX chỉ dùng khi audio không active, nhưng không khuyến nghị

3. **GPIO 21: SD SCLK vs I2C SDA**
   - SD SCLK cần hoạt động khi truy cập SD card
   - I2C SDA cần hoạt động khi điều khiển codec
   - **Giải pháp:** Không thể share - cần dùng GPIO khác cho I2C

### Conflict Có Thể Xử Lý:

1. **GPIO 7: LCD Backlight vs Audio DOUT**
   - Có thể được thiết kế để share nếu hardware hỗ trợ
   - Hoặc cần dùng GPIO khác cho một trong hai

2. **GPIO 10: LCD CS vs SD CS**
   - Có thể share vì cùng SPI3_HOST, nhưng cần quản lý CS riêng biệt

### GPIO Có Vấn Đề:

1. **GPIO 22: I2C SCL**
   - Có thể không hợp lệ trên một số ESP32-S3
   - Cần kiểm tra hardware và datasheet

---

## ✅ Đề Xuất Giải Pháp

### 1. **I2C Pin Mapping (Ưu tiên cao)**

**Vấn đề:** GPIO 21 conflict với SD SCLK, GPIO 22 có thể không hợp lệ

**Đề xuất:**
- **Option 1:** Dùng GPIO 18 (SCL) và GPIO 19 (SDA) - nhưng GPIO 19 đã dùng cho BLE RTS
- **Option 2:** Dùng GPIO 4 (SCL) và GPIO 5 (SDA) - cần kiểm tra hardware
- **Option 3:** Dùng GPIO 2 (SCL) và GPIO 3 (SDA) - cần kiểm tra hardware

**⚠️ CẦN XÁC NHẬN VỚI HARDWARE THỰC TẾ**

### 2. **IR Pin Mapping (Ưu tiên trung bình)**

**Vấn đề:** GPIO 14 conflict với LCD CLK, GPIO 15 conflict với Audio BCLK

**Đề xuất:**
- **IR TX:** Dùng GPIO 4 hoặc GPIO 5
- **IR RX:** Dùng GPIO 2 hoặc GPIO 3

**⚠️ CẦN XÁC NHẬN VỚI HARDWARE THỰC TẾ**

### 3. **Audio DOUT (Ưu tiên thấp)**

**Vấn đề:** GPIO 7 conflict với LCD Backlight

**Đề xuất:**
- Nếu hardware không hỗ trợ share, dùng GPIO 4 hoặc GPIO 5

**⚠️ CẦN XÁC NHẬN VỚI HARDWARE THỰC TẾ**

---

## 📝 GPIO Trống (Có Thể Sử Dụng)

Các GPIO chưa được sử dụng và có thể dùng cho các chức năng mới:

- **GPIO 0-3:** Chưa sử dụng (cần kiểm tra strapping pins - GPIO 0 có thể là boot pin)
- **GPIO 4, 5:** Đã xác nhận cho microphone (cần xác nhận chức năng cụ thể)
- **GPIO 11:** Chưa sử dụng
- **GPIO 17:** Chưa sử dụng
- **GPIO 18:** Chưa sử dụng (dự kiến cho Touch I2C SCL)
- **GPIO 20:** Chưa sử dụng
- **GPIO 24-46:** Chưa sử dụng (trừ GPIO 47)
- **GPIO 48:** Chưa sử dụng

**⚠️ LƯU Ý:** Một số GPIO có thể là strapping pins hoặc có chức năng đặc biệt - cần kiểm tra datasheet ESP32-S3

---

## 🎯 Kết Luận

1. **Tổng số GPIO đang sử dụng thực tế:** 13 chân
2. **Số conflict nghiêm trọng:** 1 (GPIO 14 - LCD CLK vs IR TX)
3. **Số conflict có thể xử lý:** 2 (GPIO 7 - LCD Backlight vs Audio OUT, GPIO 10 - LCD CS vs SD CS)
4. **GPIO đã tắt:** 4 chân (GPIO 15 IR RX, GPIO 19/23 BLE, GPIO 22 I2C)

**Audio GPIO xác nhận (PCM5102A):**
- **Microphone Input:**
  - **GPIO 4:** Mic Control 1 ⚠️ Cần xác nhận chức năng
  - **GPIO 5:** Mic Control 2 ⚠️ Cần xác nhận chức năng
  - **GPIO 6:** Microphone Data Input (DIN) ✅
- **Audio Output (PCM5102A):**
  - **GPIO 7:** PCM5102A Data Output (DOUT) ⚠️ Conflict với LCD Backlight
  - **GPIO 15:** PCM5102A Bit Clock (BCLK) ✅
  - **GPIO 16:** PCM5102A Word Select (WS) ✅

**Hành động cần thiết:**
1. ✅ Xác nhận lại pin mapping với hardware thực tế
2. ✅ Giải quyết các conflict nghiêm trọng
3. ✅ Validate GPIO 22 có hợp lệ không
4. ✅ Cập nhật pin mapping sau khi xác nhận

---

## 📌 File Tham Khảo

- `components/sx_platform/sx_platform.c` - LCD pin mapping
- `components/sx_core/sx_bootstrap.c` - SD card và IR pin mapping
- `components/sx_platform/sx_platform_volume.c` - I2C pin mapping
- `components/sx_services/sx_audio_service.c` - Audio I2S pin mapping
- `sdkconfig` - BLE HCI pin mapping

---

**Lưu ý:** Tài liệu này dựa trên code hiện tại. Cần xác nhận lại với hardware thực tế và schematic.

