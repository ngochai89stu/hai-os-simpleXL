# Phase 3: Architecture Improvements (P2 - Optional) - Completed

**Ngày:** 2025-01-02  
**Phase:** Phase 3 - Architecture Improvements  
**Status:** ✅ Partially Completed (Core improvement done)

---

## Tổng Quan

Đã implement **Handler Return Dirty-Mask** - cải tiến kiến trúc quan trọng nhất để giảm coupling giữa orchestrator và handlers.

**Các improvements khác (UI queue separation, SD state domain) có thể implement sau nếu cần.**

---

## 1. Handler Return Dirty-Mask ✅

### File: `components/sx_core/include/sx_event_handler.h`, `components/sx_core/sx_event_handler.c`, `components/sx_core/sx_orchestrator.c`, và tất cả handler files

### Vấn Đề:
- Orchestrator phải tự tính `dirty_mask` dựa trên event type
- Switch-case lớn trong orchestrator → coupling cao
- Khó maintain khi thêm event mới (phải update orchestrator)
- Handler không thể chỉ định domain chính xác (ví dụ: một event có thể affect nhiều domains)

### Fix Applied:

**1. Updated handler signature:**
```c
// BEFORE:
typedef bool (*sx_event_handler_t)(const sx_event_t *evt, sx_state_t *state);

// AFTER:
typedef uint32_t (*sx_event_handler_t)(const sx_event_t *evt, sx_state_t *state);
// Returns dirty_mask (0 = not handled or no update needed)
```

**2. Updated orchestrator:**
```c
// BEFORE:
if (sx_event_handler_process(&evt, &st)) {
    // Large switch-case to determine dirty_mask
    switch (evt.type) {
        case SX_EVT_WIFI_CONNECTED: dirty_mask = SX_STATE_DIRTY_WIFI; break;
        // ... many cases ...
    }
    sx_state_update_version_and_dirty(&st, dirty_mask);
    sx_dispatcher_set_state(&st);
}

// AFTER:
uint32_t dirty_mask = sx_event_handler_process(&evt, &st);
if (dirty_mask != 0) {
    // Handler returned dirty_mask - use it directly
    sx_state_update_version_and_dirty(&st, dirty_mask);
    sx_dispatcher_set_state(&st);
}
```

**3. Updated all handlers:**

**Example - WiFi handler:**
```c
// BEFORE:
bool sx_event_handler_wifi_state_update(...) {
    // ... update state ...
    return true; // Orchestrator will guess dirty_mask
}

// AFTER:
uint32_t sx_event_handler_wifi_state_update(...) {
    // ... update state ...
    return SX_STATE_DIRTY_WIFI; // Handler specifies exact domain
}
```

**Example - Multi-domain handler:**
```c
// AFTER:
uint32_t sx_event_handler_chatbot_audio_channel_opened(...) {
    // ... update state ...
    return SX_STATE_DIRTY_UI | SX_STATE_DIRTY_AUDIO; // Handler can specify multiple domains
}
```

### Impact:
- ✅ **Giảm coupling**: Orchestrator không cần biết event → domain mapping
- ✅ **Dễ maintain**: Thêm event mới chỉ cần update handler, không cần orchestrator
- ✅ **Chính xác hơn**: Handler có thể chỉ định multiple domains nếu cần
- ✅ **Code sạch hơn**: Loại bỏ switch-case lớn trong orchestrator

### Files Modified:

**Core:**
1. `components/sx_core/include/sx_event_handler.h` - Updated typedef
2. `components/sx_core/sx_event_handler.c` - Updated process function
3. `components/sx_core/sx_orchestrator.c` - Simplified logic

**Handlers (all updated):**
4. `components/sx_core/sx_event_handlers/wifi_handler.c` - WiFi handlers
5. `components/sx_core/sx_event_handlers/audio_handler.c` - Audio handlers
6. `components/sx_core/sx_event_handlers/ui_input_handler.c` - UI input handler
7. `components/sx_core/sx_event_handlers/radio_handler.c` - Radio handler
8. `components/sx_core/sx_event_handlers/stt_tts_handler.c` - STT/TTS handlers
9. `components/sx_core/sx_event_handlers/chatbot_handler.c` - Chatbot handlers (10+ handlers)
10. `components/sx_core/sx_event_handlers/event_handlers_ota.c` - OTA handlers
11. `components/sx_core/sx_event_handlers/event_handlers.h` - Updated all signatures

### Handler Return Values:

| Handler | Return Value | Reason |
|---------|--------------|--------|
| WiFi handlers | `SX_STATE_DIRTY_WIFI` | WiFi domain changed |
| Audio handlers | `SX_STATE_DIRTY_AUDIO` | Audio domain changed |
| UI handlers | `SX_STATE_DIRTY_UI` | UI domain changed |
| System handlers | `SX_STATE_DIRTY_SYSTEM` | System domain changed |
| Multi-domain handlers | `SX_STATE_DIRTY_UI \| SX_STATE_DIRTY_AUDIO` | Multiple domains changed |
| No-op handlers | `0` | No state update needed |

---

## Verification Checklist

- [x] Code compiles without errors
- [x] No linter errors
- [x] All handlers updated to return `uint32_t`
- [x] Orchestrator simplified (no switch-case)
- [x] Handler signatures updated in header
- [ ] Manual testing: Verify dirty_mask propagation
- [ ] Manual testing: Verify UI updates correctly

---

## Performance Impact

| Metric | Before | After | Change |
|--------|--------|-------|--------|
| **Orchestrator complexity** | Large switch-case | Simple if-check | ✅ Reduced |
| **Coupling** | High (orchestrator knows all events) | Low (handlers specify domains) | ✅ Reduced |
| **Maintainability** | Low (update orchestrator for new events) | High (only update handler) | ✅ Improved |

---

## Next Steps (Optional)

### 2. UI Event Queue Separation (Not Implemented)
- **Rationale**: Tách queue riêng cho UI events để tránh event loss khi UI task busy
- **Status**: Pending (có thể implement sau nếu cần)
- **Impact**: Medium (chỉ cần thiết nếu UI events bị drop)

### 3. SD State Domain (Not Implemented)
- **Rationale**: Thêm SD state domain vào state structure để UI có thể filter SD-related updates
- **Status**: Pending (có thể implement sau nếu cần)
- **Impact**: Low (SD state hiện tại được handle qua UI domain)

---

## Summary

**Completed:**
- ✅ Handler return dirty-mask (core architecture improvement)

**Pending (Optional):**
- ⏸️ UI event queue separation
- ⏸️ SD state domain

**Recommendation:**
- Core improvement (handler return dirty-mask) đã hoàn thành và đủ để cải thiện architecture
- Các improvements khác có thể implement sau nếu cần thiết

---

**Status:** ✅ Core architecture improvement completed  
**Ready for:** Phase 4 (Test Compliance) hoặc Production Testing
