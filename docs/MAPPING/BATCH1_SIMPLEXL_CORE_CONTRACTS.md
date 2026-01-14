# BATCH 1: SIMPLEXL Core Contracts

**Ngày tạo:** 2025-01-02  
**Mục đích:** Phân tích sâu sx_core để hiểu contracts mà XIAOZHI phải tuân theo khi tích hợp

## Tổng Quan

SIMPLEXL core (`sx_core`) cung cấp 4 contracts chính:
1. **Dispatcher:** Event posting/polling với priority queues và backpressure
2. **Orchestrator:** Single-writer state management, event processing
3. **Service Lifecycle:** Service interface vtable, lazy loading
4. **State Management:** Double-buffer pattern, version + dirty mask

## 1. Dispatcher Contract

### 1.1 Event Posting Mechanism

**File:** `components/sx_core/sx_dispatcher.c:L158-298`

**API:**
```c
bool sx_dispatcher_post_event(const sx_event_t *evt);
bool sx_dispatcher_post_event_with_policy(
    const sx_event_t *evt,
    sx_backpressure_policy_t policy,
    uint32_t coalesce_key
);
```

**Priority Queues:**
- `s_evt_q_critical`: 8 events (L163-186)
- `s_evt_q_high`: 16 events (L188-191)
- `s_evt_q_normal`: 32 events (L192-195)
- `s_evt_q_low`: 16 events (L196-199)

**Backpressure Policy:**
- `SX_BP_DROP`: Drop nếu queue full (default cho NORMAL/LOW) (L223-238)
- `SX_BP_COALESCE`: Giữ event mới nhất theo key (L240-270)
- `SX_BP_BLOCK`: Block với timeout (chỉ cho CRITICAL/HIGH) (L272-291)

**Evidence:**
- Queue creation: `sx_dispatcher.c:L55-66`
- Event posting: `sx_dispatcher.c:L158-298`
- Metrics: `sx_dispatcher.c:L211, L228, L250`

**Call Sites (>=2):**
1. `components/sx_ui/sx_ui_task.c` - UI input events
2. `components/sx_services/sx_wifi_service.c` - WiFi events
3. `components/sx_services/sx_protocol_ws.c` - Protocol events
4. `components/sx_services/sx_radio_service.c` - Radio events
5. `components/sx_core/sx_error_handler.c` - Error events

### 1.2 Event Polling Mechanism

**File:** `components/sx_core/sx_dispatcher.c:L300-319`

**API:**
```c
bool sx_dispatcher_poll_event(sx_event_t *out_evt);
```

**Algorithm:**
- Poll theo priority order: CRITICAL → HIGH → NORMAL → LOW (L305-317)
- Non-blocking (timeout = 0)
- Return `true` nếu có event, `false` nếu không có

**Evidence:**
- Polling logic: `sx_dispatcher.c:L300-319`
- Consumer: `sx_orchestrator.c:L104` (orchestrator task)

### 1.3 Event Taxonomy

**File:** `components/sx_core/include/sx_event.h:L25-175`

**Range Reservation:**
- Mỗi domain có 0x0100 (256) IDs để tránh collision
- Format: `BASE = 0xNN00`, range = `0xNN00-0xNNFF`

**Domains:**
- `SX_EVT_LIFECYCLE_BASE`: 0x0000-0x00FF
- `SX_EVT_UI_BASE`: 0x0100-0x01FF
- `SX_EVT_AUDIO_BASE`: 0x0200-0x02FF
- `SX_EVT_RADIO_BASE`: 0x0300-0x03FF
- `SX_EVT_WIFI_BASE`: 0x0400-0x04FF
- `SX_EVT_CHATBOT_BASE`: 0x0500-0x05FF
- `SX_EVT_SYSTEM_BASE`: 0x0600-0x06FF
- `SX_EVT_PROTOCOL_BASE`: 0x0700-0x07FF
- `SX_EVT_OTA_BASE`: 0x0800-0x08FF

**Evidence:**
- Range definitions: `sx_event.h:L29-43`
- Event type enum: `sx_event.h:L47-175`

**Contract cho XIAOZHI:**
- XIAOZHI events phải map vào taxonomy SIMPLEXL
- Không được tạo event IDs ngoài reserved ranges
- Khuyến nghị: Dùng `SX_EVT_CHATBOT_BASE` hoặc `SX_EVT_SYSTEM_BASE` cho XIAOZHI events

## 2. Orchestrator Contract

### 2.1 Single-Writer State Rule

**File:** `components/sx_core/sx_orchestrator.c:L15-176`

**Task:**
- Name: `sx_orch`
- Priority: 8 (highest)
- Core: `tskNO_AFFINITY` (any core)
- Stack: 3072 bytes (L180)

**Responsibilities:**
1. Single consumer của event queue (L104)
2. Single writer của state (L113-161)
3. Event handler registry (L24-59)

**Evidence:**
- Task creation: `sx_orchestrator.c:L178-181`
- Event processing loop: `sx_orchestrator.c:L99-175`
- State update: `sx_orchestrator.c:L160-161`

**Contract cho XIAOZHI:**
- XIAOZHI không được set state trực tiếp
- XIAOZHI phải post events, orchestrator update state
- XIAOZHI có thể read state (lock-free)

### 2.2 Event Handler Registry

**File:** `components/sx_core/sx_event_handler.c:L11-59`

**API:**
```c
esp_err_t sx_event_handler_register(sx_event_type_t event_type, sx_event_handler_t handler);
esp_err_t sx_event_handler_unregister(sx_event_type_t event_type);
bool sx_event_handler_process(const sx_event_t *evt, sx_state_t *state);
```

**Registry:**
- Static array: `s_handlers[MAX_EVENT_TYPES]` (L7)
- Max event types: 64 (L5)
- Thread safety: ❌ NOT THREAD-SAFE (chỉ được gọi từ orchestrator init)

**Evidence:**
- Registry: `sx_event_handler.c:L7`
- Registration: `sx_event_handler.c:L21-31`
- Processing: `sx_event_handler.c:L42-59`

**Registered Handlers (L24-59):**
- `SX_EVT_UI_INPUT` → `sx_event_handler_ui_input`
- `SX_EVT_CHATBOT_STT` → `sx_event_handler_chatbot_stt`
- `SX_EVT_CHATBOT_TTS_SENTENCE` → `sx_event_handler_chatbot_tts_sentence`
- `SX_EVT_WIFI_CONNECTED` → `sx_event_handler_wifi_connected`
- ... (20+ handlers)

**Contract cho XIAOZHI:**
- XIAOZHI có thể register handlers cho XIAOZHI-specific events
- Handlers phải return `true` nếu state changed, `false` nếu không
- Handlers không được block lâu (>10ms)

### 2.3 State Update Pattern

**File:** `components/sx_core/sx_orchestrator.c:L113-161`

**Pattern:**
```c
sx_dispatcher_get_state(&st);  // Get current state
// Modify state fields
sx_state_update_version_and_dirty(&st, dirty_mask);  // Update version + dirty_mask
sx_dispatcher_set_state(&st);  // Publish state
```

**Dirty Mask Mapping:**
- `SX_EVT_WIFI_*` → `SX_STATE_DIRTY_WIFI` (L122-125)
- `SX_EVT_AUDIO_*` → `SX_STATE_DIRTY_AUDIO` (L127-134)
- `SX_EVT_UI_*`, `SX_EVT_CHATBOT_*` → `SX_STATE_DIRTY_UI` (L136-145)
- `SX_EVT_SYSTEM_*` → `SX_STATE_DIRTY_SYSTEM` (L147-151)

**Evidence:**
- State update: `sx_orchestrator.c:L113-161`
- Dirty mask mapping: `sx_orchestrator.c:L121-157`

**Contract cho XIAOZHI:**
- XIAOZHI event handlers phải set dirty_mask đúng domain
- XIAOZHI không được skip `sx_state_update_version_and_dirty()`

## 3. Service Lifecycle Contract

### 3.1 Service Interface Vtable

**File:** `components/sx_core/include/sx_service_if.h:L20-56`

**Vtable:**
```c
typedef struct {
    esp_err_t (*init)(void);
    esp_err_t (*start)(void);
    esp_err_t (*stop)(void);
    esp_err_t (*deinit)(void);
    esp_err_t (*on_event)(const sx_event_t *evt);
} sx_service_if_t;
```

**Lifecycle Order:**
1. `init()` - Allocate resources, setup
2. `start()` - Begin operation
3. `stop()` - Pause operation (optional)
4. `deinit()` - Free resources

**Evidence:**
- Vtable definition: `sx_service_if.h:L20-56`
- Registration: `sx_service_if.c:L29-66`

**Contract cho XIAOZHI:**
- XIAOZHI adapter phải implement `sx_service_if_t`
- `init()`: Initialize XIAOZHI Application singleton
- `start()`: Start XIAOZHI main event loop
- `on_event()`: Handle SIMPLEXL events → XIAOZHI actions

### 3.2 Service Registry

**File:** `components/sx_core/sx_service_if.c:L10-197`

**Registry:**
- Static array: `s_service_registry[MAX_SERVICES]` (L18)
- Max services: 32 (L11)
- Thread safety: ✅ THREAD-SAFE (mutex protected) (L39, L78)

**API:**
```c
esp_err_t sx_service_register(const char *name, const sx_service_if_t *iface);
esp_err_t sx_service_unregister(const char *name);
esp_err_t sx_service_init_all(void);
esp_err_t sx_service_start_all(void);
```

**Evidence:**
- Registry: `sx_service_if.c:L18`
- Registration: `sx_service_if.c:L29-66`
- Init all: `sx_service_if.c:L93-117`
- Start all: `sx_service_if.c:L119-143`

**Contract cho XIAOZHI:**
- XIAOZHI adapter phải register trong bootstrap
- Service name: `"xiaozhi_adapter"`
- Init/start order: Sau SIMPLEXL core, trước XIAOZHI Application::Start()

## 4. State Management Contract

### 4.1 Double-Buffer Pattern

**File:** `components/sx_core/sx_dispatcher.c:L20-27, L321-365`

**Buffers:**
- `s_state_front`: Front buffer (read-only for readers) (L23)
- `s_state_back`: Back buffer (write-only for orchestrator) (L24)
- `s_state_read_ptr`: Atomic read pointer (L25)

**Write Pattern:**
```c
xSemaphoreTake(s_state_write_mutex, portMAX_DELAY);  // Lock
// Write to back buffer
s_state_back = *state;
// Atomic swap pointer
s_state_read_ptr = &s_state_back;
xSemaphoreGive(s_state_write_mutex);  // Unlock
```

**Read Pattern:**
```c
volatile sx_state_t *read_ptr = s_state_read_ptr;  // Atomic read pointer
*out_state = *read_ptr;  // Copy from stable buffer (lock-free)
```

**Evidence:**
- Buffer definitions: `sx_dispatcher.c:L23-25`
- Write: `sx_dispatcher.c:L321-352`
- Read: `sx_dispatcher.c:L354-365`

**Contract cho XIAOZHI:**
- XIAOZHI có thể read state (lock-free, thread-safe)
- XIAOZHI không được write state trực tiếp
- XIAOZHI phải copy state snapshot (không giữ pointer)

### 4.2 Version + Dirty Mask

**File:** `components/sx_core/include/sx_state.h:L82-97`, `components/sx_core/include/sx_state_helper.h:L18-24`

**State Structure:**
```c
typedef struct {
    uint32_t version;      // Monotonically increasing
    uint32_t dirty_mask;   // Bitmask of changed domains
    uint32_t seq;          // Legacy sequence number
    sx_wifi_state_t wifi;
    sx_audio_state_t audio;
    sx_ui_state_t ui;
} sx_state_t;
```

**Dirty Mask Bits:**
- `SX_STATE_DIRTY_WIFI`: Bit 0 (L84)
- `SX_STATE_DIRTY_AUDIO`: Bit 1 (L85)
- `SX_STATE_DIRTY_UI`: Bit 2 (L86)
- `SX_STATE_DIRTY_SYSTEM`: Bit 3 (L87)

**Helper Function:**
```c
static inline void sx_state_update_version_and_dirty(sx_state_t *state, uint32_t dirty_mask) {
    state->version++;
    state->dirty_mask = dirty_mask;
    state->seq++;
}
```

**Evidence:**
- State structure: `sx_state.h:L90-97`
- Dirty mask bits: `sx_state.h:L84-87`
- Helper: `sx_state_helper.h:L18-24`

**Contract cho XIAOZHI:**
- XIAOZHI event handlers phải set dirty_mask đúng domain
- XIAOZHI không được skip version increment
- XIAOZHI UI screens có thể subscribe theo dirty_mask

## 5. Call Sites Analysis

### 5.1 Event Posting Call Sites

**Top 5 Call Sites:**
1. `components/sx_ui/sx_ui_task.c` - UI input events (>=10 calls)
2. `components/sx_services/sx_wifi_service.c` - WiFi events (>=5 calls)
3. `components/sx_services/sx_protocol_ws.c` - Protocol events (>=10 calls)
4. `components/sx_services/sx_radio_service.c` - Radio events (>=5 calls)
5. `components/sx_core/sx_error_handler.c` - Error events (>=3 calls)

**Pattern:**
- Services post events khi có state change
- UI post events khi có user input
- Protocol post events khi có message/error

### 5.2 State Read Call Sites

**Top 5 Call Sites:**
1. `components/sx_ui/sx_ui_task.c` - UI state polling (mỗi frame, ~60 FPS)
2. `components/sx_core/sx_orchestrator.c` - Orchestrator state update (mỗi event)
3. `components/sx_services/sx_wifi_service.c` - WiFi state check (>=3 calls)
4. `components/sx_services/sx_radio_service.c` - Radio state check (>=2 calls)
5. `components/sx_services/sx_chatbot_service.c` - Chatbot state check (>=2 calls)

**Pattern:**
- UI reads state mỗi frame (lock-free)
- Services read state khi cần (lock-free)
- Orchestrator reads state trước khi update (lock-free)

## 6. Contract Summary cho XIAOZHI Adapter

### 6.1 Event Posting Contract

**MUST:**
- Post events với priority đúng (CRITICAL/HIGH/NORMAL/LOW)
- Dùng backpressure policy phù hợp (DROP cho NORMAL/LOW, BLOCK cho CRITICAL)
- Event IDs phải trong reserved ranges (khuyến nghị: `SX_EVT_CHATBOT_BASE`)

**MUST NOT:**
- Post events từ ISR
- Post events với priority = 0 (sẽ dùng default)
- Tạo event IDs ngoài reserved ranges

### 6.2 State Management Contract

**MUST:**
- Read state lock-free (dùng `sx_dispatcher_get_state()`)
- Copy state snapshot (không giữ pointer)
- Post events để update state (không write trực tiếp)

**MUST NOT:**
- Write state trực tiếp (chỉ orchestrator được write)
- Giữ pointer đến state (state có thể thay đổi)
- Skip version increment khi update state

### 6.3 Service Lifecycle Contract

**MUST:**
- Implement `sx_service_if_t` vtable
- Register service trong bootstrap
- Call `init()` trước `start()`

**MUST NOT:**
- Skip lifecycle methods
- Call `start()` trước `init()`
- Deinit service trong runtime (chỉ trong shutdown)

### 6.4 Event Handler Contract

**MUST:**
- Return `true` nếu state changed, `false` nếu không
- Set dirty_mask đúng domain
- Không block lâu (>10ms)

**MUST NOT:**
- Block trong handler (max 10ms)
- Skip dirty_mask update
- Call LVGL APIs (chỉ UI task được gọi)

## 7. Next Steps

1. **BATCH 2:** Phân tích XIAOZHI core runtime để hiểu cách XIAOZHI hoạt động
2. **BATCH 3:** Implement adapter với contracts trên
3. **BATCH 4:** Map protocol callbacks
4. **BATCH 5:** Map UI state updates

## 8. File Đã Đọc

- ✅ `components/sx_core/include/sx_dispatcher.h` (57 lines)
- ✅ `components/sx_core/sx_dispatcher.c` (385 lines)
- ✅ `components/sx_core/include/sx_orchestrator.h` (12 lines)
- ✅ `components/sx_core/sx_orchestrator.c` (182 lines)
- ✅ `components/sx_core/include/sx_event.h` (188 lines)
- ✅ `components/sx_core/include/sx_state.h` (102 lines)
- ✅ `components/sx_core/include/sx_state_helper.h` (29 lines)
- ✅ `components/sx_core/sx_event_handler.c` (71 lines)
- ✅ `components/sx_core/include/sx_service_if.h` (115 lines)
- ✅ `components/sx_core/sx_service_if.c` (207 lines)

**Tổng:** ~1,368 lines đã đọc trong BATCH 1

