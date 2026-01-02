# DANH SÁCH CÔNG VIỆC CÒN LẠI

> Cập nhật: Sau khi hoàn thành P0, P1, và board config

---

## ✅ ĐÃ HOÀN THÀNH

### P0 - Stability Fixes (6/6) ✅
- ✅ P0-01: Router double on_hide() - ĐÃ FIX
- ✅ P0-02: LVGL lock discipline - ĐÃ FIX
- ✅ P0-03: Dispatcher drop events - ĐÃ FIX (thêm metric + log rate-limit)
- ✅ P0-04: Resource leak init fail - ĐÃ FIX (cleanup io_handle)
- ✅ P0-05: Double-handle event - ĐÃ FIX (gom xử lý event)
- ✅ P0-06: String pool size - ĐÃ FIX (tăng từ 8 lên 16)

### P1 - Refactoring & Optimization (4/4) ✅
- ✅ P1-01: Lưu chat content vào state - ĐÃ FIX
- ✅ P1-02: Refactor JSON parser chung - ĐÃ FIX
- ✅ P1-03: Fix audio hot path malloc - ĐÃ FIX (dùng reusable buffer)
- ✅ P1-04: Đưa pinmap vào Kconfig - ĐÃ FIX (LCD/Touch pins)

### Board Config & LCD Support ✅
- ✅ Tạo board config hai-os-simpleXL
- ✅ Hỗ trợ chọn loại LCD (ST7796, ST7789, ILI9341)
- ✅ Externalize LCD/Touch pins vào Kconfig
- ✅ Tài liệu hướng dẫn sử dụng

---

## 🔴 ƯU TIÊN CAO (Cần làm sớm)

### 1. Test Build với các cấu hình khác nhau
- **Mục tiêu:** Đảm bảo board config hoạt động đúng với mọi loại LCD
- **Công việc:**
  - Test build với ST7796 320x480
  - Test build với ST7789 240x320
  - Test build với ST7789 240x240
  - Test build với ILI9341 240x320
  - Test với touch enabled/disabled
- **Thời gian:** 1-2 giờ
- **Lợi ích:** Xác nhận tính năng hoạt động đúng

### 2. Hoàn thiện LCD Driver Support
- **Vấn đề:** Custom LCD vẫn dùng ST7796 driver
- **Công việc:**
  - Có thể thêm hỗ trợ các LCD driver khác nếu cần
  - Hoặc cải thiện custom LCD config
- **Thời gian:** 2-3 giờ
- **Lợi ích:** Linh hoạt hơn cho custom hardware

### 3. Rà soát và xử lý TODO/FIXME còn lại
- **Hiện tại:** 17 TODO/FIXME trong 9 files
- **Công việc:**
  - Phân loại: có thể fix ngay / để lại / obsolete
  - Fix những cái đơn giản
  - Cải thiện comment cho những cái để lại
- **Thời gian:** 2-3 giờ
- **Lợi ích:** Code quality tốt hơn

---

## 🟡 ƯU TIÊN TRUNG BÌNH (P2 - Có thể làm sau)

### P2-01: Bổ sung Unit Tests
- **Setup:** ESP-IDF unit test framework
- **Focus:** Core modules (dispatcher, orchestrator, state)
- **Thời gian:** 1-2 tuần
- **Lợi ích:** Tăng độ tin cậy, dễ refactor

### P2-02: Bổ sung Integration Tests
- **Setup:** Test framework cho services
- **Focus:** Audio, network, chatbot flows
- **Thời gian:** 2-3 tuần
- **Lợi ích:** Đảm bảo tính năng hoạt động đúng

### P2-03: Security Audit
- **Focus:** JSON parsing, string operations, network input
- **Kiểm tra:**
  - Buffer overflow risks
  - Input validation
  - Bounds checking
- **Thời gian:** 1 tuần
- **Lợi ích:** Tăng bảo mật

### P2-04: API Documentation
- **Setup:** Doxygen hoặc similar
- **Focus:** Public APIs của các components
- **Thời gian:** 1 tuần
- **Lợi ích:** Dễ sử dụng và maintain

---

## 🟢 ƯU TIÊN THẤP (Nice to have)

### 1. Cải thiện Error Handling
- **Vấn đề:** Một số nơi chưa có error handling đầy đủ
- **Công việc:**
  - Review error paths
  - Thêm error recovery
  - Cải thiện error messages

### 2. Performance Optimization
- **Vấn đề:** Có thể tối ưu thêm một số hot paths
- **Công việc:**
  - Profile code
  - Tối ưu memory allocation
  - Tối ưu UI rendering

### 3. Code Cleanup
- **Vấn đề:** Một số code có thể refactor
- **Công việc:**
  - Remove dead code
  - Simplify complex functions
  - Improve naming

### 4. Additional Features
- **Công việc:**
  - OGG decoder support (đã có TODO)
  - Gapless playback (đã có TODO)
  - Full ID3v2 parsing (đã có TODO)
  - RMT encoder migration (đã có TODO)
  - ADC migration (đã có TODO)

---

## 📊 TỔNG KẾT

### Đã hoàn thành:
- ✅ Tất cả P0 (6/6)
- ✅ Tất cả P1 (4/4)
- ✅ Board config và LCD support
- ✅ Tài liệu hướng dẫn

### Còn lại:
- 🔴 **Ưu tiên cao:** 3 mục (test build, hoàn thiện LCD, TODO review)
- 🟡 **Ưu tiên trung bình:** 4 mục P2 (testing, security, docs)
- 🟢 **Ưu tiên thấp:** 4 mục (optimization, cleanup, features)

### Khuyến nghị:
1. **Làm ngay:** Test build với các cấu hình LCD khác nhau
2. **Làm tiếp:** Rà soát và xử lý TODO/FIXME
3. **Làm sau:** P2 items (testing, security audit)

---

*Cập nhật lần cuối: Sau khi hoàn thành board config và LCD support*









