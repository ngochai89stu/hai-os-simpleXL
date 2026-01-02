# Async Job Event Contract

> **Mục tiêu:** Chuẩn hóa pattern emit event progress/error/finished cho các service async (OTA, Download, Upload...) để UI có thể xử lý theo cùng một pattern.

---

## 📋 TỔNG QUAN

Các service async (OTA, Download, Chat upload...) cần emit events theo pattern chuẩn để:
- UI screen có thể xử lý progress/error từ nhiều service khác nhau theo cùng một pattern
- Dễ tạo "generic progress dialog" component
- Code review dễ hơn (biết ngay service nào emit event đúng/không đúng contract)

---

## 🎯 PATTERN CHUẨN

### Progress Event

**Event Type:** `SX_EVT_<SERVICE>_PROGRESS`

**Payload:**
- `arg0`: percent (0-100)
- `arg1`: speed (KB/s) hoặc bytes_transferred
- `ptr`: NULL hoặc optional status string

**Example:**
```c
sx_event_t evt = {
    .type = SX_EVT_OTA_PROGRESS,
    .priority = SX_EVT_PRIORITY_NORMAL,
    .arg0 = (uint32_t)progress,      // 0-100
    .arg1 = (uint32_t)speed_kbps,    // KB/s
    .ptr = NULL
};
sx_dispatcher_post_event(&evt);
```

---

### Finished Event

**Event Type:** `SX_EVT_<SERVICE>_FINISHED`

**Payload:**
- `arg0`: 0 (reserved)
- `arg1`: 0 (reserved)
- `ptr`: result string (version, file path, etc.)

**Example:**
```c
sx_event_t evt = {
    .type = SX_EVT_OTA_FINISHED,
    .priority = SX_EVT_PRIORITY_NORMAL,
    .arg0 = 0,
    .arg1 = 0,
    .ptr = sx_event_alloc_string("1.0.0")  // Version string
};
sx_dispatcher_post_event(&evt);
```

---

### Error Event

**Event Type:** `SX_EVT_<SERVICE>_ERROR`

**Payload:**
- `arg0`: error code (esp_err_t cast to uint32_t)
- `arg1`: 0 (reserved)
- `ptr`: error message string

**Example:**
```c
sx_event_t evt = {
    .type = SX_EVT_OTA_ERROR,
    .priority = SX_EVT_PRIORITY_NORMAL,
    .arg0 = (uint32_t)ESP_ERR_NO_MEM,
    .arg1 = 0,
    .ptr = sx_event_alloc_string("Out of memory")
};
sx_dispatcher_post_event(&evt);
```

---

## 🔧 OPTIONAL HELPER MACROS

Các service có thể dùng helper macros để code ngắn gọn hơn (không bắt buộc):

```c
// sx_async_job_helper.h (optional)

#define SX_ASYNC_JOB_EMIT_PROGRESS(evt_type, percent, speed) \
    do { \
        sx_event_t evt = { \
            .type = evt_type, \
            .priority = SX_EVT_PRIORITY_NORMAL, \
            .arg0 = (uint32_t)(percent), \
            .arg1 = (uint32_t)(speed), \
            .ptr = NULL \
        }; \
        sx_dispatcher_post_event(&evt); \
    } while(0)

#define SX_ASYNC_JOB_EMIT_FINISHED(evt_type, result_str) \
    do { \
        sx_event_t evt = { \
            .type = evt_type, \
            .priority = SX_EVT_PRIORITY_NORMAL, \
            .arg0 = 0, \
            .arg1 = 0, \
            .ptr = sx_event_alloc_string(result_str) \
        }; \
        sx_dispatcher_post_event(&evt); \
    } while(0)

#define SX_ASYNC_JOB_EMIT_ERROR(evt_type, err_code, err_msg) \
    do { \
        sx_event_t evt = { \
            .type = evt_type, \
            .priority = SX_EVT_PRIORITY_NORMAL, \
            .arg0 = (uint32_t)(err_code), \
            .arg1 = 0, \
            .ptr = sx_event_alloc_string(err_msg) \
        }; \
        sx_dispatcher_post_event(&evt); \
    } while(0)
```

**Usage:**
```c
// Instead of:
sx_event_t evt = { ... };
sx_dispatcher_post_event(&evt);

// Can use:
SX_ASYNC_JOB_EMIT_PROGRESS(SX_EVT_OTA_PROGRESS, 50, 100);
```

---

## 📚 EXAMPLES

### OTA Service (Reference Implementation)

**File:** `components/sx_services/sx_ota_full.cpp`

**Progress:**
```cpp
sx_event_t evt = {
    .type = SX_EVT_OTA_PROGRESS,
    .priority = SX_EVT_PRIORITY_NORMAL,
    .arg0 = (uint32_t)progress,      // 0-100
    .arg1 = (uint32_t)speed_kbps,    // KB/s
    .ptr = NULL
};
sx_dispatcher_post_event(&evt);
```

**Finished:**
```cpp
sx_event_t evt = {
    .type = SX_EVT_OTA_FINISHED,
    .priority = SX_EVT_PRIORITY_NORMAL,
    .arg0 = 0,
    .arg1 = 0,
    .ptr = sx_event_alloc_string(app ? app->version : "unknown")
};
sx_dispatcher_post_event(&evt);
```

**Error:**
```cpp
sx_event_t evt = {
    .type = SX_EVT_OTA_ERROR,
    .priority = SX_EVT_PRIORITY_NORMAL,
    .arg0 = 0,
    .arg1 = 0,
    .ptr = sx_event_alloc_string("OTA check failed")
};
sx_dispatcher_post_event(&evt);
```

---

## ✅ CHECKLIST

Khi implement service async mới, đảm bảo:

- [ ] **Progress event** có `arg0` = percent (0-100), `arg1` = speed/bytes
- [ ] **Finished event** có `ptr` = result string (version, file path, etc.)
- [ ] **Error event** có `arg0` = error code, `ptr` = error message
- [ ] **Event priority** là `SX_EVT_PRIORITY_NORMAL` (trừ khi có lý do đặc biệt)
- [ ] **String payload** dùng `sx_event_alloc_string()` để tránh memory leak

---

## 🎓 TÀI LIỆU THAM KHẢO

- **Event System:** `components/sx_core/include/sx_event.h`
- **OTA Service:** `components/sx_services/sx_ota_full.cpp` (reference implementation)
- **Dispatcher:** `components/sx_core/include/sx_dispatcher.h`

---

**Tài liệu này sẽ được cập nhật khi có thêm patterns hoặc thay đổi contract.**






