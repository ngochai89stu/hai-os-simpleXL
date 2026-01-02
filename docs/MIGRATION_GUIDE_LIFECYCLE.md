# Migration Guide: Lifecycle Interfaces
## Hướng Dẫn Migrate Services và Screens Sang Lifecycle Interfaces

> **Version:** 1.0  
> **Date:** 2024-12-31  
> **Purpose:** Hướng dẫn migrate existing services và screens sang lifecycle interfaces (P1.4)

---

## 1. TỔNG QUAN

Lifecycle interfaces được định nghĩa trong:
- **Services:** `components/sx_core/include/sx_service_if.h` (Section 6.1)
- **Screens:** `components/sx_ui/include/sx_screen_if.h` (Section 6.2)

Migration là **optional** nhưng **recommended** để:
- Standardize lifecycle management
- Enable centralized lifecycle control
- Improve testability
- Better resource management

---

## 2. MIGRATE SERVICE

### 2.1 Tạo Lifecycle Implementation File

Tạo file `components/sx_services/<service_name>_lifecycle.c`:

```c
#include "<service_name>.h"
#include "sx_service_if.h"
#include "sx_event.h"
#include <esp_log.h>

static const char *TAG = "<service_name>_lifecycle";

// Lifecycle interface implementation
static esp_err_t service_init(void) {
    return <service_name>_init();
}

static esp_err_t service_start(void) {
    return <service_name>_start();
}

static esp_err_t service_stop(void) {
    // Implement stop logic
    return ESP_OK;
}

static esp_err_t service_deinit(void) {
    // Implement deinit logic
    return ESP_OK;
}

static esp_err_t service_on_event(const sx_event_t *evt) {
    // Handle events if needed
    return ESP_ERR_NOT_SUPPORTED;
}

// Service interface vtable
static const sx_service_if_t s_<service_name>_if = {
    .init = service_init,
    .start = service_start,
    .stop = service_stop,
    .deinit = service_deinit,
    .on_event = service_on_event,
};

// Registration function
esp_err_t <service_name>_register_lifecycle(void) {
    return sx_service_register("<service_name>", &s_<service_name>_if);
}
```

### 2.2 Register Service trong Bootstrap

Trong `components/sx_core/sx_bootstrap.c`:

```c
#include "<service_name>_lifecycle.h"

// During bootstrap
<service_name>_register_lifecycle();
```

### 2.3 Update CMakeLists.txt

Thêm file lifecycle vào `components/sx_services/CMakeLists.txt`:

```cmake
SRCS
    ...
    "<service_name>_lifecycle.c"
```

---

## 3. MIGRATE SCREEN

### 3.1 Tạo Lifecycle Implementation File

Tạo file `components/sx_ui/screens/<screen_name>_lifecycle.c`:

```c
#include "<screen_name>.h"
#include "sx_screen_if.h"
#include "sx_state.h"
#include "sx_lvgl.h"
#include <esp_log.h>

static const char *TAG = "<screen_name>_lifecycle";

// Forward declarations from <screen_name>.c
extern lv_obj_t* <screen_name>_create(lv_obj_t *parent);
extern void <screen_name>_destroy(void);
extern void <screen_name>_on_enter(void);
extern void <screen_name>_on_exit(void);
extern void <screen_name>_on_state_change(const sx_state_t *state);

// Lifecycle interface implementation
static lv_obj_t* screen_create(lv_obj_t *parent) {
    return <screen_name>_create(parent);
}

static void screen_destroy(lv_obj_t *screen) {
    (void)screen;
    <screen_name>_destroy();
}

static void screen_on_enter(lv_obj_t *screen) {
    (void)screen;
    <screen_name>_on_enter();
}

static void screen_on_exit(lv_obj_t *screen) {
    (void)screen;
    <screen_name>_on_exit();
}

static void screen_on_state_change(lv_obj_t *screen, uint32_t dirty_mask, const sx_state_t *state) {
    (void)screen;
    (void)dirty_mask;  // Use if needed
    <screen_name>_on_state_change(state);
}

// Screen interface vtable
static const sx_screen_if_t s_<screen_name>_if = {
    .create = screen_create,
    .destroy = screen_destroy,
    .on_enter = screen_on_enter,
    .on_exit = screen_on_exit,
    .on_state_change = screen_on_state_change,
};

// Registration function
esp_err_t <screen_name>_register_lifecycle(void) {
    return sx_screen_register(SCREEN_ID_<SCREEN_NAME>, &s_<screen_name>_if);
}
```

### 3.2 Register Screen trong UI Initialization

Trong `components/sx_ui/screens/register_all_screens.c`:

```c
#include "<screen_name>_lifecycle.h"

// During UI initialization
<screen_name>_register_lifecycle();
```

### 3.3 Update CMakeLists.txt

Thêm file lifecycle vào `components/sx_ui/CMakeLists.txt`:

```cmake
SRCS
    ...
    "screens/<screen_name>_lifecycle.c"
```

---

## 4. EXAMPLES

### 4.1 Service Example: `sx_audio_service`

Xem file: `components/sx_services/sx_audio_service_lifecycle.c`

### 4.2 Screen Example: `screen_home`

Xem file: `components/sx_ui/screens/screen_home_lifecycle.c`

---

## 5. BEST PRACTICES

### 5.1 Service Migration

1. **Keep existing API:** Don't break existing code
2. **Gradual migration:** Migrate services one by one
3. **Lifecycle wrapper:** Create wrapper file, don't modify existing service
4. **Event handling:** Use `on_event()` for service-specific events

### 5.2 Screen Migration

1. **Keep existing callbacks:** Use existing screen functions
2. **Dirty mask:** Use `dirty_mask` to optimize updates
3. **State snapshot:** Always use immutable state snapshot
4. **LVGL operations:** All LVGL calls must be in UI task

---

## 6. TESTING

### 6.1 Service Testing

```c
// Test lifecycle
sx_service_init_all();  // Should call all service init()
sx_service_start_all(); // Should call all service start()
sx_service_stop_all();  // Should call all service stop()
sx_service_deinit_all(); // Should call all service deinit()
```

### 6.2 Screen Testing

```c
// Test lifecycle
const sx_screen_if_t *iface = sx_screen_get_interface(SCREEN_ID_HOME);
lv_obj_t *screen = iface->create(parent);
iface->on_enter(screen);
iface->on_state_change(screen, dirty_mask, &state);
iface->on_exit(screen);
iface->destroy(screen);
```

---

## 7. MIGRATION CHECKLIST

### Service Migration:
- [ ] Create `<service_name>_lifecycle.c`
- [ ] Implement all lifecycle functions
- [ ] Register service in bootstrap
- [ ] Update CMakeLists.txt
- [ ] Test lifecycle functions

### Screen Migration:
- [ ] Create `<screen_name>_lifecycle.c`
- [ ] Implement all lifecycle functions
- [ ] Register screen in UI initialization
- [ ] Update CMakeLists.txt
- [ ] Test lifecycle functions

---

## 8. NOTES

- **Migration is optional:** Existing code works without lifecycle interfaces
- **Gradual migration:** Migrate services/screens one by one
- **No breaking changes:** Lifecycle interfaces are additive
- **Backward compatible:** Existing API still works

---

**Kết thúc Migration Guide.**






