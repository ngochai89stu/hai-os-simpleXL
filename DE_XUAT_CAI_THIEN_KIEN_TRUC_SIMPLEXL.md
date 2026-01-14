# ĐỀ XUẤT CẢI THIỆN KIẾN TRÚC SIMPLEXL

> **Mục tiêu:** Cải thiện kiến trúc SIMPLEXL để tăng tính ổn định, hiệu năng, và khả năng maintain mà vẫn giữ nguyên nguyên tắc cốt lõi

---

## 📋 MỤC LỤC

1. [Phân tích kiến trúc hiện tại](#1-phân-tích-kiến-trúc-hiện-tại)
2. [Vấn đề và rủi ro](#2-vấn-đề-và-rủi-ro)
3. [Đề xuất cải thiện](#3-đề-xuất-cải-thiện)
4. [Implementation plan](#4-implementation-plan)
5. [Kết luận](#5-kết-luận)

---

## 1. PHÂN TÍCH KIẾN TRÚC HIỆN TẠI

### 1.1 Kiến trúc cốt lõi (SIMPLEXL_ARCH)

**Nguyên tắc bất biến:**
1. **Event-driven:** Services emit events, orchestrator consumes
2. **State snapshot:** Single-writer (orchestrator), multi-reader (UI + services)
3. **Component boundaries:** sx_core, sx_ui, sx_platform, sx_services tách biệt
4. **LVGL ownership:** Chỉ UI task được gọi LVGL APIs
5. **Communication:** UI ↔ services chỉ qua events và state snapshots

**Flow hiện tại:**
```
Services/UI → sx_dispatcher_post_event() → Queue (64 events)
    ↓
Orchestrator → sx_dispatcher_poll_event() → Process events
    ↓
Orchestrator → sx_dispatcher_set_state() → Update state (mutex-protected)
    ↓
UI Task → sx_dispatcher_get_state() → Read snapshot → Update UI
```

### 1.2 Điểm mạnh

- ✅ **Modular:** Component boundaries rõ ràng
- ✅ **Thread-safe:** Mutex cho state, queue cho events
- ✅ **Testable:** Có thể mock events và state
- ✅ **Scalable:** Dễ thêm services mới
- ✅ **Separation of concerns:** UI không biết về services

### 1.3 Điểm yếu (từ phân tích code)

1. **Orchestrator quá lớn:** Xử lý tất cả events trong 1 loop, khó maintain
2. **Event handling rải rác:** Logic xử lý event không có structure
3. **Không có event priority:** Tất cả events đều bình đẳng
4. **LVGL lock discipline:** Nested locks, inconsistent usage
5. **Event drop:** Queue đầy → mất events
6. **State thiếu fields:** Không đủ thông tin cho UI
7. **String pool nhỏ:** 8 slots → fragmentation khi burst
8. **Resource leaks:** Không cleanup khi init fail

---

## 2. VẤN ĐỀ VÀ RỦI RO

### 2.1 P0 Risks (từ RISKS_P0_P1.md)

#### P0-01: Router gọi `on_hide()` 2 lần
- **Vấn đề:** Double cleanup → crash hiếm
- **Root cause:** `ui_router_navigate_to()` gọi `on_hide()` ở 2 chỗ

#### P0-02: LVGL lock discipline không nhất quán
- **Vấn đề:** Deadlock hoặc crash ngẫu nhiên
- **Root cause:** Router lock, screen callbacks lock, UI task lock → nested locks

#### P0-03: Event drop khi queue đầy
- **Vấn đề:** Mất events → UI/logic lệch
- **Root cause:** `xQueueSend(..., 0)` non-blocking

#### P0-04: Resource leak khi init fail
- **Vấn đề:** Leak SPI bus/IO/LEDC khi display init fail
- **Root cause:** Không cleanup trên fail path

#### P0-05: Double-handle event trong orchestrator
- **Vấn đề:** Logic khó hiểu, có thể enable/disable lặp
- **Root cause:** Xử lý event type bị trùng nhánh

#### P0-06: String pool size nhỏ
- **Vấn đề:** Pool đầy → `strdup` nhiều → fragmentation
- **Root cause:** `SX_EVENT_STRING_POOL_SIZE 8`

### 2.2 P1 Risks

#### P1-01: State thiếu fields
- **Vấn đề:** UI không có đủ thông tin để render
- **Root cause:** State chỉ có `status_text`, thiếu chat/STT/TTS text

#### P1-02: JSON parser duplicate code
- **Vấn đề:** Sửa logic phải sửa 2 nơi
- **Root cause:** WS và MQTT parse JSON tương tự

#### P1-03: Audio hot path malloc
- **Vấn đề:** Jitter audio, fragmentation
- **Root cause:** `feed_pcm()` malloc mỗi call

#### P1-04: Pinmap hardcode
- **Vấn đề:** Đổi board phải sửa code
- **Root cause:** Macro `#define LCD_PIN...` trong code

---

## 3. ĐỀ XUẤT CẢI THIỆN

### 3.1 Nguyên tắc thiết kế

**QUAN TRỌNG:** Tất cả cải thiện phải:
- ✅ Tuân thủ SIMPLEXL_ARCH (không phá nguyên tắc cốt lõi)
- ✅ Giữ component boundaries
- ✅ Không thay đổi event-driven pattern
- ✅ Cải thiện stability và performance

### 3.2 Đề xuất 1: Event Handler Registry Pattern

**Vấn đề:** Orchestrator quá lớn, xử lý tất cả events trong 1 loop lớn

**Giải pháp:** Tách event handlers thành các functions riêng, đăng ký trong registry

**File mới:** `components/sx_core/include/sx_event_handler.h`

```c
#pragma once

#include "sx_event.h"
#include "sx_state.h"

#ifdef __cplusplus
extern "C" {
#endif

// Event handler function type
// Returns true if event was handled, false otherwise
typedef bool (*sx_event_handler_t)(const sx_event_t *evt, sx_state_t *state);

// Register event handler for specific event type
esp_err_t sx_event_handler_register(sx_event_type_t event_type, sx_event_handler_t handler);

// Unregister event handler
esp_err_t sx_event_handler_unregister(sx_event_type_t event_type);

// Process event using registered handlers
bool sx_event_handler_process(const sx_event_t *evt, sx_state_t *state);

#ifdef __cplusplus
}
#endif
```

**File mới:** `components/sx_core/sx_event_handler.c`

```c
#include "sx_event_handler.h"
#include <esp_log.h>
#include <string.h>

#define MAX_HANDLERS 32

static sx_event_handler_t s_handlers[SX_EVT_MAX] = {0};
static const char *TAG = "sx_event_handler";

esp_err_t sx_event_handler_register(sx_event_type_t event_type, sx_event_handler_t handler) {
    if (event_type >= SX_EVT_MAX || handler == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_handlers[event_type] = handler;
    ESP_LOGI(TAG, "Registered handler for event type %d", event_type);
    return ESP_OK;
}

bool sx_event_handler_process(const sx_event_t *evt, sx_state_t *state) {
    if (evt == NULL || state == NULL) {
        return false;
    }
    
    if (evt->type >= SX_EVT_MAX) {
        ESP_LOGW(TAG, "Invalid event type: %d", evt->type);
        return false;
    }
    
    sx_event_handler_t handler = s_handlers[evt->type];
    if (handler == NULL) {
        ESP_LOGD(TAG, "No handler registered for event type %d", evt->type);
        return false;
    }
    
    return handler(evt, state);
}
```

**Refactor orchestrator:**

```c
// sx_orchestrator.c
#include "sx_event_handler.h"
#include "sx_event_handlers/ui_input_handler.h"
#include "sx_event_handlers/chatbot_handler.h"
#include "sx_event_handlers/audio_handler.h"
// ... other handlers

static void sx_orchestrator_task(void *arg) {
    // ... initialization ...
    
    // Register all event handlers
    sx_event_handler_register(SX_EVT_UI_INPUT, sx_event_handler_ui_input);
    sx_event_handler_register(SX_EVT_CHATBOT_STT, sx_event_handler_chatbot_stt);
    sx_event_handler_register(SX_EVT_CHATBOT_TTS_SENTENCE, sx_event_handler_chatbot_tts_sentence);
    // ... register all handlers ...
    
    for (;;) {
        sx_event_t evt;
        while (sx_dispatcher_poll_event(&evt)) {
            sx_dispatcher_get_state(&st);
            st.seq++;
            
            // Process event using registry
            if (sx_event_handler_process(&evt, &st)) {
                sx_dispatcher_set_state(&st);
            }
        }
        
        vTaskDelayUntil(&last_wake_time, poll_interval);
    }
}
```

**Lợi ích:**
- ✅ Orchestrator gọn hơn, dễ đọc
- ✅ Event handlers tách biệt, dễ test
- ✅ Dễ thêm handlers mới
- ✅ Tuân thủ SIMPLEXL_ARCH

**Files impacted:**
- `components/sx_core/sx_event_handler.[ch]` (mới)
- `components/sx_core/sx_event_handlers/*.c` (mới, tách từ orchestrator)
- `components/sx_core/sx_orchestrator.c` (refactor)

### 3.3 Đề xuất 2: Event Priority System

**Vấn đề:** Tất cả events đều bình đẳng, critical events có thể bị delay

**Giải pháp:** Thêm priority cho events, xử lý high-priority trước

**File:** `components/sx_core/include/sx_event.h`

```c
// Event priority (higher = more important)
typedef enum {
    SX_EVT_PRIORITY_LOW = 0,
    SX_EVT_PRIORITY_NORMAL = 1,
    SX_EVT_PRIORITY_HIGH = 2,
    SX_EVT_PRIORITY_CRITICAL = 3,
} sx_event_priority_t;

typedef struct {
    sx_event_type_t type;
    sx_event_priority_t priority;  // NEW
    uint32_t arg0;
    uint32_t arg1;
    const void *ptr;
} sx_event_t;
```

**File:** `components/sx_core/sx_dispatcher.c`

```c
// Priority queue: 4 queues (one per priority level)
static QueueHandle_t s_evt_q_low;
static QueueHandle_t s_evt_q_normal;
static QueueHandle_t s_evt_q_high;
static QueueHandle_t s_evt_q_critical;

bool sx_dispatcher_post_event(const sx_event_t *evt) {
    if (!evt || s_evt_q == NULL) {
        return false;
    }
    
    // Select queue based on priority
    QueueHandle_t target_q = s_evt_q_normal;  // Default
    switch (evt->priority) {
        case SX_EVT_PRIORITY_CRITICAL:
            target_q = s_evt_q_critical;
            break;
        case SX_EVT_PRIORITY_HIGH:
            target_q = s_evt_q_high;
            break;
        case SX_EVT_PRIORITY_NORMAL:
            target_q = s_evt_q_normal;
            break;
        case SX_EVT_PRIORITY_LOW:
            target_q = s_evt_q_low;
            break;
    }
    
    // Critical events: block with timeout (10ms)
    TickType_t timeout = (evt->priority >= SX_EVT_PRIORITY_HIGH) ? 
                         pdMS_TO_TICKS(10) : 0;
    
    if (xQueueSend(target_q, evt, timeout) == pdTRUE) {
        return true;
    }
    
    // Log drop (rate-limited)
    // ... existing drop logging ...
    return false;
}

bool sx_dispatcher_poll_event(sx_event_t *out_evt) {
    // Poll in priority order: critical → high → normal → low
    if (xQueueReceive(s_evt_q_critical, out_evt, 0) == pdTRUE) return true;
    if (xQueueReceive(s_evt_q_high, out_evt, 0) == pdTRUE) return true;
    if (xQueueReceive(s_evt_q_normal, out_evt, 0) == pdTRUE) return true;
    if (xQueueReceive(s_evt_q_low, out_evt, 0) == pdTRUE) return true;
    return false;
}
```

**Lợi ích:**
- ✅ Critical events được xử lý trước
- ✅ Giảm latency cho important events
- ✅ Vẫn non-blocking cho low-priority events

**Files impacted:**
- `components/sx_core/include/sx_event.h`
- `components/sx_core/sx_dispatcher.c`
- Tất cả nơi post events (thêm priority)

### 3.4 Đề xuất 3: LVGL Lock Wrapper với RAII Pattern

**Vấn đề:** LVGL lock discipline không nhất quán, nested locks

**Giải pháp:** Tạo wrapper với RAII pattern (C-style)

**File mới:** `components/sx_ui/include/sx_lvgl_lock.h`

```c
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// LVGL lock guard (RAII-style for C)
typedef struct {
    bool locked;
} sx_lvgl_lock_guard_t;

// Acquire lock (returns guard, check .locked)
sx_lvgl_lock_guard_t sx_lvgl_lock_acquire(void);

// Release lock (must match acquire)
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

**File:** `components/sx_ui/sx_lvgl_lock.c`

```c
#include "sx_lvgl_lock.h"
#include "esp_lvgl_port.h"
#include <esp_log.h>

static const char *TAG = "sx_lvgl_lock";
static bool s_lock_acquired = false;  // Track lock state for debugging

sx_lvgl_lock_guard_t sx_lvgl_lock_acquire(void) {
    sx_lvgl_lock_guard_t guard = {.locked = false};
    
    // Prevent nested locks
    if (s_lock_acquired) {
        ESP_LOGE(TAG, "Nested LVGL lock detected! This may cause deadlock.");
        return guard;
    }
    
    guard.locked = lvgl_port_lock(0);
    if (guard.locked) {
        s_lock_acquired = true;
    }
    
    return guard;
}

void sx_lvgl_lock_release(sx_lvgl_lock_guard_t *guard) {
    if (guard == NULL || !guard->locked) {
        return;
    }
    
    lvgl_port_unlock();
    s_lock_acquired = false;
    guard->locked = false;
}
```

**Usage:**

```c
// Before (error-prone):
if (lvgl_port_lock(0)) {
    // ... LVGL calls ...
    lvgl_port_unlock();
}

// After (safe):
SX_LVGL_LOCK() {
    // ... LVGL calls ...
    // Automatically unlocked when block exits
}
```

**Lợi ích:**
- ✅ Prevent nested locks
- ✅ Automatic unlock (không quên unlock)
- ✅ Debug-friendly (log nested lock attempts)

**Files impacted:**
- `components/sx_ui/include/sx_lvgl_lock.h` (mới)
- `components/sx_ui/sx_lvgl_lock.c` (mới)
- Tất cả nơi dùng `lvgl_port_lock()` (refactor)

### 3.5 Đề xuất 4: Router Lifecycle Fix

**Vấn đề:** Router gọi `on_hide()` 2 lần, lifecycle không rõ ràng

**Giải pháp:** Chuẩn hóa lifecycle, chỉ gọi callbacks đúng 1 lần

**File:** `components/sx_ui/ui_router.c`

```c
void ui_router_navigate_to(ui_screen_id_t screen_id) {
    // ... validation ...
    
    // Prevent duplicate navigation
    if (s_current_screen == screen_id) {
        return;
    }
    
    // FIXED: Only call on_hide once, before clearing container
    SX_LVGL_LOCK() {
        // Step 1: Hide old screen (if exists)
        if (s_current_screen != SCREEN_ID_MAX) {
            const ui_screen_callbacks_t *old_callbacks = ui_screen_registry_get(s_current_screen);
            if (old_callbacks && old_callbacks->on_hide) {
                old_callbacks->on_hide();  // Called once, inside lock
            }
        }
        
        // Step 2: Clear container
        if (s_screen_container != NULL) {
            lv_obj_clean(s_screen_container);
        }
        
        // Step 3: Destroy old screen
        if (s_current_screen != SCREEN_ID_MAX) {
            const ui_screen_callbacks_t *old_callbacks = ui_screen_registry_get(s_current_screen);
            if (old_callbacks && old_callbacks->on_destroy) {
                old_callbacks->on_destroy();
            }
        }
        
        // Step 4: Create new screen
        const ui_screen_callbacks_t *callbacks = ui_screen_registry_get(screen_id);
        if (callbacks && callbacks->on_create) {
            callbacks->on_create();
        }
        
        // Step 5: Update current screen
        s_current_screen = screen_id;
        
        // Step 6: Show new screen
        if (callbacks && callbacks->on_show) {
            callbacks->on_show();  // Called once, after create
        }
    }
}
```

**Lợi ích:**
- ✅ Lifecycle rõ ràng: hide → destroy → create → show
- ✅ Không duplicate callbacks
- ✅ Thread-safe với LVGL lock

**Files impacted:**
- `components/sx_ui/ui_router.c`

### 3.6 Đề xuất 5: Mở rộng State với đầy đủ thông tin

**Vấn đề:** State thiếu fields, UI không có đủ thông tin

**Giải pháp:** Mở rộng state với tất cả thông tin cần thiết

**File:** `components/sx_core/include/sx_state.h`

```c
typedef struct {
    sx_device_state_t device_state;
    const char *status_text;
    const char *emotion_id;
    
    // Chat messages (existing)
    char last_user_message[SX_UI_MESSAGE_MAX_LEN];
    char last_assistant_message[SX_UI_MESSAGE_MAX_LEN];
    
    // NEW: Chatbot state
    bool chatbot_connected;
    bool audio_channel_opened;
    uint32_t server_sample_rate;
    uint32_t server_frame_duration;
    char session_id[64];
    
    // NEW: Error state
    bool has_error;
    char error_message[128];
    uint32_t error_code;
    
    // NEW: Alert state
    bool has_alert;
    char alert_status[64];
    char alert_message[256];
    char alert_emotion[32];
    
    // NEW: Audio state (detailed)
    bool audio_playing;
    bool audio_recording;
    uint8_t volume;
    uint8_t volume_muted;
    
    // NEW: WiFi state (detailed)
    bool wifi_connected;
    int8_t wifi_rssi;
    char wifi_ssid[32];
} sx_ui_state_t;
```

**Lợi ích:**
- ✅ UI có đủ thông tin để render
- ✅ Không cần direct calls đến services
- ✅ State là single source of truth

**Files impacted:**
- `components/sx_core/include/sx_state.h`
- `components/sx_core/sx_orchestrator.c` (update state)
- UI screens (read từ state)

### 3.7 Đề xuất 6: Tăng String Pool Size và Cải thiện Management

**Vấn đề:** String pool nhỏ (8 slots) → fragmentation

**Giải pháp:** Tăng pool size, thêm metrics, cải thiện allocation strategy

**File:** `components/sx_core/include/sx_event_string_pool.h`

```c
// Increased from 8 to 32
#define SX_EVENT_STRING_POOL_SIZE 32

// Pool metrics
typedef struct {
    uint32_t total_allocations;
    uint32_t pool_hits;
    uint32_t pool_misses;
    uint32_t malloc_fallbacks;
    uint32_t current_usage;
} sx_event_string_pool_metrics_t;

// Get pool metrics
void sx_event_string_pool_get_metrics(sx_event_string_pool_metrics_t *metrics);
```

**File:** `components/sx_core/sx_event_string_pool.c`

```c
// Use ring buffer instead of simple array
static char s_pool_strings[SX_EVENT_STRING_POOL_SIZE][SX_EVENT_STRING_MAX_LEN];
static bool s_pool_used[SX_EVENT_STRING_POOL_SIZE];
static uint32_t s_pool_index = 0;  // Ring buffer index

char *sx_event_alloc_string(const char *src) {
    if (src == NULL) {
        return NULL;
    }
    
    size_t len = strlen(src);
    if (len >= SX_EVENT_STRING_MAX_LEN) {
        len = SX_EVENT_STRING_MAX_LEN - 1;
    }
    
    // Try to find free slot (start from current index for better cache locality)
    for (uint32_t i = 0; i < SX_EVENT_STRING_POOL_SIZE; i++) {
        uint32_t idx = (s_pool_index + i) % SX_EVENT_STRING_POOL_SIZE;
        if (!s_pool_used[idx]) {
            memcpy(s_pool_strings[idx], src, len);
            s_pool_strings[idx][len] = '\0';
            s_pool_used[idx] = true;
            s_pool_index = (idx + 1) % SX_EVENT_STRING_POOL_SIZE;
            s_metrics.pool_hits++;
            return s_pool_strings[idx];
        }
    }
    
    // Pool full - fallback to malloc
    s_metrics.pool_misses++;
    s_metrics.malloc_fallbacks++;
    return strdup(src);
}
```

**Lợi ích:**
- ✅ Giảm fragmentation
- ✅ Better cache locality (ring buffer)
- ✅ Metrics để monitor

**Files impacted:**
- `components/sx_core/include/sx_event_string_pool.h`
- `components/sx_core/sx_event_string_pool.c`

### 3.8 Đề xuất 7: Resource Cleanup trên Fail Path

**Vấn đề:** Resource leak khi init fail

**Giải pháp:** Thêm cleanup functions, dùng goto cleanup pattern

**File:** `components/sx_platform/sx_platform.c`

```c
esp_err_t sx_platform_display_init(sx_display_handles_t *handles) {
    esp_err_t ret = ESP_OK;
    spi_bus_config_t bus_cfg = {0};
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_handle_t panel_handle = NULL;
    ledc_channel_handle_t ledc_channel = NULL;
    
    // ... init bus_cfg ...
    
    ret = spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed");
        goto cleanup;
    }
    
    ret = esp_lcd_new_panel_io_spi(/* ... */, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_io_spi failed");
        goto cleanup_bus;
    }
    
    ret = esp_lcd_new_panel_st7796(/* ... */, &panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_st7796 failed");
        goto cleanup_io;
    }
    
    // ... init LEDC ...
    
    handles->io_handle = io_handle;
    handles->panel_handle = panel_handle;
    handles->ledc_channel = ledc_channel;
    return ESP_OK;

cleanup_io:
    if (io_handle != NULL) {
        esp_lcd_panel_io_del(io_handle);
    }
cleanup_bus:
    spi_bus_free(LCD_SPI_HOST);
cleanup:
    return ret;
}
```

**Lợi ích:**
- ✅ No resource leaks
- ✅ Clean error handling
- ✅ Easy to maintain

**Files impacted:**
- `components/sx_platform/sx_platform.c`
- Các init functions khác

### 3.9 Đề xuất 8: JSON Parser Abstraction

**Vấn đề:** Duplicate code giữa WS và MQTT

**Giải pháp:** Tạo shared JSON parser

**File mới:** `components/sx_protocol/include/sx_protocol_msg_parser.h`

```c
#pragma once

#include <cJSON.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Parse protocol message and emit events
// Returns true if message was handled
bool sx_protocol_parse_message(cJSON *root, const char *raw_fallback);

// Parse hello message and extract audio params
typedef struct {
    uint32_t sample_rate;
    uint32_t frame_duration;
    char session_id[64];
    bool has_udp;
    char udp_server[128];
    int udp_port;
    char udp_key[64];
    char udp_nonce[64];
} sx_protocol_hello_data_t;

bool sx_protocol_parse_hello(cJSON *root, sx_protocol_hello_data_t *out);

#ifdef __cplusplus
}
#endif
```

**File:** `components/sx_protocol/sx_protocol_msg_parser.c`

```c
#include "sx_protocol_msg_parser.h"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_chatbot_service.h"
#include <esp_log.h>
#include <string.h>

static const char *TAG = "sx_protocol_parser";

bool sx_protocol_parse_message(cJSON *root, const char *raw_fallback) {
    if (root == NULL) {
        return false;
    }
    
    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(type)) {
        // Try MCP handler
        if (raw_fallback) {
            sx_chatbot_handle_mcp_message(raw_fallback);
        }
        return false;
    }
    
    const char *msg_type = type->valuestring;
    
    // Route to appropriate handler
    if (strcmp(msg_type, "stt") == 0) {
        // ... handle STT ...
        return true;
    } else if (strcmp(msg_type, "tts") == 0) {
        // ... handle TTS ...
        return true;
    } else if (strcmp(msg_type, "llm") == 0) {
        // ... handle LLM ...
        return true;
    } else if (strcmp(msg_type, "mcp") == 0) {
        // ... handle MCP ...
        return true;
    } else if (strcmp(msg_type, "hello") == 0) {
        // ... handle hello ...
        return true;
    } else if (strcmp(msg_type, "system") == 0) {
        // ... handle system ...
        return true;
    } else if (strcmp(msg_type, "alert") == 0) {
        // ... handle alert ...
        return true;
    }
    
    return false;
}
```

**Lợi ích:**
- ✅ No duplicate code
- ✅ Single source of truth
- ✅ Dễ thêm message types mới

**Files impacted:**
- `components/sx_protocol/sx_protocol_msg_parser.[ch]` (mới)
- `components/sx_protocol/sx_protocol_ws.c` (refactor)
- `components/sx_protocol/sx_protocol_mqtt.c` (refactor)

### 3.10 Đề xuất 9: Audio Buffer Pool

**Vấn đề:** Malloc trong hot path → jitter

**Giải pháp:** Pre-allocated buffer pool

**File mới:** `components/sx_services/include/sx_audio_buffer_pool.h`

```c
#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Audio buffer pool for hot path
typedef struct {
    int16_t *data;
    size_t size;
    bool in_use;
} sx_audio_buffer_t;

// Allocate buffer from pool
sx_audio_buffer_t *sx_audio_buffer_pool_alloc(size_t sample_count);

// Free buffer back to pool
void sx_audio_buffer_pool_free(sx_audio_buffer_t *buf);

// Initialize pool
esp_err_t sx_audio_buffer_pool_init(void);

#ifdef __cplusplus
}
#endif
```

**Lợi ích:**
- ✅ No malloc in hot path
- ✅ Predictable performance
- ✅ Lower latency

**Files impacted:**
- `components/sx_services/sx_audio_buffer_pool.[ch]` (mới)
- `components/sx_services/sx_audio_service.c` (refactor)

### 3.11 Đề xuất 10: Board Configuration System

**Vấn đề:** Pinmap hardcode

**Giải pháp:** Kconfig + board profiles

**File mới:** `components/sx_platform/include/sx_board_config.h`

```c
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // LCD pins
    int lcd_mosi;
    int lcd_sclk;
    int lcd_cs;
    int lcd_dc;
    int lcd_rst;
    int lcd_backlight;
    
    // Touch pins
    int touch_sda;
    int touch_scl;
    int touch_int;
    
    // Audio pins
    int audio_i2s_bclk;
    int audio_i2s_ws;
    int audio_i2s_dout;
    int audio_i2s_din;
    
    // SD pins
    int sd_mosi;
    int sd_miso;
    int sd_sclk;
    int sd_cs;
} sx_board_config_t;

// Get board configuration
const sx_board_config_t *sx_board_config_get(void);

#ifdef __cplusplus
}
#endif
```

**File:** `components/sx_platform/sx_board_config.c`

```c
#include "sx_board_config.h"
#include "sdkconfig.h"

static const sx_board_config_t s_board_config = {
    .lcd_mosi = CONFIG_LCD_MOSI_PIN,
    .lcd_sclk = CONFIG_LCD_SCLK_PIN,
    // ... from Kconfig ...
};

const sx_board_config_t *sx_board_config_get(void) {
    return &s_board_config;
}
```

**Lợi ích:**
- ✅ No hardcode
- ✅ Easy to port
- ✅ Configurable via menuconfig

**Files impacted:**
- `components/sx_platform/sx_board_config.[ch]` (mới)
- `components/sx_platform/sx_platform.c` (refactor)
- `Kconfig` (thêm config options)

---

## 4. IMPLEMENTATION PLAN

### 4.1 Phase 1: Critical Fixes (P0) - 1 tuần

**Priority:** Fix các vấn đề P0 trước

1. **Router Lifecycle Fix** (1 ngày)
   - Fix double `on_hide()` calls
   - Chuẩn hóa lifecycle

2. **LVGL Lock Wrapper** (1 ngày)
   - Implement `sx_lvgl_lock.h/c`
   - Refactor tất cả nơi dùng lock

3. **Event Drop Fix** (1 ngày)
   - Thêm priority system
   - Critical events block với timeout

4. **Resource Cleanup** (1 ngày)
   - Fix cleanup trên fail path
   - Test với init failures

5. **String Pool Increase** (0.5 ngày)
   - Tăng pool size
   - Thêm metrics

6. **Double-handle Event Fix** (0.5 ngày)
   - Refactor orchestrator
   - Remove duplicate handling

### 4.2 Phase 2: Architecture Improvements - 1 tuần

**Priority:** Cải thiện kiến trúc

1. **Event Handler Registry** (2 ngày)
   - Implement registry pattern
   - Tách handlers từ orchestrator
   - Test từng handler

2. **State Expansion** (1 ngày)
   - Mở rộng state
   - Update orchestrator
   - Update UI screens

3. **JSON Parser Abstraction** (1 ngày)
   - Implement shared parser
   - Refactor WS/MQTT

4. **Audio Buffer Pool** (1 ngày)
   - Implement buffer pool
   - Refactor audio service

### 4.3 Phase 3: Configuration & Polish - 3 ngày

1. **Board Configuration** (1 ngày)
   - Implement board config system
   - Add Kconfig options

2. **Testing & Documentation** (2 ngày)
   - Test tất cả improvements
   - Update documentation
   - Performance benchmarks

---

## 5. KẾT LUẬN

### 5.1 Tổng kết đề xuất

**10 đề xuất cải thiện:**
1. ✅ Event Handler Registry Pattern
2. ✅ Event Priority System
3. ✅ LVGL Lock Wrapper
4. ✅ Router Lifecycle Fix
5. ✅ State Expansion
6. ✅ String Pool Improvement
7. ✅ Resource Cleanup
8. ✅ JSON Parser Abstraction
9. ✅ Audio Buffer Pool
10. ✅ Board Configuration System

**Tất cả đều tuân thủ SIMPLEXL_ARCH:**
- ✅ Không phá component boundaries
- ✅ Giữ event-driven pattern
- ✅ State snapshot pattern
- ✅ LVGL ownership rules

### 5.2 Lợi ích

**Stability:**
- ✅ Fix tất cả P0 risks
- ✅ Prevent deadlocks và crashes
- ✅ No resource leaks

**Performance:**
- ✅ Lower latency (priority system)
- ✅ No malloc in hot path (buffer pool)
- ✅ Better cache locality (ring buffer)

**Maintainability:**
- ✅ Modular event handlers
- ✅ No duplicate code
- ✅ Easy to test

**Portability:**
- ✅ Board configuration system
- ✅ No hardcode

### 5.3 Timeline

- **Phase 1 (P0 fixes):** 1 tuần
- **Phase 2 (Architecture):** 1 tuần
- **Phase 3 (Polish):** 3 ngày
- **Tổng cộng:** ~2.5 tuần

### 5.4 Khuyến nghị

**Ưu tiên:**
1. **Phase 1 trước** - Fix critical issues
2. **Phase 2 sau** - Cải thiện architecture
3. **Phase 3 cuối** - Polish và testing

**Sau khi implement:**
- Kiến trúc SIMPLEXL sẽ ổn định hơn
- Performance tốt hơn
- Dễ maintain và mở rộng hơn
- Vẫn giữ nguyên nguyên tắc cốt lõi

---

*Báo cáo này đảm bảo tất cả cải thiện đều tuân thủ SIMPLEXL_ARCH, không phá vỡ kiến trúc hiện tại, và cải thiện stability, performance, maintainability.*











