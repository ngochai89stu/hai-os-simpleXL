# R2 DEEP AUDIT - PHASE 4
## Đề Xuất Hoàn Thiện Lên 10/10

> **Tiếp nối từ:** `R2_DEEP_AUDIT_PHASE2_PHASE3.md`  
> **Mục tiêu:** Roadmap cụ thể để nâng điểm từ 6.05/10 → 10/10

---

## 1. TỔNG QUAN ROADMAP

### Phân loại theo Priority:

- **P0 (Blocking - Phải fix ngay):** 4 tasks
- **P1 (High Priority):** 4 tasks  
- **P2 (Medium Priority):** 6 tasks

**Tổng:** 14 tasks để đạt 10/10

---

## 2. P0 TASKS (Blocking - Ưu tiên cao nhất)

### P0.1: Fix 4 Services Violations (LVGL calls)

**Outcome:** 0 violations - Tất cả services không include/gọi LVGL

**Files cần sửa:**
1. `components/sx_services/sx_display_service.c`
2. `components/sx_services/sx_theme_service.c`
3. `components/sx_services/sx_image_service.c`
4. `components/sx_services/sx_qr_code_service.c`

**Migration Steps:**

#### sx_display_service.c:
- **Hiện tại:** Include `lvgl.h`, gọi `lv_display_get_default()`, dùng `lv_obj_t*`
- **Pattern đúng:**
  1. Service chuẩn bị data (screen capture buffer, config)
  2. Phát event `SX_EVT_DISPLAY_CAPTURE_REQUEST` với payload
  3. UI task nhận event và gọi LVGL APIs
  4. UI phát event `SX_EVT_DISPLAY_CAPTURE_DONE` với buffer data
  5. Service nhận event và xử lý (encode JPEG, upload, etc.)

**API cần thêm:**
```c
// components/sx_core/include/sx_event.h
typedef enum {
    // ... existing events ...
    SX_EVT_DISPLAY_CAPTURE_REQUEST,  // UI → service: request capture
    SX_EVT_DISPLAY_CAPTURE_DONE,     // UI → service: capture done (payload: buffer)
    SX_EVT_DISPLAY_PREVIEW_REQUEST,  // Service → UI: show preview
    SX_EVT_DISPLAY_PREVIEW_DONE,     // UI → service: preview shown
} sx_event_id_t;

// Payload struct
typedef struct {
    uint8_t *buffer;
    uint16_t width;
    uint16_t height;
    uint32_t buffer_size;
} sx_display_capture_payload_t;
```

**Files tạo mới:**
- `components/sx_ui/screens/screen_display_helper.c` - UI helper để handle display events

#### sx_theme_service.c:
- **Hiện tại:** Include `lvgl.h`, có thể gọi LVGL theme APIs
- **Pattern đúng:**
  1. Service chỉ quản lý theme data (colors, config)
  2. Phát event `SX_EVT_THEME_CHANGED` với theme data
  3. UI task nhận event và apply theme qua LVGL APIs

**API cần thêm:**
```c
// components/sx_core/include/sx_event.h
typedef enum {
    SX_EVT_THEME_CHANGED,  // Service → UI: theme changed (payload: theme_data_t)
} sx_event_id_t;

typedef struct {
    sx_theme_type_t theme_type;
    sx_theme_colors_t colors;
} sx_theme_data_t;
```

#### sx_image_service.c:
- **Hiện tại:** Include `lvgl.h`, dùng LVGL image descriptors
- **Pattern đúng:**
  1. Service decode image (PNG/JPEG) → raw buffer
  2. Phát event `SX_EVT_IMAGE_LOADED` với buffer data
  3. UI task nhận event và tạo LVGL image object

**API cần thêm:**
```c
// components/sx_core/include/sx_event.h
typedef enum {
    SX_EVT_IMAGE_LOAD_REQUEST,  // UI → service: load image (payload: path)
    SX_EVT_IMAGE_LOADED,        // Service → UI: image loaded (payload: buffer)
} sx_event_id_t;
```

#### sx_qr_code_service.c:
- **Hiện tại:** Include `lvgl.h`, gọi `lv_qrcode_create()`, `lv_scr_act()`
- **Pattern đúng:**
  1. Service generate QR code data (matrix)
  2. Phát event `SX_EVT_QR_CODE_GENERATED` với QR data
  3. UI task nhận event và render QR code qua LVGL widget

**API cần thêm:**
```c
// components/sx_core/include/sx_event.h
typedef enum {
    SX_EVT_QR_CODE_GENERATE_REQUEST,  // UI → service: generate QR (payload: text)
    SX_EVT_QR_CODE_GENERATED,           // Service → UI: QR generated (payload: matrix)
} sx_event_id_t;
```

**Risk/Rollback:** 
- Low risk - Chỉ refactor, không thay đổi logic
- Có thể test từng service một
- Rollback: Git revert từng commit

**Estimated effort:** 2-3 days

---

### P0.2: Tạo sx_lvgl.h Wrapper Header

**Outcome:** File `sx_ui/include/sx_lvgl.h` tồn tại với compile-time guard

**Files cần tạo:**
- `components/sx_ui/include/sx_lvgl.h`

**Implementation:**
```c
// components/sx_ui/include/sx_lvgl.h
#pragma once

// Compile-time guard: Chỉ cho phép include trong sx_ui component
#if !defined(SX_COMPONENT_SX_UI)
#error "LVGL headers can only be included in sx_ui component. Include sx_lvgl.h only from sx_ui source files."
#endif

// Include LVGL headers
#include "lvgl.h"
#include "esp_lvgl_port.h"

#ifdef __cplusplus
extern "C" {
#endif

// Optional: Wrapper macros for common LVGL calls with thread verification
// (if runtime assert is implemented)

#ifdef __cplusplus
}
#endif
```

**Files cần sửa:**
- `components/sx_ui/CMakeLists.txt` - Thêm compile definition:
```cmake
target_compile_definitions(${COMPONENT_LIB} PRIVATE SX_COMPONENT_SX_UI=1)
```

- `components/sx_ui/sx_ui_task.c` - Thay `#include "lvgl.h"` → `#include "sx_lvgl.h"`

**Migration Steps:**
1. Tạo file `sx_lvgl.h` với guard
2. Update CMakeLists.txt
3. Replace tất cả `#include "lvgl.h"` trong `sx_ui/` → `#include "sx_lvgl.h"`
4. Update script `check_architecture.sh` để check `sx_lvgl.h` wrapper

**Risk/Rollback:** 
- Very low risk - Chỉ thêm wrapper, không thay đổi logic
- Rollback: Git revert

**Estimated effort:** 0.5 day

---

### P0.3: Thêm Compile-Time Guard vào CMakeLists

**Outcome:** CMakeLists của sx_ui set `-DSX_COMPONENT_SX_UI=1`

**Files cần sửa:**
- `components/sx_ui/CMakeLists.txt`

**Implementation:**
```cmake
idf_component_register(
    # ... existing config ...
)

# Add compile-time guard for sx_lvgl.h
target_compile_definitions(${COMPONENT_LIB} PRIVATE SX_COMPONENT_SX_UI=1)
```

**Migration Steps:**
1. Thêm dòng `target_compile_definitions` vào CMakeLists.txt
2. Test build để đảm bảo không break
3. Verify: Try include `lvgl.h` từ service → should fail compile

**Risk/Rollback:** 
- Very low risk
- Rollback: Git revert

**Estimated effort:** 0.25 day

---

### P0.4: Thêm version và dirty_mask vào State

**Outcome:** `sx_state_t` có `version` và `dirty_mask` fields

**Files cần sửa:**
- `components/sx_core/include/sx_state.h`
- `components/sx_core/sx_dispatcher.c`
- `components/sx_core/sx_orchestrator.c`

**Implementation:**

#### sx_state.h:
```c
// components/sx_core/include/sx_state.h

// Dirty mask bits (one per domain)
#define SX_STATE_DIRTY_AUDIO     (1U << 0)
#define SX_STATE_DIRTY_UI        (1U << 1)
#define SX_STATE_DIRTY_WIFI      (1U << 2)
#define SX_STATE_DIRTY_CHATBOT   (1U << 3)
#define SX_STATE_DIRTY_SYSTEM    (1U << 4)
// ... more domains ...

typedef struct {
    uint32_t version;      // MUST: Monotonically increasing version
    uint32_t dirty_mask;   // MUST: Bitmask of changed domains
    uint32_t seq;          // Keep for backward compatibility (can deprecate later)
    sx_wifi_state_t wifi;
    sx_audio_state_t audio;
    sx_ui_state_t ui;
} sx_state_t;
```

#### sx_dispatcher.c:
```c
// Update sx_dispatcher_set_state to increment version
void sx_dispatcher_set_state(const sx_state_t *state) {
    if (!state || s_state_mutex == NULL) {
        return;
    }
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    s_state = *state;
    // Increment version if state actually changed
    if (s_state.version == 0) {
        s_state.version = 1;
    } else {
        s_state.version++;
    }
    xSemaphoreGive(s_state_mutex);
}
```

#### sx_orchestrator.c:
```c
// Update orchestrator to set dirty_mask when updating state
void update_state_with_dirty_mask(uint32_t dirty_bits) {
    sx_state_t st;
    sx_dispatcher_get_state(&st);
    
    // Set dirty_mask
    st.dirty_mask |= dirty_bits;
    
    // Increment version (dispatcher will also increment, but that's OK)
    st.version++;
    
    sx_dispatcher_set_state(&st);
}
```

**Migration Steps:**
1. Add `version` and `dirty_mask` to `sx_state_t`
2. Update `sx_dispatcher_set_state()` to handle version
3. Update orchestrator to set dirty_mask when updating state
4. Update UI screens to check dirty_mask before rendering
5. Test: Verify version increments, dirty_mask works

**Risk/Rollback:**
- Medium risk - Thay đổi state structure
- Cần test kỹ để đảm bảo không break existing code
- Rollback: Git revert

**Estimated effort:** 1 day

---

## 3. P1 TASKS (High Priority)

### P1.1: Thêm Runtime Assert SX_ASSERT_UI_THREAD()

**Outcome:** Macro `SX_ASSERT_UI_THREAD()` detect LVGL calls ngoài UI task

**Files cần sửa:**
- `components/sx_ui/include/sx_lvgl_lock.h`
- `components/sx_ui/sx_ui_task.c`

**Implementation:**
```c
// components/sx_ui/include/sx_lvgl_lock.h

// Add UI task handle storage
extern TaskHandle_t g_sx_ui_task_handle;

// Set UI task handle (call from sx_ui_task)
void sx_ui_set_ui_task_handle(TaskHandle_t handle);

// Runtime assert macro
#ifdef CONFIG_SX_ENABLE_UI_THREAD_ASSERT
#define SX_ASSERT_UI_THREAD() \
    do { \
        if (g_sx_ui_task_handle != NULL && \
            xTaskGetCurrentTaskHandle() != g_sx_ui_task_handle) { \
            ESP_LOGE("SX_LVGL", "LVGL call from non-UI task! Current: %p, UI: %p", \
                     xTaskGetCurrentTaskHandle(), g_sx_ui_task_handle); \
            assert(0 && "LVGL call must be in UI task"); \
        } \
    } while(0)
#else
#define SX_ASSERT_UI_THREAD() ((void)0)
#endif

// Wrapper macro for LVGL calls
#define SX_LVGL_CALL(stmt) \
    do { \
        SX_ASSERT_UI_THREAD(); \
        stmt; \
    } while(0)
```

**Files cần tạo:**
- `components/sx_ui/sx_lvgl_lock.c` (nếu chưa có implementation)

**Migration Steps:**
1. Add `sx_ui_set_ui_task_handle()` function
2. Call `sx_ui_set_ui_task_handle()` trong `sx_ui_task()`
3. Add `SX_ASSERT_UI_THREAD()` macro
4. Wrap critical LVGL calls với `SX_LVGL_CALL()`
5. Add Kconfig option `CONFIG_SX_ENABLE_UI_THREAD_ASSERT`

**Risk/Rollback:**
- Low risk - Chỉ thêm assert, không thay đổi logic
- Có thể disable qua Kconfig
- Rollback: Git revert

**Estimated effort:** 0.5 day

---

### P1.2: Tạo Metrics System

**Outcome:** File `sx_metrics.c` và `sx_metrics.h` tồn tại với required metrics

**Files cần tạo:**
- `components/sx_core/src/sx_metrics.c`
- `components/sx_core/include/sx_metrics.h`

**Implementation:**
```c
// components/sx_core/include/sx_metrics.h
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Metrics counters
typedef struct {
    uint32_t evt_posted_total[4];      // Per priority: LOW, NORMAL, HIGH, CRITICAL
    uint32_t evt_dropped_total[4];     // Per priority
    uint32_t evt_coalesced_total[16];  // Per domain (if implemented)
    uint32_t queue_depth_gauge[4];     // Current queue depth per priority
    uint32_t state_version_gauge;      // Current state version
    uint32_t ui_render_ms_gauge;       // Last render time (ms)
    uint32_t heap_free_min;            // Minimum free heap
    uint32_t psram_free_min;           // Minimum free PSRAM (if available)
} sx_metrics_t;

// Initialize metrics
void sx_metrics_init(void);

// Get current metrics snapshot
void sx_metrics_get(sx_metrics_t *out_metrics);

// Increment counters
void sx_metrics_inc_evt_posted(sx_event_priority_t prio);
void sx_metrics_inc_evt_dropped(sx_event_priority_t prio);
void sx_metrics_inc_evt_coalesced(uint32_t domain);

// Update gauges
void sx_metrics_set_queue_depth(sx_event_priority_t prio, uint32_t depth);
void sx_metrics_set_state_version(uint32_t version);
void sx_metrics_set_ui_render_ms(uint32_t ms);
void sx_metrics_update_heap(void);

// Dump metrics to log
void sx_metrics_dump(void);

#ifdef __cplusplus
}
#endif
```

**Files cần sửa:**
- `components/sx_core/sx_dispatcher.c` - Integrate metrics
- `components/sx_core/sx_orchestrator.c` - Update state version metric
- `components/sx_ui/sx_ui_task.c` - Update render time metric

**Migration Steps:**
1. Create `sx_metrics.h` và `sx_metrics.c`
2. Integrate vào dispatcher (count posted/dropped events)
3. Integrate vào orchestrator (update state version)
4. Integrate vào UI task (update render time)
5. Add metrics dump command (có thể qua dev console)

**Risk/Rollback:**
- Low risk - Chỉ thêm metrics, không thay đổi logic
- Rollback: Git revert

**Estimated effort:** 1 day

---

### P1.3: Tạo Lifecycle Interfaces

**Outcome:** Files `sx_service_if.h` và `sx_screen_if.h` tồn tại

**Files cần tạo:**
- `components/sx_core/include/sx_service_if.h`
- `components/sx_ui/include/sx_screen_if.h`

**Implementation:**

#### sx_service_if.h:
```c
// components/sx_core/include/sx_service_if.h
#pragma once

#include "sx_event.h"
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

// Service interface vtable
typedef struct {
    esp_err_t (*init)(void);
    esp_err_t (*start)(void);
    esp_err_t (*stop)(void);
    esp_err_t (*deinit)(void);
    esp_err_t (*on_event)(const sx_event_t *evt);
} sx_service_if_t;

// Service registration
esp_err_t sx_service_register(const char *name, const sx_service_if_t *iface);

#ifdef __cplusplus
}
#endif
```

#### sx_screen_if.h:
```c
// components/sx_ui/include/sx_screen_if.h
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screen interface vtable
typedef struct {
    void* (*create)(void);
    void (*destroy)(void *screen);
    void (*on_enter)(void *screen);
    void (*on_exit)(void *screen);
    void (*on_state_change)(void *screen, uint32_t dirty_mask);
} sx_screen_if_t;

// Screen registration
esp_err_t sx_screen_register(const char *name, const sx_screen_if_t *iface);

#ifdef __cplusplus
}
#endif
```

**Migration Steps:**
1. Create interface files
2. Refactor existing services/screens để implement interfaces (optional, có thể làm dần)
3. Update screen registry để dùng interface

**Risk/Rollback:**
- Medium risk - Thay đổi cách services/screens được quản lý
- Có thể implement dần, không cần refactor tất cả ngay
- Rollback: Git revert

**Estimated effort:** 1-2 days (tùy số lượng services/screens cần refactor)

---

### P1.4: Implement Backpressure Policy

**Outcome:** API `sx_dispatcher_post_event()` có backpressure policy (DROP/COALESCE/BLOCK)

**Files cần sửa:**
- `components/sx_core/include/sx_dispatcher.h`
- `components/sx_core/sx_dispatcher.c`

**Implementation:**
```c
// components/sx_core/include/sx_dispatcher.h

// Backpressure policy
typedef enum {
    SX_BP_DROP,        // Drop if queue full
    SX_BP_COALESCE,    // Keep only latest event (by key/type)
    SX_BP_BLOCK,       // Block with timeout (only for CRITICAL)
} sx_backpressure_policy_t;

// Post event with policy
bool sx_dispatcher_post_event_with_policy(
    const sx_event_t *evt,
    sx_backpressure_policy_t policy,
    uint32_t coalesce_key  // For COALESCE policy
);
```

**Files cần sửa:**
- `components/sx_core/sx_dispatcher.c` - Implement coalesce logic

**Migration Steps:**
1. Add backpressure policy enum
2. Implement coalesce logic (keep latest event per key)
3. Update `sx_dispatcher_post_event()` to use policy
4. Integrate với metrics (count coalesced events)

**Risk/Rollback:**
- Medium risk - Thay đổi event posting behavior
- Cần test kỹ để đảm bảo không mất events quan trọng
- Rollback: Git revert

**Estimated effort:** 1 day

---

## 4. P2 TASKS (Medium Priority)

### P2.1: Event Taxonomy với Range Reservation

**Outcome:** Event IDs có range reservation theo domain

**Files cần sửa:**
- `components/sx_core/include/sx_event.h`

**Implementation:**
```c
// components/sx_core/include/sx_event.h

// Event ID ranges (0x0100 IDs per domain)
#define SX_EVT_UI_BASE           0x0000
#define SX_EVT_UI_RANGE          0x0100
#define SX_EVT_AUDIO_BASE        0x0100
#define SX_EVT_AUDIO_RANGE       0x0100
#define SX_EVT_WIFI_BASE         0x0200
#define SX_EVT_WIFI_RANGE        0x0100
// ... more domains ...

typedef enum {
    // UI events (0x0000-0x00FF)
    SX_EVT_UI_INPUT = SX_EVT_UI_BASE,
    SX_EVT_UI_READY,
    // ... more UI events ...
    
    // Audio events (0x0100-0x01FF)
    SX_EVT_AUDIO_PLAYBACK_STARTED = SX_EVT_AUDIO_BASE,
    // ... more audio events ...
    
    // ... more domains ...
} sx_event_id_t;
```

**Estimated effort:** 0.5 day

---

### P2.2: Enhance check_architecture.sh Script

**Outcome:** Script check đầy đủ hơn (check sx_lvgl.h, compile-time guard)

**Files cần sửa:**
- `scripts/check_architecture.sh`

**Estimated effort:** 0.5 day

---

### P2.3: Add Static Analysis Config

**Outcome:** clang-tidy/cppcheck config files

**Files cần tạo:**
- `.clang-tidy`
- `scripts/run_static_analysis.sh`

**Estimated effort:** 1 day

---

### P2.4: Create Resource Ownership Table

**Outcome:** File `docs/SIMPLEXL_RESOURCE_OWNERSHIP.md`

**Files cần tạo:**
- `docs/SIMPLEXL_RESOURCE_OWNERSHIP.md`

**Estimated effort:** 0.5 day

---

### P2.5: Add Architecture Compliance Tests

**Outcome:** Test files trong `test/unit_test/` cho architecture compliance

**Files cần tạo:**
- `test/unit_test/test_architecture_compliance.c`

**Estimated effort:** 1 day

---

### P2.6: Create Quality Gates Doc

**Outcome:** File `docs/SIMPLEXL_QUALITY_GATES.md`

**Files cần tạo:**
- `docs/SIMPLEXL_QUALITY_GATES.md`

**Estimated effort:** 0.5 day

---

## 5. TỔNG KẾT ROADMAP

### Timeline ước lượng:

- **P0 Tasks:** 4-5 days
- **P1 Tasks:** 3-4 days
- **P2 Tasks:** 4-5 days

**Tổng:** ~11-14 days để đạt 10/10

### Thứ tự thực hiện đề xuất:

1. **Week 1:** P0.1 (Fix services) + P0.2 (Wrapper) + P0.3 (Guard) + P0.4 (State fields)
2. **Week 2:** P1.1 (Assert) + P1.2 (Metrics) + P1.3 (Lifecycle) + P1.4 (Backpressure)
3. **Week 3:** P2 tasks (taxonomy, scripts, tests, docs)

### PRs đề xuất:

1. **PR #1:** Fix 4 services violations (P0.1)
2. **PR #2:** Add sx_lvgl.h wrapper + compile-time guard (P0.2 + P0.3)
3. **PR #3:** Add version + dirty_mask to state (P0.4)
4. **PR #4:** Add runtime assert + metrics (P1.1 + P1.2)
5. **PR #5:** Add lifecycle interfaces + backpressure (P1.3 + P1.4)
6. **PR #6:** Enhance scripts + add tests (P2 tasks)

---

**Tiếp tục:** Phụ lục với evidence trích đoạn sẽ được tạo trong file tiếp theo.






