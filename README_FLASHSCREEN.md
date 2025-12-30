# Cài Đặt Hình Nền Flash Screen

## Hướng Dẫn Nhanh

1. **Đặt hình nền flashscreen của bạn:**
   - Tệp: `assets/boot/flashscreen_320x480.png`
   - Kích thước: 320x480 pixel (phải chính xác)
   - Định dạng: PNG (RGB)

2. **Chuyển đổi sang định dạng LVGL:**
   ```bash
   python tools/convert_flashscreen_to_lvgl_rgb565.py
   ```

3. **Biên dịch:**
   ```bash
   idf.py build
   ```

## Các Tệp Được Tạo

Sau khi chạy script chuyển đổi:
- `components/sx_assets/generated/flashscreen_img.c` - Dữ liệu hình ảnh (RGB565)
- `components/sx_assets/generated/flashscreen_img.h` - Tệp header

## Mô Tả Chức Năng

- Flash Screen (màn hình chờ/bảo vệ màn hình) hiện mặc định sử dụng hình ảnh mecha được nhúng làm nền
- Hình ảnh được nhúng vào firmware khi biên dịch (không cần thẻ SD)
- Định dạng hình ảnh: RGB565 little-endian (khớp với LVGL TRUE_COLOR 16-bit)
- Độ phân giải hiển thị: 320x480 (dọc)
- Flash Screen sẽ hiển thị hình ảnh tại vị trí (0,0), không thu phóng

## Tùy Chọn Cài Đặt

Nền Flash Screen có thể được cấu hình thông qua cài đặt:
- `"Embedded"` hoặc `"Default"` - Sử dụng hình flashscreen được nhúng (mặc định)
- `"Gradient"` - Sử dụng nền gradient (xanh đậm đến đen)
- `"Image 1"` - Tải từ thẻ SD `/sdcard/backgrounds/bg1.jpg`
- `"Image 2"` - Tải từ thẻ SD `/sdcard/backgrounds/bg2.jpg`
- `"Custom"` - Tải đường dẫn hình tùy chỉnh từ thẻ SD

## Yêu Cầu

- Python 3.x
- Thư viện Pillow: `pip install Pillow`

## Lưu Ý

- Nếu không đặt hình ảnh hoặc chuyển đổi thất bại, hệ thống sẽ quay về nền gradient
- Tệp placeholder đã được tạo, dự án vẫn có thể biên dịch ngay cả khi không có hình ảnh
- Hình ảnh sẽ tự động điều chỉnh theo kích thước màn hình (100% chiều rộng và chiều cao)

