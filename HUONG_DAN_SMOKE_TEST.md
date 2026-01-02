# HƯỚNG DẪN SỬ DỤNG SMOKE TEST

## Tổng Quan

Module `sx_selftest` cung cấp smoke test tự động để xác minh phần cứng và nền tảng sau khi boot. Test bao gồm:

1. **Boot Stability** - Kiểm tra boot ổn định (không reset loop)
2. **LVGL Init** - Kiểm tra LVGL đã init và có thể vẽ screen
3. **Touch Input** - Kiểm tra touch có phản hồi (5 điểm: 4 góc + giữa)
4. **SD Card** - Kiểm tra SD mount và list files (nếu có)
5. **Audio** - Kiểm tra audio init và playback (nếu có)
6. **Heap/PSRAM Sanity** - Kiểm tra heap và PSRAM tại boot và sau UI

## Cấu Hình

### Bật Smoke Test

1. Chạy menuconfig:
   ```bash
   idf.py menuconfig
   ```

2. Điều hướng đến:
   ```
   SimpleXL Core Configuration
     └─> Feature Configuration
         └─> Enable Smoke Test (Self-Test) [*]
   ```

3. Hoặc chỉnh sửa `sdkconfig` trực tiếp:
   ```
   CONFIG_SX_SELFTEST_ENABLE=y
   ```

### Tắt Smoke Test

Đặt `CONFIG_SX_SELFTEST_ENABLE=n` hoặc comment out trong code.

## Sử Dụng

### Tự Động (Sau Boot)

Smoke test tự động chạy sau khi UI init xong (khoảng 2 giây sau boot). Kết quả sẽ được in ra log:

```
========================================
SIMPLEXL SMOKE TEST - START
========================================
...
========================================
SMOKE TEST RESULTS
========================================
Boot Stability:        PASS
LVGL Init:             PASS
Screen Draw:           PASS
Touch Detected:        PASS
Touch 5 Points:        PASS
SD Mount:              PASS/FAIL
SD List Files:         PASS/FAIL
Audio Init:            PASS/FAIL
Audio Play:            PASS/FAIL
Heap Sanity:           PASS
PSRAM Sanity:          PASS

Boot Count:            3
Free Heap (Boot):      234567 bytes
Free Heap (UI):        198765 bytes
Free PSRAM (Boot):     1048576 bytes
Free PSRAM (UI):       987654 bytes
========================================
```

### Thủ Công (Qua Monitor)

1. Flash firmware với smoke test enabled:
   ```bash
   idf.py -p COM11 flash
   ```

2. Chạy monitor:
   ```bash
   idf.py -p COM11 monitor
   ```

3. Hoặc dùng script helper:
   ```powershell
   .\run_smoke_test.ps1 -Port COM11
   ```

4. Xem kết quả trong log (tìm "SMOKE TEST RESULTS")

## Test Cases Chi Tiết

### 1. Boot Stability Test

**Mục đích:** Kiểm tra boot ổn định, không reset loop

**Cách test:**
- Reset nguồn 3 lần liên tiếp
- Kiểm tra boot count tăng đều
- Không có panic/assert

**PASS Criteria:**
- Boot count tăng đều
- Không có reset loop
- Không có panic log

### 2. LVGL Init Test

**Mục đích:** Kiểm tra LVGL đã init và có thể vẽ screen

**Cách test:**
- Tự động: Module thử lock LVGL và tạo label test
- Kiểm tra label hiển thị được

**PASS Criteria:**
- `lvgl_port_lock()` thành công
- Tạo label thành công
- Label hiển thị được (1 giây)

### 3. Touch Test

**Mục đích:** Kiểm tra touch có phản hồi

**Cách test:**
- Chạm 5 điểm: 4 góc + giữa màn hình
- Module đếm số điểm chạm

**PASS Criteria:**
- Có phản hồi từ 5 điểm chạm
- Touch event được detect

**Lưu ý:** Test này cần implement touch listener để đếm điểm chạm.

### 4. SD Card Test

**Mục đích:** Kiểm tra SD card mount và list files

**Cách test:**
- Tự động: Module kiểm tra SD mount status
- List 3 files đầu tiên

**PASS Criteria:**
- SD mount thành công
- List được ít nhất 3 files

**Lưu ý:** Test này cần implement SD check API.

### 5. Audio Test

**Mục đích:** Kiểm tra audio init và playback

**Cách test:**
- Tự động: Module kiểm tra audio service init
- Play test tone 5-10 giây

**PASS Criteria:**
- Audio service init thành công
- Play test tone thành công

**Lưu ý:** Test này cần implement audio check API.

### 6. Heap/PSRAM Sanity Test

**Mục đích:** Kiểm tra heap và PSRAM không bị leak

**Cách test:**
- Tự động: Module đo free heap tại boot và sau UI
- So sánh để phát hiện leak

**PASS Criteria:**
- Free heap >= 50KB tại boot
- Free heap không giảm quá nhiều sau UI (tùy config)
- PSRAM OK (nếu có)

## Troubleshooting

### Smoke Test Không Chạy

1. Kiểm tra config:
   ```bash
   grep CONFIG_SX_SELFTEST_ENABLE sdkconfig
   ```
   Phải có `CONFIG_SX_SELFTEST_ENABLE=y`

2. Kiểm tra log:
   ```
   grep "sx_selftest" build_output.log
   ```

3. Kiểm tra code:
   - Đảm bảo `#include "sx_selftest.h"` trong `sx_bootstrap.c`
   - Đảm bảo `sx_selftest.c` được thêm vào CMakeLists.txt

### LVGL Test FAIL

1. Kiểm tra LVGL đã init:
   ```
   grep "LVGL" build_output.log
   ```

2. Kiểm tra lock timeout:
   - Tăng timeout trong `sx_selftest_test_lvgl()` nếu cần

### Touch Test FAIL

1. Kiểm tra touch đã init:
   ```
   grep "touch" build_output.log
   ```

2. Kiểm tra touch config:
   - Đảm bảo `CONFIG_HAI_TOUCH_ENABLE=y`

### Heap Test FAIL

1. Kiểm tra free heap:
   ```
   grep "Free Heap" build_output.log
   ```

2. Tối ưu memory usage nếu cần:
   - Giảm buffer sizes
   - Disable unused services

## API Reference

### sx_selftest_init()

Khởi tạo smoke test module.

```c
esp_err_t sx_selftest_init(void);
```

**Returns:**
- `ESP_OK` nếu thành công

### sx_selftest_run()

Chạy smoke test đầy đủ.

```c
esp_err_t sx_selftest_run(sx_selftest_result_t *result);
```

**Parameters:**
- `result` - Kết quả test (output)

**Returns:**
- `ESP_OK` nếu test chạy thành công (không có nghĩa là tất cả test đều PASS)

### sx_selftest_print_result()

In kết quả test ra log.

```c
void sx_selftest_print_result(const sx_selftest_result_t *result);
```

**Parameters:**
- `result` - Kết quả test

## Tùy Chỉnh

### Thay Đổi Timeout

Sửa trong `sx_selftest.c`:
```c
// Tăng timeout cho LVGL lock
if (!lvgl_port_lock(2000)) {  // 2 giây thay vì 1 giây
    ...
}
```

### Thêm Test Case Mới

1. Thêm field vào `sx_selftest_result_t` trong `sx_selftest.h`
2. Implement test function trong `sx_selftest.c`
3. Gọi test function trong `sx_selftest_run()`
4. Cập nhật `sx_selftest_print_result()` để in kết quả

## Lưu Ý

1. **Smoke test chạy sau UI init:** Đảm bảo UI đã init xong trước khi test LVGL
2. **Touch test cần manual:** Một số test cần tương tác thủ công (touch 5 điểm)
3. **SD/Audio test tùy chọn:** Test chỉ chạy nếu SD/Audio có sẵn
4. **Heap test:** Cần monitor theo thời gian để phát hiện leak

## Báo Cáo Bug

Nếu smoke test FAIL, vui lòng cung cấp:
1. Kết quả test đầy đủ (log)
2. Boot count
3. Free heap/PSRAM values
4. Các lỗi khác trong log

---

**Version:** 1.0  
**Last Updated:** 2025-01-27


