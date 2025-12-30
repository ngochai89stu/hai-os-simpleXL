# So sánh Menuconfig: Repo chính vs Repo mẫu (Xiaozhi Assistant)

**Ngày tạo:** 2024-12-19  
**Mục đích:** So sánh các menu items trong menuconfig giữa repo chính và repo mẫu

---

## 📋 TỔNG QUAN

### Repo chính (hai-os-simplexl):
- **Kconfig files:**
  - `components/sx_ui/Kconfig.projbuild` - SimpleXL UI Configuration
  - `components/sx_services/Kconfig.projbuild` - SimpleXL Audio Features

### Repo mẫu (xiaozhi-esp32_vietnam_ref):
- **Kconfig files:**
  - `main/Kconfig.projbuild` - Xiaozhi Assistant Configuration

---

## 🔍 PHÂN TÍCH CHI TIẾT

### 1. REPO CHÍNH - SimpleXL UI Configuration

**File:** `components/sx_ui/Kconfig.projbuild`

```kconfig
menu "SimpleXL UI Configuration"

    config UI_USE_LVGL_MUSIC_DEMO
        bool "Use LVGL Music Demo for Music Player Screen"
        default n
        depends on LV_USE_DEMO_MUSIC
        help
            When enabled, the Music Player screen will use the official LVGL Music Demo
            (lv_demo_music) instead of the custom UI implementation.
            
            This requires LV_USE_DEMO_MUSIC to be enabled in LVGL configuration.

endmenu
```

**Menu items:**
- ✅ `UI_USE_LVGL_MUSIC_DEMO` - Use LVGL Music Demo for Music Player Screen

---

### 2. REPO CHÍNH - SimpleXL Audio Features

**File:** `components/sx_services/Kconfig.projbuild`

```kconfig
menu "SimpleXL Audio Features"

    config SX_CODEC_OPUS_ENABLE
        bool "Enable Opus codec support"
        default n
        help
            Enable Opus encoder/decoder using esp-opus-encoder library.
            Requires: 78/esp-opus-encoder component

    config SX_AUDIO_AFE_ESP_SR_ENABLE
        bool "Enable ESP-SR AFE support"
        default n
        help
            Enable Audio Front-End (AFE) using ESP-SR library.
            Provides: AEC (Acoustic Echo Cancellation), VAD (Voice Activity Detection),
            NS (Noise Suppression), AGC (Automatic Gain Control).
            Requires: espressif/esp-sr component and model files

    config SX_WAKE_WORD_ESP_SR_ENABLE
        bool "Enable ESP-SR Wake Word detection"
        default n
        depends on SX_AUDIO_AFE_ESP_SR_ENABLE
        help
            Enable wake word detection using ESP-SR wakenet models.
            Requires: ESP-SR wakenet model files

endmenu
```

**Menu items:**
- ✅ `SX_CODEC_OPUS_ENABLE` - Enable Opus codec support
- ✅ `SX_AUDIO_AFE_ESP_SR_ENABLE` - Enable ESP-SR AFE support
- ✅ `SX_WAKE_WORD_ESP_SR_ENABLE` - Enable ESP-SR Wake Word detection

---

### 3. REPO MẪU - Xiaozhi Assistant Configuration

**File:** `main/Kconfig.projbuild`

**Menu:** "Xiaozhi Assistant"

**Menu items:**
1. ✅ `OTA_URL` - Default OTA URL (string)
2. ✅ `Flash Assets` (choice) - FLASH_NONE_ASSETS, FLASH_DEFAULT_ASSETS, FLASH_CUSTOM_ASSETS
3. ✅ `CUSTOM_ASSETS_FILE` - Custom Assets File (string, depends on FLASH_CUSTOM_ASSETS)
4. ✅ `Default Language` (choice) - 30+ languages (ZH_CN, EN_US, VI_VN, etc.)
5. ✅ `Board Type` (choice) - 80+ board types (ESP-BOX-3, M5Stack, Waveshare, etc.)
6. ✅ `Screen type` (choice) - HI8561, RM69A10 (depends on LILYGO_T_DISPLAY_P4)
7. ✅ `Screen color format` (choice) - RGB565, RGB888 (depends on LILYGO_T_DISPLAY_P4)
8. ✅ `EV_BOARD Type` (choice) - V1.4, V1.5 (depends on ESP_S3_LCD_EV_Board)
9. ✅ `OLED Type` (choice) - SSD1306 128x32, SSD1306 128x64, SH1106 128x64
10. ✅ `LCD Type` (choice) - 20+ LCD types (ST7789, ILI9341, ST7796, etc.)
11. ✅ `TOUCH_PANEL_ENABLE` - Enable Touch Panel (bool)
12. ✅ `SD_CARD_ENABLE` - Enable SD Card (bool)
13. ✅ `WEATHER_IDLE_DISPLAY_ENABLE` - Enable Weather Feature (bool)
14. ✅ `SD_CARD_INTERFACE_TYPE` (choice) - SDMMC, SPI
15. ✅ `ESP32S3_KORVO2_V3 LCD Type` (choice) - ST7789, ILI9341
16. ✅ `ESP32S3_AUDIO_BOARD LCD Type` (choice) - JD9853, ST7789
17. ✅ `Display Style` (choice) - DEFAULT_MESSAGE_STYLE, WECHAT_MESSAGE_STYLE, EMOTE_MESSAGE_STYLE
18. ✅ `Wake Word Type` (choice) - DISABLED, ESP_WAKE_WORD, AFE_WAKE_WORD, CUSTOM_WAKE_WORD
19. ✅ `CUSTOM_WAKE_WORD` - Custom Wake Word (string)
20. ✅ `CUSTOM_WAKE_WORD_DISPLAY` - Custom Wake Word Display (string)
21. ✅ `CUSTOM_WAKE_WORD_THRESHOLD` - Custom Wake Word Threshold % (int, 1-99)
22. ✅ `SEND_WAKE_WORD_DATA` - Send Wake Word Data (bool)
23. ✅ `USE_AUDIO_PROCESSOR` - Enable Audio Noise Reduction (bool)
24. ✅ `USE_DEVICE_AEC` - Enable Device-Side AEC (bool)
25. ✅ `USE_SERVER_AEC` - Enable Server-Side AEC (bool)
26. ✅ `USE_AUDIO_DEBUGGER` - Enable Audio Debugger (bool)
27. ✅ `AUDIO_DEBUG_UDP_SERVER` - Audio Debug UDP Server Address (string)
28. ✅ `USE_ACOUSTIC_WIFI_PROVISIONING` - Enable Acoustic WiFi Provisioning (bool)
29. ✅ `RECEIVE_CUSTOM_MESSAGE` - Enable Custom Message Reception (bool)
30. ✅ `Camera Configuration` (menu) - Hardware JPEG Encoder, Debug Mode, Rotation, etc.
31. ✅ `TAIJIPAI_S3_CONFIG` (menu) - I2S Type, I2S 2 Slot

---

## 📊 SO SÁNH MENU ITEMS

### Repo chính có:
- ✅ `UI_USE_LVGL_MUSIC_DEMO` - Use LVGL Music Demo
- ✅ `SX_CODEC_OPUS_ENABLE` - Enable Opus codec
- ✅ `SX_AUDIO_AFE_ESP_SR_ENABLE` - Enable ESP-SR AFE
- ✅ `SX_WAKE_WORD_ESP_SR_ENABLE` - Enable ESP-SR Wake Word

### Repo mẫu có (80+ menu items):
- ✅ OTA URL configuration
- ✅ Flash Assets configuration
- ✅ Language selection (30+ languages)
- ✅ Board Type selection (80+ boards)
- ✅ Display configuration (OLED, LCD types)
- ✅ Touch Panel enable
- ✅ SD Card enable & interface type
- ✅ Weather feature enable
- ✅ Display Style selection
- ✅ Wake Word Type selection
- ✅ Custom Wake Word configuration
- ✅ Audio Processor enable
- ✅ Device/Server AEC enable
- ✅ Audio Debugger enable
- ✅ Acoustic WiFi Provisioning
- ✅ Custom Message Reception
- ✅ Camera Configuration
- ✅ Board-specific configs

---

## ❌ MENU ITEMS REPO CHÍNH CHƯA CÓ

### 1. OTA Configuration ❌
- ❌ `OTA_URL` - Default OTA URL (string)
- ❌ `Flash Assets` (choice) - FLASH_NONE_ASSETS, FLASH_DEFAULT_ASSETS, FLASH_CUSTOM_ASSETS
- ❌ `CUSTOM_ASSETS_FILE` - Custom Assets File (string)

### 2. Language Configuration ❌
- ❌ `Default Language` (choice) - 30+ languages:
  - ZH_CN, ZH_TW, EN_US, JA_JP, KO_KR, VI_VN, TH_TH
  - DE_DE, FR_FR, ES_ES, IT_IT, RU_RU, AR_SA, HI_IN
  - PT_PT, PL_PL, CS_CZ, FI_FI, TR_TR, ID_ID, UK_UA
  - RO_RO, BG_BG, CA_ES, DA_DK, EL_GR, FA_IR, FIL_PH
  - HE_IL, HR_HR, HU_HU, MS_MY, NB_NO, NL_NL, SK_SK
  - SL_SI, SV_SE, SR_RS

### 3. Board Configuration ❌
- ❌ `Board Type` (choice) - 80+ board types:
  - ESP-BOX-3, ESP-BOX, ESP-BOX-LITE
  - M5Stack CoreS3, M5Stack Tab5, M5Stack Atom series
  - Waveshare series (S3, C6, P4 boards)
  - LILYGO series (T-Circle-S3, T-Display-P4, etc.)
  - Bread Compact series
  - XINGZHI_CUBE series
  - ATK DNESP32S3 series
  - Và nhiều boards khác...

### 4. Display Configuration ❌
- ❌ `OLED Type` (choice) - SSD1306 128x32, SSD1306 128x64, SH1106 128x64
- ❌ `LCD Type` (choice) - 20+ LCD types:
  - ST7789 (240x320, 170x320, 240x280, 240x240, etc.)
  - ST7796 (320x480)
  - ILI9341 (240x320)
  - ST7735 (128x160, 128x128)
  - GC9A01 (240x240 Circle)
  - LCD_CUSTOM
- ❌ `Screen type` (choice) - HI8561, RM69A10 (for LILYGO_T_DISPLAY_P4)
- ❌ `Screen color format` (choice) - RGB565, RGB888
- ❌ `EV_BOARD Type` (choice) - V1.4, V1.5
- ❌ `ESP32S3_KORVO2_V3 LCD Type` (choice)
- ❌ `ESP32S3_AUDIO_BOARD LCD Type` (choice)
- ❌ `Display Style` (choice) - DEFAULT_MESSAGE_STYLE, WECHAT_MESSAGE_STYLE, EMOTE_MESSAGE_STYLE

### 5. Hardware Configuration ❌
- ❌ `TOUCH_PANEL_ENABLE` - Enable Touch Panel (bool)
- ❌ `SD_CARD_ENABLE` - Enable SD Card (bool)
- ❌ `SD_CARD_INTERFACE_TYPE` (choice) - SDMMC, SPI

### 6. Feature Toggles ❌
- ❌ `WEATHER_IDLE_DISPLAY_ENABLE` - Enable Weather Feature (bool)

### 7. Wake Word Configuration ❌
- ❌ `Wake Word Type` (choice):
  - WAKE_WORD_DISABLED
  - USE_ESP_WAKE_WORD (Wakenet without AFE)
  - USE_AFE_WAKE_WORD (Wakenet with AFE)
  - USE_CUSTOM_WAKE_WORD (Multinet model)
- ❌ `CUSTOM_WAKE_WORD` - Custom Wake Word (string)
- ❌ `CUSTOM_WAKE_WORD_DISPLAY` - Custom Wake Word Display (string)
- ❌ `CUSTOM_WAKE_WORD_THRESHOLD` - Custom Wake Word Threshold % (int, 1-99)
- ❌ `SEND_WAKE_WORD_DATA` - Send Wake Word Data (bool)

### 8. Audio Processing Configuration ❌
- ❌ `USE_AUDIO_PROCESSOR` - Enable Audio Noise Reduction (bool)
- ❌ `USE_DEVICE_AEC` - Enable Device-Side AEC (bool)
- ❌ `USE_SERVER_AEC` - Enable Server-Side AEC (bool)
- ❌ `USE_AUDIO_DEBUGGER` - Enable Audio Debugger (bool)
- ❌ `AUDIO_DEBUG_UDP_SERVER` - Audio Debug UDP Server Address (string)

### 9. Network Configuration ❌
- ❌ `USE_ACOUSTIC_WIFI_PROVISIONING` - Enable Acoustic WiFi Provisioning (bool)
- ❌ `RECEIVE_CUSTOM_MESSAGE` - Enable Custom Message Reception (bool)

### 10. Camera Configuration ❌
- ❌ `XIAOZHI_ENABLE_HARDWARE_JPEG_ENCODER` - Enable Hardware JPEG Encoder (bool)
- ❌ `XIAOZHI_ENABLE_CAMERA_DEBUG_MODE` - Enable Camera Debug Mode (bool)
- ❌ `XIAOZHI_ENABLE_CAMERA_ENDIANNESS_SWAP` - Enable camera buffer endianness swapping (bool)
- ❌ `XIAOZHI_ENABLE_ROTATE_CAMERA_IMAGE` - Enable Camera Image Rotation (bool)
- ❌ `XIAOZHI_CAMERA_IMAGE_ROTATION_ANGLE` (choice) - 90°, 270°

### 11. Board-Specific Configuration ❌
- ❌ `TAIJIPAI_S3_CONFIG` (menu):
  - `I2S_TYPE_TAIJIPI_S3` (choice) - STD, PDM
  - `I2S_USE_2SLOT` - Enable I2S 2 Slot (bool)

---

## 📊 TỔNG KẾT SO SÁNH

| Category | Repo mẫu | Repo chính | Thiếu |
|----------|----------|------------|-------|
| **OTA Configuration** | ✅ 3 items | ❌ 0 | 3 |
| **Language** | ✅ 30+ languages | ❌ 0 | 30+ |
| **Board Type** | ✅ 80+ boards | ❌ 0 | 80+ |
| **Display** | ✅ 20+ LCD types, 3 OLED types | ❌ 0 | 23+ |
| **Hardware** | ✅ Touch, SD Card | ❌ 0 | 2 |
| **Features** | ✅ Weather | ❌ 0 | 1 |
| **Wake Word** | ✅ 4 types + custom config | ✅ 1 (ESP-SR only) | 3+ |
| **Audio Processing** | ✅ 4 items | ✅ 2 (AFE, Opus) | 2 |
| **Network** | ✅ 2 items | ❌ 0 | 2 |
| **Camera** | ✅ 5 items | ❌ 0 | 5 |
| **Board-Specific** | ✅ TAIJIPAI_S3_CONFIG | ❌ 0 | 1+ |
| **UI** | ❌ 0 | ✅ 1 (LVGL Music Demo) | - |

**Tổng cộng:** Repo mẫu có **~150+ menu items**, repo chính có **4 menu items**

---

## ✅ KHUYẾN NGHỊ

### Priority 1: Các menu items quan trọng nhất

1. **OTA Configuration** ⭐⭐⭐
   - `OTA_URL` - Cần cho OTA updates
   - `Flash Assets` - Cần cho asset management

2. **Language Configuration** ⭐⭐⭐
   - `Default Language` - Cần cho đa ngôn ngữ
   - Ít nhất: VI_VN, EN_US, ZH_CN

3. **Board Configuration** ⭐⭐⭐
   - `Board Type` - Cần cho multi-board support
   - Ít nhất: Custom board option

4. **Display Configuration** ⭐⭐
   - `LCD Type` / `OLED Type` - Cần cho multi-display support
   - Ít nhất: ST7796 320x480 (hiện tại đang dùng)

5. **Wake Word Configuration** ⭐⭐
   - `Wake Word Type` - Mở rộng từ ESP-SR only
   - `CUSTOM_WAKE_WORD` - Custom wake word support

### Priority 2: Các menu items hữu ích

6. **Hardware Configuration** ⭐
   - `SD_CARD_ENABLE` - Enable/disable SD Card
   - `TOUCH_PANEL_ENABLE` - Enable/disable Touch Panel

7. **Feature Toggles** ⭐
   - `WEATHER_IDLE_DISPLAY_ENABLE` - Enable Weather Feature

8. **Audio Processing** ⭐
   - `USE_AUDIO_PROCESSOR` - Enable Audio Noise Reduction
   - `USE_DEVICE_AEC` - Enable Device-Side AEC

### Priority 3: Các menu items tùy chọn

9. **Network Configuration**
   - `USE_ACOUSTIC_WIFI_PROVISIONING` - Acoustic WiFi Provisioning
   - `RECEIVE_CUSTOM_MESSAGE` - Custom Message Reception

10. **Camera Configuration** (nếu có camera)
    - Hardware JPEG Encoder
    - Camera Debug Mode
    - Camera Image Rotation

11. **Board-Specific Configuration**
    - TAIJIPAI_S3_CONFIG (nếu cần)
    - Các board-specific configs khác

---

## 📝 NEXT STEPS

1. ✅ Đọc file `main/Kconfig.projbuild` từ repo mẫu
2. ✅ So sánh chi tiết từng menu item
3. ⏳ Tạo Kconfig.projbuild mới với các menu items thiếu (Priority 1)
4. ⏳ Tích hợp vào repo chính

---

## 🎯 KẾT LUẬN

**Repo chính thiếu rất nhiều menu items so với repo mẫu:**

- ❌ **~150+ menu items** trong repo mẫu
- ✅ **4 menu items** trong repo chính
- 📊 **Thiếu ~146 menu items**

**Các menu items quan trọng nhất cần thêm:**
1. OTA Configuration (3 items)
2. Language Configuration (30+ languages)
3. Board Configuration (80+ boards)
4. Display Configuration (23+ display types)
5. Wake Word Configuration (mở rộng từ ESP-SR only)

**Lưu ý:**
- Repo mẫu có nhiều board-specific configs (không cần copy hết)
- Nên tập trung vào các configs cần thiết cho SimpleXL architecture
- Board-specific configs có thể để sau hoặc không cần nếu không dùng board đó

---

**Cập nhật:** 2024-12-19  
**Trạng thái:** ✅ Phân tích hoàn tất

