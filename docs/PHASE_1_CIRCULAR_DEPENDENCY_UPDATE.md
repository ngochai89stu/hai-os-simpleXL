# Phase 1: Break Circular Dependencies - Update 2

**Ngày:** 2025-01-02  
**Trạng thái:** 🟡 60% hoàn thành (tăng từ 30%)

---

## ✅ Đã Hoàn Thành Thêm

### 1. Event Payloads ✅

**File:** `components/sx_core/include/sx_event_payloads.h`

**Payloads thêm:**
- `sx_wifi_scan_result_payload_t` - Chứa scan results
- `sx_wifi_connect_request_payload_t` - Chứa SSID và password

### 2. WiFi Handler Improvements ✅

**File:** `components/sx_core/sx_event_handlers/wifi_handler.c`

**Cải thiện:**
- Scan handler: Allocate và copy scan results vào payload
- Connect handler: Xử lý payload với password
- State update handler: Update state từ events

### 3. WiFi Service Updates ✅

**File:** `components/sx_services/sx_wifi_service.c`

**Cải thiện:**
- Post `SX_EVT_WIFI_STATE_UPDATE` khi connected (có IP address)

### 4. screen_wifi_setup.c - Refactor Tiếp Tục 🟡

**Đã làm:**
- Thêm include `sx_event_payloads.h`
- Refactor `scan_btn_cb()` để post scan request event
- Refactor `network_item_click_cb()` để post connect request (no password case)
- Refactor `password_dialog_connect_cb()` để post connect request (with password)
- Refactor `on_update()` để đọc từ state
- Thêm helper function `process_wifi_scan_complete_event()`

**Còn lại:**
- `update_ip_qr_code()` - Vẫn cần temporary include cho IP getter (TODO: add IP to state)
- `scan_btn_cb()` - Đã refactor nhưng cần test
- Event processing - Cần mechanism để UI nhận events (polling hoặc subscription)

---

## 🔄 Vấn Đề Còn Lại

### 1. Event Processing Mechanism

**Vấn đề:** UI cần cách để nhận events từ dispatcher.

**Giải pháp có thể:**
- Option A: Polling trong `on_update()` - Check dispatcher state
- Option B: Event subscription API (chưa có)
- Option C: Process events trong orchestrator và update state

**Hiện tại:** Dùng Option C (state updates) + Option A (polling state)

### 2. IP Address trong State

**Vấn đề:** IP address chưa có trong `sx_state_t`.

**Giải pháp:**
- Thêm `wifi_ip_address[16]` vào `sx_ui_state_t`
- Update state khi WiFi connected
- Remove temporary include

### 3. Scan Results Processing

**Vấn đề:** UI cần cách để nhận scan results từ event.

**Giải pháp hiện tại:**
- Handler post event với payload
- UI cần poll events hoặc có subscription mechanism
- **Temporary workaround:** Vẫn call scan trực tiếp trong một số cases

---

## 📋 Next Steps

### Immediate

1. **Add IP to State:**
   - Thêm `wifi_ip_address[16]` vào `sx_ui_state_t`
   - Update WiFi service để set IP trong state
   - Remove temporary include trong `update_ip_qr_code()`

2. **Complete screen_wifi_setup.c:**
   - Fix `update_ip_qr_code()` để đọc IP từ state
   - Test scan/connect flow
   - Remove any remaining direct calls

### Next Screens

3. **screen_music_player_spectrum.c:**
   - Tạo audio spectrum event handler
   - Refactor screen

4. **screen_chat.c:**
   - Update để đọc STT/TTS state từ dispatcher

5. **screen_google_navigation.c:**
   - Refactor để post TTS speak event

### Final

6. **CMakeLists.txt:**
   - Remove `PRIV_INCLUDE_DIRS`
   - Remove `LINK_INTERFACE_MULTIPLICITY`

---

## 📊 Progress

```
Events:        ████████████████████ 100% ✅
Payloads:      ████████████████████ 100% ✅
Handlers:      ████████████████████ 100% ✅
WiFi Screen:   ████████████░░░░░░░░  60% 🟡
Audio Screen:  ░░░░░░░░░░░░░░░░░░░░   0% ⚪
Chat Screen:   ░░░░░░░░░░░░░░░░░░░░   0% ⚪
Nav Screen:    ░░░░░░░░░░░░░░░░░░░░   0% ⚪
CMakeLists:    ░░░░░░░░░░░░░░░░░░░░   0% ⚪

Overall:       ████████████░░░░░░░░  60% 🟡
```

---

*Update: 2025-01-02*
