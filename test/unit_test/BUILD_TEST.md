# BUILD UNIT TESTS - HƯỚNG DẪN

## ⚠️ QUAN TRỌNG: Cần ESP-IDF Environment

Trước khi build, bạn cần export ESP-IDF environment.

## 🚀 CÁCH BUILD

### Step 1: Tìm ESP-IDF Path

ESP-IDF thường ở:
- `D:\Espressif\frameworks\esp-idf-v5.1`
- `D:\Espressif\frameworks\esp-idf-v5.2`  
- `D:\esp\esp-idf`
- Hoặc path bạn đã cài ESP-IDF

### Step 2: Export ESP-IDF

**PowerShell:**
```powershell
# Thay đổi path theo vị trí thực tế của ESP-IDF
$env:IDF_PATH = "D:\Espressif\frameworks\esp-idf-v5.1"
. "$env:IDF_PATH\export.ps1"
```

**CMD:**
```cmd
set IDF_PATH=D:\Espressif\frameworks\esp-idf-v5.1
D:\Espressif\frameworks\esp-idf-v5.1\export.bat
```

### Step 3: Build Tests

```powershell
# Đảm bảo đang ở trong test/unit_test directory
cd D:\NEWESP32\hai-os-simplexl\test\unit_test

# Set target (chỉ cần chạy lần đầu)
idf.py set-target esp32

# Build
idf.py build
```

## ✅ VERIFY BUILD

Sau khi build thành công, bạn sẽ thấy:
- `build\unit_test.bin` được tạo
- Message: `[100%] Built target unit_test.elf`

## 📋 TEST FILES

Các test files đã được tạo:
- ✅ `test_event_handler.c` - Event Handler Registry tests (7 tests)
- ✅ `test_event_priority.c` - Event Priority System tests (4 tests)
- ✅ `test_string_pool_metrics.c` - String Pool Metrics tests (6 tests)
- ✅ `test_dispatcher.c` - Dispatcher tests (8 tests, updated)
- ✅ `test_state.c` - State tests (existing)

**Tổng cộng: 25 tests**

## ⚠️ TROUBLESHOOTING

### Lỗi: `idf.py: command not found`
**Giải pháp:** Export ESP-IDF trước (Step 2)

### Lỗi: Missing components
**Giải pháp:** 
- Check `CMakeLists.txt` có đủ components
- Verify project structure

### Lỗi: Compilation errors
**Giải pháp:**
- Check all header files included
- Verify function signatures match

---

*Nếu ESP-IDF đã được setup trong system, chỉ cần chạy `idf.py build` từ test/unit_test directory.*











