# SIMPLEXL Resource Ownership Table
## Tài Liệu Về Quyền Sở Hữu Tài Nguyên

> **Version:** 1.3  
> **Section:** 6.3 SIMPLEXL_ARCH v1.3  
> **Mục đích:** Xác định rõ ai allocate/free resources, thread nào được touch, và lifetime

---

## 1. TỔNG QUAN

Tài liệu này định nghĩa quyền sở hữu (ownership) cho tất cả resources trong hệ thống SimpleXL:
- **Allocator:** Component/task nào allocate resource
- **Owner:** Component/task nào sở hữu và chịu trách nhiệm free
- **Thread:** Thread nào được phép access/modify resource
- **Lifetime:** Resource tồn tại trong khoảng thời gian nào

---

## 2. EVENT PAYLOADS

### 2.1 Event Payload Pointers (`sx_event_t.ptr`)

**Rule:** Payload pointers trong events có ownership rules rõ ràng:

| Resource Type | Allocator | Owner | Thread | Lifetime | Notes |
|--------------|-----------|-------|--------|----------|-------|
| `sx_display_capture_payload_t*` | Service | UI (temporary) | UI task | Event processing | UI fills buffer, service must copy if needed |
| `sx_display_preview_payload_t*` | Service | UI | UI task | Event processing | UI copies data, service can free after post |
| `sx_theme_data_t*` | Service | UI | UI task | Event processing | UI applies theme, service can free after post |
| `sx_image_load_payload_t*` | Service | UI | UI task | Event processing | UI copies data, service can free after post |
| `sx_qr_code_result_t*` | Service | UI | UI task | Event processing | UI copies data, service can free after post |
| String pointers (text) | Service/UI | Event system | Any | Event processing | Use string pool for optimization |

**General Rule:**
- **Service → UI events:** Service allocates, UI copies if needed, service can free after `sx_dispatcher_post_event()`
- **UI → Service events:** UI allocates, service copies if needed, UI can free after `sx_dispatcher_post_event()`

---

## 3. STATE SNAPSHOT

### 3.1 `sx_state_t`

| Resource Type | Allocator | Owner | Thread | Lifetime | Notes |
|--------------|-----------|-------|--------|----------|-------|
| `sx_state_t` | Orchestrator | Orchestrator | Orchestrator (writer), Any (reader) | Application lifetime | Single-writer, multi-reader pattern |
| State buffers | Orchestrator | Orchestrator | Orchestrator | Application lifetime | Copy-out pattern, no shared mutable pointers |
| String fields | Orchestrator | Orchestrator | Orchestrator | State epoch | Use string pool for optimization |

**Lifetime:** State epoch (until next state update)

---

## 4. LVGL OBJECTS

### 4.1 LVGL UI Objects

| Resource Type | Allocator | Owner | Thread | Lifetime | Notes |
|--------------|-----------|-------|--------|----------|-------|
| `lv_obj_t*` | UI task | UI task | UI task ONLY | Screen lifetime | Must use `SX_LVGL_CALL()` wrapper |
| LVGL image descriptors | UI task | UI task | UI task ONLY | Image lifetime | Created from RGB565 data |
| LVGL canvas buffers | UI task | UI task | UI task ONLY | Canvas lifetime | Used for screen capture |

**Rule:** **ONLY UI task** can create/modify/destroy LVGL objects (enforced by `SX_ASSERT_UI_THREAD()`)

---

## 5. SERVICE RESOURCES

### 5.1 Audio Service

| Resource Type | Allocator | Owner | Thread | Lifetime | Notes |
|--------------|-----------|-------|--------|----------|-------|
| Audio buffers | Audio service | Audio service | Audio task | Stream lifetime | Allocated per stream |
| Codec handles | Audio service | Audio service | Audio task | Service lifetime | Hardware codec (ES8388/ES8311) |

### 5.2 WiFi Service

| Resource Type | Allocator | Owner | Thread | Lifetime | Notes |
|--------------|-----------|-------|--------|----------|-------|
| WiFi config | WiFi service | WiFi service | WiFi task | Connection lifetime | Network configuration |
| Scan results | WiFi service | WiFi service | WiFi task | Scan operation | Temporary, freed after event |

### 5.3 Display Service

| Resource Type | Allocator | Owner | Thread | Lifetime | Notes |
|--------------|-----------|-------|--------|----------|-------|
| Preview image data | Display service | UI task (after event) | UI task | Preview lifetime | Service allocates, UI copies |
| Capture buffers | Service (caller) | Service (caller) | UI task (fills) | Capture operation | Caller provides buffer |

### 5.4 Image Service

| Resource Type | Allocator | Owner | Thread | Lifetime | Notes |
|--------------|-----------|-------|--------|----------|-------|
| Decoded image data | Image service | Caller | Any | Caller lifetime | Caller must free |
| Image descriptors | Image service | Caller | Any | Caller lifetime | Removed in P0.1 (moved to UI) |

### 5.5 QR Code Service

| Resource Type | Allocator | Owner | Thread | Lifetime | Notes |
|--------------|-----------|-------|--------|----------|-------|
| QR code bitmap | QR code service | Caller | Any | Caller lifetime | Caller must free via `sx_qr_code_free_result()` |

---

## 6. PLATFORM RESOURCES

### 6.1 Hardware Handles

| Resource Type | Allocator | Owner | Thread | Lifetime | Notes |
|--------------|-----------|-------|--------|----------|-------|
| Display handles | Platform | Platform | Platform/UI | Application lifetime | LCD panel, backlight |
| Touch handles | Platform | Platform | Platform/UI | Application lifetime | Touch driver |
| SPI bus handles | Platform | Platform | Platform | Application lifetime | Shared SPI bus |
| I2C bus handles | Platform | Platform | Platform | Application lifetime | Shared I2C bus |
| SD card handles | Platform | Platform | Platform | Application lifetime | SD card driver |

**Rule:** Platform owns all hardware handles, other components use them read-only

---

## 7. EVENT QUEUES

### 7.1 Dispatcher Queues

| Resource Type | Allocator | Owner | Thread | Lifetime | Notes |
|--------------|-----------|-------|--------|----------|-------|
| Event queues | Dispatcher | Dispatcher | Any (producer), Orchestrator (consumer) | Application lifetime | FreeRTOS queues |
| Event payloads | Producer | Consumer | Producer/Consumer | Event processing | Ownership transferred via event |

**Rule:** Producer allocates payload, consumer must copy if needed beyond event processing

---

## 8. STRING POOL

### 8.1 Event String Pool

| Resource Type | Allocator | Owner | Thread | Lifetime | Notes |
|--------------|-----------|-------|--------|----------|-------|
| String pool | Event system | Event system | Any | Application lifetime | Optimized string storage for events |
| Pooled strings | Event system | Event system | Any | Pool lifetime | Shared strings, reference counted |

---

## 9. SCREEN RESOURCES

### 9.1 Screen Objects

| Resource Type | Allocator | Owner | Thread | Lifetime | Notes |
|--------------|-----------|-------|--------|----------|-------|
| Screen LVGL objects | Screen `create()` | Screen | UI task ONLY | Screen lifetime | Created in `create()`, destroyed in `destroy()` |
| Screen state | Screen | Screen | UI task | Screen lifetime | Screen-specific state |

**Lifetime:** Screen lifetime (from `create()` to `destroy()`)

---

## 10. SERVICE INTERFACE INSTANCES

### 10.1 Service Registry

| Resource Type | Allocator | Owner | Thread | Lifetime | Notes |
|--------------|-----------|-------|--------|----------|-------|
| Service registry entries | Service system | Service system | Bootstrap/Orchestrator | Application lifetime | Registered during bootstrap |
| Service interface vtables | Service | Service | Service | Service lifetime | Static vtable structures |

---

## 11. OWNERSHIP RULES SUMMARY

### 11.1 General Rules

1. **Event Payloads:**
   - Producer allocates, consumer copies if needed
   - Producer can free after `sx_dispatcher_post_event()` returns
   - Consumer must copy if lifetime extends beyond event handler

2. **LVGL Objects:**
   - **ONLY UI task** can create/modify/destroy
   - Lifetime: Screen lifetime or explicit destroy
   - Must use `SX_LVGL_CALL()` wrapper

3. **State Snapshot:**
   - Orchestrator is single writer
   - Copy-out pattern (no shared mutable pointers)
   - Readers get immutable snapshot

4. **Hardware Handles:**
   - Platform owns all hardware handles
   - Other components use read-only

5. **Service Resources:**
   - Service owns its internal resources
   - Resources freed in `deinit()` lifecycle

### 11.2 Thread Safety

- **Event queues:** Thread-safe (FreeRTOS queues)
- **State snapshot:** Protected by mutex (copy-out pattern)
- **LVGL objects:** UI task ONLY (enforced by runtime assert)
- **Service resources:** Service-specific (may need mutex per service)

---

## 12. MEMORY LEAK PREVENTION

### 12.1 Checklist

- ✅ Event payloads: Producer frees after post (if not needed)
- ✅ LVGL objects: Destroyed in screen `destroy()` callback
- ✅ Service resources: Freed in service `deinit()` callback
- ✅ State strings: Use string pool (automatic cleanup)
- ✅ Image data: Caller frees (documented in API)

---

**Kết thúc Resource Ownership Table.**






