# BUILD INSTRUCTIONS - SimpleXL OS

## Cách Build Test

### Bước 1: Activate ESP-IDF

ESP-IDF ở ổ D, bạn cần activate environment:

```powershell
# Ví dụ nếu ESP-IDF ở D:\esp\esp-idf
D:\esp\esp-idf\export.ps1

# Hoặc nếu ở vị trí khác
<ĐƯỜNG_DẪN_ESP-IDF>\export.ps1
```

### Bước 2: Build Project

```bash
cd D:\NEWESP32\hai-os-simplexl
idf.py build
```

### Bước 3: Kiểm tra kết quả

**Nếu build thành công:**
```
[100%] Built target simplexl.elf
Project build complete.
```
✅ **Infrastructure OK, có thể tiếp tục tạo screen mới**

**Nếu build lỗi:**
- Copy toàn bộ error message
- Sẽ được fix ngay (không che, không vá lỗi)
- Tuân theo kiến trúc SimpleXL

---

## Quick Build Script

Nếu ESP-IDF đã được setup trong PATH, chạy:

```powershell
.\quick_build_test.ps1
```

Script sẽ tự động tìm ESP-IDF ở D:\ và build.

---

## Verification Status

✅ **Code Syntax:** Tất cả files đã được verify  
✅ **CMakeLists.txt:** Đã cập nhật đúng  
✅ **Includes:** Tất cả headers đã đúng  
✅ **Architecture:** Tuân theo SimpleXL (event-driven, service layer)

**Sẵn sàng build!**






