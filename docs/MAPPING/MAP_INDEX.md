# Mapping Index - Cross-Repo Porting

**Ngày tạo:** 2025-01-02  
**Mục đích:** Index tổng quan về mapping giữa XIAOZHI (source) và SIMPLEXL (target)

## Tài Liệu Mapping

1. **[MAP_MODULE_MATRIX.md](./MAP_MODULE_MATRIX.md)** - Bảng ánh xạ module/component giữa 2 repo
2. **[MAP_EVENTS_STATE.md](./MAP_EVENTS_STATE.md)** - Mapping event schema và state management
3. **[MAP_PORTING_PLAN.md](./MAP_PORTING_PLAN.md)** - Kế hoạch porting chi tiết theo batch

## Tài Liệu Cross-Repo

4. **[../CALL_GRAPH_CROSS_REPO.md](../CALL_GRAPH_CROSS_REPO.md)** - Call graph xuyên repo (boot/ui/audio/network flows)
5. **[../OWNERSHIP_THREADING_RULES_CROSS_REPO.md](../OWNERSHIP_THREADING_RULES_CROSS_REPO.md)** - Ownership và threading rules xuyên repo

## Repo Information

### SIMPLEXL (Target)
- **Path:** `D:\NEWESP32\hai-os-simplexl`
- **Entry Point:** `app/app_main.c:8` → `sx_bootstrap_start()`
- **Architecture:** Event-driven dispatcher/orchestrator + service/screen lifecycle
- **UI Framework:** LVGL (chỉ trong sx_ui, compile-time guard)

### XIAOZHI (Source)
- **Path:** `D:\xiaozhi-esp32_vietnam_ref`
- **Entry Point:** `main/main.cc:55` → `Application::GetInstance().Start()`
- **Architecture:** Application singleton + main event loop + protocol callbacks
- **UI Framework:** LVGL (trong display task, core 1)

## Mapping Strategy

### Symbol Equivalence
- Lập bảng ánh xạ Function/Struct/Enum giữa XIAOZHI và SIMPLEXL
- Mỗi mapping phải có: bằng chứng line-range + ít nhất 2 call-sites

### Behavior Equivalence
- Dựng 4 call chains có trích dẫn line-range:
  1) Boot chain
  2) UI chain
  3) Audio chain
  4) Network/AI chain

### Boundary Architecture
- Xác định "điểm cấy" XIAOZHI vào layering SIMPLEXL
- Khuyến nghị: `components/sx_xiaozhi_vendor` (code) + `components/sx_xiaozhi_adapter` (facade)

### Invariant Audit
- Đặt >=10 invariants (UI thread rule, lock order, init order, memory ownership, ISR safety, backpressure policy)

## Tiến Độ

- [x] BATCH 0: Repo Discovery & Build Baseline ✅
- [x] BATCH 1: SIMPLEXL Core Contracts ✅
- [x] BATCH 2: XIAOZHI Core Runtime ✅
- [ ] BATCH 3: Mapping Voice Loop End-to-End
- [ ] BATCH 4: Mapping Protocol & Device Control
- [ ] BATCH 5: UI Binding

## File Đã Đọc

### SIMPLEXL
- ✅ `app/app_main.c` (12 lines)
- ✅ `components/sx_core/sx_bootstrap.c` (832 lines)
- ✅ `CMakeLists.txt` (9 lines)
- ✅ `sdkconfig.defaults` (31 lines)
- ✅ `partitions.csv` (9 lines)
- ✅ `docs/API_CATALOG.md` (84 lines)
- ✅ `docs/SIMPLEXL_ARCH_v1.3.md` (375 lines)
- ✅ `docs/CALL_GRAPH.md` (719 lines)
- ✅ `docs/OWNERSHIP_THREADING_RULES.md` (497 lines)
- ✅ `docs/REPORT_PHASE_2_RUNTIME.md` (675 lines)

**Tổng:** ~2,800+ lines đã đọc (~5-8% codebase)

### XIAOZHI
- ✅ `main/main.cc` (75 lines)
- ✅ `main/application.h` (133 lines)
- ✅ `main/application.cc` (200 lines đầu)
- ✅ `CMakeLists.txt` (14 lines)
- ✅ `sdkconfig.defaults` (78 lines)
- ✅ `partitions/v2/16m.csv` (9 lines)
- ✅ `docs/API_CATALOG.md` (40 lines)
- ✅ `docs/README.md` (316 lines)
- ✅ `docs/REPORT_PHASE_0_BASELINE.md` (344 lines)
- ✅ `docs/REPORT_PHASE_1_MODULE_MAP.md` (555 lines)
- ✅ `docs/REPORT_PHASE_2_RUNTIME.md` (536 lines)

**Tổng:** ~2,200+ lines đã đọc (~5-8% codebase)

### BATCH 1 & 2 Files

**SIMPLEXL (BATCH 1):**
- ✅ `components/sx_core/include/sx_dispatcher.h` (57 lines)
- ✅ `components/sx_core/sx_dispatcher.c` (385 lines)
- ✅ `components/sx_core/include/sx_orchestrator.h` (12 lines)
- ✅ `components/sx_core/sx_orchestrator.c` (182 lines)
- ✅ `components/sx_core/include/sx_event.h` (188 lines)
- ✅ `components/sx_core/include/sx_state.h` (102 lines)
- ✅ `components/sx_core/include/sx_state_helper.h` (29 lines)
- ✅ `components/sx_core/sx_event_handler.c` (71 lines)
- ✅ `components/sx_core/include/sx_service_if.h` (115 lines)
- ✅ `components/sx_core/sx_service_if.c` (207 lines)

**Tổng BATCH 1:** ~1,368 lines

**XIAOZHI (BATCH 2):**
- ✅ `main/application.h` (133 lines)
- ✅ `main/application.cc` (650+ lines)
- ✅ `main/audio/audio_service.h` (162 lines)
- ✅ `main/audio/audio_service.cc` (690+ lines)
- ✅ `main/protocols/protocol.h` (98 lines)
- ✅ `main/protocols/mqtt_protocol.h` (61 lines)
- ✅ `main/protocols/websocket_protocol.h` (35 lines)
- ✅ `main/protocols/mqtt_protocol.cc` (150+ lines)
- ✅ `main/protocols/websocket_protocol.cc` (150+ lines)
- ✅ `main/device_state.h` (18 lines)
- ✅ `main/device_state_event.cc` (46 lines)

**Tổng BATCH 2:** ~2,200+ lines

## Tổng Kết BATCH 0

**Status:** ✅ Completed

**Output Files Created:**
1. ✅ `docs/API_CATALOG_SIMPLEXL.md` - SIMPLEXL API catalog index
2. ✅ `docs/API_CATALOG_XIAOZHI.md` - XIAOZHI API catalog index
3. ✅ `docs/MAPPING/MAP_INDEX.md` - Mapping index (this file)
4. ✅ `docs/MAPPING/MAP_MODULE_MATRIX.md` - Module mapping matrix
5. ✅ `docs/MAPPING/MAP_EVENTS_STATE.md` - Events & state mapping
6. ✅ `docs/MAPPING/MAP_PORTING_PLAN.md` - Porting plan với test protocol
7. ✅ `docs/CALL_GRAPH_CROSS_REPO.md` - Call graph xuyên repo
8. ✅ `docs/OWNERSHIP_THREADING_RULES_CROSS_REPO.md` - Ownership & threading rules với 10 invariants

**Key Findings:**
- SIMPLEXL: Event-driven dispatcher với priority queues, double-buffer state, LVGL compile-time guard
- XIAOZHI: Application singleton với Event Group, DeviceState enum, LVGL runtime-only
- Mapping challenges: Event system mismatch, state management mismatch, audio ownership conflict
- Adapter strategy: `sx_xiaozhi_adapter` facade, SIMPLEXL audio_service làm owner, state updates qua dispatcher

**Next Steps:**
- BATCH 1: Phân tích sâu SIMPLEXL core contracts (dispatcher, orchestrator, service lifecycle)
- BATCH 2: Phân tích sâu XIAOZHI core runtime (main/, protocol, audio/voice core)
- BATCH 3: Implement voice loop adapter (minimum viable)

