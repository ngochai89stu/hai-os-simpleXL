# API Catalog: sx_ui

Component **sx_ui** cung cấp UI framework dựa trên LVGL, bao gồm screen lifecycle management, navigation router, và integration với state management.

## Tổng Quan Component

**sx_ui** cung cấp:
- **UI Task**: Main loop cho LVGL rendering và state polling
- **Screen Router**: Navigation system với lifecycle callbacks
- **Screen Registry**: Centralized screen registration và lookup
- **Screen Interface**: Vtable pattern cho screen lifecycle (create, show, hide, destroy, update)
- **LVGL Wrapper**: Compile-time và runtime guards để enforce LVGL chỉ được dùng trong sx_ui
- **29 Screens**: Boot, Home, Chat, Music Player, Settings, và nhiều screens khác

---

## 1. sx_ui.h / sx_ui_task.c

### A) Vai Trò File

**sx_ui** là entry point cho UI system, tạo UI task và khởi tạo LVGL. File này:
- Tạo FreeRTOS task cho UI rendering
- Khởi tạo LVGL port với display và touch
- Setup screen router và registry
- Main loop: poll state, update UI, handle LVGL timers

**Dependencies trực tiếp:**
```c
// sx_ui_task.c:1-20
#include "sx_ui.h"
#include "sx_lvgl.h"
#include "sx_lvgl_guard.h"
#include "esp_lvgl_port_disp.h"
#include "esp_lvgl_port_touch.h"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_state.h"
#include "sx_metrics.h"
#include "ui_router.h"
#include "ui_screen_registry.h"
```

### B) Public API

```c
// sx_ui.h:12
esp_err_t sx_ui_start(const sx_display_handles_t *handles, const sx_touch_handles_t *touch_handles);
```

**Contract:**
- **Input**: `handles` (display), `touch_handles` (optional, can be NULL)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Display đã được init, LVGL port chưa được init
- **Post-conditions**: UI task đã được tạo và đang chạy
- **Error model**: 
  - `ESP_ERR_INVALID_ARG`: handles hoặc panel_handle là NULL
  - `ESP_OK`: Task created successfully

### C) Data Model

**Task Arguments** (```29:32:components/sx_ui/sx_ui_task.c```):
```c
typedef struct {
    const sx_display_handles_t *display_handles;
    const sx_touch_handles_t *touch_handles;
} sx_ui_task_args_t;
```

**Static State** (trong task):
- `state`: Local state copy (```192:192:components/sx_ui/sx_ui_task.c```)
- `last_screen`: Last active screen ID (```193:193:components/sx_ui/sx_ui_task.c```)
- `last_state_seq`: Last state sequence number (```194:194:components/sx_ui/sx_ui_task.c```)

**Invariants:**
- UI task chỉ có một instance (created once tại boot)
- LVGL chỉ được access từ UI task (enforced bởi lvgl_port_lock)

### D) Concurrency

- **Context**: Chạy trong FreeRTOS task "sx_ui" (priority 7, core tskNO_AFFINITY)
- **Thread Safety**: 
  - LVGL operations: Protected bởi `lvgl_port_lock()` (```54:175:components/sx_ui/sx_ui_task.c```)
  - State read: Lock-free (```202:202:components/sx_ui/sx_ui_task.c```)
  - Event poll: Lock-free (```269:277:components/sx_ui/sx_ui_task.c```)
- **Blocking**: 
  - LVGL lock: Blocking (timeout 0 = wait indefinitely)
  - Render interval: 16ms (~60 FPS) (```198:198:components/sx_ui/sx_ui_task.c```)

### E) Memory Ownership

- **Display/Touch handles**: 
  - **Owner**: Caller owns (passed from bootstrap)
  - **Lifetime**: Valid trong suốt lifetime của UI task
  - **Cleanup**: ESP-IDF tự cleanup

- **LVGL objects**: 
  - **Owner**: LVGL owns (managed bởi LVGL memory pool)
  - **Lifetime**: Valid cho đến khi được delete bởi screen destroy callbacks
  - **Cleanup**: Screens tự cleanup trong `on_destroy()` callbacks

### F) Side Effects

1. **LVGL**: Initialize LVGL port, add display và touch input device (```47:159:components/sx_ui/sx_ui_task.c```)
2. **FreeRTOS**: Tạo UI task (```308:308:components/sx_ui/sx_ui_task.c```)
3. **State**: Poll state snapshot mỗi frame (```202:202:components/sx_ui/sx_ui_task.c```)
4. **Events**: Poll và handle display service events (```269:277:components/sx_ui/sx_ui_task.c```)
5. **Metrics**: Update UI render time metrics (```281:288:components/sx_ui/sx_ui_task.c```)

### G) Call Sites

1. **sx_bootstrap_start()** - Start UI task (```223:227:components/sx_core/sx_bootstrap.c```)

### H) Issues/Risks

1. **P0 - LVGL Lock Deadlock**: Nếu LVGL lock được hold lâu, có thể block other operations.
   - **Điều kiện**: Screen callback hold lock quá lâu
   - **Cách tái hiện**: Screen create/destroy callback block trong critical section
   - **Impact**: UI task blocked, frame drops, system hang

2. **P1 - State Polling Overhead**: Poll state mỗi frame (16ms) có thể tốn CPU nếu state không thay đổi.
   - **Điều kiện**: State không thay đổi
   - **Cách tái hiện**: Idle system
   - **Impact**: CPU overhead không cần thiết

3. **P2 - Navigation Race**: Nếu nhiều places gọi navigate đồng thời, có thể có race condition.
   - **Điều kiện**: Nhiều events trigger navigation cùng lúc
   - **Cách tái hiện**: Multiple events post navigation events
   - **Impact**: Screen navigation không nhất quán

### I) Đề Xuất Cải Thiện

1. **P0**: Thêm timeout cho LVGL lock operations
2. **P1**: Chỉ poll state khi có state change notification (dùng dirty_mask)
3. **P2**: Thêm navigation queue để serialize navigation requests

---

## 2. ui_router.h / ui_router.c

### A) Vai Trò File

**ui_router** cung cấp navigation system cho screens, quản lý screen lifecycle (create, show, hide, destroy) và screen container.

**Dependencies trực tiếp:**
```c
// ui_router.c:1-8
#include "ui_router.h"
#include "ui_screen_registry.h"
#include "sx_lvgl.h"
#include "sx_dispatcher.h"
#include "sx_state.h"
```

### B) Public API

```c
// ui_router.h:12-22
void ui_router_init(void);
void ui_router_navigate_to(ui_screen_id_t screen_id);
ui_screen_id_t ui_router_get_current_screen(void);
lv_obj_t* ui_router_get_container(void);
```

**Contract:**

**`ui_router_init()`**
- **Input**: Không có
- **Output**: Không có (void)
- **Pre-conditions**: LVGL đã được init, UI task đang chạy
- **Post-conditions**: Screen container đã được tạo, router sẵn sàng
- **Error model**: Log error nếu LVGL lock failed (```44:45:components/sx_ui/ui_router.c```)

**`ui_router_navigate_to()`**
- **Input**: `screen_id` (screen ID to navigate to)
- **Output**: Không có (void)
- **Pre-conditions**: Router đã được init, screen đã được registered
- **Post-conditions**: Screen đã được navigate (old screen destroyed, new screen created và shown)
- **Error model**: 
  - Log warning nếu screen not registered (```66:68:components/sx_ui/ui_router.c```)
  - Log debug nếu duplicate navigation (```72:75:components/sx_ui/ui_router.c```)
  - Log error nếu LVGL lock failed (```123:124:components/sx_ui/ui_router.c```)

**`ui_router_get_current_screen()`**
- **Input**: Không có
- **Output**: Current active screen ID
- **Pre-conditions**: Không có
- **Post-conditions**: Không có
- **Error model**: Return `SCREEN_ID_MAX` nếu chưa có screen active

**`ui_router_get_container()`**
- **Input**: Không có
- **Output**: Screen container object (lv_obj_t*)
- **Pre-conditions**: Router đã được init
- **Post-conditions**: Không có
- **Error model**: Return NULL nếu router chưa init

### C) Data Model

**Static State** (```13:15:components/sx_ui/ui_router.c```):
- `s_current_screen`: Current active screen ID
- `s_screen_container`: LVGL container object cho screens
- `s_router_initialized`: Init flag

**Invariants:**
- Chỉ có một screen active tại một thời điểm
- Screen container được tạo một lần và reuse cho tất cả screens
- Navigation sequence: hide old → destroy old → create new → show new

### D) Concurrency

- **Context**: 
  - **Init**: Chạy từ UI task init (single-threaded)
  - **Navigate**: Phải được gọi từ UI task hoặc với LVGL lock
- **Thread Safety**: 
  - **Static state**: Không được protect bởi mutex
  - **LVGL operations**: Protected bởi `lvgl_port_lock()` (```79:121:components/sx_ui/ui_router.c```)
  - **⚠️ RISK**: Concurrent navigation có thể race condition

### E) Memory Ownership

- **Screen container**: 
  - **Owner**: ui_router owns (static)
  - **Lifetime**: Persistent trong suốt lifetime của router
  - **Cleanup**: LVGL tự cleanup khi deinit

- **Screen objects**: 
  - **Owner**: Screens own (created trong `on_create()` callbacks)
  - **Lifetime**: Valid từ create đến destroy
  - **Cleanup**: Screens tự cleanup trong `on_destroy()` callbacks

### F) Side Effects

1. **LVGL**: Create/destroy screen objects, clear container content (```89:104:components/sx_ui/ui_router.c```)
2. **Screen Lifecycle**: Call screen callbacks (on_hide, on_destroy, on_create, on_show) (```82:116:components/sx_ui/ui_router.c```)

### G) Call Sites

1. **sx_ui_task()** - Init router (```113:113:components/sx_ui/sx_ui_task.c```)
2. **sx_ui_task()** - Navigate to boot screen (```179:179:components/sx_ui/sx_ui_task.c```)
3. **sx_ui_task()** - Navigate based on device state (```243:243:components/sx_ui/sx_ui_task.c```)
4. **Screen callbacks** - Navigate to other screens (ví dụ: boot screen timer, home menu items)

### H) Issues/Risks

1. **P1 - Navigation Race**: Nếu nhiều places gọi navigate đồng thời, có thể có race condition.
   - **Điều kiện**: Nhiều events trigger navigation cùng lúc
   - **Cách tái hiện**: Multiple events post navigation events
   - **Impact**: Screen navigation không nhất quán, có thể skip screens

2. **P1 - Screen Container Cleanup**: Container content được clear trước khi destroy old screen (```89:91:components/sx_ui/ui_router.c```), có thể gây issue nếu screen destroy callback cần access objects.
   - **Điều kiện**: Screen destroy callback access objects sau khi container cleared
   - **Cách tái hiện**: Screen destroy callback access objects
   - **Impact**: Null pointer dereference, crash

3. **P2 - Duplicate Navigation**: Duplicate navigation được prevent (```72:76:components/sx_ui/ui_router.c```), nhưng không có queue để handle navigation requests.
   - **Điều kiện**: Nhiều navigation requests trong thời gian ngắn
   - **Cách tái hiện**: Rapid navigation requests
   - **Impact**: Navigation requests bị drop

### I) Đề Xuất Cải Thiện

1. **P1**: Thêm mutex để protect navigation operations
2. **P1**: Fix screen container cleanup order (destroy trước khi clear)
3. **P2**: Thêm navigation queue để serialize navigation requests

---

## 3. sx_screen_if.h / sx_screen_if.c

### A) Vai Trò File

**sx_screen_if** định nghĩa screen lifecycle interface (vtable) và registry để quản lý tất cả screens. Đây là contract cho screen implementations.

**Dependencies trực tiếp:**
```c
// sx_screen_if.c:1-7
#include "sx_screen_if.h"
#include "ui_screen_registry.h"
#include <esp_log.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
```

### B) Public API

```c
// sx_screen_if.h:71-87
esp_err_t sx_screen_register(uint32_t screen_id, const sx_screen_if_t *iface);
esp_err_t sx_screen_unregister(uint32_t screen_id);
const sx_screen_if_t* sx_screen_get_interface(uint32_t screen_id);
```

**Contract:**

**`sx_screen_register()`**
- **Input**: `screen_id`, `iface` (vtable)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Registry chưa đầy (max 64 screens)
- **Post-conditions**: Screen đã được đăng ký
- **Error model**: 
  - `ESP_ERR_INVALID_ARG`: iface là NULL hoặc screen_id >= SCREEN_ID_MAX
  - `ESP_ERR_INVALID_STATE`: Screen đã được đăng ký
  - `ESP_ERR_NO_MEM`: Registry đầy

**`sx_screen_get_interface()`**
- **Input**: `screen_id`
- **Output**: Screen interface hoặc NULL nếu not registered
- **Pre-conditions**: Không có
- **Post-conditions**: Không có
- **Error model**: Return NULL nếu screen not registered hoặc invalid screen_id

### C) Data Model

```c
// sx_screen_if.h:24-62
typedef struct {
    lv_obj_t* (*create)(lv_obj_t *parent);
    void (*destroy)(lv_obj_t *screen);
    void (*on_enter)(lv_obj_t *screen);
    void (*on_exit)(lv_obj_t *screen);
    void (*on_state_change)(lv_obj_t *screen, uint32_t dirty_mask, const sx_state_t *state);
} sx_screen_if_t;
```

**Static State** (```12:22:components/sx_ui/sx_screen_if.c```):
- `s_screen_registry[]`: Array of screen entries (max 64)
- `s_screen_count`: Số lượng screens đã đăng ký
- `s_registry_mutex`: Mutex để protect registry

**Invariants:**
- Mỗi screen_id chỉ được đăng ký một lần
- Registry không vượt quá 64 screens

### D) Concurrency

- **Context**: 
  - **Register/Unregister**: Thường được gọi từ screen registration (single-threaded boot)
  - **Get Interface**: Có thể được gọi từ bất kỳ task nào
- **Thread Safety**: 
  - Registry được protect bởi `s_registry_mutex` (```40:63:components/sx_ui/sx_screen_if.c```)
  - Screen vtable calls không được protect (screen phải tự thread-safe)

### E) Memory Ownership

- **Screen vtable**: 
  - **Owner**: Screen module owns vtable (thường là static const)
  - **Lifetime**: Persistent trong suốt lifetime của screen

- **Screen registry**: 
  - **Owner**: sx_screen_if owns registry
  - **Lifetime**: Persistent trong suốt lifetime của hệ thống

### F) Side Effects

1. **FreeRTOS**: Tạo mutex để protect registry
2. **Screen Lifecycle**: Screen vtable được store để router có thể gọi

### G) Call Sites

1. **Screen modules** - Register screens (từ `register_all_screens()`)
2. **ui_router** - Get screen interface để call lifecycle methods (không trực tiếp, dùng ui_screen_registry)

### H) Issues/Risks

1. **P1 - Registry Full**: Registry có giới hạn 64 screens (```12:12:components/sx_ui/sx_screen_if.c```).
   - **Điều kiện**: Nhiều hơn 64 screens
   - **Cách tái hiện**: Đăng ký hơn 64 screens
   - **Impact**: Screen không thể đăng ký, có thể mất tính năng

2. **P2 - Screen Vtable Lifetime**: Screen vtable phải valid trong suốt lifetime của system.
   - **Điều kiện**: Screen vtable được free hoặc invalid
   - **Cách tái hiện**: Free screen module
   - **Impact**: Null pointer dereference, crash

### I) Đề Xuất Cải Thiện

1. **P1**: Tăng registry size hoặc dùng dynamic allocation
2. **P2**: Document rõ vtable lifetime requirements

---

## 4. ui_screen_registry.h / ui_screen_registry.c

### A) Vai Trò File

**ui_screen_registry** cung cấp screen registry với callbacks (on_create, on_show, on_hide, on_destroy, on_update) và screen name mapping.

**Dependencies trực tiếp:**
```c
// ui_screen_registry.c:1-4
#include "ui_screen_registry.h"
#include <esp_log.h>
#include <string.h>
```

### B) Public API

```c
// ui_screen_registry.h:66-69
void ui_screen_registry_init(void);
bool ui_screen_registry_register(ui_screen_id_t screen_id, const ui_screen_callbacks_t *callbacks);
const ui_screen_callbacks_t* ui_screen_registry_get(ui_screen_id_t screen_id);
const char* ui_screen_registry_get_name(ui_screen_id_t screen_id);
```

**Contract:**

**`ui_screen_registry_register()`**
- **Input**: `screen_id`, `callbacks`
- **Output**: `true` nếu thành công, `false` nếu failed
- **Pre-conditions**: Registry đã được init, screen_id < SCREEN_ID_MAX
- **Post-conditions**: Screen callbacks đã được registered
- **Error model**: 
  - `false`: Registry chưa init, invalid screen_id, hoặc callbacks là NULL

**`ui_screen_registry_get()`**
- **Input**: `screen_id`
- **Output**: Screen callbacks hoặc NULL nếu not registered
- **Pre-conditions**: Registry đã được init
- **Post-conditions**: Không có
- **Error model**: Return NULL nếu screen not registered hoặc invalid screen_id

### C) Data Model

```c
// ui_screen_registry.h:57-63
typedef struct {
    void (*on_create)(void);
    void (*on_show)(void);
    void (*on_hide)(void);
    void (*on_destroy)(void);
    void (*on_update)(const sx_state_t *state);
} ui_screen_callbacks_t;
```

**Static State** (```11:12:components/sx_ui/ui_screen_registry.c```):
- `s_screen_callbacks[]`: Array of callbacks (size = SCREEN_ID_MAX = 32)
- `s_registry_initialized`: Init flag
- `s_screen_names[]`: Screen name mapping (```16:52:components/sx_ui/ui_screen_registry.c```)

**Invariants:**
- Registry size = SCREEN_ID_MAX (32 screens)
- Screen names được map 1:1 với screen IDs

### D) Concurrency

- **Context**: 
  - **Init/Register**: Chạy từ UI task init (single-threaded boot)
  - **Get**: Có thể được gọi từ bất kỳ task nào
- **Thread Safety**: 
  - **Registry**: Không được protect bởi mutex
  - **⚠️ RISK**: Concurrent get có thể race condition (nhưng ít risk vì chỉ read)

### E) Memory Ownership

- **Screen callbacks**: 
  - **Owner**: Screen module owns callbacks (thường là static)
  - **Lifetime**: Persistent trong suốt lifetime của screen

- **Screen registry**: 
  - **Owner**: ui_screen_registry owns registry
  - **Lifetime**: Persistent trong suốt lifetime của hệ thống

### F) Side Effects

1. **Screen Lifecycle**: Callbacks được store để router có thể gọi

### G) Call Sites

1. **ui_router_init()** - Init registry (```24:24:components/sx_ui/ui_router.c```)
2. **register_all_screens()** - Register all screens (```45:84:components/sx_ui/screens/register_all_screens.c```)
3. **ui_router_navigate_to()** - Get callbacks để call lifecycle methods (```64:116:components/sx_ui/ui_router.c```)
4. **sx_ui_task()** - Get callbacks để call on_update (```255:263:components/sx_ui/sx_ui_task.c```)

### H) Issues/Risks

1. **P2 - Thread Safety**: Registry không được protect bởi mutex, concurrent get có thể race condition.
   - **Điều kiện**: Nhiều tasks get callbacks đồng thời
   - **Cách tái hiện**: Get callbacks từ nhiều tasks
   - **Impact**: Có thể đọc được inconsistent callbacks (nhưng ít risk vì chỉ read)

2. **P2 - Screen Name Mapping**: Screen names được hard-coded (```16:52:components/sx_ui/ui_screen_registry.c```), có thể không match với actual screen IDs.
   - **Điều kiện**: Screen ID thay đổi nhưng name mapping không update
   - **Cách tái hiện**: Thay đổi screen ID enum
   - **Impact**: Wrong screen name trong logs

### I) Đề Xuất Cải Thiện

1. **P2**: Thêm mutex để protect registry (nếu cần)
2. **P2**: Generate screen name mapping từ enum (compile-time check)

---

## 5. sx_lvgl.h / sx_lvgl_lock.h / sx_lvgl_lock.c

### A) Vai Trò File

**sx_lvgl** là wrapper để enforce LVGL chỉ được include trong sx_ui component. **sx_lvgl_lock** cung cấp RAII-style lock guard cho LVGL operations.

**Dependencies trực tiếp:**
```c
// sx_lvgl.h:21-22
#include "lvgl.h"
#include "esp_lvgl_port.h"
```

### B) Public API

```c
// sx_lvgl_lock.h:19-26
sx_lvgl_lock_guard_t sx_lvgl_lock_acquire(void);
void sx_lvgl_lock_release(sx_lvgl_lock_guard_t *guard);
```

**Contract:**

**`sx_lvgl_lock_acquire()`**
- **Input**: Không có
- **Output**: Guard với `locked` flag
- **Pre-conditions**: Không có nested locks
- **Post-conditions**: LVGL lock đã được acquire (nếu thành công)
- **Error model**: 
  - `locked = false`: Nested lock detected hoặc lock failed
  - Log error nếu nested lock (```12:14:components/sx_ui/sx_lvgl_lock.c```)

**`sx_lvgl_lock_release()`**
- **Input**: `guard` (returned from acquire)
- **Output**: Không có (void)
- **Pre-conditions**: Guard đã được acquire
- **Post-conditions**: LVGL lock đã được release
- **Error model**: No-op nếu guard là NULL hoặc not locked

**Macro:**
```c
// sx_lvgl_lock.h:29-32
#define SX_LVGL_LOCK() \
    for (sx_lvgl_lock_guard_t _guard = sx_lvgl_lock_acquire(); \
         _guard.locked; \
         sx_lvgl_lock_release(&_guard), _guard.locked = false)
```

### C) Data Model

```c
// sx_lvgl_lock.h:10-12
typedef struct {
    bool locked;
} sx_lvgl_lock_guard_t;
```

**Static State** (```6:6:components/sx_ui/sx_lvgl_lock.c```):
- `s_lock_acquired`: Lock state flag (để detect nested locks)

**Invariants:**
- Lock/unlock phải được paired
- Nested locks không được phép

### D) Concurrency

- **Context**: Bất kỳ task nào có thể acquire/release lock (nhưng chỉ UI task nên dùng)
- **Thread Safety**: 
  - Lock guard: Thread-safe (ESP-IDF lvgl_port_lock là thread-safe)
  - Nested lock detection: Không thread-safe (static flag) - **⚠️ RISK**

### E) Memory Ownership

- **Lock guard**: 
  - **Owner**: Caller owns guard struct
  - **Lifetime**: Valid trong scope của lock

### F) Side Effects

1. **LVGL**: Acquire/release LVGL port lock
2. **Logging**: Log error nếu nested lock detected

### G) Call Sites

1. **Screen implementations** - Protect LVGL operations (nếu dùng SX_LVGL_LOCK macro)
2. **ui_router** - Protect navigation operations (dùng lvgl_port_lock trực tiếp)

### H) Issues/Risks

1. **P1 - Nested Lock Detection Not Thread-Safe**: Nested lock detection dùng static flag, không thread-safe (```6:6:components/sx_ui/sx_lvgl_lock.c```).
   - **Điều kiện**: Nhiều tasks acquire lock đồng thời
   - **Cách tái hiện**: Multiple tasks acquire lock
   - **Impact**: False positive nested lock detection

2. **P2 - Lock Guard Not Used**: Nhiều places dùng `lvgl_port_lock()` trực tiếp thay vì lock guard.
   - **Điều kiện**: Code không dùng SX_LVGL_LOCK macro
   - **Cách tái hiện**: Check code - nhiều places dùng lvgl_port_lock trực tiếp
   - **Impact**: Không có nested lock detection, risk deadlock

### I) Đề Xuất Cải Thiện

1. **P1**: Thêm mutex để protect nested lock detection
2. **P2**: Enforce dùng SX_LVGL_LOCK macro (code review hoặc linter)

---

## 6. screen_boot.c (Example Screen)

### A) Vai Trò File

**screen_boot** là boot screen hiển thị bootscreen image trong 3 giây, sau đó tự động navigate đến flash screen.

**Dependencies trực tiếp:**
```c
// screen_boot.c:1-10
#include "screen_boot.h"
#include "sx_lvgl.h"
#include "ui_router.h"
#include "sx_ui_verify.h"
#include "ui_assets_wrapper.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
```

### B) Public API

```c
// screen_boot.h (implied)
void screen_boot_register(void);
```

**Contract:**
- **Input**: Không có
- **Output**: Không có (void)
- **Pre-conditions**: Screen registry đã được init
- **Post-conditions**: Boot screen đã được registered

### C) Data Model

**Static State** (```14:16:components/sx_ui/screens/screen_boot.c```):
- `s_bootscreen_img`: Bootscreen image object
- `s_container`: Screen container reference
- `s_boot_timer`: LVGL timer để auto-navigate

**Invariants:**
- Timer chỉ được tạo một lần trong `on_show()`
- Timer được delete trong `on_hide()` và `on_destroy()`

### D) Concurrency

- **Context**: 
  - **Lifecycle callbacks**: Chạy từ UI task với LVGL lock
  - **Timer callback**: Chạy từ LVGL timer (trong UI task context)
- **Thread Safety**: 
  - LVGL operations: Protected bởi LVGL lock (từ router)
  - Static state: Không được protect (nhưng chỉ UI task access)

### E) Memory Ownership

- **LVGL objects**: 
  - **Owner**: Screen owns (static pointers)
  - **Lifetime**: Valid từ create đến destroy
  - **Cleanup**: Delete trong `on_destroy()` (```146:149:components/sx_ui/screens/screen_boot.c```)

- **LVGL timer**: 
  - **Owner**: Screen owns (static pointer)
  - **Lifetime**: Valid từ show đến hide/destroy
  - **Cleanup**: Delete trong `on_hide()` và `on_destroy()` (```126:130:140:144:components/sx_ui/screens/screen_boot.c```)

### F) Side Effects

1. **LVGL**: Create image object, create timer (```55:116:components/sx_ui/screens/screen_boot.c```)
2. **Navigation**: Navigate to flash screen sau 3 giây (```97:97:components/sx_ui/screens/screen_boot.c```)
3. **Assets**: Load bootscreen image từ assets (```32:32:components/sx_ui/screens/screen_boot.c```)

### G) Call Sites

1. **register_all_screens()** - Register boot screen (```47:47:components/sx_ui/screens/register_all_screens.c```)
2. **sx_ui_task()** - Navigate to boot screen initially (```179:179:components/sx_ui/sx_ui_task.c```)
3. **boot_timer_cb()** - Auto-navigate to flash screen (```97:97:components/sx_ui/screens/screen_boot.c```)

### H) Issues/Risks

1. **P1 - Timer Leak**: Timer được delete trong cả `on_hide()` và `on_destroy()` (```126:130:140:144:components/sx_ui/screens/screen_boot.c```), có thể double-delete.
   - **Điều kiện**: on_hide() được gọi trước on_destroy()
   - **Cách tái hiện**: Navigate away từ boot screen
   - **Impact**: Double-delete timer, có thể crash

2. **P2 - Image Fallback**: Nếu bootscreen image không available, chỉ set black background (```35:37:components/sx_ui/screens/screen_boot.c```), không có error handling.
   - **Điều kiện**: Bootscreen image không load được
   - **Cách tái hiện**: Assets không available
   - **Impact**: Black screen, không có visual feedback

### I) Đề Xuất Cải Thiện

1. **P1**: Chỉ delete timer trong `on_destroy()`, check NULL trong `on_hide()`
2. **P2**: Thêm error handling và fallback UI cho missing image

---

## 7. screen_home.c (Example Screen)

### A) Vai Trò File

**screen_home** là home screen (launcher) hiển thị menu grid với 7 items (Music Player, Online Music, Radio, SD Card, IR Control, Settings, Chatbot) và có idle timeout (30 giây) để return to screensaver.

**Dependencies trực tiếp:**
```c
// screen_home.c:1-15
#include "screen_home.h"
#include "sx_lvgl.h"
#include "ui_router.h"
#include "ui_screen_registry.h"
#include "sx_dispatcher.h"
#include "sx_state.h"
#include "sx_ui_verify.h"
#include "screen_common.h"
#include "ui_icons.h"
#include "ui_theme_tokens.h"
```

### B) Public API

```c
// screen_home.h (implied)
void screen_home_register(void);
```

**Contract:**
- **Input**: Không có
- **Output**: Không có (void)
- **Pre-conditions**: Screen registry đã được init
- **Post-conditions**: Home screen đã được registered

### C) Data Model

**Static State** (```19:23:components/sx_ui/screens/screen_home.c```):
- `s_top_bar`: Top bar object (NULL nếu không dùng)
- `s_grid`: Menu grid container
- `s_container`: Screen container reference
- `s_idle_timer`: LVGL timer để detect idle timeout

**Menu Items** (```37:45:components/sx_ui/screens/screen_home.c```):
```c
static const home_menu_item_t s_home_menu_items[] = {
    {"Music Player", UI_ICON_MUSIC_PLAYER, SCREEN_ID_MUSIC_PLAYER},
    {"Online Music", UI_ICON_MUSIC_ONLINE, SCREEN_ID_MUSIC_ONLINE_LIST},
    // ... 7 items total
};
```

**Invariants:**
- Idle timer được reset mỗi khi có touch event hoặc state update
- Menu items được create một lần trong `on_create()`

### D) Concurrency

- **Context**: 
  - **Lifecycle callbacks**: Chạy từ UI task với LVGL lock
  - **Touch event callbacks**: Chạy từ LVGL event system (trong UI task context)
  - **Timer callback**: Chạy từ LVGL timer (trong UI task context)
- **Thread Safety**: 
  - LVGL operations: Protected bởi LVGL lock (từ router)
  - Static state: Không được protect (nhưng chỉ UI task access)

### E) Memory Ownership

- **LVGL objects**: 
  - **Owner**: Screen owns (static pointers)
  - **Lifetime**: Valid từ create đến destroy
  - **Cleanup**: Delete trong `on_destroy()` (```193:200:components/sx_ui/screens/screen_home.c```)

- **LVGL timer**: 
  - **Owner**: Screen owns (static pointer)
  - **Lifetime**: Valid từ show đến hide/destroy
  - **Cleanup**: Delete trong `on_hide()` và `on_destroy()` (```175:178:188:191:components/sx_ui/screens/screen_home.c```)

### F) Side Effects

1. **LVGL**: Create grid, menu items, timer (```89:156:components/sx_ui/screens/screen_home.c```)
2. **Navigation**: Navigate to other screens khi menu item clicked (```54:54:components/sx_ui/screens/screen_home.c```)
3. **Navigation**: Navigate to flash screen khi idle timeout (```136:136:components/sx_ui/screens/screen_home.c```)

### G) Call Sites

1. **register_all_screens()** - Register home screen (```49:49:components/sx_ui/screens/register_all_screens.c```)
2. **Menu item click callbacks** - Navigate to target screens (```54:54:components/sx_ui/screens/screen_home.c```)
3. **Idle timer callback** - Navigate to flash screen (```136:136:components/sx_ui/screens/screen_home.c```)

### H) Issues/Risks

1. **P1 - Timer Leak**: Timer được delete trong cả `on_hide()` và `on_destroy()` (```175:178:188:191:components/sx_ui/screens/screen_home.c```), có thể double-delete.
   - **Điều kiện**: on_hide() được gọi trước on_destroy()
   - **Cách tái hiện**: Navigate away từ home screen
   - **Impact**: Double-delete timer, có thể crash

2. **P2 - Idle Timer Reset**: Idle timer được reset trong `on_update()` (```205:205:components/sx_ui/screens/screen_home.c```), có thể reset quá thường xuyên nếu state update liên tục.
   - **Điều kiện**: State update liên tục
   - **Cách tái hiện**: High-frequency state updates
   - **Impact**: Idle timer không bao giờ fire, screensaver không activate

### I) Đề Xuất Cải Thiện

1. **P1**: Chỉ delete timer trong `on_destroy()`, check NULL trong `on_hide()`
2. **P2**: Thêm debounce cho idle timer reset (ví dụ: chỉ reset nếu state update từ user action)

---

## 8. screen_chat.c (Example Screen)

### A) Vai Trò File

**screen_chat** là chat screen hiển thị conversation với chatbot, có input bar để send messages, và update UI dựa trên state changes và events.

**Dependencies trực tiếp:**
```c
// screen_chat.c:1-16
#include "screen_chat.h"
#include "sx_lvgl.h"
#include "ui_router.h"
#include "sx_ui_verify.h"
#include "screen_common.h"
#include "sx_dispatcher.h"
#include "sx_state.h"
#include "sx_event.h"
#include "sx_stt_service.h"
#include "sx_tts_service.h"
#include "ui_theme_tokens.h"
#include "ui_list.h"
```

### B) Public API

```c
// screen_chat.h (implied)
void screen_chat_register(void);
```

**Contract:**
- **Input**: Không có
- **Output**: Không có (void)
- **Pre-conditions**: Screen registry đã được init
- **Post-conditions**: Chat screen đã được registered

### C) Data Model

**Static State** (```20:34:components/sx_ui/screens/screen_chat.c```):
- `s_top_bar`, `s_message_list`, `s_input_bar`, `s_textarea`, `s_send_btn`: UI objects
- `s_status_label`, `s_typing_indicator`, `s_stt_status_label`, `s_tts_status_label`, `s_emotion_label`: Status indicators
- `s_last_state_seq`: Last state sequence number
- `s_chatbot_connected`, `s_tts_speaking`, `s_stt_active`: State flags

**Invariants:**
- Message list được update từ state và events
- Status labels được update dựa trên state changes

### D) Concurrency

- **Context**: 
  - **Lifecycle callbacks**: Chạy từ UI task với LVGL lock
  - **Event callbacks**: Chạy từ LVGL event system (trong UI task context)
  - **on_update**: Chạy từ UI task với LVGL lock (```258:263:components/sx_ui/sx_ui_task.c```)
- **Thread Safety**: 
  - LVGL operations: Protected bởi LVGL lock
  - Static state: Không được protect (nhưng chỉ UI task access)
  - **⚠️ RISK**: Event polling trong `on_update()` có thể race với main loop event polling

### E) Memory Ownership

- **Message text**: 
  - **Owner**: Caller owns (strdup trong send button callback) (```54:54:components/sx_ui/screens/screen_chat.c```)
  - **Lifetime**: Valid cho đến khi orchestrator consume event
  - **Cleanup**: Orchestrator phải free sau khi consume

- **LVGL objects**: 
  - **Owner**: Screen owns (static pointers)
  - **Lifetime**: Valid từ create đến destroy
  - **Cleanup**: Delete trong `on_destroy()` (implied, không có explicit cleanup code)

### F) Side Effects

1. **Events**: Post `SX_EVT_UI_INPUT` event khi send button clicked (```64:70:components/sx_ui/screens/screen_chat.c```)
2. **Events**: Poll events trong `on_update()` để handle chatbot events (```287:287:components/sx_ui/screens/screen_chat.c```)
3. **LVGL**: Create message list, input bar, status labels (```77:175:components/sx_ui/screens/screen_chat.c```)

### G) Call Sites

1. **register_all_screens()** - Register chat screen (```50:50:components/sx_ui/screens/register_all_screens.c```)
2. **Home menu** - Navigate to chat screen (từ home screen menu item)
3. **Send button callback** - Post UI input event (```64:70:components/sx_ui/screens/screen_chat.c```)

### H) Issues/Risks

1. **P0 - Event Polling Race**: `on_update()` poll events (```287:287:components/sx_ui/screens/screen_chat.c```), có thể race với main loop event polling (```269:277:components/sx_ui/sx_ui_task.c```).
   - **Điều kiện**: on_update() và main loop poll events đồng thời
   - **Cách tái hiện**: State update trigger on_update() trong khi main loop đang poll
   - **Impact**: Events có thể bị consume bởi cả hai, duplicate handling

2. **P1 - Memory Leak**: Message text được strdup (```54:54:components/sx_ui/screens/screen_chat.c```), orchestrator phải free, nhưng không có guarantee.
   - **Điều kiện**: Orchestrator không free message text
   - **Cách tái hiện**: Orchestrator consume event nhưng không free ptr
   - **Impact**: Memory leak

3. **P2 - State Update Frequency**: `on_update()` được gọi mỗi frame nếu state changed (```251:264:components/sx_ui/sx_ui_task.c```), có thể tốn CPU nếu state update liên tục.
   - **Điều kiện**: High-frequency state updates
   - **Cách tái hiện**: Continuous state updates
   - **Impact**: CPU overhead, UI lag

### I) Đề Xuất Cải Thiện

1. **P0**: Remove event polling từ `on_update()`, chỉ dùng state updates
2. **P1**: Document rõ ownership rules cho message text, hoặc dùng string pool
3. **P2**: Thêm debounce hoặc rate limiting cho `on_update()` calls

---

## Tổng Kết Component

### Điểm Mạnh

1. **Screen Lifecycle**: Rõ ràng và consistent (create, show, hide, destroy, update)
2. **Navigation Router**: Centralized navigation system
3. **LVGL Wrapper**: Compile-time và runtime guards để enforce LVGL chỉ trong sx_ui
4. **29 Screens**: Comprehensive screen coverage
5. **State Integration**: UI update từ state changes với dirty_mask

### Điểm Yếu

1. **Thread Safety**: Một số static state không được protect bởi mutex
2. **Event Polling Race**: on_update() poll events có thể race với main loop
3. **Timer Leak Risk**: Timer được delete trong cả on_hide() và on_destroy()
4. **Memory Ownership**: Message text ownership không rõ ràng

### Đề Xuất Cải Thiện Tổng Thể

1. **P0**: Fix event polling race trong on_update()
2. **P1**: Fix timer leak risk (chỉ delete trong on_destroy())
3. **P1**: Document rõ memory ownership rules cho event payloads
4. **P2**: Thêm mutex để protect static state nếu cần
5. **P2**: Thêm rate limiting cho on_update() calls
