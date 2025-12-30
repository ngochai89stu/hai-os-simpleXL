# Hướng Dẫn Enable BLE cho Navigation

## Tổng Quan

Để sử dụng tính năng BLE Navigation (nhận dữ liệu từ Android app), bạn cần enable BLE trong ESP-IDF.

## Cách Enable BLE

### Phương Pháp 1: Sử dụng menuconfig

```bash
idf.py menuconfig
```

Sau đó điều hướng đến:
```
Component config → Bluetooth → [*] Bluetooth
```

Chọn `Bluetooth` và nhấn Space để enable.

### Phương Pháp 2: Sử dụng sdkconfig

Mở file `sdkconfig` và tìm dòng:
```
# CONFIG_BT_ENABLED is not set
```

Thay đổi thành:
```
CONFIG_BT_ENABLED=y
```

### Phương Pháp 3: Sử dụng sdkconfig.defaults

Thêm vào file `sdkconfig.defaults`:
```
CONFIG_BT_ENABLED=y
```

## Cấu Hình BLE NimBLE

Sau khi enable BLE, ESP-IDF sẽ sử dụng NimBLE stack. Các cấu hình mặc định thường đủ cho navigation service.

### Kiểm Tra Cấu Hình

Chạy `idf.py menuconfig` và kiểm tra:
```
Component config → Bluetooth → Controller Options
Component config → Bluetooth → NimBLE Options
```

## Build và Flash

Sau khi enable BLE:

```bash
idf.py build
idf.py flash monitor
```

## Kiểm Tra BLE Hoạt Động

Khi ESP32 khởi động, bạn sẽ thấy log:
```
I (xxx) sx_nav_ble: BLE Navigation service initialized
I (xxx) sx_nav_ble: BLE GATT Server initialized
I (xxx) sx_nav_ble: BLE advertising started
```

## Kết Nối với Android App

1. Mở Android app trên điện thoại
2. Scan và tìm device tên "SimpleXL-Nav"
3. Kết nối với device
4. Mở Google Maps và bắt đầu navigation
5. Android app sẽ tự động đọc notification từ Google Maps và gửi data qua BLE

## Lưu Ý

- BLE sẽ tiêu tốn thêm RAM và flash
- Nếu không cần BLE, có thể disable để tiết kiệm tài nguyên
- Navigation service vẫn hoạt động ở chế độ stub (không cần BLE) nếu chỉ dùng MCP tools

## Troubleshooting

### BLE không khởi động
- Kiểm tra `CONFIG_BT_ENABLED=y` trong sdkconfig
- Kiểm tra log xem có lỗi gì không
- Đảm bảo ESP32 có hỗ trợ BLE (ESP32, ESP32-S2, ESP32-S3 đều hỗ trợ)

### Không tìm thấy device
- Kiểm tra BLE advertising đã start chưa (xem log)
- Đảm bảo điện thoại đã bật Bluetooth
- Thử reset ESP32 và scan lại

### Kết nối nhưng không nhận data
- Kiểm tra Android app đã được cài đặt và cấp quyền Notification Listener chưa
- Kiểm tra Google Maps đang chạy navigation
- Xem log ESP32 để kiểm tra có nhận được data không



