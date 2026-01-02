# A/B TEST: Khoanh Vùng Nguyên Nhân "Âm Bản Giả"

## 🎯 MỤC TIÊU

Xác định nguyên nhân gốc của hiện tượng "âm bản giả" trên ST7796U:
- Nghi phạm #1: COLMOD conflict (0x55 vs 0x05)
- Nghi phạm #2: swap_bytes double-swap với BGR

## 📋 BƯỚC 1: CHUẨN HÓA INIT (ĐÃ THỰC HIỆN)

### Thay đổi:
**File:** `components/sx_platform/sx_platform.c`

**Loại bỏ 2 command ghi đè format:**
1. **COLMOD (0x3A)** - Line 38: `{0x3A, (uint8_t[]){0x55}, 1, 0}` → **COMMENTED OUT**
2. **MADCTL (0x36)** - Line 52: `{0x36, (uint8_t[]){0x48}, 1, 0}` → **COMMENTED OUT**

### Lý do:
- Để driver internal tự set MADCTL/COLMOD theo `panel_dev_config` trước
- Tránh "overwrite by external init sequence" warnings
- Giữ post-init commands (INVOFF + MADCTL + Gamma) để test nhất quán

### Kết quả mong đợi:
- ✅ Không còn warning: "The 3Ah command has been used and will be overwritten"
- ✅ Không còn warning: "The 36h command has been used and will be overwritten"
- ✅ Driver internal set COLMOD = 0x05 (RGB565) từ `bits_per_pixel = 16`
- ✅ Driver internal set MADCTL = 0x48 (BGR) từ `rgb_ele_order = ESP_LCD_COLOR_SPACE_BGR`

### Patch:
```c
// BEFORE:
{0x3A, (uint8_t[]){0x55}, 1, 0},   // COLMOD - RGB565 (0x55)
{0x36, (uint8_t[]){0x48}, 1, 0},   // MADCTL - BGR=ON (0x48)

// AFTER:
// {0x3A, (uint8_t[]){0x55}, 1, 0},   // COLMOD - REMOVED: Let driver internal set (0x05 for RGB565)
// {0x36, (uint8_t[]){0x48}, 1, 0},   // MADCTL - REMOVED: Let driver internal set, then post-init override
```

---

## 📋 BƯỚC 2: A/B TEST #1 - swap_bytes

### Test A: swap_bytes = 1 (HIỆN TẠI)
**File:** `components/sx_ui/sx_ui_task.c:81`
```c
.swap_bytes = 1,  // Enable byte swap
```

### Test B: swap_bytes = 0 (SẼ TEST)
**File:** `components/sx_ui/sx_ui_task.c:81`
```c
.swap_bytes = 0,  // Disable byte swap
```

### Cấu hình hiện tại:
```c
// sx_platform.c
panel_config.rgb_ele_order = ESP_LCD_COLOR_SPACE_BGR;  // BGR
panel_config.bits_per_pixel = 16;  // RGB565

// sx_ui_task.c
.swap_bytes = 1,  // LVGL port byte swap enabled
```

### Kết quả mong đợi:

**Nếu swap_bytes là root cause:**
- ✅ Test B (swap_bytes=0): Màu "âm bản giả" biến mất
- ⚠️ Có thể bị đảo kênh (RGB ↔ BGR) → cần Test #2 điều chỉnh `rgb_ele_order`

**Nếu swap_bytes KHÔNG phải root cause:**
- ❌ Test B: Vẫn còn "âm bản giả" → chuyển sang Test #2

### Deliverable:
- [ ] Ảnh chụp màn hình Test A (swap_bytes=1)
- [ ] Ảnh chụp màn hình Test B (swap_bytes=0)
- [ ] Log cấu hình LVGL
- [ ] Kết luận: swap_bytes có phải root cause không?

---

## 📋 BƯỚC 3: A/B TEST #2 - BGR/RGB element order

### Điều kiện:
- Chỉ test nếu Test #1 không giải quyết
- Giữ `.swap_bytes = 0` (từ Test #1)

### Test A: rgb_ele_order = BGR (HIỆN TẠI)
**File:** `components/sx_platform/sx_platform.c:102`
```c
.rgb_ele_order = ESP_LCD_COLOR_SPACE_BGR,
```

### Test B: rgb_ele_order = RGB (SẼ TEST)
**File:** `components/sx_platform/sx_platform.c:102`
```c
.rgb_ele_order = ESP_LCD_COLOR_SPACE_RGB,
```

### Kết quả mong đợi:

**Nếu rgb_ele_order là root cause:**
- ✅ Test B: Màu đúng, không còn "âm bản"
- ⚠️ Có thể đảo kênh (đỏ ↔ xanh) → cần điều chỉnh lại

**Nếu rgb_ele_order KHÔNG phải root cause:**
- ❌ Test B: Vẫn còn "âm bản giả" → chuyển sang Test #4

### Deliverable:
- [ ] Xác nhận cấu hình cuối cùng: `rgb_ele_order` + `swap_bytes`
- [ ] Kết luận: rgb_ele_order có phải root cause không?

---

## 📋 BƯỚC 4: CÔ LẬP COLMOD (Nếu vẫn sai)

### Mục tiêu:
- Xác định giá trị COLMOD đúng cho board/panel này
- Test từng giá trị COLMOD riêng biệt

### Test A: COLMOD = 0x05 (RGB565 standard)
**Post-init command:**
```c
// Sau panel_init, trước DISPON
uint8_t colmod = 0x05;  // RGB565
ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_handle, 0x3A, &colmod, 1));
```

### Test B: COLMOD = 0x55 (Giá trị cũ)
**Post-init command:**
```c
// Sau panel_init, trước DISPON
uint8_t colmod = 0x55;  // Giá trị cũ (có thể sai)
ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_handle, 0x3A, &colmod, 1));
```

### Logging:
Thêm log để xác nhận giá trị COLMOD thực tế:
```c
ESP_LOGI(TAG, "COLMOD set to 0x%02X (RGB565=0x05, RGB666=0x06, RGB888=0x07)", colmod);
```

### Kết quả mong đợi:
- ✅ Xác định giá trị COLMOD đúng (0x05 hoặc 0x55)
- ✅ Giải thích vì sao giá trị đó đúng cho board/panel này

### Deliverable:
- [ ] Log giá trị COLMOD thực tế
- [ ] Kết luận: Giá trị COLMOD đúng cho ST7796U

---

## 📊 TỔNG KẾT CẤU HÌNH

### Cấu hình hiện tại (TRƯỚC A/B test):
```c
// sx_platform.c
panel_config.rgb_ele_order = ESP_LCD_COLOR_SPACE_BGR;
panel_config.bits_per_pixel = 16;

// sx_ui_task.c
.swap_bytes = 1;

// External init_cmds
COLMOD = 0x55  // ❌ REMOVED
MADCTL = 0x48  // ❌ REMOVED
```

### Cấu hình sau Bước 1 (CHUẨN HÓA):
```c
// Driver internal sẽ set:
COLMOD = 0x05  // Từ bits_per_pixel = 16
MADCTL = 0x48  // Từ rgb_ele_order = BGR

// Post-init vẫn giữ:
INVOFF (0x20)
MADCTL (0x36) = 0x48
Gamma (0xE0, 0xE1)
```

### Cấu hình cuối cùng (SẼ CẬP NHẬT):
```c
// Sau khi hoàn thành tất cả A/B tests
rgb_ele_order = ?  // RGB hoặc BGR
swap_bytes = ?     // 0 hoặc 1
COLMOD = ?         // 0x05 hoặc 0x55
```

---

## 🔍 HƯỚNG DẪN ĐỌC KẾT QUẢ

### Nếu swap_bytes là root cause:
- **swap_bytes=0**: Màu "hết kiểu âm bản", nhưng có thể bị đảo kênh (RGB/BGR)
- **Giải pháp**: Điều chỉnh `rgb_ele_order` để fix đảo kênh

### Nếu COLMOD là root cause:
- **COLMOD sai**: Màu sai "toàn cục", đặc biệt vùng ảnh, đôi khi giống âm bản do cách pack pixel
- **Giải pháp**: Sửa COLMOD về giá trị đúng (0x05 cho RGB565)

### Nếu rgb_ele_order là root cause:
- **BGR/RGB sai**: Đảo kênh màu (đỏ ↔ xanh), nhưng không còn "âm bản"
- **Giải pháp**: Đổi `rgb_ele_order` và có thể cần điều chỉnh `swap_bytes`

---

## 📝 GHI CHÚ

- Mỗi lần chỉ thay đổi 1 biến để kết luận có bằng chứng
- Không thêm "guard chặn" hay workaround liên quan inversion
- Ưu tiên output: patch + log + ảnh so sánh A/B






















