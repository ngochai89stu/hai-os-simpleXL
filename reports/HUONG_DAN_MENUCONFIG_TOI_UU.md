# Hướng dẫn Menuconfig Tối Ưu - Tiếng Việt + Full Tính Năng

**Ngày tạo:** 2024-12-19  
**Mục đích:** Hướng dẫn cấu hình menuconfig tối ưu cho SimpleXL với tiếng Việt và đầy đủ tính năng

---

## 🎯 CẤU HÌNH TỐI ƯU

### Mục tiêu:
- ✅ Tiếng Việt làm ngôn ngữ mặc định
- ✅ Full tính năng enabled
- ✅ Tối ưu cho ESP32-S3 với PSRAM
- ✅ Display ST7796 320x480 (IPS)
- ✅ Touch panel enabled
- ✅ SD Card enabled
- ✅ Tất cả services enabled

---

## 📋 HƯỚNG DẪN TỪNG BƯỚC

### Bước 1: Mở menuconfig

```bash
idf.py menuconfig
```

### Bước 2: Navigate đến SimpleXL Configuration

```
Component config
  └─> SimpleXL Core Configuration
```

---

## ⚙️ CẤU HÌNH CHI TIẾT

### 1. OTA Configuration

```
SimpleXL Core Configuration
  └─> OTA Configuration
      └─> Default OTA URL: "https://api.example.com/ota/"
```

**Cấu hình:**
- ✅ `SX_OTA_DEFAULT_URL`: Đặt URL OTA server của bạn

---

### 2. Language Configuration ⭐ (QUAN TRỌNG - TIẾNG VIỆT)

```
SimpleXL Core Configuration
  └─> Language Configuration
      └─> Default Language: (X) Vietnamese (Tiếng Việt)
```

**Cấu hình:**
- ✅ `SX_DEFAULT_LANGUAGE`: Chọn **Vietnamese (Tiếng Việt)**

**Các options khác:**
- ( ) English
- ( ) Chinese (Simplified) - 简体中文
- ( ) Chinese (Traditional) - 繁體中文
- ( ) Japanese - 日本語
- ( ) Korean - 한국어

---

### 3. Display Configuration

```
SimpleXL Core Configuration
  └─> Display Configuration
      ├─> Display Type: (X) LCD ST7796 320x480 (IPS)
      ├─> Enable Touch Panel: (X) y
      └─> Default Brightness (%): 80
```

**Cấu hình:**
- ✅ `SX_DISPLAY_TYPE`: **LCD ST7796 320x480 (IPS)**
- ✅ `SX_DISPLAY_TOUCH_ENABLE`: **y** (enabled)
- ✅ `SX_DISPLAY_BRIGHTNESS_DEFAULT`: **80** (80%)

**Các display types khác:**
- ( ) LCD ST7789 240x320 (IPS)
- ( ) LCD ILI9341 240x320
- ( ) OLED SSD1306 128x64
- ( ) Custom Display

---

### 4. Storage Configuration

```
SimpleXL Core Configuration
  └─> Storage Configuration
      ├─> Enable SD Card: (X) y
      └─> SD Card Interface Type: (X) SPI Interface
```

**Cấu hình:**
- ✅ `SX_SD_CARD_ENABLE`: **y** (enabled)
- ✅ `SX_SD_CARD_INTERFACE`: **SPI Interface**

**Interface options:**
- (X) SPI Interface
- ( ) SDMMC Interface

---

### 5. Service Configuration

#### 5.1 STT Configuration

```
SimpleXL Core Configuration
  └─> Service Configuration
      └─> STT (Speech-to-Text) Configuration
          ├─> STT Endpoint URL: "https://api.example.com/stt"
          └─> STT API Key: "your-api-key-here"
```

**Cấu hình:**
- ✅ `SX_STT_ENDPOINT_URL`: Đặt URL STT endpoint
- ✅ `SX_STT_API_KEY`: Đặt STT API key

#### 5.2 TTS Configuration

```
SimpleXL Core Configuration
  └─> Service Configuration
      └─> TTS (Text-to-Speech) Configuration
          ├─> TTS Endpoint URL: "https://api.example.com/tts"
          └─> TTS API Key: "your-api-key-here"
```

**Cấu hình:**
- ✅ `SX_TTS_ENDPOINT_URL`: Đặt URL TTS endpoint
- ✅ `SX_TTS_API_KEY`: Đặt TTS API key

#### 5.3 Telegram Configuration (Optional)

```
SimpleXL Core Configuration
  └─> Service Configuration
      └─> Telegram Configuration
          ├─> Telegram Bot Token: "your-bot-token"
          └─> Telegram Chat ID: "your-chat-id"
```

**Cấu hình:**
- ⚠️ `SX_TELEGRAM_BOT_TOKEN`: Đặt Telegram bot token (nếu dùng)
- ⚠️ `SX_TELEGRAM_CHAT_ID`: Đặt Telegram chat ID (nếu dùng)

#### 5.4 Online Music Configuration (Optional)

```
SimpleXL Core Configuration
  └─> Service Configuration
      └─> Online Music Configuration
          ├─> Online Music Auth MAC: "your-mac-address"
          └─> Online Music Auth Secret: "your-secret-key"
```

**Cấu hình:**
- ⚠️ `SX_MUSIC_ONLINE_AUTH_MAC`: Đặt MAC address (nếu dùng)
- ⚠️ `SX_MUSIC_ONLINE_AUTH_SECRET`: Đặt secret key (nếu dùng)

---

### 6. Wake Word Configuration ⭐ (QUAN TRỌNG)

```
SimpleXL Core Configuration
  └─> Wake Word Configuration
      ├─> Wake Word Implementation Type: (X) ESP-SR Wakenet (with AFE)
      ├─> Wake Word Model Path: "/spiffs/wakenet_model.bin"
      └─> Wake Word Threshold (%): 50
```

**Cấu hình:**
- ✅ `SX_WAKE_WORD_TYPE`: **ESP-SR Wakenet (with AFE)**
- ✅ `SX_WAKE_WORD_MODEL_PATH`: Đường dẫn đến model file
- ✅ `SX_WAKE_WORD_THRESHOLD`: **50** (50%)

**Wake Word Type options:**
- ( ) Disabled
- (X) ESP-SR Wakenet (with AFE) ⭐ **RECOMMENDED**
- ( ) Custom Wake Word (Multinet)

**Lưu ý:** ESP-SR Wakenet yêu cầu:
- ESP32-S3 hoặc ESP32-P4
- PSRAM enabled
- `SX_AUDIO_AFE_ESP_SR_ENABLE` enabled (xem phần Audio Features)

---

### 7. Audio Configuration

```
SimpleXL Core Configuration
  └─> Audio Configuration
      ├─> Audio Sample Rate (Hz): 16000
      ├─> Audio Buffer Size (ms): 100
      ├─> Enable Hardware Volume Control: (X) y
      ├─> Audio Codec Type: (X) Auto-detect (ES8388/ES8311)
      ├─> Enable Audio Power Save: (X) y
      └─> Audio Power Save Timeout (ms): 15000
```

**Cấu hình:**
- ✅ `SX_AUDIO_SAMPLE_RATE`: **16000** Hz
- ✅ `SX_AUDIO_BUFFER_SIZE_MS`: **100** ms
- ✅ `SX_AUDIO_HARDWARE_VOLUME_ENABLE`: **y** (enabled)
- ✅ `SX_AUDIO_CODEC_TYPE`: **Auto-detect (ES8388/ES8311)**
- ✅ `SX_AUDIO_POWER_SAVE_ENABLE`: **y** (enabled)
- ✅ `SX_AUDIO_POWER_SAVE_TIMEOUT_MS`: **15000** ms (15 giây)

**Codec Type options:**
- (X) Auto-detect (ES8388/ES8311) ⭐ **RECOMMENDED**
- ( ) ES8388
- ( ) ES8311

---

### 8. Feature Configuration ⭐ (FULL TÍNH NĂNG)

```
SimpleXL Core Configuration
  └─> Feature Configuration
      ├─> Enable Music Service: (X) y
      ├─> Enable Radio Service: (X) y
      ├─> Enable Weather Service: (X) y
      ├─> Enable Navigation Service: (X) y
      ├─> Enable Telegram Service: (X) y
      ├─> Enable Bluetooth Service: (X) y
      └─> Enable MCP Server: (X) y
```

**Cấu hình (TẤT CẢ ENABLED):**
- ✅ `SX_FEATURE_MUSIC_ENABLE`: **y**
- ✅ `SX_FEATURE_RADIO_ENABLE`: **y**
- ✅ `SX_FEATURE_WEATHER_ENABLE`: **y**
- ✅ `SX_FEATURE_NAVIGATION_ENABLE`: **y**
- ✅ `SX_FEATURE_TELEGRAM_ENABLE`: **y**
- ✅ `SX_FEATURE_BLUETOOTH_ENABLE`: **y**
- ✅ `SX_FEATURE_MCP_SERVER_ENABLE`: **y**

---

### 9. Network Configuration

```
SimpleXL Core Configuration
  └─> Network Configuration
      ├─> Enable Auto Reconnect: (X) y
      ├─> Reconnect Interval (ms): 5000
      ├─> Enable MQTT Protocol: (X) y
      └─> Enable WebSocket Protocol: (X) y
```

**Cấu hình:**
- ✅ `SX_NETWORK_AUTO_RECONNECT`: **y** (enabled)
- ✅ `SX_NETWORK_RECONNECT_INTERVAL_MS`: **5000** ms (5 giây)
- ✅ `SX_MQTT_ENABLE`: **y** (enabled)
- ✅ `SX_WEBSOCKET_ENABLE`: **y** (enabled)

---

### 10. SimpleXL UI Configuration

```
Component config
  └─> SimpleXL UI Configuration
      ├─> Use LVGL Music Demo for Music Player Screen: ( ) n
      ├─> Default Theme: "dark"
      └─> Enable UI Animations: (X) y
```

**Cấu hình:**
- ⚠️ `UI_USE_LVGL_MUSIC_DEMO`: **n** (disabled - dùng custom UI)
- ✅ `SX_UI_THEME_DEFAULT`: **"dark"**
- ✅ `SX_UI_ANIMATION_ENABLE`: **y** (enabled)

**Lưu ý:** Nếu muốn dùng LVGL Music Demo, cần enable:
- `LV_USE_DEMO_MUSIC` trong LVGL configuration
- `UI_USE_LVGL_MUSIC_DEMO`: **y**

---

### 11. SimpleXL Audio Features ⭐ (QUAN TRỌNG)

```
Component config
  └─> SimpleXL Audio Features
      ├─> Enable Opus codec support: (X) y
      ├─> Enable ESP-SR AFE support: (X) y
      └─> Enable ESP-SR Wake Word detection: (X) y
```

**Cấu hình (FULL AUDIO FEATURES):**
- ✅ `SX_CODEC_OPUS_ENABLE`: **y** (enabled)
- ✅ `SX_AUDIO_AFE_ESP_SR_ENABLE`: **y** (enabled) ⭐ **REQUIRED for Wake Word**
- ✅ `SX_WAKE_WORD_ESP_SR_ENABLE`: **y** (enabled) ⭐ **REQUIRED for Wake Word**

**Lưu ý:** ESP-SR AFE và Wake Word yêu cầu:
- ESP32-S3 hoặc ESP32-P4
- PSRAM enabled
- Model files trong SPIFFS/partition

---

## 🔧 CẤU HÌNH ESP-IDF BỔ SUNG

### 1. PSRAM Configuration (REQUIRED)

```
Component config
  └─> ESP32S3-Specific
      └─> Support for external, SPI-connected RAM
          └─> Support for external PSRAM: (X) y
```

**Cấu hình:**
- ✅ `SPIRAM`: **y** (enabled) ⭐ **REQUIRED**

---

### 2. Partition Table

```
Partition Table
  └─> Partition Table: (X) Custom partition table CSV
      └─> Custom partition CSV file: "partitions.csv"
```

**Đảm bảo partition table có:**
- `spiffs` partition cho model files
- Đủ flash space cho firmware

---

### 3. LVGL Configuration (nếu dùng LVGL Music Demo)

```
Component config
  └─> LVGL configuration
      └─> Demos
          └─> LV_USE_DEMO_MUSIC: (X) y
```

**Cấu hình:**
- ⚠️ `LV_USE_DEMO_MUSIC`: **y** (chỉ nếu dùng LVGL Music Demo)

---

## 📝 TÓM TẮT CẤU HÌNH TỐI ƯU

### Quick Reference - Tất cả settings cần bật:

#### SimpleXL Core Configuration:
- ✅ **Language**: Vietnamese (Tiếng Việt)
- ✅ **Display**: ST7796 320x480, Touch enabled, Brightness 80%
- ✅ **Storage**: SD Card enabled, SPI interface
- ✅ **Wake Word**: ESP-SR Wakenet, Threshold 50%
- ✅ **Audio**: Sample rate 16000, Hardware volume, Power save enabled
- ✅ **Features**: TẤT CẢ enabled (Music, Radio, Weather, Navigation, Telegram, Bluetooth, MCP)
- ✅ **Network**: Auto reconnect, MQTT, WebSocket enabled

#### SimpleXL Audio Features:
- ✅ **Opus**: Enabled
- ✅ **ESP-SR AFE**: Enabled ⭐
- ✅ **ESP-SR Wake Word**: Enabled ⭐

#### SimpleXL UI Configuration:
- ✅ **Theme**: dark
- ✅ **Animations**: Enabled

#### ESP-IDF Configuration:
- ✅ **PSRAM**: Enabled ⭐

---

## 🎯 CHECKLIST TRƯỚC KHI BUILD

### Bắt buộc:
- [ ] PSRAM enabled
- [ ] Language = Vietnamese
- [ ] ESP-SR AFE enabled
- [ ] ESP-SR Wake Word enabled
- [ ] Display type = ST7796 320x480
- [ ] Touch panel enabled
- [ ] SD Card enabled

### Khuyến nghị (Full tính năng):
- [ ] Tất cả Features enabled
- [ ] Opus codec enabled
- [ ] Hardware volume enabled
- [ ] Audio power save enabled
- [ ] Network auto reconnect enabled
- [ ] MQTT enabled
- [ ] WebSocket enabled
- [ ] UI animations enabled

### Tùy chọn:
- [ ] STT/TTS endpoints configured
- [ ] Telegram bot token configured
- [ ] Online Music auth configured
- [ ] OTA URL configured

---

## 🚀 BUILD VÀ FLASH

Sau khi cấu hình xong:

```bash
# 1. Save và exit menuconfig
# (Nhấn S để save, Q để quit)

# 2. Build
idf.py build

# 3. Flash
idf.py -p COM23 flash monitor
```

---

## ⚠️ LƯU Ý QUAN TRỌNG

1. **PSRAM bắt buộc** cho ESP-SR AFE và Wake Word
2. **Model files** cần được flash vào SPIFFS partition
3. **STT/TTS endpoints** có thể để trống và config sau qua Settings
4. **Telegram/Online Music** có thể để trống nếu không dùng
5. **OTA URL** cần đặt đúng URL server của bạn

---

## 📊 CẤU HÌNH MẪU (sdkconfig.defaults)

Nếu muốn tạo file `sdkconfig.defaults` để tự động apply:

```ini
# SimpleXL Core Configuration
CONFIG_SX_DEFAULT_LANGUAGE_VI_VN=y
CONFIG_SX_DISPLAY_LCD_ST7796_320X480=y
CONFIG_SX_DISPLAY_TOUCH_ENABLE=y
CONFIG_SX_DISPLAY_BRIGHTNESS_DEFAULT=80
CONFIG_SX_SD_CARD_ENABLE=y
CONFIG_SX_SD_CARD_SPI=y
CONFIG_SX_WAKE_WORD_ESP_SR=y
CONFIG_SX_WAKE_WORD_THRESHOLD=50
CONFIG_SX_AUDIO_SAMPLE_RATE=16000
CONFIG_SX_AUDIO_BUFFER_SIZE_MS=100
CONFIG_SX_AUDIO_HARDWARE_VOLUME_ENABLE=y
CONFIG_SX_AUDIO_CODEC_AUTO=y
CONFIG_SX_AUDIO_POWER_SAVE_ENABLE=y
CONFIG_SX_AUDIO_POWER_SAVE_TIMEOUT_MS=15000
CONFIG_SX_FEATURE_MUSIC_ENABLE=y
CONFIG_SX_FEATURE_RADIO_ENABLE=y
CONFIG_SX_FEATURE_WEATHER_ENABLE=y
CONFIG_SX_FEATURE_NAVIGATION_ENABLE=y
CONFIG_SX_FEATURE_TELEGRAM_ENABLE=y
CONFIG_SX_FEATURE_BLUETOOTH_ENABLE=y
CONFIG_SX_FEATURE_MCP_SERVER_ENABLE=y
CONFIG_SX_NETWORK_AUTO_RECONNECT=y
CONFIG_SX_NETWORK_RECONNECT_INTERVAL_MS=5000
CONFIG_SX_MQTT_ENABLE=y
CONFIG_SX_WEBSOCKET_ENABLE=y

# SimpleXL Audio Features
CONFIG_SX_CODEC_OPUS_ENABLE=y
CONFIG_SX_AUDIO_AFE_ESP_SR_ENABLE=y
CONFIG_SX_WAKE_WORD_ESP_SR_ENABLE=y

# SimpleXL UI Configuration
CONFIG_SX_UI_THEME_DEFAULT="dark"
CONFIG_SX_UI_ANIMATION_ENABLE=y

# ESP-IDF Configuration
CONFIG_SPIRAM=y
```

---

**Cập nhật:** 2024-12-19  
**Trạng thái:** ✅ Hướng dẫn hoàn tất





















