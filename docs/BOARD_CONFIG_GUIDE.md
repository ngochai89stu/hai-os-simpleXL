# Hướng Dẫn Cấu Hình Board hai-os-simpleXL

## Tổng Quan

Dự án hai-os-simpleXL hỗ trợ cấu hình board và màn hình LCD thông qua Kconfig, cho phép dễ dàng thay đổi phần cứng mà không cần sửa code.

## Cách Sử Dụng

### 1. Mở Menu Cấu Hình

```bash
idf.py menuconfig
```

### 2. Chọn Board Type

Điều hướng đến menu: **hai-os-simplexl board config**

- Chọn **Board Type** → **hai-os-simpleXL**

### 3. Chọn Loại Màn Hình LCD

Trong menu **LCD Display Type**, chọn một trong các tùy chọn:

- **ST7796 320x480 IPS** (mặc định)
  - Độ phân giải: 320x480
  - RGB order: BGR
  - Phù hợp cho màn hình lớn

- **ST7789 240x320 IPS**
  - Độ phân giải: 240x320
  - RGB order: RGB
  - Phù hợp cho màn hình trung bình

- **ST7789 240x240**
  - Độ phân giải: 240x240
  - RGB order: RGB
  - Phù hợp cho màn hình vuông

- **ILI9341 240x320**
  - Độ phân giải: 240x320
  - RGB order: BGR
  - Phù hợp cho màn hình TFT phổ biến

- **Custom LCD (Tùy chỉnh)**
  - Cho phép tùy chỉnh độ phân giải
  - Sử dụng driver ST7796 mặc định

### 4. Cấu Hình Pins LCD

Điều chỉnh các chân GPIO cho LCD nếu cần:

- **LCD MOSI Pin** (mặc định: 47)
- **LCD CLK Pin** (mặc định: 21)
- **LCD CS Pin** (mặc định: 41)
- **LCD DC Pin** (mặc định: 40)
- **LCD RST Pin** (mặc định: 45)
- **LCD Backlight Pin** (mặc định: 42)

### 5. Cấu Hình Touch Panel

- **Enable Touch Panel** (mặc định: bật)
  - Bật/tắt hỗ trợ màn hình cảm ứng

Nếu bật touch, cấu hình các pins:

- **Touch I2C SDA Pin** (mặc định: 8)
- **Touch I2C SCL Pin** (mặc định: 11)
- **Touch RST Pin** (mặc định: 9)
- **Touch INT Pin** (mặc định: 13)

### 6. Cấu Hình Custom LCD

Nếu chọn **Custom LCD**, cần cấu hình thêm:

- **LCD Width (pixels)** (mặc định: 320)
- **LCD Height (pixels)** (mặc định: 480)

## Ví Dụ Cấu Hình

### Ví Dụ 1: ST7789 240x320 với Touch

```
Board Type: hai-os-simpleXL
LCD Display Type: ST7789 240x320 IPS
Enable Touch Panel: y
Touch pins: SDA=8, SCL=11, RST=9, INT=13
```

### Ví Dụ 2: ILI9341 không Touch

```
Board Type: hai-os-simpleXL
LCD Display Type: ILI9341 240x320
Enable Touch Panel: n
```

### Ví Dụ 3: Custom LCD 480x320

```
Board Type: hai-os-simpleXL
LCD Display Type: Custom LCD
LCD Width: 480
LCD Height: 320
```

## Lưu Ý

1. **RGB Order**: Mỗi loại LCD có RGB order khác nhau:
   - ST7796, ILI9341: BGR
   - ST7789: RGB
   - Code tự động xử lý theo loại LCD đã chọn

2. **Touch Panel**: 
   - Chỉ khởi tạo khi `Enable Touch Panel = y`
   - Sử dụng I2C bus số 1 (không conflict với volume I2C bus 0)

3. **SPI Bus**: 
   - LCD sử dụng SPI3_HOST
   - MISO pin (12) được chia sẻ với SD card

4. **Backlight**: 
   - Sử dụng PWM (LEDC) để điều khiển độ sáng
   - Có thể điều chỉnh từ 0-100%

## Build và Flash

Sau khi cấu hình xong:

```bash
idf.py build
idf.py flash monitor
```

## Troubleshooting

### LCD không hiển thị

1. Kiểm tra pins đã cấu hình đúng chưa
2. Kiểm tra SPI bus đã được khởi tạo
3. Kiểm tra backlight có bật không
4. Xem log để biết loại LCD nào đang được khởi tạo

### Touch không hoạt động

1. Kiểm tra `Enable Touch Panel = y`
2. Kiểm tra I2C pins (SDA, SCL)
3. Kiểm tra interrupt pin (INT)
4. Xem log để biết touch driver có khởi tạo thành công không

### Lỗi Build

1. Đảm bảo đã chọn `Board Type = hai-os-simpleXL`
2. Đảm bảo đã chọn một loại LCD
3. Kiểm tra các pins không conflict với các peripheral khác

## Tham Khảo

- [ESP-IDF LCD Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd.html)
- [ST7796 Datasheet](https://www.displayfuture.com/Display/datasheet/controller/ST7796.pdf)
- [ST7789 Datasheet](https://www.displayfuture.com/Display/datasheet/controller/ST7789.pdf)
- [ILI9341 Datasheet](https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf)

