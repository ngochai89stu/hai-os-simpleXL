# Phân Tích Lỗi LoadProhibited Khi Khởi Tạo Touch

## Tổng Quan

Sau khi sửa code touch (thêm `scl_speed_hz`), hệ thống gặp lỗi **Guru Meditation Error: LoadProhibited** ngay sau khi display được khởi tạo.

## Thông Tin Lỗi

```
Guru Meditation Error: Core  1 panic'ed (LoadProhibited). Exception was unhandled.

Core  1 register dump:
PC      : 0x420d8839  PS      : 0x00060930  A0      : 0x820b03bf  A1      : 0x3fcee1f0
A2      : 0x8214c9e8  A3      : 0x3fcee220  A4      : 0x3fcaddec  A5      : 0x3fcee260
A6      : 0x00002580  A7      : 0x3fcee7d0  A8      : 0x0000000a  A9      : 0x3fcee1b0
A10     : 0x8214c9e8  A11     : 0x3fcee220  A12     : 0x3fcaddec  A13     : 0x3fcee7d0
A14     : 0x0000001e  A15     : 0x00000140  SAR     : 0x0000001d  EXCCAUSE: 0x0000001c
EXCVADDR: 0x8214c9f8  LBEG    : 0x400570e8  LEND    : 0x400570f3  LCOUNT  : 0x00000000

Backtrace: 0x420d8836:0x3fcee1f0 0x420b03bc:0x3fcee220 0x4200b507:0x3fcee250
```

**Thông tin quan trọng:**
- **EXCCAUSE: 0x0000001c** = LoadProhibited (truy cập vào địa chỉ không hợp lệ)
- **EXCVADDR: 0x8214c9f8** = Địa chỉ vi phạm (có thể là NULL hoặc invalid pointer)
- **PC: 0x420d8839** = Địa chỉ lệnh gây lỗi

## Phân Tích Code Hiện Tại

### Code Touch Init (sx_platform.c:249-265)

```c
// Create io_config manually (match repo mẫu) instead of using macro
esp_lcd_panel_io_i2c_config_t io_config = {
    .dev_addr = ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS,
    .control_phase_bytes = 1,
    .lcd_cmd_bits = 8,
    .flags = {
        .disable_control_phase = 0,
    },
};
// Set I2C clock frequency (required for ESP-IDF v5)
io_config.scl_speed_hz = CTP_I2C_FREQ_HZ;  // 400000

ESP_LOGI(TAG, "Touch IO config: dev_addr=0x%02X, control_phase_bytes=%d, lcd_cmd_bits=%d, scl_speed_hz=%d", 
         io_config.dev_addr, io_config.control_phase_bytes, io_config.lcd_cmd_bits, io_config.scl_speed_hz);

esp_lcd_panel_io_handle_t touch_io_handle = NULL;
ESP_LOGI(TAG, "Creating touch panel IO...");
esp_err_t ret = esp_lcd_new_panel_io_i2c(s_touch_i2c_bus_handle, &io_config, &touch_io_handle);
```

## Nguyên Nhân Có Thể

### 1. Struct Không Được Khởi Tạo Đầy Đủ

**Vấn đề:** Khi khởi tạo struct với designated initializer, các field không được khai báo sẽ có giá trị **không xác định** (uninitialized). Trong ESP-IDF v5, struct `esp_lcd_panel_io_i2c_config_t` có thể có thêm các field mới mà code không khởi tạo.

**Giải pháp:** Sử dụng `memset` hoặc khởi tạo toàn bộ struct về 0 trước:

```c
esp_lcd_panel_io_i2c_config_t io_config = {0};  // Zero-initialize
io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS;
io_config.control_phase_bytes = 1;
io_config.lcd_cmd_bits = 8;
io_config.scl_speed_hz = CTP_I2C_FREQ_HZ;
io_config.flags.disable_control_phase = 1;  // Sửa thành 1 (match repo mẫu)
```

### 2. Sử Dụng Macro Thay Vì Khởi Tạo Thủ Công

**Vấn đề:** Code hiện tại khởi tạo struct thủ công thay vì dùng macro `ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG()`. Macro này đảm bảo tất cả field được khởi tạo đúng.

**Giải pháp:** Sử dụng macro như repo mẫu:

```c
esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
io_config.scl_speed_hz = CTP_I2C_FREQ_HZ;  // Override frequency
```

### 3. I2C Bus Handle Không Hợp Lệ

**Vấn đề:** `s_touch_i2c_bus_handle` có thể là NULL hoặc không hợp lệ khi gọi `esp_lcd_new_panel_io_i2c()`.

**Kiểm tra:** Thêm assert hoặc check NULL:

```c
if (s_touch_i2c_bus_handle == NULL) {
    ESP_LOGE(TAG, "I2C bus handle is NULL!");
    return ESP_ERR_INVALID_STATE;
}
```

### 4. Field `scl_speed_hz` Được Set Sau Khi Khởi Tạo

**Vấn đề:** Trong ESP-IDF v5, có thể có field nào đó phụ thuộc vào `scl_speed_hz` và cần được set cùng lúc trong initializer.

**Giải pháp:** Set `scl_speed_hz` trong initializer:

```c
esp_lcd_panel_io_i2c_config_t io_config = {
    .dev_addr = ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS,
    .control_phase_bytes = 1,
    .lcd_cmd_bits = 8,
    .scl_speed_hz = CTP_I2C_FREQ_HZ,  // Set trong initializer
    .flags = {
        .disable_control_phase = 1,
    },
};
```

## So Sánh Với Repo Mẫu

### Repo Mẫu (esp32-s3-touch-lcd-3.5.cc:236-238)

```cpp
esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
tp_io_config.scl_speed_hz = 400 * 1000;
ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle));
```

**Điểm khác biệt:**
1. Repo mẫu dùng macro `ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG()` để khởi tạo
2. Sau đó mới override `scl_speed_hz`
3. Macro đảm bảo tất cả field được khởi tạo đúng

## Giải Pháp Đề Xuất

### Giải Pháp 1: Sử Dụng Macro (Khuyến Nghị)

```c
// Sử dụng macro như repo mẫu
esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
io_config.scl_speed_hz = CTP_I2C_FREQ_HZ;  // Override frequency
```

### Giải Pháp 2: Zero-Initialize Struct

```c
// Zero-initialize toàn bộ struct
esp_lcd_panel_io_i2c_config_t io_config = {0};
io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS;
io_config.control_phase_bytes = 1;
io_config.lcd_cmd_bits = 8;
io_config.scl_speed_hz = CTP_I2C_FREQ_HZ;
io_config.flags.disable_control_phase = 1;  // Sửa thành 1
```

### Giải Pháp 3: Set scl_speed_hz Trong Initializer

```c
esp_lcd_panel_io_i2c_config_t io_config = {
    .dev_addr = ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS,
    .control_phase_bytes = 1,
    .lcd_cmd_bits = 8,
    .scl_speed_hz = CTP_I2C_FREQ_HZ,  // Set trong initializer
    .flags = {
        .disable_control_phase = 1,  // Sửa thành 1
    },
};
```

## Kết Luận

**Nguyên nhân chính:** Struct `esp_lcd_panel_io_i2c_config_t` không được khởi tạo đầy đủ, dẫn đến một số field có giá trị rác. Khi ESP-IDF v5 truy cập vào các field này (có thể là pointer hoặc struct lồng nhau), gây ra lỗi LoadProhibited.

**Giải pháp tốt nhất:** Sử dụng macro `ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG()` như repo mẫu, sau đó override `scl_speed_hz` nếu cần.

**Lưu ý:** Cũng cần sửa `disable_control_phase = 1` để match với repo mẫu.


