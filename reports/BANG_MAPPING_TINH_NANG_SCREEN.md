# Bảng Mapping Tính Năng - Screen (Cập Nhật)

## 📋 Tổng Quan

Bảng mapping chi tiết giữa các tính năng (services) và screens trong repo chính, xác định các tính năng đã link đúng, chưa link, hoặc link sai screen.

**Cập nhật lần cuối:** Sau khi merge Audio Effects vào Equalizer, tích hợp Music Online và IR Control, thiết kế lại UI theo web demo style.

---

## 📊 Bảng Mapping Chi Tiết

| # | Service/Tính Năng | Screen Tương Ứng | Trạng Thái | Ghi Chú |
|---|-------------------|------------------|------------|---------|
| 1 | **Chatbot Service** | `screen_chat.c` | ✅ **ĐÚNG** | Link đúng, xử lý events (STT, TTS, Emotion, Connected), audio streaming |
| 2 | **Audio Service** | `screen_music_player.c` | ✅ **ĐÚNG** | Link đúng, playback controls, volume, custom UI (LVGL demo đã tắt) |
| 3 | **Radio Service** | `screen_radio.c` | ✅ **ĐÚNG** | Link đúng, station list, playback, icon system |
| 4 | **SD Music Service** | `screen_sd_card_music.c` | ✅ **ĐÚNG** | Link đúng, file browser, playback, icon system |
| 5 | **Music Online Service** | `screen_music_online_list.c` | ✅ **ĐÃ TÍCH HỢP** | Đã include service, search, play song, track info |
| 6 | **WiFi Service** | `screen_wifi_setup.c` | ✅ **ĐÚNG** | Link đúng, scan, connect, QR code |
| 7 | **Bluetooth Service** | `screen_bluetooth_setting.c` | ❌ **CHƯA LINK** | Screen có nhưng chỉ placeholder, chưa include service |
| 8 | **Theme Service** | `screen_display_setting.c` | ✅ **ĐÚNG** | Link đúng, theme selector |
| 9 | **Platform (Brightness)** | `screen_display_setting.c` | ✅ **ĐÚNG** | Link đúng, brightness slider |
| 10 | **Weather Service** | `screen_flash.c` | ✅ **ĐÚNG** | Link đúng, weather widget (đã chuyển từ home sang flash/screensaver) |
| 11 | **Audio EQ Service** | `screen_equalizer.c` | ✅ **ĐÚNG** | Link đúng, EQ presets, bands, Reverb (đã merge Audio Effects) |
| 12 | **Audio Effects** | `screen_equalizer.c` | ✅ **ĐÃ MERGE** | Đã merge vào Equalizer (Bass, Treble, Reverb) |
| 13 | **OTA Service** | `screen_ota.c` | ✅ **ĐÚNG** | Link đúng, OTA update |
| 14 | **Settings Service** | `screen_settings.c` | ✅ **ĐÚNG** | Link đúng, menu list style (đã xóa volume/brightness trùng lặp) |
| 15 | **Settings Service** | `screen_display_setting.c` | ✅ **ĐÚNG** | Link đúng, settings persistence |
| 16 | **Settings Service** | `screen_wifi_setup.c` | ✅ **ĐÚNG** | Link đúng, WiFi credentials |
| 17 | **Settings Service** | `screen_ota.c` | ✅ **ĐÚNG** | Link đúng, OTA URL |
| 18 | **IR Service** | `screen_ir_control.c` | ✅ **ĐÃ TÍCH HỢP** | Đã include service, device selection, send IR commands |
| 19 | **Navigation Service** | `screen_google_navigation.c` | ❌ **CHƯA KIỂM TRA** | Cần kiểm tra |
| 20 | **Voice Settings** | `screen_voice_settings.c` | ❌ **CHƯA LINK** | Screen có UI nhưng chưa link với voice services |
| 21 | **Wake Word Service** | `screen_wakeword_feedback.c` | ❌ **CHƯA LINK** | Screen có nhưng chưa link với wake_word_service |
| 22 | **Screensaver** | `screen_flash.c` | ✅ **ĐÚNG** | Clock, date, weather, swipe to unlock, background image |
| 23 | **Playlist Manager** | `screen_music_player.c` | ⚠️ **GIÁN TIẾP** | Được sử dụng qua audio service |
| 24 | **STT Service** | ❌ **KHÔNG CÓ SCREEN** | Service có nhưng không có screen riêng |
| 25 | **TTS Service** | ❌ **KHÔNG CÓ SCREEN** | Service có nhưng không có screen riêng |
| 26 | **Image Service** | ❌ **KHÔNG CÓ SCREEN** | Service có nhưng không có screen riêng |
| 27 | **QR Code Service** | ❌ **KHÔNG CÓ SCREEN** | Service có nhưng không có screen riêng (có thể dùng trong wifi_setup) |
| 28 | **LED Service** | ❌ **KHÔNG CÓ SCREEN** | Service có nhưng không có screen riêng |
| 29 | **Power Service** | ❌ **KHÔNG CÓ SCREEN** | Service có nhưng không có screen riêng |
| 30 | **Telegram Service** | ❌ **KHÔNG CÓ SCREEN** | Service có nhưng không có screen riêng |

---

## 🔍 Phân Tích Chi Tiết

### ✅ Tính Năng Đã Link Đúng Screen

1. **Chatbot Service → screen_chat.c**
   - ✅ Xử lý events: STT, TTS, Emotion, Connected/Disconnected
   - ✅ Hiển thị messages (user/assistant)
   - ✅ Connection status indicator
   - ✅ Typing indicator
   - ✅ Input handling
   - ✅ Audio streaming (send/receive)

2. **Audio Service → screen_music_player.c**
   - ✅ Playback controls (play/pause/next/prev) với icon system
   - ✅ Volume control
   - ✅ Progress display
   - ✅ Track info display
   - ✅ Custom UI (LVGL demo music đã tắt)
   - ✅ Web demo style design

3. **Radio Service → screen_radio.c**
   - ✅ Station list
   - ✅ Playback controls với icon system
   - ✅ Metadata display
   - ✅ Error handling

4. **SD Music Service → screen_sd_card_music.c**
   - ✅ File browser
   - ✅ Playback integration
   - ✅ SD service integration
   - ✅ Icon system (folder/music file icons)

5. **Music Online Service → screen_music_online_list.c** ⭐ **MỚI**
   - ✅ Đã include `sx_music_online_service.h`
   - ✅ Search functionality
   - ✅ Play song integration
   - ✅ Track info display (title, artist, album)
   - ✅ Status updates

6. **WiFi Service → screen_wifi_setup.c**
   - ✅ Network scanning
   - ✅ Connection handling
   - ✅ Password input
   - ✅ Status display
   - ✅ QR code integration

7. **Theme Service → screen_display_setting.c**
   - ✅ Theme selector
   - ✅ Theme persistence

8. **Platform (Brightness) → screen_display_setting.c**
   - ✅ Brightness slider
   - ✅ Settings persistence

9. **Weather Service → screen_flash.c** ⭐ **ĐÃ CHUYỂN**
   - ✅ Weather widget (đã chuyển từ home sang screensaver)
   - ✅ City, temperature, description
   - ✅ Clock và date display

10. **Audio EQ Service → screen_equalizer.c**
    - ✅ EQ presets
    - ✅ Band sliders (10 bands)
    - ✅ EQ service integration
    - ✅ Reverb control (đã merge từ Audio Effects)

11. **Audio Effects → screen_equalizer.c** ⭐ **ĐÃ MERGE**
    - ✅ Bass control (dùng EQ bands)
    - ✅ Treble control (dùng EQ bands)
    - ✅ Reverb control (slider trong Equalizer)

12. **OTA Service → screen_ota.c**
    - ✅ OTA update UI
    - ✅ URL input
    - ✅ Progress display

13. **IR Service → screen_ir_control.c** ⭐ **MỚI**
    - ✅ Đã include `sx_ir_service.h`
    - ✅ Device selection
    - ✅ Send IR commands
    - ✅ Status display

14. **Screensaver → screen_flash.c** ⭐ **MỚI**
    - ✅ Clock display
    - ✅ Date display
    - ✅ Weather widget
    - ✅ Swipe to unlock
    - ✅ Background image customization

---

### ⚠️ Tính Năng Link Trùng Lặp (ĐÃ SỬA)

**Trước đây:**
- Volume control trong `screen_settings.c` - ⚠️ Trùng lặp
- Brightness control trong `screen_settings.c` - ⚠️ Trùng lặp
- WiFi list trong `screen_settings.c` - ⚠️ Trùng lặp

**Hiện tại:**
- ✅ `screen_settings.c` đã chuyển thành menu list style
- ✅ Chỉ có navigation đến các settings screens
- ✅ Không còn trùng lặp controls

**Đề xuất đã thực hiện:**
- ✅ Xóa volume control khỏi `screen_settings.c`, chỉ giữ trong `screen_music_player.c`
- ✅ Xóa brightness control khỏi `screen_settings.c`, chỉ giữ trong `screen_display_setting.c`
- ✅ Xóa WiFi list khỏi `screen_settings.c`, chỉ giữ trong `screen_wifi_setup.c`
- ✅ `screen_settings.c` giờ là menu để navigate đến các settings screens

---

### ❌ Tính Năng Chưa Link Hoặc Link Sai

#### 1. Bluetooth Service → screen_bluetooth_setting.c
**Vấn đề:**
- Screen có nhưng chỉ là placeholder
- Chưa include `sx_bluetooth_service.h`
- Chưa tích hợp với service

**Cần làm:**
```c
// screen_bluetooth_setting.c
#include "sx_bluetooth_service.h"

// Tích hợp:
- sx_bluetooth_scan() - Scan devices
- sx_bluetooth_connect() - Connect device
- sx_bluetooth_disconnect() - Disconnect
- sx_bluetooth_get_paired_devices() - Get paired list
- Events: SX_EVT_BLUETOOTH_* (nếu có)
```

#### 2. Voice Settings → screen_voice_settings.c
**Vấn đề:**
- Screen có UI (VAD, AEC, NS, Mic Gain) nhưng chưa link với services
- Chưa include voice-related services

**Cần làm:**
```c
// screen_voice_settings.c
#include "sx_audio_afe.h"  // For AEC, NS
#include "sx_wake_word_service.h"  // For wake word settings
#include "sx_stt_service.h"  // For STT settings

// Tích hợp:
- sx_audio_afe_set_aec_enabled() - AEC switch
- sx_audio_afe_set_ns_enabled() - NS switch
- sx_audio_afe_set_vad_threshold() - VAD slider
- sx_audio_afe_set_mic_gain() - Mic gain slider
- sx_wake_word_set_sensitivity() - Wake word sensitivity
```

#### 3. Wake Word Service → screen_wakeword_feedback.c
**Vấn đề:**
- Screen có animation nhưng chưa link với wake_word_service
- Chưa xử lý wake word events

**Cần làm:**
```c
// screen_wakeword_feedback.c
#include "sx_wake_word_service.h"
#include "sx_event.h"

// Tích hợp:
- SX_EVT_WAKE_WORD_DETECTED event
- Wake word sensitivity settings
- Wake word feedback animation
```

#### 4. Navigation Service → screen_google_navigation.c
**Vấn đề:**
- Cần kiểm tra xem đã link chưa

**Cần làm:**
```c
// screen_google_navigation.c
#include "sx_navigation_service.h"

// Tích hợp:
- sx_navigation_start() - Start navigation
- sx_navigation_set_destination() - Set destination
- Navigation display
```

---

### ❌ Services Không Có Screen Riêng

#### 1. STT Service
**Service:** `sx_stt_service.h`
**Vấn đề:** Không có screen riêng
**Đề xuất:**
- Có thể tích hợp vào `screen_voice_settings.c` (STT settings)
- Hoặc tích hợp vào `screen_chat.c` (STT status)

#### 2. TTS Service
**Service:** `sx_tts_service.h`
**Vấn đề:** Không có screen riêng
**Đề xuất:**
- Có thể tích hợp vào `screen_voice_settings.c` (TTS settings)
- Hoặc tích hợp vào `screen_chat.c` (TTS status)

#### 3. Image Service
**Service:** `sx_image_service.h`
**Vấn đề:** Không có screen riêng
**Đề xuất:**
- Tạo `screen_image_viewer.c` - Image viewer screen
- Hoặc tích hợp vào `screen_snapshot_manager.c`

#### 4. QR Code Service
**Service:** `sx_qr_code_service.h`
**Vấn đề:** Không có screen riêng
**Đề xuất:**
- Tạo `screen_qr_code.c` - QR code display screen
- Hoặc tích hợp vào `screen_wifi_setup.c` (IP QR code)
- Hoặc tích hợp vào `screen_network_diagnostic.c`

#### 5. LED Service
**Service:** `sx_led_service.h`
**Vấn đề:** Không có screen riêng
**Đề xuất:**
- Tạo `screen_led_control.c` - LED control screen
- Hoặc tích hợp vào `screen_settings.c` (LED settings)

#### 6. Power Service
**Service:** `sx_power_service.h`
**Vấn đề:** Không có screen riêng
**Đề xuất:**
- Tích hợp vào `screen_settings.c` (Power settings)
- Hoặc tích hợp vào `screen_home.c` (Battery indicator)

#### 7. Telegram Service
**Service:** `sx_telegram_service.h`
**Vấn đề:** Không có screen riêng
**Đề xuất:**
- Tạo `screen_telegram.c` - Telegram interface screen
- Hoặc tích hợp vào `screen_chat.c` (Telegram messages)

---

## 📋 Tổng Kết Vấn Đề

### ✅ Đã Link Đúng (14)
1. Chatbot → screen_chat
2. Audio → screen_music_player
3. Radio → screen_radio
4. SD Music → screen_sd_card_music
5. Music Online → screen_music_online_list ⭐ **MỚI**
6. WiFi → screen_wifi_setup
7. Theme → screen_display_setting
8. Brightness → screen_display_setting
9. Weather → screen_flash (screensaver) ⭐ **ĐÃ CHUYỂN**
10. Audio EQ → screen_equalizer
11. Audio Effects → screen_equalizer ⭐ **ĐÃ MERGE**
12. OTA → screen_ota
13. IR Service → screen_ir_control ⭐ **MỚI**
14. Screensaver → screen_flash ⭐ **MỚI**

### ⚠️ Link Trùng Lặp (0) ✅ **ĐÃ SỬA**
- ✅ Đã xóa volume/brightness/WiFi khỏi `screen_settings.c`
- ✅ `screen_settings.c` giờ là menu list style

### ❌ Chưa Link Hoặc Link Sai (4)
1. Bluetooth → screen_bluetooth_setting (placeholder)
2. Voice Settings → screen_voice_settings (chưa link services)
3. Wake Word → screen_wakeword_feedback (chưa link service)
4. Navigation → screen_google_navigation (cần kiểm tra)

### ❌ Services Không Có Screen (7)
1. STT Service
2. TTS Service
3. Image Service
4. QR Code Service
5. LED Service
6. Power Service
7. Telegram Service

---

## 🎯 Đề Xuất Sửa Lỗi

### Priority 1: Hoàn Thiện Link Chưa Đúng

1. **screen_bluetooth_setting.c**
   - Include `sx_bluetooth_service.h`
   - Tích hợp scan, connect, paired devices

2. **screen_voice_settings.c**
   - Include voice-related services
   - Tích hợp VAD, AEC, NS, Mic Gain

3. **screen_wakeword_feedback.c**
   - Include `sx_wake_word_service.h`
   - Xử lý wake word events

4. **screen_google_navigation.c**
   - Kiểm tra và tích hợp navigation service

### Priority 2: Tạo Screens Mới (Optional)

1. **screen_image_viewer.c** - Image viewer
2. **screen_qr_code.c** - QR code display
3. **screen_led_control.c** - LED control
4. **screen_telegram.c** - Telegram interface (optional)

---

## 📊 Bảng Tổng Hợp

| Loại | Số Lượng | Danh Sách |
|------|----------|-----------|
| ✅ Đã Link Đúng | 14 | Chatbot, Audio, Radio, SD Music, Music Online, WiFi, Theme, Brightness, Weather, EQ, Audio Effects (merged), OTA, IR, Screensaver |
| ⚠️ Link Trùng Lặp | 0 | ✅ Đã sửa - không còn trùng lặp |
| ❌ Chưa Link | 4 | Bluetooth, Voice Settings, Wake Word, Navigation |
| ❌ Không Có Screen | 7 | STT, TTS, Image, QR Code, LED, Power, Telegram |

**Tổng cộng:**
- Services có screen: 21
- Services chưa có screen: 7
- Screens cần sửa: 4 (chưa link)

---

## 🎨 Cải Tiến UI Gần Đây

### Icon System
- ✅ Đã thiết kế lại icon system sử dụng LVGL symbols
- ✅ Thay thế emoji bằng icon system nhất quán
- ✅ Tất cả screens đã sử dụng icon system mới

### Web Demo Style
- ✅ Thiết kế lại tất cả screens theo web demo style
- ✅ Top bar với back button icon
- ✅ List items với icon arrows
- ✅ Menu cards với rounded corners
- ✅ Consistent spacing và typography

### LVGL Demo Music
- ✅ Đã tắt LVGL demo music
- ✅ Music Player chỉ dùng custom UI
- ✅ Web demo style design

### Settings Screen
- ✅ Chuyển thành menu list style
- ✅ Xóa trùng lặp controls
- ✅ Navigation đến sub-settings screens

---

## 📝 Ghi Chú

- **Audio Effects** đã được merge vào **Equalizer** (Bass, Treble, Reverb)
- **Weather widget** đã chuyển từ `screen_home.c` sang `screen_flash.c` (screensaver)
- **Music Online** và **IR Control** đã được tích hợp service
- **Settings screen** đã chuyển thành menu list, không còn trùng lặp
- **Icon system** đã được thiết kế lại, tất cả screens sử dụng LVGL symbols
- **UI design** đã được cải thiện theo web demo style
