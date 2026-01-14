# Tối Ưu Bootstrap - Lazy Loading Services

**Ngày:** 2025-01-27  
**Mục tiêu:** Xác định tính năng cần init ngay vs lazy load để tối ưu memory và boot time

---

## 📋 Phân Tích Bootstrap Hiện Tại

### ✅ **Tính Năng CẦN Init Ngay (Boot Time)**

Các tính năng này cần thiết để system hoạt động cơ bản:

| Tính Năng | Lý Do | Memory Impact |
|-----------|-------|---------------|
| **NVS Flash** | Cần cho settings, WiFi config | Thấp |
| **Settings Service** | Cần đọc config từ NVS | Thấp |
| **Dispatcher** | Core event system | Trung bình |
| **Orchestrator** | Core state management | Trung bình |
| **Display (LCD)** | UI cần hiển thị ngay | Trung bình |
| **Touch** | UI cần input ngay | Thấp (I2C) |
| **UI Task** | Main UI loop | Cao (stack) |
| **Theme Service** | UI cần theme | Thấp |
| **Audio Service** | Core audio I/O | Cao (buffers) |

**Tổng:** ~8 tính năng cần thiết

---

### ⚠️ **Tính Năng CÓ THỂ Lazy Load**

Các tính năng này chỉ cần khi user sử dụng:

| Tính Năng | Khi Nào Cần | Memory Impact | Đề Xuất |
|-----------|-------------|---------------|---------|
| **SD Card Service** | Khi cần đọc file từ SD | Trung bình | ✅ Lazy load |
| **STT Service** | Khi user nói/voice input | Cao | ✅ Lazy load |
| **Wake Word Service** | Khi enable wake word | Cao (model) | ✅ Lazy load |
| **AFE Service** | Khi cần audio processing | Cao | ✅ Lazy load |
| **WiFi Service** | Khi cần network | Cao (stack) | ✅ Lazy load |
| **TTS Service** | Khi cần text-to-speech | Cao | ✅ Lazy load |
| **BLE Navigation** | Khi cần navigation | Cao | ✅ Lazy load |
| **Chatbot Service** | Khi user chat | Cao | ✅ Lazy load |
| **Radio Service** | Khi user nghe radio | Trung bình | ✅ Lazy load |
| **IR Service** | Khi user dùng IR | Thấp | ✅ Lazy load |
| **Music Online** | Khi user nghe nhạc online | Trung bình | ✅ Lazy load |
| **Weather Service** | Khi user xem weather | Trung bình | ✅ Lazy load |
| **Navigation Service** | Khi user dùng navigation | Trung bình | ✅ Lazy load |
| **Audio Protocol Bridge** | Khi cần audio streaming | Trung bình | ✅ Lazy load |
| **Bluetooth Service** | Khi user dùng Bluetooth | Cao | ✅ Lazy load |
| **Diagnostics Service** | Khi cần diagnostics | Thấp | ✅ Lazy load |
| **OTA Service** | Khi cần update | Thấp | ✅ Lazy load |
| **Playlist Manager** | Khi user quản lý playlist | Thấp | ✅ Lazy load |
| **Audio Ducking** | Khi cần ducking | Thấp | ✅ Lazy load |
| **Audio Power** | Khi cần power management | Thấp | ✅ Lazy load |
| **Audio Router** | Khi cần route audio | Thấp | ✅ Lazy load |
| **Intent Service** | Khi cần intent parsing | Trung bình | ✅ Lazy load |
| **Protocol (WS/MQTT)** | Khi cần protocol | Cao | ✅ Lazy load |
| **LED Service** | Khi cần LED control | Thấp | ✅ Lazy load |
| **Power Service** | Khi cần power management | Thấp | ✅ Lazy load |
| **State Manager** | Core state (có thể cần) | Trung bình | ⚠️ Cần xem xét |

**Tổng:** ~25 tính năng có thể lazy load

---

## 🎯 Đề Xuất Tối Ưu

### **Phase 1: Boot Time (Bắt Buộc)**

Chỉ init các tính năng cần thiết để system boot và hiển thị UI:

```c
// 1. NVS & Settings (bắt buộc)
nvs_flash_init();
sx_settings_service_init();
sx_theme_service_init();

// 2. Core System (bắt buộc)
sx_dispatcher_init();
sx_orchestrator_start();

// 3. Display & Touch (bắt buộc)
sx_platform_display_init();
sx_platform_touch_init();
sx_ui_start();

// 4. Audio Service (bắt buộc - cần cho UI sounds)
sx_audio_service_init();
sx_audio_service_start();
```

**Memory saved:** ~200-300KB (ước tính)

---

### **Phase 2: Lazy Loading (On-Demand)**

Init các service khi user thực sự sử dụng:

#### **A. Network Services (Lazy Load)**
- WiFi: Init khi user vào WiFi settings hoặc cần network
- Protocol (WS/MQTT): Init khi user enable chatbot/protocol
- BLE Navigation: Init khi user vào navigation screen

#### **B. Audio Processing (Lazy Load)**
- STT: Init khi user bắt đầu voice input
- Wake Word: Init khi user enable wake word
- AFE: Init khi cần audio processing
- TTS: Init khi cần text-to-speech

#### **C. Media Services (Lazy Load)**
- SD Card: Init khi user vào file browser hoặc cần đọc file
- Radio: Init khi user vào radio screen
- Music Online: Init khi user vào music screen
- Playlist Manager: Init khi user vào playlist screen

#### **D. Control Services (Lazy Load)**
- IR Service: Init khi user vào IR control screen
- LED Service: Init khi user vào LED settings
- Bluetooth: Init khi user vào Bluetooth settings

#### **E. Feature Services (Lazy Load)**
- Chatbot: Init khi user vào chat screen
- Weather: Init khi user vào weather screen
- Navigation: Init khi user vào navigation screen
- Diagnostics: Init khi user vào diagnostics screen

---

## 🔧 Implementation Plan

### **Step 1: Tạo Lazy Loading Infrastructure**

```c
// sx_lazy_loader.h
typedef enum {
    SX_LAZY_SERVICE_WIFI,
    SX_LAZY_SERVICE_STT,
    SX_LAZY_SERVICE_WAKE_WORD,
    SX_LAZY_SERVICE_TTS,
    SX_LAZY_SERVICE_BLE_NAV,
    SX_LAZY_SERVICE_CHATBOT,
    SX_LAZY_SERVICE_RADIO,
    SX_LAZY_SERVICE_MUSIC_ONLINE,
    SX_LAZY_SERVICE_IR,
    SX_LAZY_SERVICE_WEATHER,
    // ... etc
} sx_lazy_service_t;

esp_err_t sx_lazy_service_init(sx_lazy_service_t service);
bool sx_lazy_service_is_initialized(sx_lazy_service_t service);
```

### **Step 2: Refactor Bootstrap**

- Di chuyển các service không cần thiết ra khỏi bootstrap
- Thêm lazy loading calls trong các screen/feature tương ứng

### **Step 3: Update Screens**

- Mỗi screen tự init service cần thiết khi được hiển thị
- Check xem service đã init chưa trước khi init

---

## 📊 Ước Tính Memory Savings

| Service | Memory Saved (ước tính) |
|---------|-------------------------|
| WiFi | ~50KB (stack + buffers) |
| STT | ~30KB (buffers) |
| Wake Word | ~100KB (model) |
| AFE | ~50KB (buffers) |
| TTS | ~40KB (buffers) |
| BLE Navigation | ~60KB (stack + buffers) |
| Chatbot | ~40KB (buffers) |
| Radio | ~30KB (buffers) |
| Music Online | ~30KB (buffers) |
| **Tổng** | **~430KB** |

---

## ✅ Kết Luận

**Lợi ích:**
1. Giảm memory usage ~400-500KB
2. Giảm boot time ~2-3 giây
3. Giảm lỗi ESP_ERR_NO_MEM
4. System ổn định hơn

**Rủi ro:**
1. Cần refactor code
2. Cần test kỹ lazy loading
3. Có thể có delay nhỏ khi init service lần đầu

**Ưu tiên:**
1. ✅ Implement touch hardware
2. ⚠️ Tối ưu bootstrap (lazy loading)
3. ⚠️ Test và verify




















