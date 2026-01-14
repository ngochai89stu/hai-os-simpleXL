# API Catalog: sx_core

Component **sx_core** là trái tim của hệ thống, quản lý event-driven architecture, state management, và service lifecycle.

## Tổng Quan Component

**sx_core** cung cấp:
- **Event Dispatcher**: Multi-producer, single-consumer event queue với priority queues
- **State Management**: Single-writer, multi-reader với double-buffer pattern
- **Orchestrator**: Single consumer task xử lý events và update state
- **Bootstrap**: Entry point khởi tạo toàn bộ hệ thống
- **Service Interface**: Vtable cho service lifecycle management
- **Event Handler Registry**: Hệ thống đăng ký và xử lý events
- **Lazy Loader**: Khởi tạo services on-demand
- **Error Handler**: Centralized error handling
- **Metrics**: Hệ thống thu thập metrics

---

## 1. sx_bootstrap.h / sx_bootstrap.c

### A) Vai Trò File

**sx_bootstrap** là entry point duy nhất của hệ thống, được gọi từ `app_main()`. File này khởi tạo toàn bộ hệ thống theo thứ tự:
1. NVS flash
2. Error handler
3. Settings service
4. Dispatcher
5. Orchestrator task
6. Platform (display, touch)
7. SD card
8. Assets
9. UI task
10. Core services (audio, ducking, router)
11. Lazy services (được khởi tạo on-demand)

**Dependencies trực tiếp:**
```c
// Từ sx_bootstrap.c:1-51
#include "sx_dispatcher.h"
#include "sx_orchestrator.h"
#include "sx_error_handler.h"
#include "sx_platform.h"
#include "sx_ui.h"
#include "sx_assets.h"
// ... và nhiều service headers
```

### B) Public API

```c
// sx_bootstrap.h:10
esp_err_t sx_bootstrap_start(void);
```

**Contract:**
- **Input**: Không có
- **Output**: `ESP_OK` nếu thành công, `ESP_FAIL` nếu có lỗi critical
- **Pre-conditions**: ESP-IDF đã được khởi tạo, NVS partition tồn tại
- **Post-conditions**: Hệ thống đã sẵn sàng, orchestrator và UI task đang chạy
- **Error model**: 
  - `ESP_OK`: Bootstrap thành công
  - `ESP_FAIL`: Lỗi critical (dispatcher init failed, display init failed)
  - Các lỗi non-critical được log nhưng không dừng bootstrap

**Call site:**
```12:10:app/app_main.c
void app_main(void) {
    ESP_LOGI(TAG, "hai-os-simplexl starting...");
    ESP_ERROR_CHECK(sx_bootstrap_start());
}
```

### C) Data Model

Không có data structure public. File này chỉ chứa logic khởi tạo.

### D) Concurrency

- **Context**: Chạy trong `app_main()` task (main task, priority 1)
- **Thread safety**: Không thread-safe - chỉ được gọi một lần duy nhất tại boot
- **Blocking**: Có thể block trong thời gian dài (khởi tạo display, mount SD card)
- **Tasks tạo ra**:
  - Orchestrator task (priority 8, core tskNO_AFFINITY)
  - UI task (được tạo bởi `sx_ui_start()`)

### E) Memory Ownership

- **Static state**: Không có static state trong file này
- **Allocated resources**: 
  - NVS flash (managed bởi ESP-IDF)
  - Display handles (owned bởi sx_platform)
  - Touch handles (owned bởi sx_platform)
  - Service instances (owned bởi các service modules)

### F) Side Effects

1. **NVS**: Khởi tạo NVS flash, có thể erase nếu cần (```62:68:components/sx_core/sx_bootstrap.c```)
2. **Display**: Khởi tạo LCD panel và backlight
3. **Touch**: Khởi tạo touch driver (nếu enabled)
4. **SD Card**: Mount SD card vào filesystem (```191:211:components/sx_core/sx_bootstrap.c```)
5. **SPI Bus**: Khởi tạo SPI bus manager cho SD card
6. **FreeRTOS**: Tạo orchestrator task và UI task

### G) Call Sites

1. **app_main()** - Entry point duy nhất (```10:10:app/app_main.c```)

### H) Issues/Risks

1. **P0 - Boot Time**: Bootstrap có thể mất nhiều thời gian (display init, SD mount). Không có timeout cho các operations.
   - **Điều kiện**: SD card chậm hoặc không có
   - **Cách tái hiện**: Cắm SD card chậm hoặc không cắm SD card
   - **Impact**: Boot time tăng đáng kể

2. **P1 - Error Recovery**: Nếu một service init failed (non-critical), hệ thống vẫn tiếp tục nhưng có thể thiếu tính năng.
   - **Điều kiện**: Service init failed nhưng không critical
   - **Cách tái hiện**: Cấu hình sai cho một service
   - **Impact**: Tính năng không hoạt động nhưng hệ thống vẫn boot

3. **P2 - Memory Leak**: Nếu một service init failed sau khi đã allocate resources, có thể leak memory.
   - **Điều kiện**: Service init failed ở giữa chừng
   - **Cách tái hiện**: Force error trong service init
   - **Impact**: Memory leak nhỏ

### I) Đề Xuất Cải Thiện

1. **P0**: Thêm timeout cho SD card mount (ví dụ: 5 giây)
2. **P1**: Thêm rollback mechanism khi service init failed
3. **P2**: Thêm health check sau bootstrap để verify tất cả critical services đã sẵn sàng

---

## 2. sx_dispatcher.h / sx_dispatcher.c

### A) Vai Trò File

**sx_dispatcher** là trung tâm event-driven architecture, cung cấp:
- **Event Queue**: 4 priority queues (LOW, NORMAL, HIGH, CRITICAL)
- **State Management**: Double-buffer pattern cho state snapshot
- **Backpressure Policies**: DROP, COALESCE, BLOCK
- **Metrics**: Event drop/coalesce counters

**Dependencies trực tiếp:**
```c
// sx_dispatcher.c:1-3
#include "sx_dispatcher.h"
#include "sx_event_string_pool.h"
#include "sx_metrics.h"
```

### B) Public API

```c
// sx_dispatcher.h:25-52
bool sx_dispatcher_init(void);
bool sx_dispatcher_post_event(const sx_event_t *evt);
bool sx_dispatcher_post_event_with_policy(const sx_event_t *evt, sx_backpressure_policy_t policy, uint32_t coalesce_key);
bool sx_dispatcher_poll_event(sx_event_t *out_evt);
void sx_dispatcher_set_state(const sx_state_t *state);
void sx_dispatcher_get_state(sx_state_t *out_state);
void sx_dispatcher_get_drop_count(uint32_t drop_count[4]);
void sx_dispatcher_get_coalesce_count(uint32_t coalesce_count[4]);
```

**Contract:**

**`sx_dispatcher_init()`**
- **Input**: Không có
- **Output**: `true` nếu thành công, `false` nếu failed
- **Pre-conditions**: FreeRTOS đã được khởi tạo
- **Post-conditions**: 4 priority queues và state buffers đã được khởi tạo
- **Error model**: `false` nếu queue creation failed

**`sx_dispatcher_post_event()`**
- **Input**: `evt` - event to post
- **Output**: `true` nếu posted, `false` nếu dropped
- **Pre-conditions**: Dispatcher đã được init
- **Post-conditions**: Event đã được đưa vào queue hoặc dropped
- **Error model**: `false` nếu queue full và policy là DROP

**`sx_dispatcher_post_event_with_policy()`**
- **Input**: `evt`, `policy` (DROP/COALESCE/BLOCK), `coalesce_key`
- **Output**: `true` nếu accepted, `false` nếu dropped
- **Pre-conditions**: Dispatcher đã được init
- **Post-conditions**: Event đã được xử lý theo policy
- **Error model**: `false` nếu dropped hoặc block timeout

**`sx_dispatcher_poll_event()`**
- **Input**: `out_evt` - buffer để nhận event
- **Output**: `true` nếu có event, `false` nếu không có
- **Pre-conditions**: Dispatcher đã được init
- **Post-conditions**: Event đã được copy vào `out_evt` (nếu có)
- **Error model**: `false` nếu không có event

**`sx_dispatcher_set_state()`**
- **Input**: `state` - state snapshot
- **Output**: Không có
- **Pre-conditions**: Dispatcher đã được init
- **Post-conditions**: State đã được publish (double-buffer swap)
- **Error model**: Không có - function là void

**`sx_dispatcher_get_state()`**
- **Input**: `out_state` - buffer để nhận state
- **Output**: Không có
- **Pre-conditions**: Dispatcher đã được init
- **Post-conditions**: State snapshot đã được copy vào `out_state`
- **Error model**: Không có - function là void

### C) Data Model

```c
// sx_dispatcher.h:19-23
typedef enum {
    SX_BP_DROP,        // Drop if queue full
    SX_BP_COALESCE,    // Keep only latest event
    SX_BP_BLOCK,       // Block with timeout
} sx_backpressure_policy_t;
```

**Static State** (```14:45:components/sx_core/sx_dispatcher.c```):
- `s_evt_q_low`, `s_evt_q_normal`, `s_evt_q_high`, `s_evt_q_critical`: 4 priority queues
- `s_state_front`, `s_state_back`: Double-buffer cho state
- `s_state_read_ptr`: Atomic pointer đến front buffer
- `s_state_write_mutex`: Mutex cho write operations
- `s_drop_count[4]`, `s_coalesce_count[4]`: Metrics counters
- `s_coalesce_table[]`: Coalesce tracking table

**Invariants:**
- `s_state_read_ptr` luôn point đến một trong hai buffers
- `s_state_write_mutex` được hold khi write state
- Queue sizes: LOW=16, NORMAL=32, HIGH=16, CRITICAL=8

### D) Concurrency

- **Context**: 
  - **Writers (post_event)**: Bất kỳ task nào (multi-producer)
  - **Reader (poll_event)**: Chỉ orchestrator task (single-consumer)
  - **State writer (set_state)**: Chỉ orchestrator task
  - **State readers (get_state)**: Bất kỳ task nào (multi-reader)

- **Thread Safety**:
  - **Event queues**: Thread-safe (FreeRTOS queues)
  - **State write**: Protected bởi `s_state_write_mutex` (```327:347:components/sx_core/sx_dispatcher.c```)
  - **State read**: Lock-free (đọc từ atomic pointer) (```354:365:components/sx_core/sx_dispatcher.c```)

- **Lock Order**: 
  - Không có lock order issue vì chỉ có một mutex (`s_state_write_mutex`)
  - Event queues không cần lock (FreeRTOS queues là thread-safe)

- **Deadlock Risk**: 
  - **LOW**: Chỉ có một mutex, không có circular dependency
  - **Potential issue**: Nếu orchestrator task bị block khi write state, có thể delay event processing

### E) Memory Ownership

- **Event payload (`evt->ptr`)**: 
  - **Ownership**: Caller owns pointer, dispatcher chỉ copy event struct
  - **Lifetime**: Caller phải đảm bảo pointer valid cho đến khi event được consume
  - **String pool**: Một số events sử dụng `sx_event_string_pool` để allocate strings (```92:100:components/sx_core/sx_error_handler.c```)

- **State buffers**: 
  - **Ownership**: Dispatcher owns buffers
  - **Lifetime**: Persistent trong suốt lifetime của dispatcher

- **Coalesce table**: 
  - **Ownership**: Dispatcher owns table
  - **Lifetime**: Persistent, entries được invalidate khi flush

### F) Side Effects

1. **FreeRTOS**: Tạo 4 queues và 1 mutex
2. **Metrics**: Update drop/coalesce counters
3. **Logging**: Rate-limited logging khi events bị dropped (```230:236:components/sx_core/sx_dispatcher.c```)

### G) Call Sites

1. **sx_bootstrap_start()** - Init dispatcher (```125:129:components/sx_core/sx_bootstrap.c```)
2. **sx_orchestrator_task()** - Poll events (```104:104:components/sx_core/sx_orchestrator.c```)
3. **sx_orchestrator_task()** - Set state (```161:161:components/sx_core/sx_orchestrator.c```)
4. **sx_ui_task()** - Get state (từ UI task, không có trong batch này)
5. **Các services** - Post events (nhiều nơi)

### H) Issues/Risks

1. **P0 - Queue Overflow**: Nếu queue đầy và policy là DROP, events sẽ bị mất.
   - **Điều kiện**: High event rate, queue đầy
   - **Cách tái hiện**: Post events liên tục với rate cao
   - **Impact**: Mất events, có thể mất tính năng

2. **P1 - Coalesce Table Overflow**: Coalesce table có giới hạn 32 entries (```37:44:components/sx_core/sx_dispatcher.c```).
   - **Điều kiện**: Nhiều event types cần coalesce
   - **Cách tái hiện**: Post nhiều event types với COALESCE policy
   - **Impact**: Coalesce table đầy, events bị drop

3. **P2 - State Read Race**: Mặc dù lock-free, nhưng nếu state struct lớn, copy có thể không atomic.
   - **Điều kiện**: State struct lớn, nhiều readers
   - **Cách tái hiện**: Đọc state từ nhiều tasks đồng thời
   - **Impact**: Có thể đọc được state không nhất quán (nhưng không crash)

### I) Đề Xuất Cải Thiện

1. **P0**: Tăng queue sizes hoặc thêm dynamic queue allocation
2. **P1**: Tăng coalesce table size hoặc dùng hash table
3. **P2**: Thêm version check cho state read để detect inconsistent reads

---

## 3. sx_orchestrator.h / sx_orchestrator.c

### A) Vai Trò File

**sx_orchestrator** là single consumer của event queue, xử lý events và update state. Đây là single-writer cho state.

**Dependencies trực tiếp:**
```c
// sx_orchestrator.c:1-7
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_state.h"
#include "sx_state_helper.h"
#include "sx_event_handler.h"
#include "sx_metrics.h"
```

### B) Public API

```c
// sx_orchestrator.h:7
void sx_orchestrator_start(void);
```

**Contract:**
- **Input**: Không có
- **Output**: Không có (void)
- **Pre-conditions**: Dispatcher đã được init
- **Post-conditions**: Orchestrator task đã được tạo và đang chạy
- **Error model**: Không có - function là void, task creation có thể fail silently

**Internal API (không public):**
- `sx_orchestrator_task()`: Main loop của orchestrator task

### C) Data Model

Không có data structure public. File này chỉ chứa task logic.

**Static State** (trong task):
- `st`: Local state copy (```61:62:components/sx_core/sx_orchestrator.c```)
- Event handler registry (managed bởi `sx_event_handler`)

### D) Concurrency

- **Context**: Chạy trong FreeRTOS task "sx_orch" (priority 8, core tskNO_AFFINITY)
- **Thread Safety**: 
  - Single consumer - chỉ có một orchestrator task
  - State write được protect bởi dispatcher mutex
- **Blocking**: 
  - Poll events với timeout 0 (non-blocking) (```104:104:components/sx_core/sx_orchestrator.c```)
  - Sleep 10ms nếu không có events (```170:174:components/sx_core/sx_orchestrator.c```)

### E) Memory Ownership

- **Event payload**: 
  - **Ownership**: Orchestrator không own - chỉ đọc
  - **Lifetime**: Valid trong suốt event processing
  - **Cleanup**: Caller (service) phải cleanup

- **State**: 
  - **Ownership**: Orchestrator owns state updates
  - **Lifetime**: Persistent trong dispatcher buffers

### F) Side Effects

1. **State**: Update global state thông qua dispatcher
2. **Event Handlers**: Gọi event handlers đã đăng ký (```118:118:components/sx_core/sx_orchestrator.c```)
3. **Metrics**: Update metrics (```163:165:components/sx_core/sx_orchestrator.c```)

### G) Call Sites

1. **sx_bootstrap_start()** - Start orchestrator (```133:134:components/sx_core/sx_bootstrap.c```)

### H) Issues/Risks

1. **P0 - Event Handler Crash**: Nếu event handler crash, orchestrator task có thể bị kill.
   - **Điều kiện**: Event handler có bug (null pointer, assert)
   - **Cách tái hiện**: Post event trigger bug trong handler
   - **Impact**: Orchestrator task crash, hệ thống mất event processing

2. **P1 - State Update Race**: Dirty mask được tính từ event type, có thể không chính xác (```121:157:components/sx_core/sx_orchestrator.c```).
   - **Điều kiện**: Event handler update nhiều domains
   - **Cách tái hiện**: Handler update nhiều state fields
   - **Impact**: Dirty mask không chính xác, UI có thể không update đúng

3. **P2 - Polling Overhead**: Poll events với timeout 0 có thể tốn CPU nếu không có events.
   - **Điều kiện**: Không có events
   - **Cách tái hiện**: Idle system
   - **Impact**: CPU usage cao không cần thiết

### I) Đề Xuất Cải Thiện

1. **P0**: Thêm try-catch wrapper cho event handlers
2. **P1**: Event handlers nên return dirty_mask thay vì tính từ event type
3. **P2**: Dùng blocking queue receive với timeout thay vì polling

---

## 4. sx_event.h

### A) Vai Trò File

**sx_event.h** định nghĩa event taxonomy, priority, và event structure. Đây là contract cơ bản cho event-driven architecture.

**Dependencies trực tiếp:**
```c
// sx_event.h:4
#include "sx_event_payloads.h"
```

### B) Public API

Không có functions, chỉ có type definitions.

### C) Data Model

```c
// sx_event.h:11-16
typedef enum {
    SX_EVT_PRIORITY_LOW = 0,
    SX_EVT_PRIORITY_NORMAL = 1,
    SX_EVT_PRIORITY_HIGH = 2,
    SX_EVT_PRIORITY_CRITICAL = 3,
} sx_event_priority_t;
```

```c
// sx_event.h:177-183
typedef struct {
    sx_event_type_t type;
    sx_event_priority_t priority;
    uint32_t arg0;
    uint32_t arg1;
    const void *ptr;
} sx_event_t;
```

**Event ID Ranges** (```29:43:components/sx_core/include/sx_event.h```):
- `SX_EVT_LIFECYCLE_BASE` (0x0000-0x00FF)
- `SX_EVT_UI_BASE` (0x0100-0x01FF)
- `SX_EVT_AUDIO_BASE` (0x0200-0x02FF)
- `SX_EVT_RADIO_BASE` (0x0300-0x03FF)
- `SX_EVT_WIFI_BASE` (0x0400-0x04FF)
- `SX_EVT_CHATBOT_BASE` (0x0500-0x05FF)
- `SX_EVT_SYSTEM_BASE` (0x0600-0x06FF)
- `SX_EVT_PROTOCOL_BASE` (0x0700-0x07FF)
- `SX_EVT_OTA_BASE` (0x0800-0x08FF)
- `SX_EVT_DISPLAY_BASE` (0x0900-0x09FF)
- `SX_EVT_THEME_BASE` (0x0A00-0x0AFF)
- `SX_EVT_IMAGE_BASE` (0x0B00-0x0BFF)
- `SX_EVT_QRCODE_BASE` (0x0C00-0x0CFF)

**Invariants:**
- Event type phải nằm trong range đã định nghĩa
- Priority mặc định được tính từ event type (```19:23:components/sx_core/include/sx_event.h```)
- `ptr` field có thể là NULL hoặc pointer đến payload

### D) Concurrency

Không có concurrency concerns - chỉ là type definitions.

### E) Memory Ownership

- **Event struct**: 
  - **Ownership**: Caller owns event struct
  - **Lifetime**: Valid cho đến khi được consume
  - **Copy**: Dispatcher copy event struct vào queue

- **Event payload (`ptr` field)**: 
  - **Ownership**: Caller owns pointer
  - **Lifetime**: Valid cho đến khi được consume
  - **String payload**: Có thể dùng `sx_event_string_pool` để allocate

### F) Side Effects

Không có side effects - chỉ là type definitions.

### G) Call Sites

Được sử dụng ở mọi nơi post/consume events:
1. **sx_dispatcher_post_event()** - Post events
2. **sx_orchestrator_task()** - Consume events
3. **Các services** - Post events

### H) Issues/Risks

1. **P1 - Event Type Collision**: Nếu thêm event type mới không theo range, có thể collision.
   - **Điều kiện**: Thêm event type mới
   - **Cách tái hiện**: Define event type ngoài range
   - **Impact**: Event type collision, handler có thể nhầm lẫn

2. **P2 - Payload Lifetime**: Nếu payload pointer không valid khi consume, có thể crash.
   - **Điều kiện**: Payload được free trước khi consume
   - **Cách tái hiện**: Free payload ngay sau post event
   - **Impact**: Null pointer dereference, crash

### I) Đề Xuất Cải Thiện

1. **P1**: Thêm compile-time check để đảm bảo event types nằm trong range
2. **P2**: Document rõ ownership rules cho payload pointers

---

## 5. sx_state.h

### A) Vai Trò File

**sx_state.h** định nghĩa global state structure, được dùng cho single-writer, multi-reader pattern.

**Dependencies trực tiếp:**
Không có dependencies ngoài standard types.

### B) Public API

Không có functions, chỉ có type definitions.

### C) Data Model

```c
// sx_state.h:14-20
typedef enum {
    SX_DEV_UNKNOWN = 0,
    SX_DEV_BOOTING,
    SX_DEV_IDLE,
    SX_DEV_BUSY,
    SX_DEV_ERROR,
} sx_device_state_t;
```

```c
// sx_state.h:90-97
typedef struct {
    uint32_t version;      // Monotonically increasing version
    uint32_t dirty_mask;   // Bitmask indicating which domains changed
    uint32_t seq;         // Legacy: monotonically increasing snapshot sequence
    sx_wifi_state_t wifi;
    sx_audio_state_t audio;
    sx_ui_state_t ui;
} sx_state_t;
```

**Dirty Mask Bits** (```83:88:components/sx_core/include/sx_state.h```):
- `SX_STATE_DIRTY_WIFI` (bit 0)
- `SX_STATE_DIRTY_AUDIO` (bit 1)
- `SX_STATE_DIRTY_UI` (bit 2)
- `SX_STATE_DIRTY_SYSTEM` (bit 3)

**Invariants:**
- `version` luôn tăng khi state thay đổi
- `dirty_mask` chỉ set bits cho domains đã thay đổi
- `seq` được giữ cho backward compatibility

### D) Concurrency

- **Writer**: Chỉ orchestrator task (single-writer)
- **Readers**: Bất kỳ task nào (multi-reader)
- **Thread Safety**: 
  - Write: Protected bởi dispatcher mutex
  - Read: Lock-free (double-buffer pattern)

### E) Memory Ownership

- **State buffers**: 
  - **Ownership**: Dispatcher owns buffers
  - **Lifetime**: Persistent trong suốt lifetime của dispatcher

- **State snapshots**: 
  - **Ownership**: Caller owns snapshot copy
  - **Lifetime**: Valid cho đến khi được free hoặc overwrite

### F) Side Effects

Không có side effects trực tiếp - chỉ là data structure.

### G) Call Sites

Được sử dụng ở mọi nơi đọc/ghi state:
1. **sx_dispatcher_set_state()** - Write state
2. **sx_dispatcher_get_state()** - Read state
3. **sx_orchestrator_task()** - Update state
4. **sx_ui_task()** - Read state (từ UI task)

### H) Issues/Risks

1. **P1 - State Size**: State struct lớn (~500+ bytes), copy có thể tốn thời gian.
   - **Điều kiện**: Nhiều readers đọc state đồng thời
   - **Cách tái hiện**: Đọc state từ nhiều tasks
   - **Impact**: CPU overhead khi copy state

2. **P2 - Dirty Mask Accuracy**: Dirty mask được tính từ event type, có thể không chính xác.
   - **Điều kiện**: Event handler update nhiều domains
   - **Cách tái hiện**: Handler update nhiều state fields
   - **Impact**: Dirty mask không chính xác

### I) Đề Xuất Cải Thiện

1. **P1**: Optimize state struct size hoặc dùng pointer sharing (nhưng phức tạp hơn)
2. **P2**: Event handlers nên return dirty_mask thay vì tính từ event type

---

## 6. sx_service_if.h / sx_service_if.c

### A) Vai Trò File

**sx_service_if** định nghĩa service lifecycle interface (vtable) và registry để quản lý tất cả services.

**Dependencies trực tiếp:**
```c
// sx_service_if.c:1-6
#include "sx_service_if.h"
#include <esp_log.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
```

### B) Public API

```c
// sx_service_if.h:65-101
esp_err_t sx_service_register(const char *name, const sx_service_if_t *iface);
esp_err_t sx_service_unregister(const char *name);
esp_err_t sx_service_init_all(void);
esp_err_t sx_service_start_all(void);
esp_err_t sx_service_stop_all(void);
esp_err_t sx_service_deinit_all(void);
```

**Contract:**

**`sx_service_register()`**
- **Input**: `name` (service name), `iface` (vtable)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Registry chưa đầy (max 32 services)
- **Post-conditions**: Service đã được đăng ký
- **Error model**: 
  - `ESP_ERR_INVALID_ARG`: name hoặc iface là NULL
  - `ESP_ERR_INVALID_STATE`: Service đã được đăng ký
  - `ESP_ERR_NO_MEM`: Registry đầy

**`sx_service_init_all()`**
- **Input**: Không có
- **Output**: `ESP_OK` nếu tất cả init thành công, `ESP_FAIL` nếu có service init failed
- **Pre-conditions**: Services đã được đăng ký
- **Post-conditions**: Tất cả services đã được init (hoặc failed)
- **Error model**: `ESP_FAIL` nếu có service init failed (nhưng vẫn tiếp tục init các services khác)

### C) Data Model

```c
// sx_service_if.h:20-56
typedef struct {
    esp_err_t (*init)(void);
    esp_err_t (*start)(void);
    esp_err_t (*stop)(void);
    esp_err_t (*deinit)(void);
    esp_err_t (*on_event)(const sx_event_t *evt);
} sx_service_if_t;
```

**Static State** (```11:21:components/sx_core/sx_service_if.c```):
- `s_service_registry[]`: Array of service entries (max 32)
- `s_service_count`: Số lượng services đã đăng ký
- `s_registry_mutex`: Mutex để protect registry

**Invariants:**
- Mỗi service name chỉ được đăng ký một lần
- Registry không vượt quá 32 services

### D) Concurrency

- **Context**: 
  - **Register/Unregister**: Bất kỳ task nào (thread-safe)
  - **Init/Start/Stop/Deinit**: Thường được gọi từ bootstrap hoặc shutdown
- **Thread Safety**: 
  - Registry được protect bởi `s_registry_mutex` (```39:39:components/sx_core/sx_service_if.c```)
  - Service vtable calls không được protect (service phải tự thread-safe)

### E) Memory Ownership

- **Service vtable**: 
  - **Ownership**: Service module owns vtable (thường là static const)
  - **Lifetime**: Persistent trong suốt lifetime của service

- **Service registry**: 
  - **Ownership**: sx_service_if owns registry
  - **Lifetime**: Persistent trong suốt lifetime của hệ thống

### F) Side Effects

1. **Service Lifecycle**: Gọi service lifecycle methods (init, start, stop, deinit)
2. **FreeRTOS**: Tạo mutex để protect registry

### G) Call Sites

1. **Service modules** - Register services (nhiều nơi)
2. **sx_bootstrap_start()** - Có thể gọi init_all/start_all (nhưng hiện tại không dùng)

### H) Issues/Risks

1. **P1 - Registry Full**: Registry có giới hạn 32 services (```11:11:components/sx_core/sx_service_if.c```).
   - **Điều kiện**: Nhiều hơn 32 services
   - **Cách tái hiện**: Đăng ký hơn 32 services
   - **Impact**: Service không thể đăng ký, có thể mất tính năng

2. **P2 - Service Init Order**: `init_all()` không có dependency ordering.
   - **Điều kiện**: Service A phụ thuộc Service B
   - **Cách tái hiện**: Init service A trước service B
   - **Impact**: Service A init failed vì thiếu dependency

### I) Đề Xuất Cải Thiện

1. **P1**: Tăng registry size hoặc dùng dynamic allocation
2. **P2**: Thêm dependency graph để init services theo thứ tự đúng

---

## 7. sx_event_handler.h / sx_event_handler.c

### A) Vai Trò File

**sx_event_handler** cung cấp event handler registry để đăng ký và xử lý events theo type.

**Dependencies trực tiếp:**
```c
// sx_event_handler.c:1-2
#include "sx_event_handler.h"
#include <esp_log.h>
```

### B) Public API

```c
// sx_event_handler.h:28-50
esp_err_t sx_event_handler_register(sx_event_type_t event_type, sx_event_handler_t handler);
esp_err_t sx_event_handler_unregister(sx_event_type_t event_type);
bool sx_event_handler_process(const sx_event_t *evt, sx_state_t *state);
esp_err_t sx_event_handler_init(void);
```

**Contract:**

**`sx_event_handler_register()`**
- **Input**: `event_type`, `handler` function
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Handler system đã được init
- **Post-conditions**: Handler đã được đăng ký cho event type
- **Error model**: 
  - `ESP_ERR_INVALID_STATE`: Handler system chưa init
  - `ESP_ERR_INVALID_ARG`: event_type >= 64 hoặc handler là NULL

**`sx_event_handler_process()`**
- **Input**: `evt`, `state`
- **Output**: `true` nếu handled, `false` nếu không có handler
- **Pre-conditions**: Handler system đã được init
- **Post-conditions**: Handler đã được gọi (nếu có)
- **Error model**: `false` nếu không có handler hoặc invalid event type

### C) Data Model

```c
// sx_event_handler.h:19
typedef bool (*sx_event_handler_t)(const sx_event_t *evt, sx_state_t *state);
```

**Static State** (```7:9:components/sx_core/sx_event_handler.c```):
- `s_handlers[]`: Array of handlers (max 64 event types)
- `s_initialized`: Init flag

**Invariants:**
- Mỗi event type chỉ có một handler (register mới sẽ overwrite)
- Handler array size = 64 (MAX_EVENT_TYPES)

### D) Concurrency

- **Context**: 
  - **Register/Unregister**: Thường được gọi từ orchestrator task init
  - **Process**: Chỉ được gọi từ orchestrator task
- **Thread Safety**: 
  - Không thread-safe - handlers array không được protect
  - Nhưng chỉ orchestrator task gọi process, nên an toàn

### E) Memory Ownership

- **Handler functions**: 
  - **Ownership**: Service module owns handler functions (thường là static)
  - **Lifetime**: Persistent trong suốt lifetime của service

- **Handler registry**: 
  - **Ownership**: sx_event_handler owns registry
  - **Lifetime**: Persistent trong suốt lifetime của hệ thống

### F) Side Effects

1. **State**: Handlers có thể modify state
2. **Events**: Handlers có thể post events mới

### G) Call Sites

1. **sx_orchestrator_task()** - Register handlers (```24:50:components/sx_core/sx_orchestrator.c```)
2. **sx_orchestrator_task()** - Process events (```118:118:components/sx_core/sx_orchestrator.c```)

### H) Issues/Risks

1. **P1 - Handler Array Size**: Handler array có giới hạn 64 event types (```5:5:components/sx_core/sx_event_handler.c```).
   - **Điều kiện**: Nhiều hơn 64 event types
   - **Cách tái hiện**: Đăng ký handler cho event type >= 64
   - **Impact**: Handler không được đăng ký, events không được xử lý

2. **P2 - Handler Overwrite**: Register handler mới sẽ overwrite handler cũ.
   - **Điều kiện**: Đăng ký nhiều handlers cho cùng event type
   - **Cách tái hiện**: Register handler mới cho event type đã có handler
   - **Impact**: Handler cũ bị mất, có thể mất tính năng

### I) Đề Xuất Cải Thiện

1. **P1**: Tăng array size hoặc dùng hash table
2. **P2**: Thêm warning khi overwrite handler hoặc support multiple handlers per event type

---

## 8. sx_lazy_loader.h / sx_lazy_loader.c

### A) Vai Trò File

**sx_lazy_loader** quản lý lazy initialization của services - chỉ init services khi cần thiết để giảm boot time và memory usage.

**Dependencies trực tiếp:**
```c
// sx_lazy_loader.c:1-33
#include "sx_lazy_loader.h"
// ... nhiều service headers
```

### B) Public API

```c
// sx_lazy_loader.h:48-67
esp_err_t sx_lazy_service_init(sx_lazy_service_t service);
bool sx_lazy_service_is_initialized(sx_lazy_service_t service);
esp_err_t sx_lazy_service_deinit(sx_lazy_service_t service);
```

**Contract:**

**`sx_lazy_service_init()`**
- **Input**: `service` type
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Service chưa được init (hoặc đã init thì no-op)
- **Post-conditions**: Service đã được init (nếu chưa init)
- **Error model**: 
  - `ESP_ERR_INVALID_ARG`: Invalid service type
  - Service-specific errors từ service init

**`sx_lazy_service_is_initialized()`**
- **Input**: `service` type
- **Output**: `true` nếu đã init, `false` nếu chưa
- **Pre-conditions**: Không có
- **Post-conditions**: Không có
- **Error model**: `false` nếu invalid service type

### C) Data Model

```c
// sx_lazy_loader.h:15-37
typedef enum {
    SX_LAZY_SERVICE_WIFI,
    SX_LAZY_SERVICE_STT,
    SX_LAZY_SERVICE_WAKE_WORD,
    // ... nhiều services
    SX_LAZY_SERVICE_MAX,
} sx_lazy_service_t;
```

**Static State** (```37:38:components/sx_core/sx_lazy_loader.c```):
- `s_service_initialized[]`: Array of init flags
- `s_mutex`: Mutex để protect init flags

**Invariants:**
- Mỗi service chỉ được init một lần (subsequent calls là no-op)
- Init flags được protect bởi mutex

### D) Concurrency

- **Context**: Bất kỳ task nào có thể gọi lazy init
- **Thread Safety**: 
  - Init flags được protect bởi `s_mutex` (```57:67:components/sx_core/sx_lazy_loader.c```)
  - Service init calls không được protect (service phải tự thread-safe)

### E) Memory Ownership

- **Service instances**: 
  - **Ownership**: Service module owns instances
  - **Lifetime**: Persistent sau khi init (không có deinit thực sự)

- **Init flags**: 
  - **Ownership**: sx_lazy_loader owns flags
  - **Lifetime**: Persistent trong suốt lifetime của hệ thống

### F) Side Effects

1. **Service Lifecycle**: Gọi service init/start methods
2. **Settings**: Đọc settings từ NVS để config services
3. **FreeRTOS**: Tạo mutex để protect init flags

### G) Call Sites

1. **UI screens** - Init services khi cần (ví dụ: init WiFi khi vào WiFi settings)
2. **Event handlers** - Init services khi nhận events (ví dụ: init STT khi bắt đầu voice input)

### H) Issues/Risks

1. **P1 - Concurrent Init**: Nếu nhiều tasks gọi init cùng lúc, có thể init service nhiều lần.
   - **Điều kiện**: Race condition giữa check và set init flag
   - **Cách tái hiện**: Nhiều tasks gọi init cùng lúc
   - **Impact**: Service có thể được init nhiều lần, có thể leak memory

2. **P2 - Deinit Not Implemented**: `deinit()` không được implement (```394:399:components/sx_core/sx_lazy_loader.c```).
   - **Điều kiện**: Cần deinit service để free memory
   - **Cách tái hiện**: Gọi deinit
   - **Impact**: Không thể deinit services, memory không được free

### I) Đề Xuất Cải Thiện

1. **P1**: Thêm double-check locking để tránh race condition
2. **P2**: Implement deinit cho các services có thể deinit được

---

## 9. sx_error_handler.h / sx_error_handler.c

### A) Vai Trò File

**sx_error_handler** cung cấp centralized error handling system để track và report errors theo category.

**Dependencies trực tiếp:**
```c
// sx_error_handler.c:1-9
#include "sx_error_handler.h"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_event_string_pool.h"
```

### B) Public API

```c
// sx_error_handler.h:43-92
esp_err_t sx_error_handler_init(void);
esp_err_t sx_error_handler_set_error(sx_error_category_t category, esp_err_t error, const char *message, sx_error_severity_t severity);
bool sx_error_handler_get_error(sx_error_category_t category, sx_error_info_t *out_info);
void sx_error_handler_clear_error(sx_error_category_t category);
void sx_error_handler_clear_all(void);
bool sx_error_handler_has_any_error(void);
esp_err_t sx_error_handler_get_error_message(sx_error_category_t category, char *out_message, size_t max_len);
```

**Contract:**

**`sx_error_handler_set_error()`**
- **Input**: `category`, `error` code, `message`, `severity`
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Handler đã được init (hoặc tự init nếu chưa)
- **Post-conditions**: Error đã được set và logged
- **Error model**: `ESP_ERR_INVALID_ARG` nếu invalid category

### C) Data Model

```c
// sx_error_handler.h:14-29
typedef enum {
    SX_ERROR_CATEGORY_PROTOCOL = 0,
    SX_ERROR_CATEGORY_AUDIO,
    SX_ERROR_CATEGORY_NETWORK,
    SX_ERROR_CATEGORY_SYSTEM,
    SX_ERROR_CATEGORY_UI,
    SX_ERROR_CATEGORY_COUNT
} sx_error_category_t;

typedef enum {
    SX_ERROR_SEVERITY_INFO = 0,
    SX_ERROR_SEVERITY_WARNING,
    SX_ERROR_SEVERITY_ERROR,
    SX_ERROR_SEVERITY_CRITICAL
} sx_error_severity_t;
```

```c
// sx_error_handler.h:32-37
typedef struct {
    esp_err_t code;
    sx_error_severity_t severity;
    uint32_t timestamp;
    char message[128];
} sx_error_info_t;
```

**Static State** (```16:17:components/sx_core/sx_error_handler.c```):
- `s_errors[]`: Array of error info (one per category)
- `s_initialized`: Init flag

**Invariants:**
- Mỗi category chỉ có một error (set mới sẽ overwrite)
- Error với severity >= ERROR sẽ emit event (```91:100:components/sx_core/sx_error_handler.c```)

### D) Concurrency

- **Context**: Bất kỳ task nào có thể set/get errors
- **Thread Safety**: 
  - Không thread-safe - errors array không được protect
  - Nhưng chỉ write khi set error, read khi get error, nên ít risk

### E) Memory Ownership

- **Error messages**: 
  - **Ownership**: sx_error_handler owns messages (stored in struct)
  - **Lifetime**: Persistent cho đến khi clear

- **Error events**: 
  - **Ownership**: String pool owns error message strings (```92:92:components/sx_core/sx_error_handler.c```)
  - **Lifetime**: Managed bởi string pool

### F) Side Effects

1. **Logging**: Log errors theo severity (```66:88:components/sx_core/sx_error_handler.c```)
2. **Events**: Emit error events cho severity >= ERROR (```91:100:components/sx_core/sx_error_handler.c```)

### G) Call Sites

1. **sx_bootstrap_start()** - Init error handler (```71:76:components/sx_core/sx_bootstrap.c```)
2. **Services** - Set errors khi có lỗi (nhiều nơi)

### H) Issues/Risks

1. **P2 - Thread Safety**: Errors array không được protect bởi mutex.
   - **Điều kiện**: Nhiều tasks set/get errors đồng thời
   - **Cách tái hiện**: Set/get errors từ nhiều tasks
   - **Impact**: Có thể đọc được inconsistent error info

2. **P2 - Error Overwrite**: Set error mới sẽ overwrite error cũ (mất lịch sử).
   - **Điều kiện**: Set nhiều errors cho cùng category
   - **Cách tái hiện**: Set error mới cho category đã có error
   - **Impact**: Mất lịch sử errors

### I) Đề Xuất Cải Thiện

1. **P2**: Thêm mutex để protect errors array
2. **P2**: Thêm error history (ring buffer) để giữ lịch sử errors

---

## 10. sx_metrics.h / sx_metrics.c

### A) Vai Trò File

**sx_metrics** cung cấp metrics collection system để track performance và resource usage.

**Dependencies trực tiếp:**
```c
// sx_metrics.c:1-7
#include "sx_metrics.h"
#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_heap_caps.h>
```

### B) Public API

```c
// sx_metrics.h:48-120
bool sx_metrics_init(void);
void sx_metrics_get(sx_metrics_t *out_metrics);
void sx_metrics_reset(void);
void sx_metrics_inc_evt_posted(uint32_t priority);
void sx_metrics_inc_evt_dropped(uint32_t priority);
void sx_metrics_inc_evt_coalesced(uint32_t priority);
void sx_metrics_set_queue_depth(uint32_t priority, uint32_t depth);
void sx_metrics_set_state_version(uint32_t version);
void sx_metrics_inc_state_updates(void);
void sx_metrics_update_ui_render(uint32_t render_ms);
void sx_metrics_update_heap(uint32_t free_current, uint32_t free_min);
void sx_metrics_update_psram(uint32_t free_current, uint32_t free_min);
void sx_metrics_dump(void);
```

**Contract:**

**`sx_metrics_init()`**
- **Input**: Không có
- **Output**: `true` nếu thành công
- **Pre-conditions**: FreeRTOS đã được khởi tạo
- **Post-conditions**: Metrics system đã được init

**`sx_metrics_get()`**
- **Input**: `out_metrics` buffer
- **Output**: Không có (void)
- **Pre-conditions**: Metrics đã được init
- **Post-conditions**: Metrics snapshot đã được copy vào buffer

### C) Data Model

```c
// sx_metrics.h:17-42
typedef struct {
    uint32_t evt_posted_total[4];
    uint32_t evt_dropped_total[4];
    uint32_t evt_coalesced_total[4];
    uint32_t evt_processed_total;
    uint32_t queue_depth[4];
    uint32_t state_version;
    uint32_t state_updates_total;
    uint32_t ui_render_ms_last;
    uint32_t ui_render_ms_avg;
    uint32_t ui_render_ms_max;
    uint32_t ui_frames_total;
    uint32_t heap_free_min;
    uint32_t heap_free_current;
    uint32_t psram_free_min;
    uint32_t psram_free_current;
} sx_metrics_t;
```

**Static State** (```12:14:components/sx_core/sx_metrics.c```):
- `s_metrics`: Metrics storage
- `s_metrics_mutex`: Mutex để protect metrics

**Invariants:**
- Metrics counters chỉ tăng (trừ khi reset)
- Priority indices: 0=LOW, 1=NORMAL, 2=HIGH, 3=CRITICAL

### D) Concurrency

- **Context**: Bất kỳ task nào có thể update/get metrics
- **Thread Safety**: 
  - Metrics được protect bởi `s_metrics_mutex` (```53:55:components/sx_core/sx_metrics.c```)
  - Tất cả update/get operations đều được protect

### E) Memory Ownership

- **Metrics storage**: 
  - **Ownership**: sx_metrics owns storage
  - **Lifetime**: Persistent trong suốt lifetime của hệ thống

- **Metrics snapshots**: 
  - **Ownership**: Caller owns snapshot copy
  - **Lifetime**: Valid cho đến khi được free hoặc overwrite

### F) Side Effects

1. **Heap**: Đọc heap/PSRAM stats từ ESP-IDF (```31:42:components/sx_core/sx_metrics.c```)
2. **Logging**: Dump metrics to log (```191:237:components/sx_core/sx_metrics.c```)

### G) Call Sites

1. **sx_dispatcher_init()** - Init metrics (```49:49:components/sx_core/sx_dispatcher.c```)
2. **sx_dispatcher_post_event()** - Update event metrics (```211:217:components/sx_core/sx_dispatcher.c```)
3. **sx_orchestrator_task()** - Update state metrics (```163:165:components/sx_core/sx_orchestrator.c```)

### H) Issues/Risks

1. **P2 - Metrics Mutex Contention**: Nếu nhiều tasks update metrics đồng thời, có thể có contention.
   - **Điều kiện**: Nhiều tasks update metrics cùng lúc
   - **Cách tái hiện**: Update metrics từ nhiều tasks
   - **Impact**: Performance overhead do mutex contention

2. **P2 - Metrics Accuracy**: UI render metrics được update từ UI task, có thể không chính xác nếu UI task bị delay.
   - **Điều kiện**: UI task bị delay
   - **Cách tái hiện**: Block UI task
   - **Impact**: Metrics không chính xác

### I) Đề Xuất Cải Thiện

1. **P2**: Dùng lock-free counters cho một số metrics (nếu có thể)
2. **P2**: Thêm timestamp cho metrics để detect stale data

---

## Tổng Kết Component

### Điểm Mạnh

1. **Event-Driven Architecture**: Rõ ràng, dễ mở rộng
2. **State Management**: Double-buffer pattern hiệu quả
3. **Service Lifecycle**: Vtable pattern linh hoạt
4. **Lazy Loading**: Giảm boot time và memory usage
5. **Metrics**: Hệ thống metrics đầy đủ

### Điểm Yếu

1. **Error Recovery**: Không có rollback mechanism
2. **Thread Safety**: Một số components không thread-safe
3. **Resource Limits**: Nhiều hard-coded limits (queue sizes, registry sizes)
4. **Documentation**: Ownership rules chưa rõ ràng ở một số nơi

### Đề Xuất Cải Thiện Tổng Thể

1. **P0**: Thêm timeout cho các operations trong bootstrap
2. **P1**: Thêm thread safety cho error handler và event handler registry
3. **P1**: Tăng resource limits hoặc dùng dynamic allocation
4. **P2**: Document rõ ownership rules cho tất cả APIs

