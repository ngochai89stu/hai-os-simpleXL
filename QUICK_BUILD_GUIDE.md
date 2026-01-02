# QUICK BUILD GUIDE - UNIT TESTS

## 🚀 CÁCH BUILD NHANH

### Option 1: Sử dụng build script (Khuyến nghị)

```powershell
# Tự động tìm ESP-IDF
.\build_test.ps1

# Hoặc chỉ định path cụ thể
.\build_test.ps1 -IDF_PATH "D:\Espressif\frameworks\esp-idf-v5.1"
```

### Option 2: Build thủ công

#### Step 1: Set ESP-IDF Environment

**Tìm ESP-IDF path:**
- Thường ở: `D:\Espressif\frameworks\esp-idf-v5.x`
- Hoặc check environment variable: `$env:IDF_PATH`

**Export ESP-IDF:**
```powershell
# Thay đổi path theo vị trí thực tế
$env:IDF_PATH = "D:\Espressif\frameworks\esp-idf-v5.1"
. "$env:IDF_PATH\export.ps1"
```

#### Step 2: Build Tests

```powershell
cd test\unit_test
idf.py set-target esp32  # Chỉ cần chạy lần đầu
idf.py build
```

#### Step 3: Verify Build

Kiểm tra output:
- ✅ `build\unit_test.bin` được tạo
- ✅ Không có compilation errors
- ✅ All components linked successfully

---

## 📋 TEST FILES

Các test files đã được tạo:
- ✅ `test_event_handler.c` - 7 tests
- ✅ `test_event_priority.c` - 4 tests  
- ✅ `test_string_pool_metrics.c` - 6 tests
- ✅ `test_dispatcher.c` - 8 tests (updated)

**Tổng cộng: 25 tests**

---

## ⚠️ TROUBLESHOOTING

### Lỗi: `idf.py: command not found`
**Giải pháp:**
```powershell
# Export ESP-IDF trước
. "D:\Espressif\frameworks\esp-idf-v5.1\export.ps1"
```

### Lỗi: Missing components
**Giải pháp:**
- Check `test/unit_test/CMakeLists.txt` có đủ components
- Verify `REQUIRES` section

### Lỗi: Compilation errors
**Giải pháp:**
- Check all header files included
- Verify function signatures
- Check for typos trong test code

---

## 📊 EXPECTED BUILD OUTPUT

```
[100%] Built target unit_test.elf
```

Nếu thấy message này → Build thành công! ✅

---

*Sử dụng `build_test.ps1` script để build tự động.*









