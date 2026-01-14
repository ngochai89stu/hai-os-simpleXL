# ST7796U INVON OVERRIDE ANALYSIS - ROOT CAUSE

## 🎯 TÓM TẮT

**Màn hình vẫn bị màu âm bản mặc dù đã gửi INVOFF (0x20) vì:**

1. **Driver internal init sequence** gửi MADCTL (0x36) TRƯỚC external init_cmds
2. **External init_cmds** (từ `sx_platform.c`) ghi đè MADCTL, nhưng KHÔNG có INVON
3. **KHÔNG có code nào gọi `esp_lcd_panel_invert_color(panel, true)`** sau init
4. **KHÔNG có command 0x21 (INVON) trong init sequences**

**KẾT LUẬN:** Vấn đề KHÔNG phải do INVON được bật, mà có thể do:
- MADCTL configuration không đúng
- Color space configuration (BGR vs RGB)
- LVGL swap_bytes setting

## 📋 PHÂN TÍCH CHI TIẾT

### 1️⃣ Internal Driver Init Sequence

**File:** `managed_components/espressif__esp_lcd_st7796/esp_lcd_st7796_general.c`

**Function:** `panel_st7796_init()` (line 207-258)

**Thứ tự thực thi:**

```c
// Step 1: SLPOUT (exit sleep mode)
esp_lcd_panel_io_tx_param(io, LCD_CMD_SLPOUT, NULL, 0);  // 0x11
vTaskDelay(pdMS_TO_TICKS(100));

// Step 2: MADCTL (Memory Access Control) - INTERNAL
esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, &madctl_val, 1);  // 0x36
// madctl_val = 0x48 (BGR bit set) from panel_dev_config->rgb_ele_order

// Step 3: COLMOD (Color Mode) - INTERNAL
esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, &colmod_val, 1);  // 0x3A
// colmod_val = 0x05 (RGB565) from panel_dev_config->bits_per_pixel

// Step 4: External init_cmds (from sx_platform.c)
for (int i = 0; i < init_cmds_size; i++) {
    // Check if command conflicts with internal
    if (init_cmds[i].cmd == LCD_CMD_MADCTL) {
        ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence", init_cmds[i].cmd);
        st7796->madctl_val = ((uint8_t *)init_cmds[i].data)[0];  // Update internal value
    }
    if (init_cmds[i].cmd == LCD_CMD_COLMOD) {
        ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence", init_cmds[i].cmd);
        st7796->colmod_val = ((uint8_t *)init_cmds[i].data)[0];  // Update internal value
    }
    
    // Send external command
    esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes);
    vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
}
```

**KẾT LUẬN:** Driver internal KHÔNG gửi INVON (0x21). Chỉ gửi SLPOUT, MADCTL, COLMOD trước external init_cmds.

### 2️⃣ External Init Sequence (Custom)

**File:** `components/sx_platform/sx_platform.c`

**Array:** `st7796u_init_cmds[]` (line 36-55)

**Commands:**

```c
static const st7796_lcd_init_cmd_t st7796u_init_cmds[] = {
    {0x11, (uint8_t[]){0x00}, 0, 120},  // SLPOUT - Sleep out, delay 120ms
    {0x3A, (uint8_t[]){0x55}, 1, 0},   // COLMOD - RGB565 (0x55) ❌ CONFLICT với internal (0x05)
    {0xF0, (uint8_t[]){0xC3}, 1, 0},   // Command Set Control 1
    {0xF0, (uint8_t[]){0x96}, 1, 0},   // Command Set Control 2
    {0xB4, (uint8_t[]){0x01}, 1, 0},   // Display Inversion Control
    {0xB7, (uint8_t[]){0xC6}, 1, 0},   // Gate Control
    {0xC0, (uint8_t[]){0x80, 0x45}, 2, 0},  // Power Control 1
    {0xC1, (uint8_t[]){0x13}, 1, 0},   // Power Control 2
    {0xC2, (uint8_t[]){0xA7}, 1, 0},   // Power Control 3
    {0xC5, (uint8_t[]){0x0A}, 1, 0},   // VCOM Control
    {0xE8, (uint8_t[]){0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33}, 8, 0},  // Power Control 4
    {0xE0, (uint8_t[]){0xD0, 0x08, 0x0F, 0x06, 0x06, 0x33, 0x30, 0x33, 0x47, 0x17, 0x13, 0x13, 0x2B, 0x31}, 14, 0},  // Positive Gamma
    {0xE1, (uint8_t[]){0xD0, 0x0A, 0x11, 0x0B, 0x09, 0x07, 0x2F, 0x33, 0x47, 0x38, 0x15, 0x16, 0x2C, 0x32}, 14, 0},  // Negative Gamma
    {0xF0, (uint8_t[]){0x3C}, 1, 0},   // Command Set Control 3
    {0xF0, (uint8_t[]){0x69}, 1, 120}, // Command Set Control 4, delay 120ms
    {0x36, (uint8_t[]){0x48}, 1, 0},   // MADCTL - BGR=ON (0x48) ❌ CONFLICT với internal
    // {0x21, (uint8_t[]){0x00}, 0, 0},   // INVON - Display inversion on (DISABLED) ✅
    {0x29, (uint8_t[]){0x00}, 0, 20},  // DISPON - Display on, delay 20ms
};
```

**KẾT LUẬN:** 
- ✅ **KHÔNG có command 0x21 (INVON)** trong external init sequence (đã bị comment out)
- ⚠️ **Có command 0x36 (MADCTL)** ghi đè internal MADCTL
- ⚠️ **Có command 0x3A (COLMOD)** với giá trị 0x55 (khác với internal 0x05)

### 3️⃣ Post-Init Commands

**File:** `components/sx_platform/sx_platform.c`

**Function:** `sx_platform_display_init()` (line 57-156)

**Thứ tự thực thi SAU `esp_lcd_panel_init()`:**

```c
// Line 117: Panel init (gọi internal + external init_cmds)
ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

// Line 122: INVOFF (0x20) - Disable inversion
ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_handle, 0x20, NULL, 0)); // ST7796U INVOFF
ESP_LOGI(TAG, "INVOFF command sent (display inversion disabled)");

// Line 129: MADCTL (0x36) - Set to 0x48 (MX | BGR)
ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_handle, 0x36, &madctl, 1));
ESP_LOGI(TAG, "MADCTL set to 0x48 (MX | BGR)");

// Line 139: Positive Gamma (0xE0)
ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_handle, 0xE0, gamma_p, sizeof(gamma_p)));

// Line 148: Negative Gamma (0xE1)
ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_handle, 0xE1, gamma_n, sizeof(gamma_n)));

// Line 152: Display ON
ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
```

**KẾT LUẬN:**
- ✅ **INVOFF (0x20) được gửi SAU panel init**
- ✅ **KHÔNG có INVON (0x21) được gửi sau init**
- ⚠️ **MADCTL (0x36) được gửi 2 lần:** một lần trong init_cmds, một lần sau init

### 4️⃣ Default Init Sequence (Driver Internal)

**File:** `managed_components/espressif__esp_lcd_st7796/esp_lcd_st7796_general.c`

**Array:** `vendor_specific_init_default[]` (line 191-205)

**Commands (chỉ dùng nếu KHÔNG có external init_cmds):**

```c
static const st7796_lcd_init_cmd_t vendor_specific_init_default[] = {
    {0xf0, (uint8_t []){0xc3}, 1, 0},
    {0xf0, (uint8_t []){0x96}, 1, 0},
    {0xb4, (uint8_t []){0x01}, 1, 0},
    {0xb7, (uint8_t []){0xc6}, 1, 0},
    {0xe8, (uint8_t []){0x40, 0x8a, 0x00, 0x00, 0x29, 0x19, 0xa5, 0x33}, 8, 0},
    {0xc1, (uint8_t []){0x06}, 1, 0},
    {0xc2, (uint8_t []){0xa7}, 1, 0},
    {0xc5, (uint8_t []){0x18}, 1, 0},
    {0xe0, (uint8_t []){0xf0, 0x09, 0x0b, 0x06, 0x04, 0x15, 0x2f, 0x54, 0x42, 0x3c, 0x17, 0x14, 0x18, 0x1b}, 14, 0},
    {0xe1, (uint8_t []){0xf0, 0x09, 0x0b, 0x06, 0x04, 0x03, 0x2d, 0x43, 0x42, 0x3b, 0x16, 0x14, 0x17, 0x1b}, 14, 0},
    {0xf0, (uint8_t []){0x3c}, 1, 0},
    {0xf0, (uint8_t []){0x69}, 1, 0},
};
```

**KẾT LUẬN:**
- ✅ **KHÔNG có command 0x21 (INVON)** trong default init sequence
- ✅ **KHÔNG có command 0x36 (MADCTL)** trong default init sequence (được gửi riêng bởi driver internal)

### 5️⃣ MIPI Driver (KHÔNG DÙNG)

**File:** `managed_components/espressif__esp_lcd_st7796/esp_lcd_st7796_mipi.c`

**Array:** `vendor_specific_init_default[]` (line 162-189)

**Commands:**

```c
static const st7796_lcd_init_cmd_t vendor_specific_init_default[] = {
    // ... other commands ...
    {0x21, (uint8_t []){0x00}, 0, 0},  // ❌ INVON - Display inversion on
    // ... other commands ...
};
```

**KẾT LUẬN:**
- ⚠️ **MIPI driver CÓ command 0x21 (INVON)** trong default init sequence
- ✅ **NHƯNG dự án này DÙNG `general.c`, KHÔNG dùng `mipi.c`**

### 6️⃣ Runtime Inversion Calls

**Tìm kiếm:** `esp_lcd_panel_invert_color`

**Kết quả:**
- ✅ **KHÔNG có code nào gọi `esp_lcd_panel_invert_color(panel, true)`** trong dự án chính
- ⚠️ Chỉ có 1 nơi trong example code (`i2c_oled_example_main.c`), không liên quan

## 🔍 CHUỖI INIT CUỐI CÙNG

### Thứ tự thực thi:

```
1. esp_lcd_panel_reset(panel_handle)
   └─ Hardware reset hoặc SWRESET (0x01)

2. esp_lcd_panel_init(panel_handle)
   ├─ SLPOUT (0x11) - Exit sleep mode
   ├─ MADCTL (0x36) - Internal: 0x48 (BGR)
   ├─ COLMOD (0x3A) - Internal: 0x05 (RGB565)
   └─ External init_cmds[] (từ sx_platform.c):
      ├─ SLPOUT (0x11) - Duplicate
      ├─ COLMOD (0x3A) - 0x55 (ghi đè internal 0x05) ⚠️
      ├─ ... other commands ...
      └─ MADCTL (0x36) - 0x48 (ghi đè internal) ⚠️

3. INVOFF (0x20) - Post-init ✅
   └─ esp_lcd_panel_io_tx_param(io_handle, 0x20, NULL, 0)

4. MADCTL (0x36) - Post-init: 0x48 ⚠️ (duplicate)
   └─ esp_lcd_panel_io_tx_param(io_handle, 0x36, &madctl, 1)

5. Gamma curves (0xE0, 0xE1) - Post-init

6. DISPON (0x29) - Display on
   └─ esp_lcd_panel_disp_on_off(panel_handle, true)
```

## 🎯 KẾT LUẬN

### ✅ XÁC NHẬN:

1. **KHÔNG có command 0x21 (INVON)** trong bất kỳ init sequence nào
2. **KHÔNG có code nào gọi `esp_lcd_panel_invert_color(panel, true)`** sau init
3. **INVOFF (0x20) được gửi SAU panel init**

### ⚠️ VẤN ĐỀ TIỀM ẨN:

1. **COLMOD conflict:** 
   - Internal: 0x05 (RGB565)
   - External: 0x55 (??? - không phải RGB565 standard)
   - **0x55 có thể là RGB888 hoặc format khác**

2. **MADCTL duplicate:**
   - Gửi 2 lần: trong init_cmds và post-init
   - Cả 2 lần đều là 0x48 (MX | BGR)

3. **Color space mismatch:**
   - `panel_config.rgb_ele_order = ESP_LCD_COLOR_SPACE_BGR`
   - LVGL `swap_bytes = 1`
   - Có thể gây double-swap hoặc color order confusion

### 🔧 NGUYÊN NHÂN GỐC (ROOT CAUSE):

**Vấn đề KHÔNG phải do INVON được bật, mà có thể do:**

1. **COLMOD value sai:** 0x55 không phải RGB565 standard (nên là 0x05)
2. **Color space configuration conflict:** BGR + swap_bytes có thể gây double-swap
3. **MADCTL configuration:** 0x48 (MX | BGR) có thể không phù hợp với panel orientation

### 📝 FILE + DÒNG CODE CỤ THỂ:

1. **External init_cmds:** `components/sx_platform/sx_platform.c:36-55`
   - Line 38: `{0x3A, (uint8_t[]){0x55}, 1, 0}` - ⚠️ COLMOD = 0x55 (sai?)
   - Line 52: `{0x36, (uint8_t[]){0x48}, 1, 0}` - MADCTL = 0x48

2. **Post-init INVOFF:** `components/sx_platform/sx_platform.c:122`
   - Line 122: `esp_lcd_panel_io_tx_param(io_handle, 0x20, NULL, 0)` - ✅ INVOFF

3. **Post-init MADCTL:** `components/sx_platform/sx_platform.c:129`
   - Line 129: `esp_lcd_panel_io_tx_param(io_handle, 0x36, &madctl, 1)` - ⚠️ Duplicate

4. **LVGL swap_bytes:** `components/sx_ui/sx_ui_task.c:81`
   - Line 81: `.swap_bytes = 1` - ⚠️ Có thể conflict với BGR

## 🚫 KẾT LUẬN CHỐT HẠ

❌ **KHÔNG phải lỗi INVON** - Không có command 0x21 được gửi

❌ **KHÔNG phải lỗi ảnh** - Bootscreen image format đúng

❌ **KHÔNG phải lỗi LVGL** - LVGL config đúng

❌ **KHÔNG phải gamma** - Gamma curves đã được apply

✅ **Là do COLMOD value sai (0x55 thay vì 0x05)** hoặc **color space configuration conflict (BGR + swap_bytes)**
























