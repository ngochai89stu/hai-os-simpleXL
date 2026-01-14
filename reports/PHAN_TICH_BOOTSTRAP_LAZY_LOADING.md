# Phân Tích Bootstrap - Lazy Loading Services

**Ngày:** 2025-01-27  
**Mục tiêu:** Xác định tính năng cần init ngay vs lazy load để tối ưu memory

---

## 📋 Phân Tích Bootstrap Hiện Tại

### ✅ **Tính Năng CẦN Init Ngay (Boot Time - Bắt Buộc)**

Các tính năng này cần thiết để system boot và hiển thị UI:

| # | Tính Năng | Lý Do | Memory | Vị Trí |
|---|-----------|-------|--------|--------|
| 1 | **NVS Flash** | Cần cho settings, WiFi config | Thấp | Line 53 |
| 2 | **Settings Service** | Cần đọc config từ NVS | Thấp | Line 62 |
| 3 | **Theme Service** | UI cần theme ngay | Thấp | Line 70 |
| 4 | **OTA Service** | Cần cho OTA updates | Thấp | Line 78 |
| 5 | **Dispatcher** | Core event system | Trung bình | Line 86 |
| 6 | **Orchestrator** | Core state management | Trung bình | Line 92 |
| 7 | **Display (LCD)** | UI cần hiển thị ngay | Trung bình | Line 95 |
| 8 | **Touch** | UI cần input ngay | Thấp | Line 103 |
| 9 | **SPI Bus Manager** | Cần cho SD service | Thấp | Line 112 |
| 10 | **UI Task** | Main UI loop | Cao (stack) | Line 150 |
| 11 | **Audio Service** | Core audio I/O, UI sounds | Cao | Line 309 |

**Tổng:** 11 tính năng cần thiết (~200-300KB memory)

---

### ⚠️ **Tính Năng CÓ THỂ Lazy Load (On-Demand)**

Các tính năng này chỉ cần khi user sử dụng:

#### **A. Network Services (Lazy Load)**

| # | Tính Năng | Khi Nào Cần | Memory | Vị Trí | Đề Xuất |
|---|-----------|-------------|--------|--------|---------|
| 1 | **WiFi Service** | Khi user vào WiFi settings hoặc cần network | ~50KB | Line 471 | ✅ Lazy load |
| 2 | **Protocol (WS/MQTT)** | Khi user enable chatbot/protocol | ~40KB | Line 390-433 | ✅ Lazy load |
| 3 | **BLE Navigation** | Khi user vào navigation screen | ~60KB | Line 575 | ✅ Lazy load |

#### **B. Audio Processing (Lazy Load)**

| # | Tính Năng | Khi Nào Cần | Memory | Vị Trí | Đề Xuất |
|---|-----------|-------------|--------|--------|---------|
| 1 | **STT Service** | Khi user bắt đầu voice input | ~30KB | Line 195 | ✅ Lazy load |
| 2 | **Wake Word Service** | Khi user enable wake word | ~100KB (model) | Line 260 | ✅ Lazy load |
| 3 | **AFE Service** | Khi cần audio processing | ~50KB | Line 210 | ✅ Lazy load |
| 4 | **TTS Service** | Khi cần text-to-speech | ~40KB | Line 555 | ✅ Lazy load |

#### **C. Media Services (Lazy Load)**

| # | Tính Năng | Khi Nào Cần | Memory | Vị Trí | Đề Xuất |
|---|-----------|-------------|--------|--------|---------|
| 1 | **SD Card Service** | Khi user vào file browser hoặc cần đọc file | ~30KB | Line 128 | ✅ Lazy load |
| 2 | **Radio Service** | Khi user vào radio screen | ~30KB | Line 326 | ✅ Lazy load |
| 3 | **Music Online** | Khi user vào music screen | ~30KB | Line 488 | ✅ Lazy load |
| 4 | **Playlist Manager** | Khi user vào playlist screen | ~20KB | Line 268 | ✅ Lazy load |

#### **D. Control Services (Lazy Load)**

| # | Tính Năng | Khi Nào Cần | Memory | Vị Trí | Đề Xuất |
|---|-----------|-------------|--------|--------|---------|
| 1 | **IR Service** | Khi user vào IR control screen | ~20KB | Line 344 | ✅ Lazy load |
| 2 | **Bluetooth Service** | Khi user vào Bluetooth settings | ~30KB | Line 632 | ✅ Lazy load |
| 3 | **LED Service** | Khi user vào LED settings | ~20KB | - | ✅ Lazy load |

#### **E. Feature Services (Lazy Load)**

| # | Tính Năng | Khi Nào Cần | Memory | Vị Trí | Đề Xuất |
|---|-----------|-------------|--------|--------|---------|
| 1 | **Chatbot Service** | Khi user vào chat screen | ~40KB | Line 440 | ✅ Lazy load |
| 2 | **Weather Service** | Khi user vào weather screen | ~30KB | Line 667 | ✅ Lazy load |
| 3 | **Navigation Service** | Khi user vào navigation screen | ~30KB | Line 563 | ✅ Lazy load |
| 4 | **Diagnostics Service** | Khi user vào diagnostics screen | ~20KB | Line 640 | ✅ Lazy load |
| 5 | **Intent Service** | Khi cần intent parsing | ~30KB | Line 357 | ✅ Lazy load |
| 6 | **Audio Protocol Bridge** | Khi cần audio streaming | ~30KB | Line 456 | ✅ Lazy load |
| 7 | **Audio Ducking** | Khi cần ducking | ~20KB | Line 280 | ✅ Lazy load |
| 8 | **Audio Power** | Khi cần power management | ~20KB | Line 293 | ✅ Lazy load |
| 9 | **Audio Router** | Khi cần route audio | ~20KB | Line 301 | ✅ Lazy load |
| 10 | **Assets Loader** | Khi cần load assets | ~20KB | Line 142 | ⚠️ Có thể lazy load |

**Tổng:** ~25 tính năng có thể lazy load (~700-800KB memory)

---

## 🎯 Đề Xuất Tối Ưu

### **Phase 1: Boot Time (Bắt Buộc - Giữ Nguyên)**

Chỉ init các tính năng cần thiết để system boot và hiển thị UI:

```c
// 1. NVS & Settings (bắt buộc)
nvs_flash_init();
sx_settings_service_init();
sx_theme_service_init();
sx_ota_service_init();  // Có thể lazy load nếu không cần OTA ngay

// 2. Core System (bắt buộc)
sx_dispatcher_init();
sx_orchestrator_start();

// 3. Display & Touch (bắt buộc)
sx_platform_display_init();
sx_platform_touch_init();  // ✅ Đã implement
sx_ui_start();

// 4. Audio Service (bắt buộc - cần cho UI sounds)
sx_audio_service_init();
sx_audio_service_start();

// 5. SPI Bus Manager (bắt buộc - cần cho SD)
sx_spi_bus_manager_init();
```

**Memory saved:** ~700-800KB (ước tính)

---

### **Phase 2: Lazy Loading (On-Demand)**

#### **A. Network Services**
- **WiFi:** Init khi user vào WiFi settings screen
- **Protocol (WS/MQTT):** Init khi user enable chatbot
- **BLE Navigation:** Init khi user vào navigation screen

#### **B. Audio Processing**
- **STT:** Init khi user bắt đầu voice input (button press)
- **Wake Word:** Init khi user enable wake word trong settings
- **AFE:** Init khi cần audio processing (cùng với STT/Wake Word)
- **TTS:** Init khi cần text-to-speech (first TTS call)

#### **C. Media Services**
- **SD Card:** Init khi user vào file browser screen
- **Radio:** Init khi user vào radio screen
- **Music Online:** Init khi user vào music screen
- **Playlist Manager:** Init khi user vào playlist screen

#### **D. Control Services**
- **IR Service:** Init khi user vào IR control screen
- **Bluetooth:** Init khi user vào Bluetooth settings screen
- **LED Service:** Init khi user vào LED settings screen

#### **E. Feature Services**
- **Chatbot:** Init khi user vào chat screen
- **Weather:** Init khi user vào weather screen
- **Navigation:** Init khi user vào navigation screen
- **Diagnostics:** Init khi user vào diagnostics screen
- **Intent Service:** Init khi chatbot được init
- **Audio Protocol Bridge:** Init khi protocol được init
- **Audio Ducking/Power/Router:** Init khi audio service được init (có thể giữ trong bootstrap)

---

## 🔧 Implementation Plan

### **Step 1: Tạo Lazy Loading Infrastructure**

Tạo file `components/sx_core/sx_lazy_loader.h` và `sx_lazy_loader.c`:

```c
// sx_lazy_loader.h
typedef enum {
    SX_LAZY_SERVICE_WIFI,
    SX_LAZY_SERVICE_STT,
    SX_LAZY_SERVICE_WAKE_WORD,
    SX_LAZY_SERVICE_AFE,
    SX_LAZY_SERVICE_TTS,
    SX_LAZY_SERVICE_BLE_NAV,
    SX_LAZY_SERVICE_CHATBOT,
    SX_LAZY_SERVICE_RADIO,
    SX_LAZY_SERVICE_MUSIC_ONLINE,
    SX_LAZY_SERVICE_SD_CARD,
    SX_LAZY_SERVICE_IR,
    SX_LAZY_SERVICE_BLUETOOTH,
    SX_LAZY_SERVICE_WEATHER,
    SX_LAZY_SERVICE_NAVIGATION,
    SX_LAZY_SERVICE_DIAGNOSTICS,
    SX_LAZY_SERVICE_INTENT,
    SX_LAZY_SERVICE_PROTOCOL_BRIDGE,
    SX_LAZY_SERVICE_PLAYLIST,
    // ... etc
} sx_lazy_service_t;

esp_err_t sx_lazy_service_init(sx_lazy_service_t service);
bool sx_lazy_service_is_initialized(sx_lazy_service_t service);
void sx_lazy_service_deinit(sx_lazy_service_t service);  // Optional: để free memory
```

### **Step 2: Refactor Bootstrap**

Di chuyển các service không cần thiết ra khỏi bootstrap, chỉ giữ:
- NVS, Settings, Theme, OTA
- Dispatcher, Orchestrator
- Display, Touch
- UI Task
- Audio Service (core)
- SPI Bus Manager

### **Step 3: Update Screens**

Mỗi screen tự init service cần thiết khi được hiển thị:

```c
// Example: screen_radio.c
void screen_radio_onShow(ui_screen_id_t screen_id) {
    // Lazy load radio service
    if (!sx_lazy_service_is_initialized(SX_LAZY_SERVICE_RADIO)) {
        sx_lazy_service_init(SX_LAZY_SERVICE_RADIO);
    }
    // ... rest of screen code
}
```

---

## 📊 Ước Tính Memory Savings

| Category | Services | Memory Saved (ước tính) |
|----------|----------|-------------------------|
| **Network** | WiFi, Protocol, BLE | ~150KB |
| **Audio Processing** | STT, Wake Word, AFE, TTS | ~220KB |
| **Media** | SD, Radio, Music, Playlist | ~110KB |
| **Control** | IR, Bluetooth, LED | ~70KB |
| **Features** | Chatbot, Weather, Nav, etc. | ~200KB |
| **Tổng** | 25 services | **~750KB** |

---

## ✅ Kết Luận

**Lợi ích:**
1. ✅ Giảm memory usage ~750KB
2. ✅ Giảm boot time ~3-5 giây
3. ✅ Giảm lỗi ESP_ERR_NO_MEM
4. ✅ System ổn định hơn

**Rủi ro:**
1. ⚠️ Cần refactor code
2. ⚠️ Cần test kỹ lazy loading
3. ⚠️ Có thể có delay nhỏ khi init service lần đầu

**Ưu tiên:**
1. ✅ **Đã implement touch hardware** - Touch sẽ hoạt động
2. ⚠️ **Tối ưu bootstrap** - Cần implement lazy loading infrastructure
3. ⚠️ **Test và verify** - Cần test touch và lazy loading

---

## 📝 Next Steps

1. **Touch Implementation:** ✅ **HOÀN THÀNH** - Đã implement FT5x06 touch
2. **Lazy Loading Infrastructure:** ⏳ **CẦN IMPLEMENT** - Tạo sx_lazy_loader
3. **Refactor Bootstrap:** ⏳ **CẦN IMPLEMENT** - Di chuyển services sang lazy load
4. **Update Screens:** ⏳ **CẦN IMPLEMENT** - Thêm lazy loading calls trong screens




















