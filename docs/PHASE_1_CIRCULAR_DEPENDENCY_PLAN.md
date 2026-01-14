# Phase 1: Break Circular Dependencies - Kế Hoạch Chi Tiết

**Ngày:** 2025-01-02  
**Trạng thái:** 🔄 Đang lập kế hoạch

---

## 📋 Phân Tích Dependencies Hiện Tại

### Direct Calls từ sx_ui → sx_services

1. **screen_music_player_spectrum.c**
   - `sx_audio_get_spectrum()` - Lấy spectrum data

2. **screen_wifi_setup.c**
   - `sx_wifi_scan()` - Scan WiFi networks
   - `sx_wifi_connect()` - Connect to WiFi
   - `sx_wifi_is_connected()` - Check connection status
   - `sx_wifi_get_ip_address()` - Get IP address
   - `sx_wifi_get_ssid()` - Get SSID

3. **screen_chat.c**
   - `sx_stt_is_active()` - Check STT status
   - `sx_tts_is_speaking()` - Check TTS status

4. **screen_google_navigation.c**
   - `sx_tts_speak_simple()` - Speak text

### CMakeLists.txt Issues

- `PRIV_INCLUDE_DIRS "../sx_services/include"` - Direct include path
- `LINK_INTERFACE_MULTIPLICITY 3` - Workaround cho circular dependency
- `REQUIRES sx_services` - Direct dependency

---

## 🎯 Giải Pháp

### Strategy 1: Event-Based Communication (Recommended)

**Nguyên tắc:** UI chỉ post events, Services xử lý và post state changes

**Implementation:**
1. Tạo events cho các operations:
   - `SX_EVT_WIFI_SCAN_REQUEST`
   - `SX_EVT_WIFI_CONNECT_REQUEST`
   - `SX_EVT_TTS_SPEAK_REQUEST`
   - etc.

2. Services subscribe và xử lý events

3. UI subscribe state changes để update display

**Ưu điểm:**
- Hoàn toàn decoupled
- Dễ test
- Scalable

**Nhược điểm:**
- Cần refactor nhiều code
- Async communication (có thể phức tạp hơn)

### Strategy 2: State Access Layer (Alternative)

**Nguyên tắc:** Tạo thin wrapper trong sx_core để expose state

**Implementation:**
1. Tạo `sx_state_access.h` trong sx_core
2. Wrapper functions chỉ đọc state (read-only)
3. UI include sx_core thay vì sx_services

**Ưu điểm:**
- Ít thay đổi code hơn
- Vẫn có direct calls nhưng qua layer

**Nhược điểm:**
- Vẫn có dependency (nhưng qua sx_core)
- Không hoàn toàn decoupled

---

## 📝 Kế Hoạch Thực Hiện (Strategy 1 - Event-Based)

### Bước 1: Tạo Events mới

**File:** `components/sx_core/include/sx_event.h`

Thêm events:
```c
// WiFi events
SX_EVT_WIFI_SCAN_REQUEST,
SX_EVT_WIFI_SCAN_RESULT,
SX_EVT_WIFI_CONNECT_REQUEST,
SX_EVT_WIFI_CONNECTED,
SX_EVT_WIFI_DISCONNECTED,

// TTS events
SX_EVT_TTS_SPEAK_REQUEST,
SX_EVT_TTS_SPEAKING,
SX_EVT_TTS_FINISHED,

// Audio events
SX_EVT_AUDIO_SPECTRUM_REQUEST,
SX_EVT_AUDIO_SPECTRUM_DATA,
```

### Bước 2: Refactor screen_wifi_setup.c

**Thay đổi:**
- `sx_wifi_scan()` → Post `SX_EVT_WIFI_SCAN_REQUEST`
- Subscribe `SX_EVT_WIFI_SCAN_RESULT` để update UI
- `sx_wifi_connect()` → Post `SX_EVT_WIFI_CONNECT_REQUEST`
- Subscribe `SX_EVT_WIFI_CONNECTED/DISCONNECTED` để update UI
- `sx_wifi_is_connected()` → Read từ state (via dispatcher)
- `sx_wifi_get_ip_address()` → Read từ state

### Bước 3: Refactor screen_music_player_spectrum.c

**Thay đổi:**
- `sx_audio_get_spectrum()` → Post `SX_EVT_AUDIO_SPECTRUM_REQUEST`
- Subscribe `SX_EVT_AUDIO_SPECTRUM_DATA` để update UI

### Bước 4: Refactor screen_chat.c và screen_google_navigation.c

**Thay đổi:**
- `sx_stt_is_active()` → Read từ state
- `sx_tts_is_speaking()` → Read từ state
- `sx_tts_speak_simple()` → Post `SX_EVT_TTS_SPEAK_REQUEST`

### Bước 5: Update Services để handle events

**Files:**
- `sx_wifi_service.c` - Handle scan/connect events
- `sx_audio_service.c` - Handle spectrum request
- `sx_tts_service.c` - Handle speak events

### Bước 6: Loại bỏ CMakeLists.txt dependencies

**File:** `components/sx_ui/CMakeLists.txt`

- Xóa `PRIV_INCLUDE_DIRS "../sx_services/include"`
- Xóa `LINK_INTERFACE_MULTIPLICITY 3`
- Xóa `REQUIRES sx_services` (nếu không cần)
- Xóa `PRIV_REQUIRES sx_services` (nếu không cần)

### Bước 7: Test và Fix

- Build test
- Fix missing symbols
- Verify functionality

---

## ⚠️ Rủi Ro

1. **Breaking Changes:** Nhiều code cần refactor
2. **Testing:** Cần test kỹ lưỡng tất cả screens
3. **Performance:** Event-based có thể chậm hơn direct calls (nhưng không đáng kể)

---

## 📊 Ước Tính Effort

- **Bước 1-2:** 2-3 ngày (Events + WiFi screen)
- **Bước 3-4:** 1-2 ngày (Audio + TTS screens)
- **Bước 5:** 2-3 ngày (Service event handlers)
- **Bước 6-7:** 1-2 ngày (CMakeLists + Testing)

**Tổng:** ~1-2 tuần

---

## 🎯 Kết Quả Mong Đợi

- ✅ Zero circular dependencies
- ✅ UI và Services hoàn toàn decoupled
- ✅ Communication chỉ qua events/state
- ✅ Dễ test và maintain hơn
- ✅ Scalable architecture

---

*Plan created: 2025-01-02*
