# Xác Nhận GPIO từ Board Mẫu `bread-compact-wifi-lcd`

**Ngày:** 2025-01-27  
**Nguồn:** `D:\xiaozhi-esp32_vietnam_ref\main\boards\bread-compact-wifi-lcd\config.h`

---

## ✅ Audio GPIO - Xác Nhận Từ Board Mẫu

### **Microphone Input (I2S Simplex Mode)**

Board mẫu sử dụng **I2S Simplex Mode** - microphone có I2S bus riêng:

| GPIO | Chức năng | File tham khảo | Trạng thái |
|------|-----------|----------------|------------|
| **GPIO 4** | **Mic I2S WS** (Word Select) | `config.h:14` | ✅ **XÁC NHẬN** |
| **GPIO 5** | **Mic I2S SCK** (Serial Clock) | `config.h:15` | ✅ **XÁC NHẬN** |
| **GPIO 6** | **Mic I2S DIN** (Data Input) | `config.h:16` | ✅ **XÁC NHẬN** |

**Code mẫu:**
```c
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6
```

### **Audio Output (PCM5102A)**

| GPIO | Chức năng | File tham khảo | Trạng thái |
|------|-----------|----------------|------------|
| **GPIO 7** | **PCM5102A DOUT** (Data Output) | `config.h:17` | ✅ **XÁC NHẬN** |
| **GPIO 15** | **PCM5102A BCLK** (Bit Clock) | `config.h:18` | ✅ **XÁC NHẬN** |
| **GPIO 16** | **PCM5102A LRCK** (Word Select) | `config.h:19` | ✅ **XÁC NHẬN** |

**Code mẫu:**
```c
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16
```

---

## 📺 LCD GPIO - So Sánh Với Hardware Hiện Tại

### **Board Mẫu `bread-compact-wifi-lcd`**

| Chức năng | GPIO | File tham khảo |
|-----------|------|----------------|
| Backlight | **GPIO 42** | `config.h:38` |
| MOSI | **GPIO 47** | `config.h:39` |
| CLK | **GPIO 21** | `config.h:40` |
| DC | **GPIO 40** | `config.h:41` |
| RST | **GPIO 45** | `config.h:42` |
| CS | **GPIO 41** | `config.h:43` |

**Code mẫu:**
```c
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_42
#define DISPLAY_MOSI_PIN      GPIO_NUM_47
#define DISPLAY_CLK_PIN       GPIO_NUM_21
#define DISPLAY_DC_PIN        GPIO_NUM_40
#define DISPLAY_RST_PIN       GPIO_NUM_45
#define DISPLAY_CS_PIN        GPIO_NUM_41
```

### **Hardware Hiện Tại (SimpleXL)**

| Chức năng | GPIO | File | So sánh |
|-----------|------|------|---------|
| Backlight | **GPIO 7** | `sx_platform.c:19` | ❌ **KHÁC** - Conflict với Audio DOUT |
| MOSI | **GPIO 13** | `sx_platform.c:13` | ❌ **KHÁC** |
| CLK | **GPIO 14** | `sx_platform.c:14` | ❌ **KHÁC** |
| DC | **GPIO 9** | `sx_platform.c:16` | ❌ **KHÁC** |
| RST | **GPIO 8** | `sx_platform.c:17` | ❌ **KHÁC** |
| CS | **GPIO 10** | `sx_platform.c:15` | ❌ **KHÁC** |

---

## 📊 Tóm Tắt So Sánh

### ✅ **Audio GPIO - KHỚP 100%**

| GPIO | Chức năng | Board mẫu | Hardware hiện tại | Status |
|------|-----------|-----------|-------------------|--------|
| 4 | Mic WS | ✅ | ✅ | ✅ **KHỚP** |
| 5 | Mic SCK | ✅ | ✅ | ✅ **KHỚP** |
| 6 | Mic DIN | ✅ | ✅ | ✅ **KHỚP** |
| 7 | Speaker DOUT | ✅ | ✅ | ✅ **KHỚP** |
| 15 | Speaker BCLK | ✅ | ✅ | ✅ **KHỚP** |
| 16 | Speaker LRCK | ✅ | ✅ | ✅ **KHỚP** |

### ❌ **LCD GPIO - KHÁC HOÀN TOÀN**

| Chức năng | Board mẫu | Hardware hiện tại | Status |
|-----------|-----------|-------------------|--------|
| Backlight | GPIO 42 | GPIO 7 | ❌ **KHÁC** |
| MOSI | GPIO 47 | GPIO 13 | ❌ **KHÁC** |
| CLK | GPIO 21 | GPIO 14 | ❌ **KHÁC** |
| DC | GPIO 40 | GPIO 9 | ❌ **KHÁC** |
| RST | GPIO 45 | GPIO 8 | ❌ **KHÁC** |
| CS | GPIO 41 | GPIO 10 | ❌ **KHÁC** |

---

## 🎯 Kết Luận

1. **Audio GPIO:** ✅ **KHỚP HOÀN TOÀN** với board mẫu
   - Microphone: GPIO 4, 5, 6 (I2S Simplex Mode)
   - Speaker (PCM5102A): GPIO 7, 15, 16

2. **LCD GPIO:** ❌ **KHÁC HOÀN TOÀN** với board mẫu
   - Board mẫu dùng GPIO cao (40-47) → không conflict với audio
   - Hardware hiện tại dùng GPIO thấp (7-14) → có conflict với audio DOUT (GPIO 7)

3. **Vấn đề:**
   - GPIO 7 conflict giữa LCD Backlight và Audio DOUT
   - Board mẫu giải quyết bằng cách dùng GPIO 42 cho LCD Backlight

4. **Đề xuất:**
   - Nếu hardware thực tế giống board mẫu → cập nhật LCD GPIO sang GPIO 40-47
   - Nếu hardware thực tế khác → cần xác nhận lại schematic

---

## 📌 File Tham Khảo

- `D:\xiaozhi-esp32_vietnam_ref\main\boards\bread-compact-wifi-lcd\config.h`
- `D:\xiaozhi-esp32_vietnam_ref\main\boards\bread-compact-wifi-lcd\compact_wifi_board_lcd.cc`


















