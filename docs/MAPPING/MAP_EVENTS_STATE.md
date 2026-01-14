# Events & State Mapping - XIAOZHI → SIMPLEXL

**Ngày tạo:** 2025-01-02  
**Mục đích:** Mapping event schema và state management giữa 2 repo

## Event System Comparison

### XIAOZHI Event System

**Pattern:** Event Group (FreeRTOS) + Schedule Queue

**File:** `main/application.h:28-34`, `main/application.cc:629-711`

```cpp
// Event bits
#define MAIN_EVENT_SCHEDULE (1 << 0)
#define MAIN_EVENT_SEND_AUDIO (1 << 1)
#define MAIN_EVENT_WAKE_WORD_DETECTED (1 << 2)
#define MAIN_EVENT_VAD_CHANGE (1 << 3)
#define MAIN_EVENT_ERROR (1 << 4)
#define MAIN_EVENT_CHECK_NEW_VERSION_DONE (1 << 5)
#define MAIN_EVENT_CLOCK_TICK (1 << 6)

// Main event loop
void Application::MainEventLoop() {
    while (true) {
        auto bits = xEventGroupWaitBits(event_group_, 
            MAIN_EVENT_SCHEDULE | MAIN_EVENT_SEND_AUDIO | ..., 
            pdTRUE, pdFALSE, portMAX_DELAY);
        // Process events...
    }
}
```

**Characteristics:**
- Single event group với 7 bits
- Single consumer task (`main_event_loop`, priority 3)
- Schedule pattern: `Application::Schedule()` → `main_tasks_` queue
- No priority levels
- No backpressure policy

### SIMPLEXL Event System

**Pattern:** Priority Queues (FreeRTOS) + Dispatcher

**File:** `components/sx_core/sx_dispatcher.c:14-18, L56-L66`

```c
// Priority queues
QueueHandle_t s_evt_q_low;      // 16 events
QueueHandle_t s_evt_q_normal;   // 32 events
QueueHandle_t s_evt_q_high;      // 16 events
QueueHandle_t s_evt_q_critical; // 8 events

// Event posting
bool sx_dispatcher_post_event(sx_event_t *evt) {
    // Select queue based on priority
    // Apply backpressure policy (DROP/COALESCE/BLOCK)
}
```

**Characteristics:**
- 4 priority queues (CRITICAL, HIGH, NORMAL, LOW)
- Single consumer task (`sx_orch`, priority 8)
- Direct event posting (no schedule queue)
- Backpressure policy: DROP (default), COALESCE, BLOCK (CRITICAL only)
- Event taxonomy: `SX_EVT_UI_INPUT_*`, `SX_EVT_AUDIO_*`, `SX_EVT_WIFI_*`, etc.

## Event Mapping Table

| XIAOZHI Event | SIMPLEXL Event | Priority | Backpressure | Status |
|---------------|----------------|----------|--------------|--------|
| `MAIN_EVENT_SCHEDULE` | `SX_EVT_*` (various, từ Schedule callback) | NORMAL | DROP | ❌ Not Mapped |
| `MAIN_EVENT_SEND_AUDIO` | `SX_EVT_AUDIO_SEND_READY` | HIGH | COALESCE | ❌ Not Mapped |
| `MAIN_EVENT_WAKE_WORD_DETECTED` | `SX_EVT_WAKE_WORD_DETECTED` | HIGH | DROP | ⚠️ Partial |
| `MAIN_EVENT_VAD_CHANGE` | `SX_EVT_AUDIO_VAD_CHANGE` | NORMAL | DROP | ❌ Not Mapped |
| `MAIN_EVENT_CLOCK_TICK` | Timer callback (internal) | LOW | N/A | ❌ Not Mapped |
| `MAIN_EVENT_ERROR` | `SX_EVT_ERROR` | CRITICAL | BLOCK | ⚠️ Partial |
| `MAIN_EVENT_CHECK_NEW_VERSION_DONE` | `SX_EVT_OTA_VERSION_CHECK_DONE` | NORMAL | DROP | ❌ Not Mapped |
| `XIAOZHI_STATE_CHANGED_EVENT` (ESP event) | State update (internal, không phát event) | N/A | N/A | ❌ Not Mapped |

## State Management Comparison

### XIAOZHI State Management

**Pattern:** Device State Enum + ESP Event System

**File:** `main/device_state.h:25-37`, `main/device_state_event.cc:15-21`

```cpp
enum DeviceState {
    kDeviceStateUnknown,        // 0
    kDeviceStateStarting,        // 1
    kDeviceStateWifiConfiguring, // 2
    kDeviceStateIdle,            // 3
    kDeviceStateConnecting,      // 4
    kDeviceStateListening,       // 5
    kDeviceStateSpeaking,        // 6
    kDeviceStateUpgrading,       // 7
    kDeviceStateActivating,      // 8
    kDeviceStateAudioTesting,    // 9
    kDeviceStateFatalError       // 10
};

// State change event
void DeviceStateEventManager::PostStateChangeEvent(DeviceState prev, DeviceState curr) {
    device_state_event_data_t data = {prev, curr};
    esp_event_post(XIAOZHI_STATE_EVENTS, XIAOZHI_STATE_CHANGED_EVENT, 
                   &data, sizeof(data), portMAX_DELAY);
}
```

**Characteristics:**
- Simple enum (11 states)
- No state machine validation
- ESP event system (multi-consumer)
- State callbacks (vector, no unregister - P1 risk)
- No versioning, no dirty mask

### SIMPLEXL State Management

**Pattern:** Double-Buffer + Version + Dirty Mask

**File:** `components/sx_core/sx_dispatcher.c:321-365`

```c
// State structure
typedef struct {
    uint32_t version;
    uint32_t dirty_mask;
    // ... state fields ...
} sx_state_t;

// Double-buffer
static sx_state_t s_state_front;
static sx_state_t s_state_back;
static sx_state_t *s_state_read_ptr = &s_state_front;

// State write (single-writer)
bool sx_dispatcher_set_state(sx_state_t *st) {
    xSemaphoreTake(s_state_write_mutex, portMAX_DELAY);
    // Write to back buffer
    memcpy(&s_state_back, st, sizeof(sx_state_t));
    // Atomic swap pointer
    s_state_read_ptr = &s_state_back;
    // Swap buffers
    sx_state_t *tmp = &s_state_front;
    s_state_front = s_state_back;
    s_state_back = *tmp;
    xSemaphoreGive(s_state_write_mutex);
}

// State read (lock-free, multi-reader)
bool sx_dispatcher_get_state(sx_state_t *st) {
    sx_state_t *ptr = s_state_read_ptr; // Atomic read
    memcpy(st, ptr, sizeof(sx_state_t)); // Copy snapshot
}
```

**Characteristics:**
- Double-buffer pattern (lock-free read)
- Version + dirty mask (UI subscription)
- Single-writer (orchestrator task only)
- Multi-reader (lock-free)
- State taxonomy: `SX_STATE_DIRTY_UI`, `SX_STATE_DIRTY_AUDIO`, etc.

## State Mapping Table

| XIAOZHI State | SIMPLEXL State Field | Dirty Mask | Status |
|---------------|----------------------|------------|--------|
| `kDeviceStateUnknown` | `state.ui.device_state = SX_DEV_UNKNOWN` | `SX_STATE_DIRTY_UI` | ❌ Not Mapped |
| `kDeviceStateStarting` | `state.ui.device_state = SX_DEV_STARTING` | `SX_STATE_DIRTY_UI` | ❌ Not Mapped |
| `kDeviceStateWifiConfiguring` | `state.wifi.connecting = true` | `SX_STATE_DIRTY_WIFI` | ❌ Not Mapped |
| `kDeviceStateIdle` | `state.ui.device_state = SX_DEV_IDLE` | `SX_STATE_DIRTY_UI` | ❌ Not Mapped |
| `kDeviceStateConnecting` | `state.wifi.connecting = true` | `SX_STATE_DIRTY_WIFI` | ❌ Not Mapped |
| `kDeviceStateListening` | `state.audio.recording = true` | `SX_STATE_DIRTY_AUDIO` | ❌ Not Mapped |
| `kDeviceStateSpeaking` | `state.audio.playing = true` | `SX_STATE_DIRTY_AUDIO` | ❌ Not Mapped |
| `kDeviceStateUpgrading` | `state.system.ota_upgrading = true` | `SX_STATE_DIRTY_SYSTEM` | ❌ Not Mapped |
| `kDeviceStateActivating` | `state.system.activating = true` | `SX_STATE_DIRTY_SYSTEM` | ❌ Not Mapped |
| `kDeviceStateAudioTesting` | `state.audio.testing = true` | `SX_STATE_DIRTY_AUDIO` | ❌ Not Mapped |
| `kDeviceStateFatalError` | `state.system.error = true` | `SX_STATE_DIRTY_SYSTEM` | ❌ Not Mapped |

## Adapter Design

### XIAOZHI → SIMPLEXL Event Adapter

**Location:** `components/sx_xiaozhi_adapter/sx_xiaozhi_event_adapter.c`

**Function:**
```c
// Convert XIAOZHI event bits → SIMPLEXL events
void sx_xiaozhi_event_adapter_process(uint32_t event_bits) {
    if (event_bits & MAIN_EVENT_SEND_AUDIO) {
        sx_event_t evt = {
            .type = SX_EVT_AUDIO_SEND_READY,
            .priority = SX_EVT_PRIORITY_HIGH,
        };
        sx_dispatcher_post_event(&evt);
    }
    // ... other mappings ...
}
```

### XIAOZHI → SIMPLEXL State Adapter

**Location:** `components/sx_xiaozhi_adapter/sx_xiaozhi_state_adapter.c`

**Function:**
```c
// Convert XIAOZHI DeviceState → SIMPLEXL state
void sx_xiaozhi_state_adapter_update(DeviceState xiaozhi_state) {
    sx_state_t st;
    sx_dispatcher_get_state(&st);
    
    // Map state
    switch (xiaozhi_state) {
        case kDeviceStateListening:
            st.audio.recording = true;
            sx_state_update_version_and_dirty(&st, SX_STATE_DIRTY_AUDIO);
            break;
        // ... other mappings ...
    }
    
    sx_dispatcher_set_state(&st);
}
```

## Next Steps

1. **BATCH 3:** Implement event adapter (minimum viable)
2. **BATCH 3:** Implement state adapter (minimum viable)
3. **BATCH 4:** Map protocol events
4. **BATCH 5:** Map UI state updates

