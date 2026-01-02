# R2 DEEP AUDIT - EVIDENCE APPENDIX
## Trích Đoạn Evidence Cho Violations Quan Trọng

> **Tiếp nối từ:** `R2_DEEP_AUDIT_PHASE4_ROADMAP.md`

---

## 1. EVIDENCE CHO VIOLATIONS P0

### Evidence 1: sx_display_service.c - Include LVGL Headers

**File:** `components/sx_services/sx_display_service.c`  
**Lines:** 8-9, 47, 19  
**Violation:** Rule 0.1, 0.2 (Services không được include/gọi LVGL)

```8:9:components/sx_services/sx_display_service.c
#include "esp_lvgl_port.h"
#include "lvgl.h"
```

```19:19:components/sx_services/sx_display_service.c
static lv_obj_t *s_preview_image_obj = NULL;
```

```47:50:components/sx_services/sx_display_service.c
    // Get default display
    lv_display_t *disp = lv_display_get_default();
    if (!disp) {
        ESP_LOGE(TAG, "No default display available");
```

**Phân tích:**
- Service include trực tiếp `lvgl.h` và `esp_lvgl_port.h` (vi phạm Rule 0.2)
- Service sử dụng `lv_obj_t*` và gọi `lv_display_get_default()` (vi phạm Rule 0.1)
- Service đang gọi LVGL APIs từ service context, không phải UI task

---

### Evidence 2: sx_qr_code_service.c - Gọi LVGL APIs

**File:** `components/sx_services/sx_qr_code_service.c`  
**Lines:** 7, 46  
**Violation:** Rule 0.1, 0.2

```7:7:components/sx_services/sx_qr_code_service.c
#include "lvgl.h"
```

```43:50:components/sx_services/sx_qr_code_service.c
    // Check if LVGL QR code widget is available
#if LV_USE_QRCODE
    // Use LVGL QR code widget to generate QR code
    // Create a temporary QR code widget to get the data
    lv_obj_t *temp_qr = lv_qrcode_create(lv_scr_act());
    if (temp_qr == NULL) {
        ESP_LOGE(TAG, "Failed to create LVGL QR code widget");
        return ESP_ERR_NO_MEM;
```

**Phân tích:**
- Service include `lvgl.h` (vi phạm Rule 0.2)
- Service gọi `lv_qrcode_create()` và `lv_scr_act()` (vi phạm Rule 0.1)
- Service đang tạo LVGL objects từ service context

---

### Evidence 3: sx_state.h - Thiếu version và dirty_mask

**File:** `components/sx_core/include/sx_state.h`  
**Lines:** 82-87  
**Violation:** Section 5.1 v1.3 (MUST có version và dirty_mask)

```82:87:components/sx_core/include/sx_state.h
typedef struct {
    uint32_t seq; // monotonically increasing snapshot sequence
    sx_wifi_state_t wifi;
    sx_audio_state_t audio;
    sx_ui_state_t ui;
} sx_state_t;
```

**Phân tích:**
- State chỉ có `seq`, không có `version` (Section 5.1 v1.3 yêu cầu `uint32_t version;`)
- State không có `dirty_mask` (Section 5.1 v1.3 yêu cầu `uint32_t dirty_mask;`)
- UI không thể optimize render dựa trên dirty_mask

---

### Evidence 4: sx_ui_task.c - Include Trực Tiếp lvgl.h

**File:** `components/sx_ui/sx_ui_task.c`  
**Lines:** 7  
**Violation:** Section 7.5 v1.3 (Phải include qua sx_lvgl.h wrapper)

```7:7:components/sx_ui/sx_ui_task.c
#include "lvgl.h"
```

**Phân tích:**
- UI task include trực tiếp `lvgl.h` thay vì qua `sx_lvgl.h` wrapper
- Section 7.5 v1.3 yêu cầu MUST có wrapper header
- Không có compile-time guard để prevent include LVGL ngoài sx_ui

---

## 2. EVIDENCE CHO VIOLATIONS P1

### Evidence 5: sx_dispatcher.c - Thiếu Backpressure Policy

**File:** `components/sx_core/sx_dispatcher.c`  
**Lines:** 61-120  
**Violation:** Section 4.3 v1.3 (MUST có backpressure policy)

```61:120:components/sx_core/sx_dispatcher.c
bool sx_dispatcher_post_event(const sx_event_t *evt) {
    if (!evt) {
        return false;
    }
    
    // Determine priority (use default if not set)
    sx_event_priority_t priority = evt->priority;
    if (priority == 0) {  // Not set, use default
        priority = SX_EVT_DEFAULT_PRIORITY(evt->type);
    }
    
    // Select queue based on priority
    QueueHandle_t target_q = s_evt_q_normal;
    TickType_t timeout = 0;
    
    switch (priority) {
        case SX_EVT_PRIORITY_CRITICAL:
            target_q = s_evt_q_critical;
            timeout = pdMS_TO_TICKS(10);  // Block up to 10ms for critical
            break;
        case SX_EVT_PRIORITY_HIGH:
            target_q = s_evt_q_high;
            timeout = pdMS_TO_TICKS(5);   // Block up to 5ms for high
            break;
        case SX_EVT_PRIORITY_NORMAL:
            target_q = s_evt_q_normal;
            timeout = 0;  // Non-blocking
            break;
        case SX_EVT_PRIORITY_LOW:
            target_q = s_evt_q_low;
            timeout = 0;  // Non-blocking
            break;
        default:
            target_q = s_evt_q_normal;
            timeout = 0;
            break;
    }
    
    if (target_q == NULL) {
        return false;
    }
    
    if (xQueueSend(target_q, evt, timeout) == pdTRUE) {
        return true;
    }
    
    // Queue full - event dropped
    s_drop_count++;
    
    // Rate-limited logging (max once per 5 seconds)
    TickType_t now = xTaskGetTickCount();
    if (now - s_last_drop_log_time >= s_drop_log_interval) {
        ESP_LOGW(TAG, "Event queue full - dropped %lu events (event type: %d, priority: %d)", 
                 (unsigned long)s_drop_count, (int)evt->type, (int)priority);
        s_drop_count = 0;
        s_last_drop_log_time = now;
    }
    
    return false;
}
```

**Phân tích:**
- Chỉ có drop policy, không có `SX_BP_COALESCE` (coalesce latest event)
- Chỉ có block cho CRITICAL, nhưng không có policy enum rõ ràng
- Section 4.3 v1.3 yêu cầu MUST có `SX_BP_DROP`, `SX_BP_COALESCE`, `SX_BP_BLOCK`

---

### Evidence 6: sx_lvgl_lock.h - Thiếu Runtime Assert

**File:** `components/sx_ui/include/sx_lvgl_lock.h`  
**Lines:** 1-37  
**Violation:** Section 7.3 v1.3 (MUST có SX_ASSERT_UI_THREAD)

```1:37:components/sx_ui/include/sx_lvgl_lock.h
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// LVGL lock guard (RAII-style for C)
typedef struct {
    bool locked;
} sx_lvgl_lock_guard_t;

/**
 * @brief Acquire LVGL lock (returns guard, check .locked)
 * 
 * @return sx_lvgl_lock_guard_t Guard with locked flag set if lock acquired
 */
sx_lvgl_lock_guard_t sx_lvgl_lock_acquire(void);

/**
 * @brief Release LVGL lock (must match acquire)
 * 
 * @param guard Guard returned from sx_lvgl_lock_acquire
 */
void sx_lvgl_lock_release(sx_lvgl_lock_guard_t *guard);

// Macro for automatic lock/unlock
#define SX_LVGL_LOCK() \
    for (sx_lvgl_lock_guard_t _guard = sx_lvgl_lock_acquire(); \
         _guard.locked; \
         sx_lvgl_lock_release(&_guard), _guard.locked = false)

#ifdef __cplusplus
}
#endif
```

**Phân tích:**
- Không có `SX_ASSERT_UI_THREAD()` macro (Section 7.3 v1.3 yêu cầu)
- Không có `sx_ui_set_ui_task_handle()` function
- Không có `SX_LVGL_CALL()` wrapper macro

---

### Evidence 7: sx_event.h - Thiếu Range Reservation

**File:** `components/sx_core/include/sx_event.h`  
**Lines:** 25-100  
**Violation:** Section 4.1 v1.3 (MUST có range reservation)

```25:100:components/sx_core/include/sx_event.h
// System-level event types. Keep stable and forward-compatible.
typedef enum {
    SX_EVT_NONE = 0,

    // lifecycle
    SX_EVT_BOOTSTRAP_STARTED,
    SX_EVT_BOOTSTRAP_READY,

    // UI
    SX_EVT_UI_READY,

    // platform
    SX_EVT_PLATFORM_READY,

    // services
    SX_EVT_SERVICES_READY,

    // input
    SX_EVT_UI_INPUT, // payload: sx_ui_input_event_t (defined in sx_ui_state.h)

    // audio service (Phase 4)
    SX_EVT_AUDIO_PLAYBACK_STARTED,
    SX_EVT_AUDIO_PLAYBACK_STOPPED,
    SX_EVT_AUDIO_PLAYBACK_PAUSED,
    SX_EVT_AUDIO_PLAYBACK_RESUMED,
    SX_EVT_AUDIO_RECORDING_STARTED,
    SX_EVT_AUDIO_RECORDING_STOPPED,
    SX_EVT_AUDIO_ERROR,

    // radio service (Phase 4 - online streaming)
    SX_EVT_RADIO_STARTED,
    SX_EVT_RADIO_STOPPED,
    SX_EVT_RADIO_METADATA,
    SX_EVT_RADIO_ERROR,

    // wifi service (Phase 5)
    SX_EVT_WIFI_CONNECTED,
    SX_EVT_WIFI_DISCONNECTED,
    SX_EVT_WIFI_SCAN_COMPLETE,

    // chatbot service (Phase 5)
    SX_EVT_CHATBOT_STT,              // STT result from server (ptr: text string)
    SX_EVT_CHATBOT_TTS_START,        // TTS started
    SX_EVT_CHATBOT_TTS_STOP,         // TTS stopped
    SX_EVT_CHATBOT_TTS_SENTENCE,     // TTS sentence (ptr: text string)
    SX_EVT_CHATBOT_EMOTION,          // LLM emotion (ptr: emotion string)
    SX_EVT_CHATBOT_CONNECTED,        // Chatbot connected
    SX_EVT_CHATBOT_DISCONNECTED,     // Chatbot disconnected
    SX_EVT_CHATBOT_AUDIO_CHANNEL_OPENED,  // Audio channel (UDP) opened (MQTT only)
    SX_EVT_CHATBOT_AUDIO_CHANNEL_CLOSED,  // Audio channel closed

    // system commands
    SX_EVT_SYSTEM_REBOOT,            // System reboot command
    SX_EVT_SYSTEM_COMMAND,           // Generic system command (ptr: command string)

    // alert messages
    SX_EVT_ALERT,                    // Alert message (ptr: alert_data_t*)

    // protocol errors and handshake
    SX_EVT_PROTOCOL_ERROR,           // Protocol error (ptr: error message string)
    SX_EVT_PROTOCOL_TIMEOUT,         // Protocol timeout
    SX_EVT_PROTOCOL_HELLO_SENT,      // Hello message sent
    SX_EVT_PROTOCOL_HELLO_RECEIVED,  // Server hello received (ptr: hello_data_t*)
    SX_EVT_PROTOCOL_HELLO_TIMEOUT,   // Server hello timeout
    SX_EVT_PROTOCOL_RECONNECTING,     // Reconnection in progress (arg0: attempt number)

    // OTA / Activation
    SX_EVT_OTA_PROGRESS,              // arg0 = percent, arg1 = speed KB/s
    SX_EVT_OTA_FINISHED,              // ptr = version string
    SX_EVT_OTA_ERROR,                 // ptr = error message
    SX_EVT_ACTIVATION_REQUIRED,       // ptr = activation code string
    SX_EVT_ACTIVATION_PENDING,        // activation pending (HTTP 202), arg0=attempt, arg1=delay_ms
    SX_EVT_ACTIVATION_DONE,           // activation success
    SX_EVT_ACTIVATION_TIMEOUT,        // activation timed out after polling

    // diagnostics
    SX_EVT_ERROR,
```

**Phân tích:**
- Event IDs không có range reservation (Section 4.1 v1.3 yêu cầu)
- Không có `SX_EVT_UI_BASE`, `SX_EVT_AUDIO_BASE`, etc.
- Có thể conflict IDs khi thêm events mới

---

## 3. EVIDENCE CHO THIẾU FILES (MUST EXIST)

### Evidence 8: sx_lvgl.h Wrapper Không Tồn Tại

**File:** `components/sx_ui/include/sx_lvgl.h`  
**Status:** ❌ **KHÔNG TỒN TẠI**  
**Violation:** Section 7.5 v1.3 (MUST có wrapper header)

**Phân tích:**
- Section 7.5 v1.3 yêu cầu MUST có `sx_ui/include/sx_lvgl.h`
- Wrapper này mới được phép include `lvgl.h`
- Phải có compile-time guard `#if !defined(SX_COMPONENT_SX_UI) #error ... #endif`

---

### Evidence 9: sx_metrics.c Không Tồn Tại

**File:** `components/sx_core/src/sx_metrics.c`  
**Status:** ❌ **KHÔNG TỒN TẠI**  
**Violation:** Section 9.1 v1.3 (MUST có metrics system)

**Phân tích:**
- Section 9.1 v1.3 yêu cầu MUST có `sx_metrics.c` và `sx_metrics.h`
- Section 9.2 yêu cầu metrics: evt_posted, evt_dropped, evt_coalesced, queue_depth, state_version, ui_render_ms, heap_free_min, psram_free_min

---

### Evidence 10: sx_service_if.h Không Tồn Tại

**File:** `components/sx_core/include/sx_service_if.h`  
**Status:** ❌ **KHÔNG TỒN TẠI**  
**Violation:** Section 6.1 v1.3 (MUST có service interface)

**Phân tích:**
- Section 6.1 v1.3 yêu cầu MUST có `sx_service_if.h` với vtable: `init() / start() / stop() / deinit() / on_event(evt)`

---

### Evidence 11: sx_screen_if.h Không Tồn Tại

**File:** `components/sx_ui/include/sx_screen_if.h`  
**Status:** ❌ **KHÔNG TỒN TẠI**  
**Violation:** Section 6.2 v1.3 (MUST có screen interface)

**Phân tích:**
- Section 6.2 v1.3 yêu cầu MUST có `sx_screen_if.h` với vtable: `create() / destroy() / on_enter() / on_exit() / on_state_change(mask)`

---

## 4. TỔNG KẾT EVIDENCE

### Violations có Evidence:
1. ✅ **sx_display_service.c** - Include và gọi LVGL (Lines 8-9, 47)
2. ✅ **sx_qr_code_service.c** - Include và gọi LVGL (Lines 7, 46)
3. ✅ **sx_state.h** - Thiếu version và dirty_mask (Lines 82-87)
4. ✅ **sx_ui_task.c** - Include trực tiếp lvgl.h (Line 7)
5. ✅ **sx_dispatcher.c** - Thiếu backpressure policy (Lines 61-120)
6. ✅ **sx_lvgl_lock.h** - Thiếu runtime assert (Lines 1-37)
7. ✅ **sx_event.h** - Thiếu range reservation (Lines 25-100)

### Files Không Tồn Tại (MUST):
1. ❌ `sx_ui/include/sx_lvgl.h` (Section 7.5 v1.3)
2. ❌ `sx_core/src/sx_metrics.c` (Section 9.1 v1.3)
3. ❌ `sx_core/include/sx_service_if.h` (Section 6.1 v1.3)
4. ❌ `sx_ui/include/sx_screen_if.h` (Section 6.2 v1.3)
5. ❌ `docs/SIMPLEXL_RESOURCE_OWNERSHIP.md` (Section 6.3 v1.3)
6. ❌ `docs/SIMPLEXL_QUALITY_GATES.md` (Section 10.2 v1.3)

---

**Kết thúc Evidence Appendix.**






