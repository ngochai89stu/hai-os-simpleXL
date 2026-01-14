# Phase 1: Break Circular Dependencies - Tiến Độ

**Ngày:** 2025-01-02  
**Trạng thái:** 🟡 Đang thực hiện (30% hoàn thành)

---

## ✅ Đã Hoàn Thành

### 1. Events Mới Đã Thêm ✅

**File:** `components/sx_core/include/sx_event.h`

**Events thêm:**
- `SX_EVT_WIFI_SCAN_REQUEST` - UI → Service: request WiFi scan
- `SX_EVT_WIFI_CONNECT_REQUEST` - UI → Service: request connect
- `SX_EVT_WIFI_STATE_UPDATE` - Service → UI: WiFi state changed
- `SX_EVT_AUDIO_SPECTRUM_REQUEST` - UI → Service: request spectrum data
- `SX_EVT_AUDIO_SPECTRUM_DATA` - Service → UI: spectrum data
- `SX_EVT_TTS_SPEAK_REQUEST` - UI → Service: request TTS speak
- `SX_EVT_TTS_STATE_UPDATE` - Service → UI: TTS state changed
- `SX_EVT_STT_STATE_UPDATE` - Service → UI: STT state changed

### 2. Event Handlers Đã Tạo ✅

**File:** `components/sx_core/sx_event_handlers/wifi_handler.c`

**Handlers:**
- `sx_event_handler_wifi_scan_request()` - Handle scan requests
- `sx_event_handler_wifi_connect_request()` - Handle connect requests
- `sx_event_handler_wifi_state_update()` - Handle state updates

**Registration:**
- Đã register trong `sx_orchestrator.c`
- Đã thêm vào `event_handlers.h`
- Đã thêm vào `CMakeLists.txt`

### 3. screen_wifi_setup.c - Partial Refactor 🟡

**Đã làm:**
- Thêm includes cho `sx_dispatcher.h` và `sx_event.h`
- Comment out `#include "sx_wifi_service.h"`
- Thêm event handler function (skeleton)
- Thay một số direct calls bằng post events:
  - `on_show()` - scan request event
  - `network_item_click_cb()` - connect request event (no password case)

**Còn lại:**
- `scan_btn_cb()` - vẫn dùng `sx_wifi_scan()` trực tiếp
- `password_dialog_connect_cb()` - vẫn dùng `sx_wifi_connect()` trực tiếp (cần password)
- `update_ip_qr_code()` - vẫn dùng `sx_wifi_is_connected()` và `sx_wifi_get_ip_address()`
- `on_update()` - vẫn dùng `sx_wifi_is_connected()` và `sx_wifi_get_ssid()`

**Vấn đề:**
- Scan results chưa có cách truyền qua event (chỉ có count)
- Password chưa có cách truyền qua event payload
- IP address chưa có trong state

---

## 🔄 Đang Làm

### 4. screen_music_player_spectrum.c - Chưa bắt đầu ⚪

**Cần làm:**
- Thay `sx_audio_get_spectrum()` bằng post event
- Subscribe `SX_EVT_AUDIO_SPECTRUM_DATA` để nhận results
- Update UI từ event data

### 5. screen_chat.c - Chưa bắt đầu ⚪

**Cần làm:**
- Thay `sx_stt_is_active()` bằng đọc từ state
- Thay `sx_tts_is_speaking()` bằng đọc từ state
- Subscribe state update events

### 6. screen_google_navigation.c - Chưa bắt đầu ⚪

**Cần làm:**
- Thay `sx_tts_speak_simple()` bằng post event
- Subscribe TTS state updates

---

## 📋 TODO

### Immediate (Để hoàn thành screen_wifi_setup.c)

1. **Cải thiện Event Payload:**
   - Tạo payload structure cho WiFi scan results
   - Tạo payload structure cho WiFi connect (có password)
   - Thêm IP address vào state

2. **Hoàn thiện screen_wifi_setup.c:**
   - Refactor `scan_btn_cb()` để dùng events
   - Refactor `password_dialog_connect_cb()` để dùng events
   - Refactor `update_ip_qr_code()` để đọc từ state
   - Refactor `on_update()` để đọc từ state

3. **Update WiFi Service:**
   - Post `SX_EVT_WIFI_STATE_UPDATE` khi state thay đổi
   - Post scan results trong event payload
   - Update state với IP address

### Next Screens

4. **screen_music_player_spectrum.c:**
   - Tạo audio spectrum event handler
   - Refactor screen để dùng events

5. **screen_chat.c:**
   - Update để đọc STT/TTS state từ dispatcher
   - Subscribe state update events

6. **screen_google_navigation.c:**
   - Refactor để post TTS speak event
   - Subscribe TTS state updates

### Final Steps

7. **Loại bỏ CMakeLists.txt Dependencies:**
   - Xóa `PRIV_INCLUDE_DIRS "../sx_services/include"` từ `sx_ui/CMakeLists.txt`
   - Xóa `LINK_INTERFACE_MULTIPLICITY 3`
   - Xóa `REQUIRES sx_services` (nếu không cần)
   - Xóa `PRIV_REQUIRES sx_services` (nếu không cần)

8. **Testing:**
   - Build test
   - Functional test cho từng screen
   - Verify không còn direct dependencies

---

## 🎯 Kết Quả Mong Đợi

- ✅ Zero direct calls từ UI → Services
- ✅ Communication chỉ qua events/state
- ✅ CMakeLists.txt không còn circular dependency workarounds
- ✅ Architecture hoàn toàn decoupled

---

## 📊 Progress

```
Events:        ████████████████████ 100% ✅
Handlers:      ████████████████████ 100% ✅
WiFi Screen:   ██████░░░░░░░░░░░░░░  30% 🟡
Audio Screen:  ░░░░░░░░░░░░░░░░░░░░   0% ⚪
Chat Screen:   ░░░░░░░░░░░░░░░░░░░░   0% ⚪
Nav Screen:    ░░░░░░░░░░░░░░░░░░░░   0% ⚪
CMakeLists:    ░░░░░░░░░░░░░░░░░░░░   0% ⚪

Overall:       ████████░░░░░░░░░░░░  30% 🟡
```

---

*Progress report: 2025-01-02*
