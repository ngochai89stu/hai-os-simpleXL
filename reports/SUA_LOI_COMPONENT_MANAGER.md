# Sửa Lỗi Component Manager - 78/esp-opus

**Ngày:** 2025-01-27  
**Lỗi:** Component "78/esp-opus" thiếu file integrity check

---

## 🔧 Vấn Đề

```
CMake Error: File .component_hash or CHECKSUMS.json for component "78/esp-opus"
in the managed components directory does not exist or cannot be parsed.
```

**Nguyên nhân:**
- Thư mục `managed_components/78__esp-opus` tồn tại nhưng rỗng
- Component manager không thể verify integrity của component
- Component này là dependency của `78/esp-opus-encoder`

---

## ✅ Giải Pháp

### 1. Xóa Thư Mục Rỗng
- Đã xóa `managed_components/78__esp-opus` (thư mục rỗng)
- Component manager sẽ tự động tải lại component khi build

### 2. Làm Sạch CMake Cache (Tùy chọn)
- Xóa `build/CMakeCache.txt` nếu cần force rebuild

---

## 📋 Các Bước Tiếp Theo

1. **Build lại project:**
   ```bash
   idf.py build
   ```

2. **Component manager sẽ:**
   - Tự động tải `78/esp-opus` từ registry
   - Tạo file `.component_hash` hoặc `CHECKSUMS.json`
   - Verify integrity

---

## 🔍 Kiểm Tra

Sau khi build thành công, kiểm tra:
- `managed_components/78__esp-opus/` có nội dung
- File `.component_hash` hoặc `CHECKSUMS.json` tồn tại
- Component được build thành công

---

## ⚠️ Lưu Ý

- Không nên tạo file `.component_hash` thủ công
- Để component manager tự động quản lý
- Nếu vẫn lỗi, có thể cần xóa toàn bộ `managed_components` và tải lại

---

## ✅ Kết Luận

Đã xóa thư mục rỗng. Component manager sẽ tự động tải lại component khi build. Build lại project để verify.




















