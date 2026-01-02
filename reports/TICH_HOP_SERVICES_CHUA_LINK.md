# Tích Hợp Services Chưa Link Screen

## 📋 Tổng Quan

Báo cáo này mô tả việc tích hợp các services chưa có screen riêng vào các screens hiện có, tham khảo repo mẫu.

**Cập nhật:** Sau khi tích hợp STT/TTS status, QR code, và image service.

---

## ✅ Đã Tích Hợp

### 1. STT Service → screen_chat.c

**Tích hợp:**
- ✅ Hiển thị STT status label ("STT: Ready" / "STT: Listening")
- ✅ Update status khi STT active/inactive
- ✅ Update status khi nhận STT result từ events
- ✅ Màu sắc: Gray (Ready), Blue (Listening)

**Code:**
```c
// screen_chat.c
#include "sx_stt_service.h"

// Status labels
static lv_obj_t *s_stt_status_label = NULL;
static bool s_stt_active = false;

// Update STT status
bool stt_is_active = sx_stt_is_active();
if (stt_is_active != s_stt_active && s_stt_status_label != NULL) {
    s_stt_active = stt_is_active;
    if (stt_is_active) {
        lv_label_set_text(s_stt_status_label, "STT: Listening");
        lv_obj_set_style_text_color(s_stt_status_label, lv_color_hex(0x5b7fff), 0);
    } else {
        lv_label_set_text(s_stt_status_label, "STT: Ready");
        lv_obj_set_style_text_color(s_stt_status_label, lv_color_hex(0x888888), 0);
    }
}
```

### 2. TTS Service → screen_chat.c

**Tích hợp:**
- ✅ Hiển thị TTS status label ("TTS: Ready" / "TTS: Speaking")
- ✅ Update status khi TTS start/stop
- ✅ Update status từ service (`sx_tts_is_speaking()`)
- ✅ Màu sắc: Gray (Ready), Blue (Speaking)

**Code:**
```c
// screen_chat.c
#include "sx_tts_service.h"

// Status labels
static lv_obj_t *s_tts_status_label = NULL;
static bool s_tts_speaking = false;

// Update TTS status
bool tts_is_speaking = sx_tts_is_speaking();
if (tts_is_speaking != s_tts_speaking && s_tts_status_label != NULL) {
    s_tts_speaking = tts_is_speaking;
    if (tts_is_speaking) {
        lv_label_set_text(s_tts_status_label, "TTS: Speaking");
        lv_obj_set_style_text_color(s_tts_status_label, lv_color_hex(0x5b7fff), 0);
    } else {
        lv_label_set_text(s_tts_status_label, "TTS: Ready");
        lv_obj_set_style_text_color(s_tts_status_label, lv_color_hex(0x888888), 0);
    }
}
```

### 3. QR Code Service → screen_wifi_setup.c

**Tích hợp:**
- ✅ Hiển thị IP address label
- ✅ Generate QR code từ IP address
- ✅ Display QR code image (150x150px)
- ✅ Auto-update khi WiFi connects
- ✅ Convert 1-bit bitmap to RGB565 cho LVGL

**Code:**
```c
// screen_wifi_setup.c
#include "sx_qr_code_service.h"

// QR code display
static lv_obj_t *s_qr_code_img = NULL;
static lv_obj_t *s_ip_label = NULL;

// Generate and display QR code
static void update_ip_qr_code(void) {
    const char *ip_address = sx_wifi_get_ip_address();
    if (ip_address != NULL && strlen(ip_address) > 0) {
        // Show IP address
        char ip_text[64];
        snprintf(ip_text, sizeof(ip_text), "IP: %s", ip_address);
        lv_label_set_text(s_ip_label, ip_text);
        
        // Generate QR code
        sx_qr_code_result_t qr_result = {0};
        esp_err_t ret = sx_qr_code_generate_ip(ip_address, 0, &qr_result);
        if (ret == ESP_OK && qr_result.data != NULL) {
            // Convert 1-bit bitmap to RGB565
            // ... (conversion code)
            lv_img_set_src(s_qr_code_img, &qr_img_dsc);
        }
    }
}
```

### 4. Image Service → screen_snapshot_manager.c

**Tích hợp:**
- ✅ Include `sx_image_service.h`
- ⚠️ **TODO**: Implement image preview functionality
- ⚠️ **TODO**: Load and display images from snapshot files

**Code:**
```c
// screen_snapshot_manager.c
#include "sx_image_service.h"

// TODO: Implement image preview
// - Load image from file using sx_image_load_from_file()
// - Display image in snapshot list items
// - Support GIF, JPEG, PNG, CBin formats
```

---

## ⚠️ Chưa Hoàn Thiện

### 1. Image Service → screen_snapshot_manager.c

**Cần làm:**
- Load images từ snapshot files
- Display image preview trong snapshot list
- Support các format: GIF, JPEG, PNG, CBin
- Image viewer khi click vào snapshot

**Đề xuất:**
```c
// screen_snapshot_manager.c
static void load_snapshot_image(const char *file_path) {
    sx_image_info_t info = {0};
    uint8_t *image_data = NULL;
    
    esp_err_t ret = sx_image_load_from_file(file_path, &info, &image_data);
    if (ret == ESP_OK && image_data != NULL) {
        // Create LVGL image descriptor
        // Display in snapshot item
        // ...
        sx_image_free(image_data);
    }
}
```

### 2. STT/TTS Settings → screen_voice_settings.c

**Cần làm:**
- Include `sx_stt_service.h` và `sx_tts_service.h`
- Tích hợp STT settings (endpoint, API key, chunk duration)
- Tích hợp TTS settings (endpoint, API key, priority)
- UI controls cho các settings

**Đề xuất:**
```c
// screen_voice_settings.c
#include "sx_stt_service.h"
#include "sx_tts_service.h"

// STT Settings
- Endpoint URL input
- API key input
- Chunk duration slider
- Auto-send toggle

// TTS Settings
- Endpoint URL input
- API key input
- Default priority selector
- Timeout setting
```

---

## 📊 Tổng Kết

### ✅ Đã Tích Hợp (3/4)
1. ✅ STT Service → screen_chat.c (status display)
2. ✅ TTS Service → screen_chat.c (status display)
3. ✅ QR Code Service → screen_wifi_setup.c (IP QR code)

### ⚠️ Chưa Hoàn Thiện (2/4)
1. ⚠️ Image Service → screen_snapshot_manager.c (cần implement preview)
2. ⚠️ STT/TTS Settings → screen_voice_settings.c (cần tích hợp settings UI)

---

## 🎯 Next Steps

### Priority 1: Hoàn Thiện Image Service
1. Implement image loading trong `screen_snapshot_manager.c`
2. Display image preview trong snapshot list
3. Image viewer screen (optional)

### Priority 2: Hoàn Thiện Voice Settings
1. Tích hợp STT settings UI
2. Tích hợp TTS settings UI
3. Save/load settings từ settings service

---

## 📝 Ghi Chú

- **STT/TTS status** đã được tích hợp vào `screen_chat.c` để hiển thị trạng thái real-time
- **QR code** đã được tích hợp vào `screen_wifi_setup.c` để hiển thị IP address QR code
- **Image service** đã include nhưng cần implement preview functionality
- **Voice settings** cần tích hợp STT/TTS configuration UI


















