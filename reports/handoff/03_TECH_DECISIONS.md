# Technical Decision Log

---

### ADR-002: Clean-room split — xiaozhi-esp32 is reference-only

- **Context**: Goal is a new product repo with SimpleXL architecture and no legacy UI/build graph.
- **Decision**: Treat `D:\NEWESP32\xiaozhi-esp32` as **reference/parts bin only**. Do not fork/copy wholesale. Port by rewriting clean modules.
- **Alternatives considered**:
  1. Fork repo cũ và dọn dần: nhanh nhưng kéo theo legacy build graph/nợ kỹ thuật.
  2. Copy toàn bộ rồi "strip" dần: rủi ro bỏ sót legacy include.
- **Consequences**:
  - Pro: build graph sạch, enforce boundary dễ.
  - Con: cần thời gian thiết kế API và port từng phần.
- **Verification**:
  - Include graph của repo mới không chứa `display/ui_system/**`.

---

### ADR-003: PSRAM config migration is surgical (no blind sdkconfig copy)

- **Context**: LVGL buffer allocation failing; need PSRAM/heap settings proven on the hardware.
- **Decision**: Extract only PSRAM/heap/flash-relevant `CONFIG_` lines from `xiaozhi-esp32/sdkconfig.defaults.esp32s3` into `hai-os-simplexl/sdkconfig.defaults`.
- **Alternatives considered**:
  1. Copy entire sdkconfig: fast but drags unrelated settings.
  2. menuconfig: interactive + hard to version as atomic changes.
- **Consequences**:
  - Pro: minimal config drift.
  - Con: may miss additional required PSRAM-related options; iterative additions needed.
- **Verification**:
  - Boot log shows PSRAM initialization and heap pool added; `lvgl_port_add_disp()` succeeds.

---

### ADR-004: Phase 2 UI Router — Screen Container Pattern (C-only, no C++)

- **Context**: Phase 2 requires UI router + screen registry. Legacy repo uses C++ ScreenBase class hierarchy.
- **Decision**: Implement router in **pure C** với screen container pattern:
  - Single `lv_obj_t*` container trong `ui_router` (created once, reused by all screens)
  - Screens register callbacks (`on_create/on_show/on_hide/on_destroy`) via `ui_screen_registry`
  - Navigation = clear container + call lifecycle callbacks
  - No inheritance, no virtual functions, no C++ dependencies
- **Alternatives considered**:
  1. Port C++ ScreenBase: nhanh nhưng vi phạm "no legacy UI" rule và tăng complexity.
  2. Hybrid C/C++: rủi ro circular dependencies và build graph phức tạp.
- **Consequences**:
  - Pro: SimpleXL architecture thuần C, dễ enforce boundaries, không kéo C++ runtime.
  - Con: Screens phải tự quản lý LVGL objects (không có base class helpers). Cần discipline để cleanup đúng.
- **Verification**:
  - `grep -r "class\|ScreenBase" components/sx_ui/` returns no matches.
  - Router successfully navigates boot → home based on state.
  - Screen registry lookup works for all registered screens.

---

### ADR-005: Phase 2 Navigation — State-based (polling) before Event-driven

- **Context**: UI cần navigation logic. SimpleXL yêu cầu event/state-driven architecture.
- **Decision**: Phase 2 dùng **state polling** trong UI task loop:
  - UI task polls `sx_dispatcher_get_state()` mỗi 100ms
  - Navigation dựa trên `sx_state_t.device_state` (BOOTING → IDLE → home screen)
  - Phase 4 sẽ thêm event-driven navigation khi services emit `SX_EVT_UI_INPUT` events
- **Alternatives considered**:
  1. Event-driven ngay từ Phase 2: cần orchestrator emit navigation events → phức tạp hơn cho demo.
  2. Direct function calls từ orchestrator: vi phạm "UI owner only calls LVGL" rule.
- **Consequences**:
  - Pro: Đơn giản cho Phase 2, đủ để demo navigation flow.
  - Con: Polling overhead (nhỏ, 100ms interval). Phase 4 sẽ chuyển sang event-driven để reactive hơn.
- **Verification**:
  - Boot log shows "Navigated to screen: boot" → "Navigated to screen: home" khi state chuyển IDLE.
  - No direct service → UI function calls (grep confirms).
