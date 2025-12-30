# Đề xuất Menu Items phù hợp với SimpleXL Architecture

**Ngày tạo:** 2024-12-19  
**Mục đích:** Đề xuất các menu items phù hợp với kiến trúc SimpleXL, không phụ thuộc board-specific

---

## 🎯 NGUYÊN TẮC ĐỀ XUẤT

### SimpleXL Architecture Principles:
1. ✅ **Board-independent** - Không phụ thuộc vào board cụ thể
2. ✅ **Platform abstraction** - Sử dụng `sx_platform` layer
3. ✅ **Service-based** - Các tính năng là services độc lập
4. ✅ **Configurable** - Có thể configure qua Kconfig hoặc Settings

### Không nên thêm:
- ❌ Board-specific configs (80+ boards từ repo mẫu)
- ❌ Hardware pin configuration (đã có trong `sx_platform`)
- ❌ Camera configs (nếu không có camera)
- ❌ Acoustic WiFi Provisioning (có thể không cần)

---

## 📋 ĐỀ XUẤT MENU ITEMS

### 1. OTA Configuration ⭐⭐⭐ (Cần thiết)

**Lý do:** OTA service đã có, cần config URL

```kconfig
menu "OTA Configuration"
    config SX_OTA_DEFAULT_URL
        string "Default OTA URL"
        default "https://api.example.com/ota/"
        help
            Default OTA server URL for firmware updates.
            Can be overridden via Settings service at runtime.
endmenu
```

**Menu items:**
- ✅ `SX_OTA_DEFAULT_URL` - Default OTA URL (string)

---

### 2. Language Configuration ⭐⭐ (Hữu ích)

**Lý do:** Hỗ trợ đa ngôn ngữ cho UI

```kconfig
menu "Language Configuration"
    choice SX_DEFAULT_LANGUAGE
        prompt "Default Language"
        default SX_LANGUAGE_VI_VN
        help
            Select default display language for SimpleXL UI

        config SX_LANGUAGE_VI_VN
            bool "Vietnamese"
        config SX_LANGUAGE_EN_US
            bool "English"
        config SX_LANGUAGE_ZH_CN
            bool "Chinese (Simplified)"
        config SX_LANGUAGE_ZH_TW
            bool "Chinese (Traditional)"
        config SX_LANGUAGE_JA_JP
            bool "Japanese"
        config SX_LANGUAGE_KO_KR
            bool "Korean"
    endchoice
endmenu
```

**Menu items:**
- ✅ `SX_DEFAULT_LANGUAGE` (choice) - 6 languages phổ biến nhất

**Lưu ý:** Có thể mở rộng thêm languages sau nếu cần

---

### 3. Display Configuration ⭐⭐⭐ (Cần thiết)

**Lý do:** Cần config display type, nhưng đơn giản hóa (không phụ thuộc board)

```kconfig
menu "Display Configuration"
    choice SX_DISPLAY_TYPE
        prompt "Display Type"
        default SX_DISPLAY_LCD_ST7796_320X480
        help
            Select display type. Pin configuration is handled by sx_platform.

        config SX_DISPLAY_LCD_ST7796_320X480
            bool "LCD ST7796 320x480 (IPS)"
        config SX_DISPLAY_LCD_ST7789_240X320
            bool "LCD ST7789 240x320 (IPS)"
        config SX_DISPLAY_LCD_ILI9341_240X320
            bool "LCD ILI9341 240x320"
        config SX_DISPLAY_OLED_SSD1306_128X64
            bool "OLED SSD1306 128x64"
        config SX_DISPLAY_CUSTOM
            bool "Custom Display (configure via sx_platform)"
    endchoice

    config SX_DISPLAY_TOUCH_ENABLE
        bool "Enable Touch Panel"
        default n
        help
            Enable touch panel support.
            Touch driver is automatically detected by sx_platform.

    config SX_DISPLAY_BRIGHTNESS_DEFAULT
        int "Default Brightness (%)"
        default 80
        range 0 100
        help
            Default display brightness (0-100%).
            Can be adjusted at runtime via Settings service.
endmenu
```

**Menu items:**
- ✅ `SX_DISPLAY_TYPE` (choice) - 5 display types phổ biến + Custom
- ✅ `SX_DISPLAY_TOUCH_ENABLE` - Enable Touch Panel (bool)
- ✅ `SX_DISPLAY_BRIGHTNESS_DEFAULT` - Default Brightness % (int)

---

### 4. Storage Configuration ⭐⭐ (Hữu ích)

**Lý do:** SD Card service đã có, cần config interface

```kconfig
menu "Storage Configuration"
    config SX_SD_CARD_ENABLE
        bool "Enable SD Card"
        default y
        help
            Enable SD Card support.
            Pin configuration is handled by sx_platform.

    choice SX_SD_CARD_INTERFACE
        prompt "SD Card Interface Type"
        default SX_SD_CARD_SPI
        depends on SX_SD_CARD_ENABLE
        help
            Select SD Card interface type.

        config SX_SD_CARD_SPI
            bool "SPI Interface"
        config SX_SD_CARD_MMC
            bool "SDMMC Interface"
    endchoice
endmenu
```

**Menu items:**
- ✅ `SX_SD_CARD_ENABLE` - Enable SD Card (bool)
- ✅ `SX_SD_CARD_INTERFACE` (choice) - SPI, SDMMC

---

### 5. Service Configuration ⭐⭐⭐ (Cần thiết)

**Lý do:** STT/TTS services cần config endpoints và API keys

```kconfig
menu "Service Configuration"
    menu "STT (Speech-to-Text) Configuration"
        config SX_STT_ENDPOINT_URL
            string "STT Endpoint URL"
            default ""
            help
                STT service endpoint URL.
                Can be configured via Settings service at runtime.

        config SX_STT_API_KEY
            string "STT API Key"
            default ""
            help
                STT service API key.
                Can be configured via Settings service at runtime.
    endmenu

    menu "TTS (Text-to-Speech) Configuration"
        config SX_TTS_ENDPOINT_URL
            string "TTS Endpoint URL"
            default ""
            help
                TTS service endpoint URL.
                Can be configured via Settings service at runtime.

        config SX_TTS_API_KEY
            string "TTS API Key"
            default ""
            help
                TTS service API key.
                Can be configured via Settings service at runtime.
    endmenu

    menu "Telegram Configuration"
        config SX_TELEGRAM_BOT_TOKEN
            string "Telegram Bot Token"
            default ""
            help
                Telegram bot token.
                Can be configured via Settings service at runtime.

        config SX_TELEGRAM_CHAT_ID
            string "Telegram Chat ID"
            default ""
            help
                Telegram chat ID.
                Can be configured via Settings service at runtime.
    endmenu

    menu "Online Music Configuration"
        config SX_MUSIC_ONLINE_AUTH_MAC
            string "Online Music Auth MAC"
            default ""
            help
                MAC address for online music authentication.
                Can be configured via Settings service at runtime.

        config SX_MUSIC_ONLINE_AUTH_SECRET
            string "Online Music Auth Secret"
            default ""
            help
                Secret key for online music authentication.
                Can be configured via Settings service at runtime.
    endmenu
endmenu
```

**Menu items:**
- ✅ `SX_STT_ENDPOINT_URL` - STT Endpoint URL (string)
- ✅ `SX_STT_API_KEY` - STT API Key (string)
- ✅ `SX_TTS_ENDPOINT_URL` - TTS Endpoint URL (string)
- ✅ `SX_TTS_API_KEY` - TTS API Key (string)
- ✅ `SX_TELEGRAM_BOT_TOKEN` - Telegram Bot Token (string)
- ✅ `SX_TELEGRAM_CHAT_ID` - Telegram Chat ID (string)
- ✅ `SX_MUSIC_ONLINE_AUTH_MAC` - Online Music Auth MAC (string)
- ✅ `SX_MUSIC_ONLINE_AUTH_SECRET` - Online Music Auth Secret (string)

---

### 6. Wake Word Configuration ⭐⭐ (Mở rộng)

**Lý do:** Mở rộng từ ESP-SR only sang nhiều options

```kconfig
menu "Wake Word Configuration"
    choice SX_WAKE_WORD_TYPE
        prompt "Wake Word Implementation Type"
        default SX_WAKE_WORD_ESP_SR
        help
            Choose the type of wake word implementation to use.

        config SX_WAKE_WORD_DISABLED
            bool "Disabled"
            help
                Disable wake word detection

        config SX_WAKE_WORD_ESP_SR
            bool "ESP-SR Wakenet (with AFE)"
            depends on SX_AUDIO_AFE_ESP_SR_ENABLE && SPIRAM
            help
                Use ESP-SR wakenet models with AFE support.
                Requires ESP32-S3/P4 and PSRAM.

        config SX_WAKE_WORD_CUSTOM
            bool "Custom Wake Word (Multinet)"
            depends on (IDF_TARGET_ESP32S3 || IDF_TARGET_ESP32P4) && SPIRAM
            help
                Use custom wake word with Multinet model.
                Requires ESP32-S3/P4 and PSRAM.
    endchoice

    config SX_WAKE_WORD_MODEL_PATH
        string "Wake Word Model Path"
        default ""
        depends on SX_WAKE_WORD_ESP_SR || SX_WAKE_WORD_CUSTOM
        help
            Path to wake word model file.
            Can be configured via Settings service at runtime.

    config SX_WAKE_WORD_THRESHOLD
        int "Wake Word Threshold (%)"
        default 50
        range 1 99
        depends on SX_WAKE_WORD_ESP_SR || SX_WAKE_WORD_CUSTOM
        help
            Wake word detection threshold (1-99%).
            Lower value = more sensitive.
            Can be configured via Settings service at runtime.

    config SX_WAKE_WORD_CUSTOM_TEXT
        string "Custom Wake Word Text"
        default "xiao zhi"
        depends on SX_WAKE_WORD_CUSTOM
        help
            Custom wake word text (use pinyin for Chinese, separated by spaces).
            Can be configured via Settings service at runtime.
endmenu
```

**Menu items:**
- ✅ `SX_WAKE_WORD_TYPE` (choice) - DISABLED, ESP_SR, CUSTOM
- ✅ `SX_WAKE_WORD_MODEL_PATH` - Model Path (string)
- ✅ `SX_WAKE_WORD_THRESHOLD` - Threshold % (int, 1-99)
- ✅ `SX_WAKE_WORD_CUSTOM_TEXT` - Custom Wake Word Text (string)

---

### 7. Audio Configuration ⭐⭐ (Mở rộng)

**Lý do:** Mở rộng audio configuration

```kconfig
menu "Audio Configuration"
    config SX_AUDIO_SAMPLE_RATE
        int "Audio Sample Rate (Hz)"
        default 16000
        range 8000 48000
        help
            Default audio sample rate.
            Can be changed at runtime per stream.

    config SX_AUDIO_BUFFER_SIZE_MS
        int "Audio Buffer Size (ms)"
        default 100
        range 50 500
        help
            Audio buffer size in milliseconds.
            Larger buffer = more stable but higher latency.

    config SX_AUDIO_HARDWARE_VOLUME_ENABLE
        bool "Enable Hardware Volume Control"
        default y
        help
            Enable hardware volume control via I2C codec (ES8388/ES8311).
            Falls back to software volume if hardware not available.

    choice SX_AUDIO_CODEC_TYPE
        prompt "Audio Codec Type"
        default SX_AUDIO_CODEC_AUTO
        depends on SX_AUDIO_HARDWARE_VOLUME_ENABLE
        help
            Audio codec type for hardware volume control.
            Auto-detect will try ES8388 first, then ES8311.

        config SX_AUDIO_CODEC_AUTO
            bool "Auto-detect (ES8388/ES8311)"
        config SX_AUDIO_CODEC_ES8388
            bool "ES8388"
        config SX_AUDIO_CODEC_ES8311
            bool "ES8311"
    endchoice

    config SX_AUDIO_POWER_SAVE_ENABLE
        bool "Enable Audio Power Save"
        default y
        help
            Enable automatic power save when audio is idle.
            Reduces power consumption when not playing audio.

    config SX_AUDIO_POWER_SAVE_TIMEOUT_MS
        int "Audio Power Save Timeout (ms)"
        default 15000
        range 5000 60000
        depends on SX_AUDIO_POWER_SAVE_ENABLE
        help
            Timeout before entering power save mode (milliseconds).
endmenu
```

**Menu items:**
- ✅ `SX_AUDIO_SAMPLE_RATE` - Sample Rate Hz (int, 8000-48000)
- ✅ `SX_AUDIO_BUFFER_SIZE_MS` - Buffer Size ms (int, 50-500)
- ✅ `SX_AUDIO_HARDWARE_VOLUME_ENABLE` - Enable Hardware Volume (bool)
- ✅ `SX_AUDIO_CODEC_TYPE` (choice) - AUTO, ES8388, ES8311
- ✅ `SX_AUDIO_POWER_SAVE_ENABLE` - Enable Power Save (bool)
- ✅ `SX_AUDIO_POWER_SAVE_TIMEOUT_MS` - Power Save Timeout ms (int)

---

### 8. Feature Toggles ⭐ (Hữu ích)

**Lý do:** Cho phép enable/disable features để giảm firmware size

```kconfig
menu "Feature Configuration"
    config SX_FEATURE_MUSIC_ENABLE
        bool "Enable Music Service"
        default y
        help
            Enable SD Card music playback service.

    config SX_FEATURE_RADIO_ENABLE
        bool "Enable Radio Service"
        default y
        help
            Enable online radio streaming service.

    config SX_FEATURE_WEATHER_ENABLE
        bool "Enable Weather Service"
        default y
        help
            Enable weather information service.

    config SX_FEATURE_NAVIGATION_ENABLE
        bool "Enable Navigation Service"
        default n
        help
            Enable navigation and route management service.

    config SX_FEATURE_TELEGRAM_ENABLE
        bool "Enable Telegram Service"
        default n
        help
            Enable Telegram bot integration service.

    config SX_FEATURE_BLUETOOTH_ENABLE
        bool "Enable Bluetooth Service"
        default n
        help
            Enable Bluetooth audio and pairing service.

    config SX_FEATURE_MCP_SERVER_ENABLE
        bool "Enable MCP Server"
        default y
        help
            Enable Model Context Protocol (MCP) server for AI integration.
endmenu
```

**Menu items:**
- ✅ `SX_FEATURE_MUSIC_ENABLE` - Enable Music Service (bool)
- ✅ `SX_FEATURE_RADIO_ENABLE` - Enable Radio Service (bool)
- ✅ `SX_FEATURE_WEATHER_ENABLE` - Enable Weather Service (bool)
- ✅ `SX_FEATURE_NAVIGATION_ENABLE` - Enable Navigation Service (bool)
- ✅ `SX_FEATURE_TELEGRAM_ENABLE` - Enable Telegram Service (bool)
- ✅ `SX_FEATURE_BLUETOOTH_ENABLE` - Enable Bluetooth Service (bool)
- ✅ `SX_FEATURE_MCP_SERVER_ENABLE` - Enable MCP Server (bool)

---

### 9. Network Configuration ⭐ (Tùy chọn)

**Lý do:** Một số network features hữu ích

```kconfig
menu "Network Configuration"
    config SX_NETWORK_AUTO_RECONNECT
        bool "Enable Auto Reconnect"
        default y
        help
            Automatically reconnect WiFi when connection is lost.

    config SX_NETWORK_RECONNECT_INTERVAL_MS
        int "Reconnect Interval (ms)"
        default 5000
        range 1000 30000
        depends on SX_NETWORK_AUTO_RECONNECT
        help
            WiFi reconnection interval in milliseconds.

    config SX_MQTT_ENABLE
        bool "Enable MQTT Protocol"
        default y
        help
            Enable MQTT protocol support.

    config SX_WEBSOCKET_ENABLE
        bool "Enable WebSocket Protocol"
        default y
        help
            Enable WebSocket protocol support.
endmenu
```

**Menu items:**
- ✅ `SX_NETWORK_AUTO_RECONNECT` - Enable Auto Reconnect (bool)
- ✅ `SX_NETWORK_RECONNECT_INTERVAL_MS` - Reconnect Interval ms (int)
- ✅ `SX_MQTT_ENABLE` - Enable MQTT Protocol (bool)
- ✅ `SX_WEBSOCKET_ENABLE` - Enable WebSocket Protocol (bool)

---

### 10. UI Configuration ⭐⭐ (Mở rộng)

**Lý do:** Mở rộng UI configuration

```kconfig
menu "UI Configuration"
    config UI_USE_LVGL_MUSIC_DEMO
        bool "Use LVGL Music Demo for Music Player Screen"
        default n
        depends on LV_USE_DEMO_MUSIC
        help
            When enabled, the Music Player screen will use the official LVGL Music Demo
            (lv_demo_music) instead of the custom UI implementation.
            
            This requires LV_USE_DEMO_MUSIC to be enabled in LVGL configuration.

    config SX_UI_THEME_DEFAULT
        string "Default Theme"
        default "dark"
        help
            Default UI theme (dark, light, auto).
            Can be changed at runtime via Settings service.

    config SX_UI_ANIMATION_ENABLE
        bool "Enable UI Animations"
        default y
        help
            Enable UI animations and transitions.
            Disable to reduce CPU usage.
endmenu
```

**Menu items:**
- ✅ `UI_USE_LVGL_MUSIC_DEMO` - (đã có)
- ✅ `SX_UI_THEME_DEFAULT` - Default Theme (string)
- ✅ `SX_UI_ANIMATION_ENABLE` - Enable UI Animations (bool)

---

## 📊 TỔNG KẾT ĐỀ XUẤT

### Menu Items đề xuất:

| Menu | Items | Priority | Lý do |
|------|-------|----------|-------|
| **OTA Configuration** | 1 | ⭐⭐⭐ | Cần cho OTA updates |
| **Language Configuration** | 1 (6 options) | ⭐⭐ | Hỗ trợ đa ngôn ngữ |
| **Display Configuration** | 3 | ⭐⭐⭐ | Cần cho multi-display |
| **Storage Configuration** | 2 | ⭐⭐ | SD Card support |
| **Service Configuration** | 8 | ⭐⭐⭐ | STT/TTS/Telegram/Music configs |
| **Wake Word Configuration** | 4 | ⭐⭐ | Mở rộng wake word options |
| **Audio Configuration** | 6 | ⭐⭐ | Audio settings |
| **Feature Configuration** | 7 | ⭐ | Enable/disable features |
| **Network Configuration** | 4 | ⭐ | Network settings |
| **UI Configuration** | 3 | ⭐⭐ | UI settings |

**Tổng cộng:** ~39 menu items (so với ~150+ trong repo mẫu)

---

## 🎯 IMPLEMENTATION PLAN

### Phase 1: Core Configuration (Priority ⭐⭐⭐)
1. ✅ OTA Configuration
2. ✅ Service Configuration (STT/TTS)
3. ✅ Display Configuration

### Phase 2: Extended Configuration (Priority ⭐⭐)
4. ✅ Language Configuration
5. ✅ Wake Word Configuration (mở rộng)
6. ✅ Audio Configuration
7. ✅ UI Configuration (mở rộng)

### Phase 3: Optional Configuration (Priority ⭐)
8. ✅ Storage Configuration
9. ✅ Feature Configuration
10. ✅ Network Configuration

---

## 📝 KHUYẾN NGHỊ

### Nên thêm ngay (Phase 1):
1. **OTA Configuration** - Cần cho OTA service
2. **Service Configuration** - STT/TTS endpoints cần config
3. **Display Configuration** - Cần cho multi-display support

### Có thể thêm sau (Phase 2):
4. **Language Configuration** - Hữu ích nhưng không critical
5. **Wake Word Configuration** - Mở rộng từ ESP-SR only
6. **Audio Configuration** - Mở rộng audio settings

### Tùy chọn (Phase 3):
7. **Feature Configuration** - Hữu ích để giảm firmware size
8. **Network Configuration** - Có thể đã đủ trong ESP-IDF
9. **Storage Configuration** - Đơn giản, có thể thêm sau

---

## ⚠️ LƯU Ý

1. **Board-independent:** Tất cả configs không phụ thuộc vào board cụ thể
2. **Platform abstraction:** Pin configuration vẫn qua `sx_platform`
3. **Settings override:** Tất cả configs có thể override qua Settings service
4. **Default values:** Tất cả configs có default values hợp lý

---

**Cập nhật:** 2024-12-19  
**Trạng thái:** ✅ Đề xuất hoàn tất




