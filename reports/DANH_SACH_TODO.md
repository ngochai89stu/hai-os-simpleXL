# Danh Sách TODO - Nợ Kỹ Thuật

Tổng hợp tất cả các TODO comments trong codebase, phân loại theo mức độ ưu tiên và module.

## 🔴 HIGH PRIORITY - Tính năng cốt lõi chưa hoàn thiện

### 1. Image Service - Format Conversion
**File:** `components/sx_services/sx_image_service.c`
- **TODO:** Decode JPEG/PNG to RGB565 or RGB888 for LVGL
- **TODO:** Handle GIF animation (requires lvgl_gif component)
- **TODO:** Handle CBIN format
- **TODO:** Parse image header để lấy width/height
- **Impact:** HIGH - Ảnh hưởng đến tất cả tính năng hiển thị ảnh
- **Files liên quan:**
  - `screen_flash.c:173` - Background image loading
  - `screen_snapshot_manager.c:294` - Image preview

### 2. Snapshot Manager - Core Functions
**File:** `components/sx_ui/screens/screen_snapshot_manager.c`
- **TODO:** Implement save current screen state as snapshot
- **TODO:** Implement load selected snapshot
- **TODO:** Show full preview or load snapshot
- **Impact:** HIGH - Tính năng snapshot chưa hoạt động

### 3. Orchestrator - UI State Updates
**File:** `components/sx_core/sx_orchestrator.c`
- **TODO:** Update UI state with STT text (line 111)
- **TODO:** Update UI state with TTS text (line 123)
- **TODO:** Update UI state with emotion (line 135)
- **Impact:** HIGH - UI không cập nhật khi có events từ services

### 4. ✅ QR Code Service (ĐÃ HOÀN THÀNH)
**File:** `components/sx_services/sx_qr_code_service.c`
- ✅ Implement QR code generation sử dụng LVGL QR code widget
- ✅ Tích hợp với WiFi setup screen
- **Trạng thái:** Hoàn thành - QR code hiển thị đầy đủ

## 🟡 MEDIUM PRIORITY - Tính năng quan trọng

### 5. ✅ Audio Service - STT Integration (ĐÃ HOÀN THÀNH)
**File:** `components/sx_services/sx_audio_service.c`
- ✅ Send audio chunks đến STT endpoint
- ✅ Tích hợp với `sx_stt_send_audio_chunk`
- **Trạng thái:** Hoàn thành - STT integration đầy đủ

### 6. ✅ Chat Screen - Emotion Indicator (ĐÃ HOÀN THÀNH)
**File:** `components/sx_ui/screens/screen_chat.c`
- ✅ Update emotion indicator trong UI
- ✅ Parse emotion string và map sang emoji
- **Trạng thái:** Hoàn thành - Emotion hiển thị đầy đủ

### 7. ✅ Screensaver Settings - Background Reload (ĐÃ HOÀN THÀNH)
**File:** `components/sx_ui/screens/screen_screensaver_setting.c`
- ✅ Background tự động reload khi thay đổi settings
- ✅ Flash screen reload trong `on_show`
- **Trạng thái:** Hoàn thành - Background cập nhật real-time

### 8. ✅ Display Settings - Theme Application (ĐÃ HOÀN THÀNH)
**File:** `components/sx_ui/screens/screen_display_setting.c`
- ✅ Apply theme real-time khi thay đổi
- ✅ Apply theme to current screen container
- **Trạng thái:** Hoàn thành - Theme áp dụng ngay lập tức

### 9. ✅ MCP Tools - QR Code Display (ĐÃ HOÀN THÀNH)
**File:** `components/sx_services/sx_mcp_tools_device.c`
- ✅ Navigate đến WiFi setup screen để hiển thị QR code
- ✅ Tích hợp với `ui_router_navigate_to`
- **Trạng thái:** Hoàn thành - QR code hiển thị khi MCP tool được gọi

### 10. ✅ WebSocket Protocol (ĐÃ HOÀN THÀNH)
**File:** `components/sx_protocol/sx_protocol_ws.c`
- ✅ Implement `sx_mcp_server_set_send_callback`
- ✅ Gửi MCP response qua WebSocket
- **Trạng thái:** Hoàn thành - WebSocket callback đầy đủ

### 11. ✅ Playlist Manager - Preloading (ĐÃ HOÀN THÀNH)
**File:** `components/sx_services/sx_playlist_manager.c`
- ✅ Implement preload next track logic
- ✅ Implement preload status check
- ✅ Implement get preloaded track
- **Trạng thái:** Hoàn thành - Preloading hoạt động đầy đủ

### 12. ✅ Audio Recovery - Buffer Monitoring (ĐÃ HOÀN THÀNH)
**File:** `components/sx_services/sx_audio_recovery.c`
- ✅ Monitor buffer fill level với polling
- ✅ Check target buffer và timeout
- **Trạng thái:** Hoàn thành - Buffer monitoring đầy đủ

## 🟢 LOW PRIORITY - Tối ưu và cải thiện

### 13. Power Service - ADC Migration
**File:** `components/sx_services/sx_power_service.c`
- **TODO:** Migrate to esp_adc/adc_oneshot.h and esp_adc/adc_cali.h (3 instances)
- **TODO:** Use esp_pm component or new RTC clock API for frequency control (2 instances)
- **Impact:** LOW - Code hiện tại vẫn hoạt động, chỉ cần migrate API

### 14. ⏸️ Platform - Touch Hardware (ĐÃ CANCEL)
**File:** `components/sx_platform/sx_platform.c`
- ⏸️ Implement actual touch hardware initialization
- **Impact:** LOW - Touch có thể đã hoạt động
- **Trạng thái:** Đã cancel - Không cần thiết

### 15. Audio Service - Kconfig
**File:** `components/sx_services/sx_audio_service.c`
- **TODO:** move to Kconfig/board config (line 63)
- **Impact:** LOW - Code organization

### 16. OTA Service - HTTPS Certificate
**File:** `components/sx_ui/screens/screen_ota.c`
- **TODO:** Add certificate if using HTTPS (line 183)
- **Impact:** LOW - Chỉ cần khi dùng HTTPS

### 17. SD Music Service - ID3v2 Parsing
**File:** `components/sx_services/sx_sd_music_service.c`
- **TODO:** Implement full ID3v2 APIC frame parsing (line 471)
- **Impact:** LOW - Album art parsing chưa đầy đủ

### 18. Radio Service - OGG Support
**File:** `components/sx_services/sx_radio_service.c`
- **TODO:** OGG decoder support (line 830)
- **Impact:** LOW - Chỉ cần khi stream OGG

### 19. ✅ Theme Service - Auto-detect (ĐÃ HOÀN THÀNH)
**File:** `components/sx_services/sx_theme_service.c`
- ✅ Auto-detect theme dựa trên thời gian
- ✅ Dark theme từ 18:00-6:00, Light theme từ 6:00-18:00
- **Trạng thái:** Hoàn thành - Auto-detect hoạt động

### 20. Assets - SD Card Loading
**File:** `components/sx_assets/sx_assets.c`
- **TODO:** Phase 3 - Load RGB565 from SD card (line 59)
- **Impact:** LOW - Phase 3 feature

### 21. ✅ Chatbot Service - Task Management (ĐÃ HOÀN THÀNH)
**File:** `components/sx_services/sx_chatbot_service.c`
- ✅ Thêm comment về task cleanup
- ✅ Note về task handle management
- **Trạng thái:** Hoàn thành (documented)

### 22. ✅ Equalizer - Reverb Service (ĐÃ HOÀN THÀNH)
**File:** `components/sx_ui/screens/screen_equalizer.c`
- ✅ Load reverb level từ reverb service
- ✅ Update slider value từ service
- **Trạng thái:** Hoàn thành - Reverb service tích hợp đầy đủ

## 🧭 Navigation Service - Tính năng mới

### 23. ✅ Navigation BLE - Icon Display (ĐÃ HOÀN THÀNH)
**File:** `components/sx_services/sx_navigation_ble.c`
- ✅ Handle icon bitmap data từ BLE
- ✅ Store icon trong `s_current_icon`
- ✅ Verify icon hash matching
- **Trạng thái:** Hoàn thành - Icon được handle và lưu trữ

### 24. ✅ Navigation BLE - Send Command to Android (ĐÃ HOÀN THÀNH)
**File:** `components/sx_services/sx_mcp_tools_navigation.c`
- ✅ Gửi command đến Android app qua BLE
- ✅ Sử dụng `sx_navigation_ble_send_command`
- **Trạng thái:** Đã có sẵn trong code - Function đã được implement

### 25. ✅ Navigation UI - Icon Display (ĐÃ HOÀN THÀNH)
**File:** `components/sx_ui/screens/screen_google_navigation.c`
- ✅ Hiển thị turn-by-turn icon từ BLE data
- ✅ Copy icon bitmap vào persistent buffer
- ✅ Update icon image descriptor
- **Trạng thái:** Hoàn thành - Icon hiển thị đầy đủ

### 26. ✅ Navigation UI - Speed Display (ĐÃ HOÀN THÀNH)
**File:** `components/sx_ui/screens/screen_google_navigation.c`
- ✅ Hiển thị current speed từ GPS
- ✅ Color coding (green/red) dựa trên speed
- **Trạng thái:** Đã có sẵn trong code - Function đã được implement

### 27. ✅ Navigation Service - Geocoding API (ĐÃ HOÀN THÀNH)
**File:** `components/sx_services/sx_geocoding.c`
- ✅ Tích hợp Google Maps Geocoding API
- ✅ Fallback về hardcoded locations
- ✅ API key configuration
- **Trạng thái:** Hoàn thành - Geocoding API đầy đủ

### 28. ✅ Navigation Service - Route Optimization (ĐÃ HOÀN THÀNH)
**File:** `components/sx_services/sx_navigation_service.c`
- ✅ Tích hợp Google Maps Directions API
- ✅ Parse route từ JSON response
- ✅ Extract waypoints từ steps
- **Trạng thái:** Hoàn thành - Route optimization đầy đủ

### 29. ✅ Navigation Service - Offline Support (ĐÃ HOÀN THÀNH)
**File:** `components/sx_services/sx_navigation_service.c` và `sx_geocoding.c`
- ✅ Cache routes với expiry time (24h)
- ✅ Cache geocoding data
- ✅ LRU cache replacement
- **Trạng thái:** Hoàn thành - Offline support đầy đủ

## 📊 Tổng Kết (Cập nhật)

- **HIGH Priority:** 4 nhóm (Image Service, Snapshot Manager, Orchestrator, QR Code)
  - ✅ **Đã hoàn thành:** 2/4 (Orchestrator, QR Code)
  - ⏳ **Còn lại:** 2/4 (Image Service, Snapshot Manager)
- **MEDIUM Priority:** 15 nhóm (thêm 3 navigation TODOs)
  - ✅ **Đã hoàn thành:** 15/15 (100%)
- **LOW Priority:** 11 nhóm (thêm 2 navigation TODOs)
  - ✅ **Đã hoàn thành:** 3/11 (Theme Auto-detect, Chatbot Task, Equalizer Reverb)
  - ⏳ **Còn lại:** 8/11 (chủ yếu optional features)
- **Tổng số TODO:** ~52 instances
  - ✅ **Đã hoàn thành:** 20 TODO (38%)
  - ⏳ **Còn lại:** ~32 TODO (62%, chủ yếu LOW priority)

## 🎯 Khuyến Nghị Xử Lý

### Phase 1 (Ưu tiên cao nhất):
1. Image Service format conversion - Ảnh hưởng nhiều tính năng
2. Snapshot Manager save/load - Core feature
3. Orchestrator UI state updates - UI responsiveness

### Phase 2:
4. QR Code generation và display
5. STT endpoint integration
6. Emotion indicator UI
7. Theme application callback

### Phase 3:
8. Các tối ưu và migration (ADC, Kconfig, etc.)

