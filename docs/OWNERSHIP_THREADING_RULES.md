# Ownership và Threading Rules

Tài liệu này mô tả ownership rules, thread context, và lock order cho toàn bộ hệ thống **hai-os-simplexl**.

## Ownership Rules

### Event Payload Ownership

**Rule 1: Event Struct Ownership**
- **Owner**: Caller (producer) owns event struct
- **Lifetime**: Valid cho đến khi được consume bởi orchestrator
- **Copy**: Dispatcher copy event struct vào queue (không copy payload)

**Rule 2: Event Payload Pointer (`evt->ptr`)**
- **Owner**: Caller (producer) owns payload pointer
- **Lifetime**: Phải valid cho đến khi orchestrator consume event
- **⚠️ RISK**: Nếu producer free payload ngay sau post, consumer có thể đọc invalid pointer

**Best Practice:**
```c
// ✅ GOOD: Dùng string pool
char *msg = sx_event_alloc_string("Hello");
sx_event_t evt = {.type = SX_EVT_CHATBOT_STT, .ptr = msg};
sx_dispatcher_post_event(&evt);
// String pool manages lifetime

// ✅ GOOD: Payload lifetime đủ dài
static char buffer[256];
strcpy(buffer, "Hello");
sx_event_t evt = {.type = SX_EVT_CHATBOT_STT, .ptr = buffer};
sx_dispatcher_post_event(&evt);
// Buffer valid trong suốt lifetime của function

// ❌ BAD: Free payload ngay sau post
char *msg = malloc(256);
strcpy(msg, "Hello");
sx_event_t evt = {.type = SX_EVT_CHATBOT_STT, .ptr = msg};
sx_dispatcher_post_event(&evt);
free(msg); // ⚠️ RISK: Payload may still be in queue
```

**Reference**: ```182:182:components/sx_core/include/sx_event.h``` - `ptr` field documentation

### State Ownership

**Rule 3: State Buffers**
- **Owner**: Dispatcher owns state buffers (`s_state_front`, `s_state_back`)
- **Lifetime**: Persistent trong suốt lifetime của dispatcher
- **Access**: 
  - Writer: Orchestrator task (single-writer)
  - Readers: Bất kỳ task nào (multi-reader, lock-free)

**Rule 4: State Snapshots**
- **Owner**: Caller owns snapshot copy
- **Lifetime**: Valid cho đến khi được free hoặc overwrite
- **Copy**: `sx_dispatcher_get_state()` copy state snapshot (lock-free read)

**Reference**: ```321:352:components/sx_core/sx_dispatcher.c``` - `sx_dispatcher_set_state()` implementation

### Service Instance Ownership

**Rule 5: Service Instances**
- **Owner**: Service module owns service instances
- **Lifetime**: Persistent sau khi init (không có deinit thực sự)
- **Cleanup**: Services tự quản lý cleanup trong deinit (nếu có)

**Rule 6: Service Vtable**
- **Owner**: Service module owns vtable (thường là static const)
- **Lifetime**: Persistent trong suốt lifetime của service
- **Registration**: Service đăng ký vtable với `sx_service_register()`

**Reference**: ```29:66:components/sx_core/sx_service_if.c``` - `sx_service_register()` implementation

### Platform Resource Ownership

**Rule 6.5: Display Handles**
- **Owner**: Caller owns display handles struct (`sx_display_handles_t`)
- **Lifetime**: Valid trong suốt lifetime của panel (managed bởi ESP-IDF)
- **Cleanup**: ESP-IDF tự cleanup khi deinit (không có explicit deinit API)
- **Reference**: ```13:16:components/sx_platform/include/sx_platform.h``` - `sx_display_handles_t` definition

**Rule 6.6: Touch Handles**
- **Owner**: Caller owns touch handles struct (`sx_touch_handles_t`)
- **Lifetime**: Valid trong suốt lifetime của touch controller
- **Cleanup**: ESP-IDF tự cleanup khi deinit (không có explicit deinit API)
- **Reference**: ```19:21:components/sx_platform/include/sx_platform.h``` - `sx_touch_handles_t` definition

**Rule 6.7: SPI Bus Mutex**
- **Owner**: sx_spi_bus_manager owns mutex
- **Lifetime**: Persistent sau khi init
- **Cleanup**: Không có explicit cleanup (mutex tồn tại trong suốt lifetime của system)
- **Usage**: Lock/unlock phải được paired (caller responsibility)
- **Reference**: ```9:9:components/sx_platform/sx_spi_bus_manager.c``` - `s_spi_bus_mutex` definition

**Rule 6.8: I2C Bus Handles**
- **Owner**: sx_platform owns I2C bus handles (static)
- **Lifetime**: Persistent sau khi init
- **Cleanup**: Cleanup khi init failed (```453:456:components/sx_platform/sx_platform.c```)
- **Touch I2C**: I2C_NUM_1 (GPIO 8/11) cho touch controller
- **Volume I2C**: I2C_NUM_0 (GPIO 22/21) - **⚠️ DISABLED** (```19:20:components/sx_platform/sx_platform_volume.c```)

## Resource Ownership

**Rule 7: FreeRTOS Resources**
- **Queues**: Dispatcher owns (created in `sx_dispatcher_init()`)
- **Mutexes**: Component owns (created in init functions)
- **Tasks**: Component owns (created in start functions)
- **Cleanup**: Resources được cleanup khi component deinit (nếu có)

**Rule 8: Hardware Resources**
- **Display**: Platform owns (created in `sx_platform_display_init()`)
- **Touch**: Platform owns (created in `sx_platform_touch_init()`)
- **SPI Bus**: Platform owns (created in `sx_spi_bus_manager_init()`)
- **I2S**: Audio service owns (created in `sx_audio_service_init()`)

## Thread Context Rules

### Task Contexts

**Rule 9: Orchestrator Task**
- **Task Name**: "sx_orch"
- **Priority**: 8
- **Core**: tskNO_AFFINITY
- **Context**: 
  - Single consumer của event queue
  - Single writer của state
  - Không được block lâu (max 10ms per event)
- **Blocking Operations**: 
  - ✅ Allowed: Queue receive (timeout 0), mutex take (state write)
  - ❌ Not Allowed: Long blocking operations (>10ms)

**Reference**: ```178:181:components/sx_core/sx_orchestrator.c``` - `sx_orchestrator_start()` implementation

**Rule 10: UI Task**
- **Task Name**: "sx_ui" (hoặc tương tự)
- **Priority**: 7
- **Core**: tskNO_AFFINITY
- **Context**: 
  - Multi-reader của state (lock-free)
  - LVGL rendering
  - Touch input handling
- **Blocking Operations**: 
  - ✅ Allowed: LVGL timer, state read (lock-free)
  - ❌ Not Allowed: Long blocking operations (>16ms để đảm bảo 60 FPS)

**Rule 11: Platform Init Context**
- **Context**: Chạy từ `sx_bootstrap_start()` (main task, single-threaded boot)
- **Blocking**: 
  - Display init: ~500ms (panel reset, init commands với delays)
  - Touch init: ~100ms (I2C bus init, touch reset)
- **Thread Safety**: 
  - Init functions: Không thread-safe (chỉ được gọi một lần tại boot)
  - Brightness control: Thread-safe (LEDC driver là thread-safe)
- **Reference**: ```133:314:components/sx_platform/sx_platform.c``` - `sx_platform_display_init()` implementation

**Rule 12: SPI Bus Lock Context**
- **Context**: Bất kỳ task nào có thể lock/unlock SPI bus
- **Blocking**: 
  - Lock operation: Blocking (portMAX_DELAY) - sẽ block cho đến khi có lock
  - **⚠️ RISK**: Nếu lock được hold lâu, có thể block other tasks
- **Thread Safety**: Mutex-based locking - thread-safe
- **Lock Order**: Chỉ có một mutex - không có lock order issue
- **Reference**: ```28:40:components/sx_platform/sx_spi_bus_manager.c``` - `sx_spi_bus_lock()` implementation

**Rule 13: UI Task Context**
- **Task Name**: "sx_ui"
- **Priority**: 7
- **Core**: tskNO_AFFINITY
- **Context**: 
  - Multi-reader của state (lock-free)
  - LVGL rendering (protected bởi lvgl_port_lock)
  - Touch input handling
  - Screen navigation
- **Blocking Operations**: 
  - ✅ Allowed: LVGL lock (timeout 0 = wait indefinitely), state read (lock-free), vTaskDelayUntil (16ms)
  - ❌ Not Allowed: Long blocking operations (>16ms để đảm bảo 60 FPS)
- **Reference**: ```34:293:components/sx_ui/sx_ui_task.c``` - `sx_ui_task()` implementation

**Rule 14: LVGL Lock Rules**
- **Pattern**: Tất cả LVGL operations phải được protect bởi `lvgl_port_lock()`
- **Acquire**: `lvgl_port_lock(0)` - blocking call (timeout 0 = wait indefinitely)
- **Release**: `lvgl_port_unlock()` - release lock
- **Guard Pattern**: Dùng `SX_LVGL_LOCK()` macro để automatic lock/unlock
- **⚠️ RISK**: Nếu lock được hold lâu, có thể block UI rendering
- **Nested Locks**: Không được phép (detected bởi sx_lvgl_lock) (```12:14:components/sx_ui/sx_lvgl_lock.c```)
- **Reference**: ```54:175:components/sx_ui/sx_ui_task.c``` - LVGL lock usage

**Rule 15: Screen Lifecycle Context**
- **Context**: Screen callbacks (on_create, on_show, on_hide, on_destroy, on_update) chạy từ UI task với LVGL lock
- **Thread Safety**: 
  - LVGL operations: Protected bởi LVGL lock (từ router)
  - Static state: Không được protect (nhưng chỉ UI task access)
- **Blocking**: Screen callbacks không được block lâu (>16ms)
- **Reference**: ```48:125:components/sx_ui/ui_router.c``` - Screen lifecycle callbacks

**Rule 16: Network & AI Service Tasks**
- **WiFi Service**: 
  - Event handlers chạy trong ESP-IDF event loop task
  - Public APIs có thể gọi từ bất kỳ task nào (state không được protect)
  - **⚠️ RISK**: State flags (`s_connected`, `s_current_ssid`) không được protect bởi mutex
- **STT Service**: 
  - Task: "sx_stt" (priority 5, core tskNO_AFFINITY)
  - Queue: `s_chunk_queue` (size 5) cho audio chunks
  - Mutex: `s_stt_mutex` cho start/stop session
- **TTS Service**: 
  - Task: "sx_tts" (priority 5, core 1)
  - Queue: `s_tts_queue` (size 10) cho text messages
  - Mutex: `s_tts_mutex` cho config và speaking state
- **Wake Word Service**: 
  - Task: "sx_wake_word" (priority 5, core tskNO_AFFINITY)
  - Queue: `s_audio_queue` (size 10) cho audio chunks
  - **⚠️ RISK**: State flags (`s_active`) không được protect bởi mutex
- **Chatbot Service**: 
  - Task: "sx_chatbot" (priority 5, core tskNO_AFFINITY)
  - Queue: `s_message_queue` (size 10) cho text messages
  - **⚠️ RISK**: State flags (`s_ready`, `s_intent_parsing_enabled`) không được protect
- **Reference**: 
  - ```214:214:components/sx_services/sx_stt_service.c``` - STT task creation
  - ```89:90:components/sx_services/sx_tts_service.c``` - TTS task creation
  - ```176:176:components/sx_services/sx_wake_word_service.c``` - Wake word task creation
  - ```84:84:components/sx_services/sx_chatbot_service.c``` - Chatbot task creation

**Rule 17: Storage Service Tasks**
- **Settings Service**: 
  - NVS operations chạy trong caller context
  - ESP-IDF NVS có internal thread safety
  - **⚠️ RISK**: Multiple commits có thể race (ESP-IDF NVS có protection nhưng không guarantee atomicity)
- **SD Service**: 
  - File operations protected bởi SPI bus lock (blocking)
  - Mount/unmount operations protected bởi SPI bus lock
  - **⚠️ RISK**: File operations block LCD operations (SPI bus shared)
- **Playlist Manager**: 
  - Playlist operations protected bởi mutex
  - Metadata cache không được protect (race condition risk)
  - **Reference**: 
    - ```88:92:components/sx_services/sx_playlist_manager.c``` - Mutex creation
    - ```27:71:components/sx_services/sx_playlist_manager.c``` - Metadata cache (no mutex)

**Rule 18: Service Tasks**
- **Priority**: 5-6 (tùy service)
- **Core**: tskNO_AFFINITY
- **Context**: 
  - Service-specific operations
  - Event posting (multi-producer)
- **Blocking Operations**: 
  - ✅ Allowed: Service-specific blocking (network I/O, audio I/O)
  - ❌ Not Allowed: Block event queue (dùng non-blocking post)

### ISR Context

**Rule 12: ISR Context**
- **Event Posting**: ❌ NOT ALLOWED từ ISR (FreeRTOS queues không ISR-safe trong một số configs)
- **State Read**: ❌ NOT ALLOWED từ ISR (lock-free read vẫn có thể không safe trong ISR)
- **Best Practice**: Defer ISR work to task (dùng queue hoặc semaphore)

## Lock Order Rules

### LVGL Lock Rules

**Rule 13.6: LVGL Lock/Unlock Pairing**
- **Pattern**: Lock/unlock phải được paired
- **Lock**: `lvgl_port_lock(0)` - blocking call (timeout 0 = wait indefinitely)
- **Unlock**: `lvgl_port_unlock()` - release lock
- **Guard Pattern**: Dùng `SX_LVGL_LOCK()` macro để automatic lock/unlock
- **⚠️ RISK**: Nếu caller không unlock sau lock, LVGL sẽ bị lock forever
- **Nested Locks**: Không được phép (detected bởi sx_lvgl_lock)
- **Best Practice**: Dùng SX_LVGL_LOCK() macro để đảm bảo unlock

**Example:**
```c
// ✅ GOOD: Dùng SX_LVGL_LOCK() macro
SX_LVGL_LOCK() {
    // LVGL operations
    lv_obj_set_text(label, "Hello");
}

// ✅ GOOD: Manual lock/unlock
if (lvgl_port_lock(0)) {
    // LVGL operations
    lvgl_port_unlock();
}

// ❌ BAD: Missing unlock
lvgl_port_lock(0);
// LVGL operations
// Missing unlock - LVGL locked forever
```

**Reference**: ```8:33:components/sx_ui/sx_lvgl_lock.c``` - Lock guard implementation

### SPI Bus Lock Rules

**Rule 13.5: SPI Bus Lock/Unlock Pairing**
- **Pattern**: Lock/unlock phải được paired
- **Lock**: `sx_spi_bus_lock()` - blocking call (portMAX_DELAY)
- **Unlock**: `sx_spi_bus_unlock()` - release lock
- **⚠️ RISK**: Nếu caller không unlock sau lock, SPI bus sẽ bị lock forever
- **Best Practice**: Dùng RAII pattern hoặc guard class để đảm bảo unlock

**Example:**
```c
// ✅ GOOD: Paired lock/unlock
sx_spi_bus_lock();
// SPI operations
sx_spi_bus_unlock();

// ❌ BAD: Missing unlock
sx_spi_bus_lock();
// SPI operations
// Missing unlock - SPI bus locked forever
```

**Reference**: ```28:51:components/sx_platform/sx_spi_bus_manager.c``` - Lock/unlock implementation

### Mutex Hierarchy

**Rule 13: Single Mutex Pattern**
- **sx_dispatcher**: Chỉ có một mutex (`s_state_write_mutex`)
- **sx_service_if**: Chỉ có một mutex (`s_registry_mutex`)
- **sx_metrics**: Chỉ có một mutex (`s_metrics_mutex`)
- **sx_lazy_loader**: Chỉ có một mutex (`s_mutex`)

**⚠️ No Lock Order Issues**: Mỗi component chỉ có một mutex, không có circular dependencies.

**Reference**: ```27:27:components/sx_core/sx_dispatcher.c``` - `s_state_write_mutex` definition

### Lock-Free Patterns

**Rule 14: State Read (Lock-Free)**
- **Pattern**: Double-buffer với atomic pointer swap
- **Writer**: Take mutex → Write back buffer → Atomic swap pointer → Release mutex
- **Reader**: Read atomic pointer → Copy state snapshot (lock-free)
- **Safety**: Atomic pointer read đảm bảo reader luôn đọc từ stable buffer

**Reference**: ```321:365:components/sx_core/sx_dispatcher.c``` - State read/write implementation

**Rule 15: Event Queues (Lock-Free)**
- **Pattern**: FreeRTOS queues là thread-safe
- **Post**: `xQueueSend()` thread-safe (multi-producer)
- **Receive**: `xQueueReceive()` thread-safe (single-consumer)
- **Safety**: FreeRTOS đảm bảo thread safety

## Thread Safety Rules

### Thread-Safe Components

**Rule 16: Thread-Safe APIs**
- ✅ **sx_dispatcher_post_event()**: Thread-safe (FreeRTOS queue)
- ✅ **sx_dispatcher_get_state()**: Thread-safe (lock-free read)
- ✅ **sx_dispatcher_set_state()**: Thread-safe (mutex protected)
- ✅ **sx_metrics_***(): Thread-safe (mutex protected)
- ✅ **sx_service_register()**: Thread-safe (mutex protected)
- ✅ **sx_lazy_service_init()**: Thread-safe (mutex protected)

### Not Thread-Safe Components

**Rule 17: Not Thread-Safe APIs**
- ❌ **sx_event_handler_register()**: Không thread-safe (không có mutex)
  - **Mitigation**: Chỉ được gọi từ orchestrator task init
- ❌ **sx_error_handler_set_error()**: Không thread-safe (không có mutex)
  - **Mitigation**: Ít risk vì chỉ write khi set error, read khi get error
- ❌ **sx_bootstrap_start()**: Không thread-safe (chỉ được gọi một lần tại boot)

**Reference**: ```21:31:components/sx_core/sx_event_handler.c``` - `sx_event_handler_register()` không có mutex

## Memory Allocation Rules

### Allocation Patterns

**Rule 18: Static Allocation (Preferred)**
- **State buffers**: Static (```23:24:components/sx_core/sx_dispatcher.c```)
- **Event queues**: Static (FreeRTOS static allocation)
- **Service registry**: Static (```18:18:components/sx_core/sx_service_if.c```)
- **Metrics storage**: Static (```12:12:components/sx_core/sx_metrics.c```)

**Rule 19: Dynamic Allocation (When Needed)**
- **Event payload strings**: Dùng string pool (```sx_event_string_pool```)
- **Service instances**: Service tự quản lý (có thể dynamic)
- **Avoid**: Malloc trong hot path (event posting, state update)

### Memory Lifetime

**Rule 20: Persistent Memory**
- **State buffers**: Lifetime = dispatcher lifetime
- **Event queues**: Lifetime = dispatcher lifetime
- **Service registry**: Lifetime = system lifetime
- **Metrics storage**: Lifetime = system lifetime

**Rule 21: Temporary Memory**
- **Event payload**: Lifetime = event processing time
- **State snapshots**: Lifetime = caller scope
- **String pool**: Managed bởi string pool (có thể reuse)

## Error Handling Rules

### Error Propagation

**Rule 22: Error Events**
- **Pattern**: Post error event thay vì return error code
- **Example**: `sx_error_handler_set_error()` post `SX_EVT_PROTOCOL_ERROR` (```91:100:components/sx_core/sx_error_handler.c```)
- **Benefit**: Async error handling, không block caller

**Rule 23: Error Recovery**
- **Pattern**: Services tự recover từ errors
- **Example**: WiFi service tự reconnect khi disconnect
- **Benefit**: Hệ thống resilient, không cần manual recovery

## Best Practices

### Event Posting

```c
// ✅ GOOD: Non-blocking post với DROP policy
sx_event_t evt = {.type = SX_EVT_UI_INPUT, .arg0 = button_id};
sx_dispatcher_post_event(&evt); // Non-blocking, may drop if queue full

// ✅ GOOD: Blocking post với BLOCK policy cho CRITICAL events
sx_event_t evt = {
    .type = SX_EVT_ERROR,
    .priority = SX_EVT_PRIORITY_CRITICAL,
    .ptr = error_msg
};
sx_dispatcher_post_event_with_policy(&evt, SX_BP_BLOCK, 0); // Block with timeout

// ❌ BAD: Blocking post cho NORMAL events
sx_dispatcher_post_event_with_policy(&evt, SX_BP_BLOCK, 0); // ⚠️ Not allowed for NORMAL
```

### State Updates

```c
// ✅ GOOD: Update state trong orchestrator task
sx_state_t st;
sx_dispatcher_get_state(&st);
st.ui.device_state = SX_DEV_BUSY;
sx_state_update_version_and_dirty(&st, SX_STATE_DIRTY_UI);
sx_dispatcher_set_state(&st);

// ❌ BAD: Update state từ service task
// ⚠️ Only orchestrator should update state
```

### Thread Safety

```c
// ✅ GOOD: Thread-safe API calls
sx_dispatcher_post_event(&evt); // Thread-safe
sx_dispatcher_get_state(&st); // Thread-safe (lock-free)

// ❌ BAD: Non-thread-safe API calls từ multiple tasks
sx_event_handler_register(type, handler); // ⚠️ Not thread-safe
// Should only be called from orchestrator task init
```

## Summary Table

| Component | Thread-Safe | Lock Type | Context |
|-----------|-------------|-----------|---------|
| `sx_dispatcher_post_event()` | ✅ Yes | FreeRTOS queue | Any task |
| `sx_dispatcher_get_state()` | ✅ Yes | Lock-free | Any task |
| `sx_dispatcher_set_state()` | ✅ Yes | Mutex | Orchestrator only |
| `sx_metrics_*()` | ✅ Yes | Mutex | Any task |
| `sx_service_register()` | ✅ Yes | Mutex | Any task |
| `sx_lazy_service_init()` | ✅ Yes | Mutex | Any task |
| `sx_event_handler_register()` | ❌ No | None | Orchestrator init only |
| `sx_error_handler_set_error()` | ❌ No | None | Any task (low risk) |
| `sx_platform_display_init()` | ❌ No | None | Bootstrap only (single-threaded) |
| `sx_platform_touch_init()` | ❌ No | None | Bootstrap only (single-threaded) |
| `sx_platform_set_brightness()` | ✅ Yes | LEDC driver | Any task |
| `sx_spi_bus_lock()` | ✅ Yes | Mutex | Any task (blocking) |
| `sx_spi_bus_unlock()` | ✅ Yes | Mutex | Any task |
| `sx_platform_hw_volume_set()` | ⚠️ Partial | I2C driver | Any task (static state not protected) |
| `sx_ui_start()` | ❌ No | None | Bootstrap only (single-threaded) |
| `ui_router_navigate_to()` | ⚠️ Partial | LVGL lock | UI task only (or with LVGL lock) |
| `sx_screen_register()` | ✅ Yes | Mutex | Any task (usually boot) |
| `ui_screen_registry_register()` | ❌ No | None | UI task init only (single-threaded) |
| Screen callbacks (on_create, etc.) | ⚠️ Partial | LVGL lock | UI task only (with LVGL lock) |
| `sx_wifi_connect()` | ❌ No | None | Any task (state not protected) |
| `sx_wifi_scan()` | ❌ No | None | Any task (blocking scan) |
| `sx_stt_start_session()` | ✅ Yes | Mutex | Any task |
| `sx_stt_send_audio_chunk()` | ⚠️ Partial | Queue | Recording task |
| `sx_tts_speak()` | ⚠️ Partial | Queue | Any task |
| `sx_wake_word_start()` | ❌ No | None | Any task (state not protected) |
| `sx_wake_word_feed_audio()` | ⚠️ Partial | Queue | Recording task |
| `sx_intent_execute()` | ❌ No | None | Any task (handler table not protected) |
| `sx_chatbot_handle_json_message()` | ❌ No | None | Protocol task |
| `sx_settings_set_string()` / `sx_settings_get_string()` | ✅ Yes | NVS internal | Any task (ESP-IDF NVS thread-safe) |
| `sx_sd_read_file()` | ⚠️ Partial | SPI bus lock | Any task (blocking) |
| `sx_playlist_next()` / `sx_playlist_previous()` | ✅ Yes | Mutex | Any task |
| `sx_meta_parse_file()` | ❌ No | None | Any task (file I/O not thread-safe) |

---

**Lưu ý**: Rules này dựa trên Batch 1 (sx_core). Sẽ được cập nhật khi hoàn thành các batches khác.


