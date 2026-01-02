# ROADMAP CẢI THIỆN CHATBOT - TUÂN THỦ KIẾN TRÚC SIMPLEXL

> **Mục tiêu:** Cải thiện tính năng chatbot của hai-os-simplexl để đạt được độ ổn định và đầy đủ tính năng như xiaozhi-esp32_vietnam_ref, nhưng **TUÂN THỦ HOÀN TOÀN** kiến trúc SIMPLEXL hiện tại.

---

## 📋 MỤC LỤC

1. [Tổng quan tình hình hiện tại](#1-tổng-quan-tình-hình-hiện-tại)
2. [Nguyên tắc thiết kế bất biến](#2-nguyên-tắc-thiết-kế-bất-biến)
3. [Roadmap chi tiết theo giai đoạn](#3-roadmap-chi-tiết-theo-giai-đoạn)
4. [So sánh trước và sau cải thiện](#4-so-sánh-trước-và-sau-cải-thiện)
5. [Kế hoạch kiểm thử](#5-kế-hoạch-kiểm-thử)
6. [Timeline và ước tính](#6-timeline-và-ước-tính)

---

## 1. TỔNG QUAN TÌNH HÌNH HIỆN TẠI

### 1.1 Điểm mạnh của hai-os-simplexl

- ✅ **Kiến trúc event-driven rõ ràng:** Dispatcher/Orchestrator pattern chuẩn
- ✅ **Separation of concerns tốt:** Services, Core, UI tách biệt rõ ràng
- ✅ **State snapshot pattern:** Immutable, thread-safe với mutex
- ✅ **Modularity cao:** Dễ test, dễ maintain
- ✅ **Documentation tốt:** SIMPLEXL_ARCH.md chi tiết

### 1.2 Những gì còn thiếu so với xiaozhi

**Theo phân tích từ 3 báo cáo:**

#### 🔴 Thiếu sót nghiêm trọng (P0):
1. **System commands** - Không xử lý `"type": "system"` với command "reboot"
2. **Alert messages** - Không xử lý `"type": "alert"` với status, message, emotion
3. **Error handling tập trung** - Thiếu SetError(), OnNetworkError(), IsTimeout()
4. **Hello handshake đầy đủ** - Thiếu server hello với timeout và audio params negotiation
5. **Protocol abstraction** - Không có base class, code duplicate giữa WS/MQTT

#### 🟡 Thiếu sót quan trọng (P1):
6. **Power management tự động** - Không disable power save khi audio channel mở
7. **Sample rate validation** - Không warning khi sample rate mismatch
8. **State cleanup** - Không cleanup state khi audio channel đóng
9. **Reconnection logic** - Không có auto reconnect khi disconnect
10. **Session ID management** - Không parse và lưu session_id từ server

#### 🟢 Thiếu sót nhỏ (P2):
11. **MCP server init sớm** - Init muộn trong chatbot service, không phải bootstrap
12. **State-based audio routing** - Không check state trước khi nhận audio
13. **Error handling khi send fail** - Không break loop khi send fail

### 1.3 Lý do xiaozhi ổn định hơn

1. **Protocol abstraction tốt:** Base class giúp code clean, dễ maintain
2. **Error handling đầy đủ:** Có timeout, reconnection, error recovery
3. **State management rõ ràng:** Direct state update (nhưng phá kiến trúc)
4. **Hello handshake:** Đảm bảo server sẵn sàng trước khi dùng
5. **Power management:** Tự động disable power save khi cần
6. **System commands:** Cho phép server điều khiển device

---

## 2. NGUYÊN TẮC THIẾT KẾ BẤT BIẾN

### 2.1 Quy tắc SIMPLEXL_ARCH (KHÔNG ĐƯỢC PHÁ VỠ)

Từ `docs/SIMPLEXL_ARCH.md`:

1. ✅ **Single UI owner task:** Chỉ UI task được gọi LVGL APIs
2. ✅ **Services không include UI headers:** Services không phụ thuộc UI
3. ✅ **UI ↔ services communication:** Chỉ qua **events** và **state snapshots**
4. ✅ **Event queue:** Multi-producer (UI + services), single-consumer (orchestrator)
5. ✅ **State snapshot:** Single-writer (orchestrator), multi-reader (UI + services)
6. ✅ **Orchestrator:** Single source of truth cho state

### 2.2 Component Boundaries (KHÔNG ĐƯỢC VI PHẠM)

```
sx_core:
  - Owns: sx_event, sx_state, sx_dispatcher, sx_orchestrator
  - Single writer cho sx_state_t
  - Consumes events từ queue

sx_ui:
  - Owns: UI task (LVGL)
  - Reads sx_state_t snapshots
  - Emits SX_EVT_UI_INPUT events only
  - Forbidden: include service headers

sx_services:
  - Owns: audio/wifi/ir/mcp/chatbot
  - Emits events
  - Exposes APIs only to sx_core (not UI)
  - Forbidden: include sx_ui/*

sx_protocol:
  - Owns: WebSocket/MQTT protocols
  - Emits events
  - Forbidden: include sx_ui/*
```

### 2.3 Event Flow Pattern (BẮT BUỘC)

```
Services/UI → sx_dispatcher_post_event() → Queue
Orchestrator → sx_dispatcher_poll_event() → Process → Update State
UI/Services → sx_dispatcher_get_state() → Read snapshot
```

**QUAN TRỌNG:** Tất cả cải thiện phải tuân thủ pattern này, KHÔNG được:
- ❌ Services gọi UI trực tiếp
- ❌ Protocol có direct callbacks đến Application
- ❌ UI gọi services trực tiếp
- ❌ State update ngoài orchestrator

---

## 3. ROADMAP CHI TIẾT THEO GIAI ĐOẠN

### 🚀 PHASE 1: Mở rộng Events và State (P0 - Ưu tiên cao nhất)

**Mục tiêu:** Thêm các events và state fields cần thiết để hỗ trợ tính năng mới

**Thời gian ước tính:** 4-6 giờ

#### Task 1.1: Thêm Events mới cho System Commands và Alerts

**File:** `components/sx_core/include/sx_event.h`

**Thêm events:**
```c
// System commands
SX_EVT_SYSTEM_REBOOT,           // System reboot command
SX_EVT_SYSTEM_COMMAND,          // Generic system command (ptr: command string)

// Alert messages
SX_EVT_ALERT,                   // Alert message (ptr: alert_data_t*)

// Protocol errors và handshake
SX_EVT_PROTOCOL_ERROR,          // Protocol error (ptr: error message string)
SX_EVT_PROTOCOL_TIMEOUT,        // Protocol timeout
SX_EVT_PROTOCOL_HELLO_SENT,     // Hello message sent
SX_EVT_PROTOCOL_HELLO_RECEIVED, // Server hello received (ptr: hello_data_t*)
SX_EVT_PROTOCOL_HELLO_TIMEOUT,  // Server hello timeout
SX_EVT_PROTOCOL_RECONNECTING,   // Reconnection in progress (arg0: attempt number)
SX_EVT_CHATBOT_AUDIO_CHANNEL_CLOSED, // Audio channel closed (hiện tại chỉ có OPENED)
```

**Lý do:** Tuân thủ event-driven pattern, không phá kiến trúc.

**Kiểm tra:**
- ✅ Events được định nghĩa trong enum
- ✅ Priority được set đúng (ERROR/ALERT = CRITICAL)
- ✅ Documentation cho mỗi event

#### Task 1.2: Mở rộng State để lưu thông tin chatbot

**File:** `components/sx_core/include/sx_state.h`

**Kiểm tra state hiện tại:**
- ✅ Đã có: `chatbot_connected`, `audio_channel_opened`, `server_sample_rate`, `server_frame_duration`, `session_id`
- ✅ Đã có: `has_error`, `error_message`, `error_code`
- ✅ Đã có: `has_alert`, `alert_status`, `alert_message`, `alert_emotion`

**Kết luận:** State đã đủ, không cần thêm fields mới.

**Lý do:** State snapshot pattern đã được thiết kế tốt từ trước.

#### Task 1.3: Cập nhật Orchestrator để xử lý events mới

**File:** `components/sx_core/sx_orchestrator.c`

**Thêm xử lý:**
```c
// System commands
if (evt.type == SX_EVT_SYSTEM_REBOOT) {
    ESP_LOGI(TAG, "System reboot requested");
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay để cleanup
    esp_restart();
}

// Alert messages
if (evt.type == SX_EVT_ALERT) {
    alert_data_t *alert = (alert_data_t *)evt.ptr;
    sx_dispatcher_get_state(&st);
    st.seq++;
    st.ui.has_alert = true;
    strncpy(st.ui.alert_status, alert->status, sizeof(st.ui.alert_status) - 1);
    strncpy(st.ui.alert_message, alert->message, sizeof(st.ui.alert_message) - 1);
    strncpy(st.ui.alert_emotion, alert->emotion, sizeof(st.ui.alert_emotion) - 1);
    sx_dispatcher_set_state(&st);
    free(alert);
}

// Protocol errors
if (evt.type == SX_EVT_PROTOCOL_ERROR) {
    const char *error_msg = (const char *)evt.ptr;
    sx_dispatcher_get_state(&st);
    st.seq++;
    st.ui.has_error = true;
    strncpy(st.ui.error_message, error_msg, sizeof(st.ui.error_message) - 1);
    sx_dispatcher_set_state(&st);
    sx_event_free_string((char *)evt.ptr);
}

// Protocol timeout
if (evt.type == SX_EVT_PROTOCOL_TIMEOUT) {
    sx_dispatcher_get_state(&st);
    st.seq++;
    st.ui.has_error = true;
    strncpy(st.ui.error_message, "Connection timeout", sizeof(st.ui.error_message) - 1);
    sx_dispatcher_set_state(&st);
}

// Hello received
if (evt.type == SX_EVT_PROTOCOL_HELLO_RECEIVED) {
    hello_data_t *hello = (hello_data_t *)evt.ptr;
    // Validation và update state
    // (xem Task 3.2)
    free(hello);
}

// Audio channel closed
if (evt.type == SX_EVT_CHATBOT_AUDIO_CHANNEL_CLOSED) {
    sx_dispatcher_get_state(&st);
    st.seq++;
    st.ui.audio_channel_opened = false;
    st.ui.has_alert = false;
    st.ui.alert_status[0] = '\0';
    st.ui.alert_message[0] = '\0';
    st.ui.alert_emotion[0] = '\0';
    sx_dispatcher_set_state(&st);
    // Power management (xem Task 4.2)
}
```

**Lý do:** Orchestrator là single source of truth, phải xử lý tất cả events.

**Kiểm tra:**
- ✅ Tất cả events mới được xử lý
- ✅ State update đúng pattern (get → modify → set)
- ✅ Memory cleanup đúng (free ptr nếu cần)

---

### 🔧 PHASE 2: Protocol Abstraction Layer (P0)

**Mục tiêu:** Tạo protocol abstraction trong sx_protocol để giảm code duplication, nhưng KHÔNG phá SIMPLEXL_ARCH

**Thời gian ước tính:** 6-8 giờ

#### Task 2.1: Tạo Protocol Base Interface (C-style)

**File mới:** `components/sx_protocol/include/sx_protocol_base.h`

**Thiết kế:**
```c
// Protocol base interface (C-style, không dùng C++ virtual)
typedef struct sx_protocol_base sx_protocol_base_t;

typedef struct {
    // Common protocol operations
    esp_err_t (*start)(sx_protocol_base_t *self, const void *config);
    esp_err_t (*stop)(sx_protocol_base_t *self);
    bool (*is_connected)(const sx_protocol_base_t *self);
    esp_err_t (*send_text)(sx_protocol_base_t *self, const char *text);
    esp_err_t (*send_audio)(sx_protocol_base_t *self, const sx_audio_stream_packet_t *packet);
    
    // Audio channel management
    esp_err_t (*open_audio_channel)(sx_protocol_base_t *self);
    void (*close_audio_channel)(sx_protocol_base_t *self);
    bool (*is_audio_channel_opened)(const sx_protocol_base_t *self);
    
    // Server info
    uint32_t (*get_server_sample_rate)(const sx_protocol_base_t *self);
    uint32_t (*get_server_frame_duration)(const sx_protocol_base_t *self);
    const char* (*get_session_id)(const sx_protocol_base_t *self);
    
    // Error handling
    bool (*is_timeout)(const sx_protocol_base_t *self);
    void (*set_error)(sx_protocol_base_t *self, const char *error_msg);
} sx_protocol_ops_t;

struct sx_protocol_base {
    const sx_protocol_ops_t *ops;
    void *impl;  // Pointer to concrete implementation (ws or mqtt)
};
```

**Lý do:** 
- ✅ Tạo abstraction TRONG sx_protocol (không phá boundary)
- ✅ Dùng function pointers (C-style, không cần C++)
- ✅ Vẫn emit events (tuân thủ SIMPLEXL)
- ✅ Giảm code duplication giữa WS và MQTT

#### Task 2.2: Implement Base Interface cho WebSocket

**File:** `components/sx_protocol/sx_protocol_ws.c`

**Thực hiện:**
1. Tạo struct `sx_protocol_ws_impl_t` chứa state hiện tại
2. Implement tất cả functions trong `sx_protocol_ops_t`
3. Tạo `sx_protocol_ws_get_base()` để trả về base interface
4. Đảm bảo tất cả operations vẫn emit events như cũ

**Lý do:** WebSocket protocol implement base interface, nhưng vẫn emit events.

#### Task 2.3: Implement Base Interface cho MQTT

**File:** `components/sx_protocol/sx_protocol_mqtt.c`

**Thực hiện:**
1. Tạo struct `sx_protocol_mqtt_impl_t` chứa state hiện tại
2. Implement tất cả functions trong `sx_protocol_ops_t`
3. Tạo `sx_protocol_mqtt_get_base()` để trả về base interface
4. Đảm bảo tất cả operations vẫn emit events như cũ

**Lý do:** MQTT protocol implement base interface, nhưng vẫn emit events.

#### Task 2.4: Refactor Chatbot Service để dùng Base Interface

**File:** `components/sx_services/sx_chatbot_service.c`

**Thực hiện:**
1. Thay vì gọi `sx_protocol_ws_*` hoặc `sx_protocol_mqtt_*` trực tiếp
2. Dùng `sx_protocol_base_t*` và gọi qua `ops->*`
3. Giảm code duplication trong chatbot service

**Lý do:** Chatbot service không cần biết chi tiết protocol, chỉ cần base interface.

**Kiểm tra:**
- ✅ Code duplication giảm
- ✅ Vẫn emit events đúng
- ✅ Không phá SIMPLEXL_ARCH

---

### 🤝 PHASE 3: Hello Handshake với Events (P0)

**Mục tiêu:** Implement hello handshake đầy đủ như xiaozhi, nhưng qua events

**Thời gian ước tính:** 4-6 giờ

#### Task 3.1: Thêm Hello Message Sending

**File:** `components/sx_protocol/sx_protocol_ws.c`, `sx_protocol_mqtt.c`

**Thực hiện:**
```c
// Trong open_audio_channel()
esp_err_t sx_protocol_ws_open_audio_channel(sx_protocol_base_t *base) {
    // Build hello message với device info
    char hello_msg[512];
    snprintf(hello_msg, sizeof(hello_msg),
        "{\"type\":\"hello\",\"device\":\"ESP32\",\"version\":\"1.0\",...}");
    
    // Send hello
    esp_err_t ret = ops->send_text(base, hello_msg);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Post event
    sx_event_t evt = {
        .type = SX_EVT_PROTOCOL_HELLO_SENT,
        .arg0 = 0,
        .ptr = NULL,
    };
    sx_dispatcher_post_event(&evt);
    
    // Start timeout timer
    // (xem Task 3.2)
    
    return ESP_OK;
}
```

**Lý do:** Hello message được send, event được emit, orchestrator xử lý timeout.

#### Task 3.2: Parse Server Hello và Emit Event

**File:** `components/sx_protocol/sx_protocol_ws.c`, `sx_protocol_mqtt.c`

**Thực hiện:**
```c
// Trong message handler
static void handle_server_hello(cJSON *root) {
    typedef struct {
        uint32_t server_sample_rate;
        uint32_t server_frame_duration;
        char session_id[64];
    } hello_data_t;
    
    hello_data_t *hello = (hello_data_t *)malloc(sizeof(hello_data_t));
    if (!hello) {
        return;
    }
    
    // Parse audio_params
    cJSON *audio_params = cJSON_GetObjectItem(root, "audio_params");
    if (audio_params) {
        cJSON *sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");
        cJSON *frame_duration = cJSON_GetObjectItem(audio_params, "frame_duration");
        if (cJSON_IsNumber(sample_rate)) {
            hello->server_sample_rate = sample_rate->valueint;
        }
        if (cJSON_IsNumber(frame_duration)) {
            hello->server_frame_duration = frame_duration->valueint;
        }
    }
    
    // Parse session_id
    cJSON *session_id = cJSON_GetObjectItem(root, "session_id");
    if (cJSON_IsString(session_id)) {
        strncpy(hello->session_id, session_id->valuestring, sizeof(hello->session_id) - 1);
    }
    
    // Emit event
    sx_event_t evt = {
        .type = SX_EVT_PROTOCOL_HELLO_RECEIVED,
        .arg0 = 0,
        .ptr = hello,
    };
    sx_dispatcher_post_event(&evt);
}
```

**Lý do:** Server hello được parse, event được emit với data, orchestrator xử lý.

#### Task 3.3: Timeout Handling trong Orchestrator

**File:** `components/sx_core/sx_orchestrator.c`

**Thực hiện:**
```c
// Trong orchestrator loop
if (evt.type == SX_EVT_PROTOCOL_HELLO_RECEIVED) {
    hello_data_t *hello = (hello_data_t *)evt.ptr;
    
    // Get current audio codec sample rate
    uint32_t codec_rate = sx_audio_get_sample_rate(); // Cần thêm API này
    uint32_t server_rate = hello->server_sample_rate;
    
    // Sample rate validation
    if (codec_rate != server_rate) {
        ESP_LOGW(TAG, "Sample rate mismatch: codec=%lu Hz, server=%lu Hz, resampling may cause distortion",
                 codec_rate, server_rate);
        sx_dispatcher_get_state(&st);
        st.seq++;
        st.ui.has_error = true;
        snprintf(st.ui.error_message, sizeof(st.ui.error_message),
                "Sample rate mismatch: %lu vs %lu Hz", codec_rate, server_rate);
        sx_dispatcher_set_state(&st);
    }
    
    // Update state với server params
    sx_dispatcher_get_state(&st);
    st.seq++;
    st.ui.server_sample_rate = hello->server_sample_rate;
    st.ui.server_frame_duration = hello->server_frame_duration;
    strncpy(st.ui.session_id, hello->session_id, sizeof(st.ui.session_id) - 1);
    st.ui.audio_channel_opened = true;
    sx_dispatcher_set_state(&st);
    
    free(hello);
}

// Timeout handling (từ protocol layer)
if (evt.type == SX_EVT_PROTOCOL_HELLO_TIMEOUT) {
    sx_dispatcher_get_state(&st);
    st.seq++;
    st.ui.has_error = true;
    strncpy(st.ui.error_message, "Server hello timeout", sizeof(st.ui.error_message) - 1);
    sx_dispatcher_set_state(&st);
}
```

**Lý do:** Orchestrator xử lý hello received, validation, và update state.

**Kiểm tra:**
- ✅ Hello message được send
- ✅ Server hello được parse và emit event
- ✅ Timeout được handle
- ✅ Sample rate validation hoạt động
- ✅ State được update đúng

---

### ⚠️ PHASE 4: Error Handling và Power Management (P0)

**Mục tiêu:** Thêm error handling tập trung và power management tự động

**Thời gian ước tính:** 5-7 giờ

#### Task 4.1: Error Detection trong Protocol Layer

**File:** `components/sx_protocol/sx_protocol_ws.c`, `sx_protocol_mqtt.c`

**Thực hiện:**
```c
// Timeout detection
static void check_timeout(void) {
    static uint32_t last_incoming_time = 0;
    uint32_t now = xTaskGetTickCount();
    
    if (last_incoming_time > 0 && (now - last_incoming_time) > pdMS_TO_TICKS(30000)) {
        // 30 seconds timeout
        sx_event_t evt = {
            .type = SX_EVT_PROTOCOL_TIMEOUT,
            .arg0 = 0,
            .ptr = NULL,
        };
        sx_dispatcher_post_event(&evt);
    }
}

// Network error handling
static void on_network_error(const char *error_msg) {
    sx_event_t evt = {
        .type = SX_EVT_PROTOCOL_ERROR,
        .arg0 = 0,
        .ptr = sx_event_alloc_string(error_msg),
    };
    sx_dispatcher_post_event(&evt);
}

// Update last_incoming_time khi nhận message
static void update_last_incoming_time(void) {
    last_incoming_time = xTaskGetTickCount();
}
```

**Lý do:** Error detection trong protocol layer, emit events, orchestrator xử lý.

#### Task 4.2: Power Management trong Orchestrator

**File:** `components/sx_platform/include/sx_platform.h` (thêm API)
**File:** `components/sx_core/sx_orchestrator.c` (xử lý)

**Thực hiện:**
```c
// sx_platform.h - Thêm API
esp_err_t sx_platform_set_power_save_mode(bool enable);

// sx_orchestrator.c
if (evt.type == SX_EVT_CHATBOT_AUDIO_CHANNEL_OPENED) {
    // Disable power save when audio channel opens
    sx_platform_set_power_save_mode(false);
    sx_audio_protocol_bridge_enable_receive(true);
    
    sx_dispatcher_get_state(&st);
    st.seq++;
    st.ui.audio_channel_opened = true;
    sx_dispatcher_set_state(&st);
}

if (evt.type == SX_EVT_CHATBOT_AUDIO_CHANNEL_CLOSED) {
    // Enable power save when audio channel closes
    sx_platform_set_power_save_mode(true);
    sx_audio_protocol_bridge_enable_receive(false);
    sx_audio_protocol_bridge_enable_send(false);
    
    sx_dispatcher_get_state(&st);
    st.seq++;
    st.ui.audio_channel_opened = false;
    // Cleanup alert state
    st.ui.has_alert = false;
    st.ui.alert_status[0] = '\0';
    st.ui.alert_message[0] = '\0';
    st.ui.alert_emotion[0] = '\0';
    sx_dispatcher_set_state(&st);
}
```

**Lý do:** Orchestrator quyết định power management dựa trên events.

**Kiểm tra:**
- ✅ Error detection hoạt động
- ✅ Power save được disable khi audio channel mở
- ✅ Power save được enable khi audio channel đóng
- ✅ State cleanup khi channel đóng

---

### 📨 PHASE 5: System Commands và Alert Messages (P1)

**Mục tiêu:** Xử lý system commands và alert messages từ server

**Thời gian ước tính:** 3-4 giờ

#### Task 5.1: Parse System Commands trong Chatbot Service

**File:** `components/sx_services/sx_chatbot_service.c`

**Thực hiện:**
```c
// Trong sx_chatbot_handle_json_message()
if (strcmp(msg_type, "system") == 0) {
    cJSON *command = cJSON_GetObjectItem(root, "command");
    if (cJSON_IsString(command)) {
        if (strcmp(command->valuestring, "reboot") == 0) {
            sx_event_t evt = {
                .type = SX_EVT_SYSTEM_REBOOT,
                .arg0 = 0,
                .ptr = NULL,
            };
            sx_dispatcher_post_event(&evt);
        } else {
            // Generic system command
            sx_event_t evt = {
                .type = SX_EVT_SYSTEM_COMMAND,
                .arg0 = 0,
                .ptr = sx_event_alloc_string(command->valuestring),
            };
            sx_dispatcher_post_event(&evt);
        }
    }
    return true;
}
```

**Lý do:** System commands được parse, emit events, orchestrator xử lý.

#### Task 5.2: Parse Alert Messages trong Chatbot Service

**File:** `components/sx_services/sx_chatbot_service.c`

**Thực hiện:**
```c
// Trong sx_chatbot_handle_json_message()
if (strcmp(msg_type, "alert") == 0) {
    cJSON *status = cJSON_GetObjectItem(root, "status");
    cJSON *message = cJSON_GetObjectItem(root, "message");
    cJSON *emotion = cJSON_GetObjectItem(root, "emotion");
    
    if (cJSON_IsString(status) && cJSON_IsString(message) && cJSON_IsString(emotion)) {
        typedef struct {
            char status[64];
            char message[256];
            char emotion[32];
        } alert_data_t;
        
        alert_data_t *alert = (alert_data_t *)malloc(sizeof(alert_data_t));
        if (alert) {
            strncpy(alert->status, status->valuestring, sizeof(alert->status) - 1);
            strncpy(alert->message, message->valuestring, sizeof(alert->message) - 1);
            strncpy(alert->emotion, emotion->valuestring, sizeof(alert->emotion) - 1);
            
            sx_event_t evt = {
                .type = SX_EVT_ALERT,
                .arg0 = 0,
                .ptr = alert,
            };
            sx_dispatcher_post_event(&evt);
        }
    }
    return true;
}
```

**Lý do:** Alert messages được parse, emit events với data, orchestrator update state.

**Kiểm tra:**
- ✅ System commands được parse và emit events
- ✅ Alert messages được parse và emit events
- ✅ Orchestrator xử lý đúng (đã implement trong Phase 1)

---

### 🔄 PHASE 6: Reconnection Logic (P1)

**Mục tiêu:** Thêm auto reconnect khi connection bị mất

**Thời gian ước tính:** 4-6 giờ

#### Task 6.1: Reconnection State Management

**File:** `components/sx_protocol/sx_protocol_ws.c`, `sx_protocol_mqtt.c`

**Thực hiện:**
```c
// Reconnection state
static bool s_reconnecting = false;
static uint32_t s_reconnect_attempts = 0;
static const uint32_t s_max_reconnect_attempts = 10;

static void schedule_reconnect(void) {
    if (s_reconnecting || s_reconnect_attempts >= s_max_reconnect_attempts) {
        return;
    }
    
    s_reconnecting = true;
    s_reconnect_attempts++;
    
    // Post reconnect event
    sx_event_t evt = {
        .type = SX_EVT_PROTOCOL_RECONNECTING,
        .arg0 = s_reconnect_attempts,
        .ptr = NULL,
    };
    sx_dispatcher_post_event(&evt);
    
    // Schedule reconnect task
    xTaskCreate(reconnect_task, "proto_reconnect", 2048, NULL, 5, NULL);
}

static void reconnect_task(void *arg) {
    // Exponential backoff
    uint32_t delay_ms = 5000 * s_reconnect_attempts;
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    
    // Try reconnect
    esp_err_t ret = sx_protocol_ws_start(&s_cfg); // Or mqtt_start()
    if (ret == ESP_OK) {
        s_reconnecting = false;
        s_reconnect_attempts = 0;
        
        // Post connected event
        sx_event_t evt = {
            .type = SX_EVT_CHATBOT_CONNECTED,
            .arg0 = 0,
            .ptr = NULL,
        };
        sx_dispatcher_post_event(&evt);
    } else {
        s_reconnecting = false;
        schedule_reconnect(); // Retry
    }
    
    vTaskDelete(NULL);
}

// Call schedule_reconnect() khi detect disconnect
static void on_disconnect(void) {
    schedule_reconnect();
}
```

**Lý do:** Reconnection trong protocol layer, emit events, orchestrator không cần biết chi tiết.

**Kiểm tra:**
- ✅ Reconnection được trigger khi disconnect
- ✅ Exponential backoff hoạt động
- ✅ Max attempts được respect
- ✅ Events được emit đúng

---

### 🧹 PHASE 7: Code Cleanup và Optimization (P2)

**Mục tiêu:** Refactor code duplication, optimize performance

**Thời gian ước tính:** 4-6 giờ

#### Task 7.1: Refactor MCP Server Init

**File:** `components/sx_bootstrap/sx_bootstrap.c`
**File:** `components/sx_services/sx_chatbot_service.c`

**Thực hiện:**
- Move MCP server init từ chatbot service sang bootstrap
- Đảm bảo MCP tools được register trước khi protocol start

**Lý do:** MCP server nên init sớm như xiaozhi.

#### Task 7.2: State-based Audio Routing

**File:** `components/sx_services/sx_audio_protocol_bridge.c`

**Thực hiện:**
```c
// Chỉ nhận audio khi device state phù hợp
static void on_incoming_audio(const sx_audio_stream_packet_t *packet) {
    sx_state_t st;
    sx_dispatcher_get_state(&st);
    
    // Chỉ nhận audio khi device đang speaking
    if (st.ui.device_state == SX_DEV_BUSY) {
        sx_audio_protocol_bridge_receive_packet(packet);
    }
}
```

**Lý do:** Tránh nhận audio khi không cần.

#### Task 7.3: Error Handling khi Send Fail

**File:** `components/sx_services/sx_audio_protocol_bridge.c`

**Thực hiện:**
```c
// Break loop khi send fail
while (packet = get_next_packet()) {
    esp_err_t ret = sx_protocol_base_send_audio(base, packet);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send audio packet, breaking loop");
        break;
    }
}
```

**Lý do:** Tránh spam send khi connection down.

**Kiểm tra:**
- ✅ Code duplication giảm
- ✅ Performance được optimize
- ✅ Error handling tốt hơn

---

## 4. SO SÁNH TRƯỚC VÀ SAU CẢI THIỆN

### 4.1 Trước cải thiện

| Tính năng | Trạng thái | Ghi chú |
|-----------|-----------|---------|
| System commands | ❌ Không có | Thiếu "reboot" command |
| Alert messages | ❌ Không có | Thiếu alert display |
| Error handling | ⚠️ Cơ bản | Thiếu timeout, centralized error |
| Hello handshake | ⚠️ Không đầy đủ | Thiếu timeout, audio params |
| Protocol abstraction | ❌ Không có | Code duplicate WS/MQTT |
| Power management | ❌ Không có | Không auto disable power save |
| Sample rate validation | ❌ Không có | Không warning mismatch |
| State cleanup | ❌ Không có | State có thể stale |
| Reconnection logic | ❌ Không có | Mất kết nối không tự reconnect |
| Session ID | ❌ Không có | Không track session |

### 4.2 Sau cải thiện

| Tính năng | Trạng thái | Ghi chú |
|-----------|-----------|---------|
| System commands | ✅ Đầy đủ | Qua events, orchestrator xử lý |
| Alert messages | ✅ Đầy đủ | Qua events, state update |
| Error handling | ✅ Đầy đủ | Timeout, centralized error qua events |
| Hello handshake | ✅ Đầy đủ | Timeout, audio params, validation |
| Protocol abstraction | ✅ Có | Base interface, giảm duplication |
| Power management | ✅ Đầy đủ | Auto disable/enable qua orchestrator |
| Sample rate validation | ✅ Đầy đủ | Warning qua state |
| State cleanup | ✅ Đầy đủ | Cleanup khi channel đóng |
| Reconnection logic | ✅ Đầy đủ | Auto reconnect với exponential backoff |
| Session ID | ✅ Đầy đủ | Parse và lưu từ server hello |

### 4.3 Tuân thủ SIMPLEXL_ARCH

**Trước cải thiện:**
- ✅ Đã tuân thủ (nhưng thiếu tính năng)

**Sau cải thiện:**
- ✅ Vẫn tuân thủ 100%
- ✅ Tất cả tính năng qua events
- ✅ Orchestrator là single source of truth
- ✅ UI chỉ đọc state snapshot
- ✅ Services chỉ emit events

---

## 5. KẾ HOẠCH KIỂM THỬ

### 5.1 Unit Tests (Nên có)

**Files cần test:**
- `sx_orchestrator.c` - Test event handling
- `sx_chatbot_service.c` - Test message parsing
- `sx_protocol_ws.c` - Test hello handshake
- `sx_protocol_mqtt.c` - Test hello handshake

**Test cases:**
1. System command "reboot" → emit event → orchestrator restart
2. Alert message → parse → emit event → state update
3. Hello handshake → send → receive → timeout handling
4. Error detection → emit event → state update
5. Reconnection → exponential backoff → max attempts

### 5.2 Integration Tests (Quan trọng)

**Test scenarios:**
1. **Full chatbot flow:**
   - Connect → Hello handshake → Audio channel open → TTS/STT → Disconnect
2. **Error recovery:**
   - Network error → Reconnection → Resume
3. **System commands:**
   - Server send "reboot" → Device restart
4. **Alert display:**
   - Server send alert → UI display alert

### 5.3 Manual Testing Checklist

- [ ] System command "reboot" hoạt động
- [ ] Alert messages hiển thị đúng
- [ ] Hello handshake với timeout
- [ ] Sample rate validation warning
- [ ] Power save disable/enable tự động
- [ ] Reconnection tự động
- [ ] State cleanup khi channel đóng
- [ ] Error handling hiển thị đúng
- [ ] Session ID được lưu và hiển thị

---

## 6. TIMELINE VÀ ƯỚC TÍNH

### 6.1 Timeline tổng thể

| Phase | Ưu tiên | Thời gian | Phụ thuộc |
|-------|---------|-----------|-----------|
| Phase 1: Events & State | P0 | 4-6 giờ | - |
| Phase 2: Protocol Abstraction | P0 | 6-8 giờ | - |
| Phase 3: Hello Handshake | P0 | 4-6 giờ | Phase 1 |
| Phase 4: Error & Power | P0 | 5-7 giờ | Phase 1 |
| Phase 5: System & Alert | P1 | 3-4 giờ | Phase 1 |
| Phase 6: Reconnection | P1 | 4-6 giờ | Phase 2, 4 |
| Phase 7: Cleanup | P2 | 4-6 giờ | Phase 1-6 |

**Tổng cộng:** 30-43 giờ (4-6 ngày làm việc)

### 6.2 Ưu tiên thực hiện

**Giai đoạn 1 (P0 - Phải làm ngay):**
- Phase 1: Events & State (4-6h)
- Phase 2: Protocol Abstraction (6-8h)
- Phase 3: Hello Handshake (4-6h)
- Phase 4: Error & Power (5-7h)
- **Tổng:** 19-27 giờ (2.5-3.5 ngày)

**Giai đoạn 2 (P1 - Nên làm sớm):**
- Phase 5: System & Alert (3-4h)
- Phase 6: Reconnection (4-6h)
- **Tổng:** 7-10 giờ (1 ngày)

**Giai đoạn 3 (P2 - Có thể làm sau):**
- Phase 7: Cleanup (4-6h)
- **Tổng:** 4-6 giờ (0.5 ngày)

### 6.3 Milestones

**Milestone 1:** Hoàn thành Phase 1-4 (P0)
- ✅ Tất cả events và state mở rộng
- ✅ Protocol abstraction
- ✅ Hello handshake đầy đủ
- ✅ Error handling và power management
- **Kết quả:** Chatbot cơ bản ổn định như xiaozhi

**Milestone 2:** Hoàn thành Phase 5-6 (P1)
- ✅ System commands và alerts
- ✅ Reconnection logic
- **Kết quả:** Chatbot đầy đủ tính năng như xiaozhi

**Milestone 3:** Hoàn thành Phase 7 (P2)
- ✅ Code cleanup và optimization
- **Kết quả:** Code quality tốt, dễ maintain

---

## 7. KẾT LUẬN

### 7.1 Tổng kết

**Roadmap này đảm bảo:**
- ✅ Tuân thủ 100% SIMPLEXL_ARCH
- ✅ Đạt được tất cả tính năng như xiaozhi
- ✅ Kiến trúc tốt hơn (event-driven vs singleton)
- ✅ Dễ maintain và mở rộng hơn

### 7.2 Lợi ích

1. **Kiến trúc tốt hơn:** Event-driven, modular, dễ test
2. **Tính năng đầy đủ:** Có tất cả tính năng như xiaozhi
3. **Tuân thủ SIMPLEXL:** Không phá kiến trúc hiện tại
4. **Maintainability:** Dễ maintain và mở rộng

### 7.3 Khuyến nghị

**Ưu tiên thực hiện:**
1. **Phase 1-4 (P0)** trước - Các tính năng quan trọng nhất
2. **Test kỹ** với server thật sau mỗi phase
3. **Phase 5-7 (P1-P2)** sau - Cải thiện thêm

**Sau khi implement:**
- hai-os sẽ có tính năng chatbot đầy đủ như xiaozhi
- Vẫn giữ được kiến trúc event-driven tốt hơn
- Dễ maintain và mở rộng hơn

---

*Roadmap này dựa trên phân tích từ 3 báo cáo:*
- *DE_XUAT_CAI_THIEN_CHATBOT_TUAN_THU_SIMPLEXL.md*
- *PHAN_TICH_CHATBOT_SO_SANH.md*
- *SO_SANH_2_DU_AN.md*

*Tất cả cải thiện đều tuân thủ SIMPLEXL_ARCH, không phá vỡ kiến trúc hiện tại.*









