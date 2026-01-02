# SIMPLEXL_ARCH v1.3 (Enforcement + Capacity + Testability)

> **Mục tiêu của v1.3:** Giữ nguyên “shape” của v1.2 nhưng **nâng rules lên mức có thể bảo trì dài hạn** bằng 3 bổ sung bắt buộc:
> 1) **Enforcement chặt hơn** (compile-time + CI + runtime) và “đóng” đường include LVGL.
> 2) **Capacity/Performance requirements** (để scale 30+ screens, asset SD, nhiều services mà không suy thoái).
> 3) **Architecture compliance tests** (để đảm bảo rules không bị phá dần theo thời gian).

---

## 0) Non‑negotiables (KHÔNG THƯƠNG LƯỢNG)

**0.1 — LVGL chỉ được gọi trong UI task**
- Tất cả `lv_*`, `esp_lvgl_port_*`, wrappers/helpers có gọi LVGL đều bị xem là “LVGL calls”.
- **MUST:** Có cơ chế phát hiện và fail (CI + runtime debug).
- **MUST:** Release profile phải đạt **0 violation**.

**0.2 — Services không được “dính UI”**
- `sx_services` **không include** `sx_ui/*`, **không include** `lvgl.h` / `esp_lvgl_port.h` / wrapper LVGL.
- Services chuẩn bị data + phát event + cập nhật state qua orchestrator, không render.

**0.3 — UI ↔ services chỉ giao tiếp qua events + state snapshots**
- UI chỉ phát `SX_EVT_UI_INPUT_*`.
- UI đọc state snapshot immutable.
- Services không gọi trực tiếp API UI.

**0.4 — Orchestrator là single writer của state**
- Mọi ghi state đi qua orchestrator.
- Snapshots immutable cho readers.

---

## 1) Architectural shape (hình dạng kiến trúc)

```
                 +-------------------------+
                 |        sx_ui            |
                 |  (UI task, LVGL only)  |
                 |  screens/widgets/render |
                 +-----------+-------------+
                             |
                             | read snapshot + emit UI_INPUT
                             v
+------------------+   events   +---------------------------+
|   sx_services     +----------->|         sx_core            |
| audio/wifi/ir/... |           | orchestrator + dispatcher   |
| NO UI / NO LVGL   |<-----------+ state(single-writer)       |
+------------------+   events   +---------------------------+
                             ^
                             |
                     +-------+--------+
                     |   sx_platform  |
                     | drivers (lcd/  |
                     | touch/sd/...)  |
                     +----------------+
```

**Nguyên tắc:** `sx_core` là “điểm chốt” (policy + state), `sx_ui` là “render”, `sx_services` là “domain producers”, `sx_platform` là “drivers”.

---

## 2) Component boundaries (ranh giới + public API)

### 2.1 Quy ước “public header”
- Public headers đặt tại: `components/<comp>/include/`
- Private headers đặt tại: `components/<comp>/src/` hoặc `components/<comp>/private/`
- **MUST:** Không include private header xuyên component.

### 2.2 `sx_core`
**Owns**
- `sx_event.*` (schema + post/dispatch)
- `sx_state.*` (snapshot, version, dirty_mask)
- `sx_orchestrator.*` (single writer)
- `sx_metrics.*` (observability)

**Forbidden**
- Không include LVGL.
- Không include UI headers.

### 2.3 `sx_ui`
**Owns**
- UI task (owner thread của LVGL)
- Screen manager + render pipeline
- Wrapper LVGL include (xem Section 7.5)

**Allowed**
- Read-only: `sx_state_view_t` / snapshot API.
- Emit only `SX_EVT_UI_INPUT_*`.

### 2.4 `sx_services`
**Owns**
- Audio, WiFi, IR, MCP, Chatbot, ... (domain services)
- Emit domain events, đọc state snapshot (read-only) nếu cần.

**Forbidden**
- `#include <lvgl.h>` / `#include <esp_lvgl_port.h>` (hoặc wrapper tương tự)
- `#include "sx_ui/*"`
- Gọi `lv_*` / `esp_lvgl_port_*`

### 2.5 `sx_platform`
**Owns**
- LCD/touch/backlight/SD, I2C/SPI/SDMMC glue (tuỳ board)
- Adapter/port layer (không được gọi LVGL)

---

## 3) Dispatch model (mô hình luồng)

### 3.1 Event system (multi-producer / single-consumer)
- Producers: UI + services (+ platform nếu có sự kiện phần cứng)
- Consumer: orchestrator (sx_core)
- Orchestrator gọi handler theo registry.

### 3.2 State snapshot (single-writer / multi-reader)
- Orchestrator là writer duy nhất.
- UI + services là readers.
- Snapshot immutable (read-only view).

---

## 4) Event spec v1.3 (đặc tả + mapping vào code)

### 4.1 Taxonomy (bắt buộc)
**MUST:** `sx_event_id_t` phải thể hiện taxonomy theo domain, ví dụ:
- `SX_EVT_UI_INPUT_*`
- `SX_EVT_AUDIO_*`
- `SX_EVT_WIFI_*`
- `SX_EVT_IR_*`
- `SX_EVT_STORAGE_*`
- `SX_EVT_SYSTEM_*`

**MUST:** Có “range reservation” để tránh trùng ID theo thời gian (ví dụ mỗi domain 0x0100 IDs).

### 4.2 Priority
- `CRITICAL`: watchdog / power / fatal recovery
- `HIGH`: audio timing, UI navigation, network reconnect
- `NORMAL`: status updates
- `LOW`: telemetry/logging

### 4.3 Backpressure policy (bắt buộc có)
**MUST:** API post event có policy:
- `SX_BP_DROP` (drop nếu queue full)
- `SX_BP_COALESCE` (chỉ giữ event mới nhất theo key)
- `SX_BP_BLOCK` (chỉ dùng cho CRITICAL, có timeout)

**MUST:** Có counters dropped/coalesced trong metrics.

### 4.4 Payload rules
- Payload phải là POD nhỏ, không chứa con trỏ lifetime mơ hồ.
- Nếu cần buffer lớn: dùng handle + ownership rõ (xem resource ownership).

---

## 5) State snapshot spec v1.3 (versioned + dirty_mask + double-buffer)

### 5.1 Fields bắt buộc trong `sx_state_t`
**MUST:**
- `uint32_t version;`
- `uint32_t dirty_mask;`

### 5.2 Dirty mask scheme (khuyến nghị mạnh)
- Bit theo domain (audio/ui/wifi/ir/storage/system…)
- UI screens subscribe theo mask.

### 5.3 Double-buffer (khuyến nghị mạnh)
- Writer ghi vào back buffer, publish atomically.
- Readers luôn đọc front buffer stable.

### 5.4 Budget rule
- Snapshot size phải nằm trong ngân sách RAM/PSRAM đã định (xem Section 11).

---

## 6) Lifecycle contract v1.3 (interfaces + ownership)

### 6.1 Service interface (bắt buộc)
**MUST:** `sx_service_if.h` định nghĩa vtable:
- `init() / start() / stop() / deinit() / on_event(evt)`

### 6.2 Screen interface (bắt buộc)
**MUST:** `sx_screen_if.h` định nghĩa vtable:
- `create() / destroy() / on_enter() / on_exit() / on_state_change(mask)`

### 6.3 Resource ownership table (bắt buộc có file)
**MUST:** `docs/SIMPLEXL_RESOURCE_OWNERSHIP.md`
- Ai allocate/free?
- Thread nào được touch?
- Lifetime theo “state epoch” hay theo “screen lifetime”?

---

## 7) Enforcement v1.3 (compile-time + CI + runtime)

### 7.1 Dependency fence (CMake) — MUST
- `sx_services` **không** `REQUIRES sx_ui`
- `sx_ui` **không** `REQUIRES sx_services`
- `sx_platform` **không** `REQUIRES lvgl`

### 7.2 Forbidden include checks (script) — MUST
**MUST file:** `scripts/check_forbidden_includes.sh`
- Fail nếu `components/sx_services/**` include `lvgl.h`, `esp_lvgl_port.h`, hoặc `sx_ui/*`
- Fail nếu `components/sx_platform/**` gọi `lv_` (trừ whitelist nếu có lý do và ghi rõ)
- Fail nếu `sx_ui/**` include private headers của services

### 7.3 LVGL call firewall (runtime) — MUST (Debug/CI)
**MUST file:** `components/sx_ui/include/sx_lvgl_guard.h`
- `sx_ui_set_ui_task_handle(TaskHandle_t)`
- `SX_ASSERT_UI_THREAD()`
- `SX_LVGL_CALL(stmt)` wrapper cho các call nhạy cảm.

### 7.4 Static analysis (khuyến nghị, nhưng đưa vào “gates”)
- `clang-tidy` / `cppcheck` cấu hình tối thiểu (tập trung include-graph + forbidden calls).

### 7.5 NEW: “LVGL include wrapper” (compile-time) — MUST
Đây là phần v1.3 bổ sung để **đóng cửa include LVGL**:

**MUST:**
- `sx_ui` cung cấp wrapper header: `components/sx_ui/include/sx_lvgl.h`
  - Wrapper này **mới được phép** include `lvgl.h`.
- Cấm include trực tiếp `lvgl.h` ở mọi nơi khác bằng script check (7.2).

**Cơ chế compile-time đề xuất (mạnh):**
- CMake của `sx_ui` thêm compile definition: `-DSX_COMPONENT_SX_UI=1`
- `sx_lvgl.h`:
  - `#if !defined(SX_COMPONENT_SX_UI)  #error ...  #endif`
  - Sau đó include `lvgl.h`
=> Kể cả ai đó “lách” script, vẫn fail compile.

---

## 8) Known violations (điểm nóng cần xoá sạch)

**MUST:** Các service sau phải “dịch” sang pattern data/event → UI render:
- `sx_display_service`
- `sx_theme_service`
- `sx_image_service`
- `sx_qr_code_service`

**Pattern bắt buộc:**
- service chuẩn bị data (state hoặc payload)
- phát event/state update
- UI nhận event/state và render.

---

## 9) Observability v1.3 (metrics tối thiểu để debug kiến trúc)

### 9.1 Metrics collection mechanism — MUST (tối thiểu)
**MUST file:** `components/sx_core/src/sx_metrics.c` + `include/sx_metrics.h`
- counters, gauges, dump API.

### 9.2 Required metrics
- `evt_posted_total{prio}`
- `evt_dropped_total{reason}`
- `evt_coalesced_total{domain}`
- `queue_depth_gauge{prio}`
- `state_version_gauge`
- `ui_render_ms_gauge` (nếu đo được)
- `heap_free_min`, `psram_free_min` (nếu board có)

---

## 10) Quality gates v1.3 (đưa vào CI + chốt ngưỡng)

### 10.1 Gates bắt buộc (CI)
**MUST:**
1) `check_forbidden_includes.sh` PASS
2) Build (debug profile) PASS
3) Unit tests tối thiểu (dispatcher + state publish) PASS
4) Report metrics dump smoke test (nếu có)

### 10.2 Ngưỡng (board thật)
**MUST file:** `docs/SIMPLEXL_QUALITY_GATES.md`
- Đặt ngưỡng theo board (ESP32-S3 N16R8 / PSRAM 8MB…).
- Ví dụ ngưỡng (placeholder):
  - Boot heap free >= X KB
  - PSRAM free >= Y KB
  - UI frame avg <= Z ms
  - Dropped events = 0 ở release profile (hoặc <= N ở dev)

---

## 11) NEW: Capacity & Performance requirements (bắt buộc có)

Phần này giải quyết “scalability gap” (v1.2 mới nói cơ chế, chưa chốt ngưỡng).

### 11.1 Event throughput
- **Target:** UI input latency (touch→render) <= 50 ms (P95) trong điều kiện bình thường.
- **Target:** Audio timing events không bị starve (no backlog > 2× tick window).

### 11.2 Queue sizing
- Mỗi priority queue phải có DoD:
  - `CRITICAL`: không drop (block with timeout)
  - `HIGH`: drop/coalesce có kiểm soát
  - `NORMAL/LOW`: drop được

### 11.3 State + Asset memory budget
- `sx_state_t` cố gắng <= 8–16 KB (tuỳ features).
- Assets lớn (ảnh, album art, tiles) ưu tiên SD; RAM chỉ cache theo LRU (nếu có).

### 11.4 UI frame budget (LVGL)
- **Target:** render pipeline không block tasks khác quá lâu.
- Nếu dùng animation/album art: phải có switch “reduced mode” theo load.

---

## 12) NEW: Architecture compliance tests (bắt buộc có tối thiểu)

Phần này nâng “testability” từ đề xuất → checklist enforce.

### 12.1 Contract tests (sx_core)
- Test: event post/dispatch đúng priority.
- Test: backpressure policy (drop/coalesce) + metrics counters.
- Test: state version tăng đúng và dirty_mask set đúng.

### 12.2 “Architecture smoke” (CI)
- Chạy `check_forbidden_includes.sh`.
- Build debug với `SX_ASSERT_UI_THREAD()` bật.
- (Tuỳ chọn) Inject 1 call LVGL từ non-UI trong test build để xác minh firewall bắt được.

---

## 13) Phase status v1.3 (gắn với DoD)

**Phase P0 — Architecture integrity**
- DoD:
  - 0 LVGL include ngoài `sx_ui/sx_lvgl.h`
  - 0 LVGL call ngoài UI task (debug/CI bắt được)
  - 4 violations đã fix

**Phase P1 — Event/state contract**
- DoD:
  - taxonomy + backpressure implemented
  - state version + dirty_mask + (option) double-buffer

**Phase P2 — Lifecycle + ownership + tests**
- DoD:
  - service/screen interfaces tồn tại
  - resource ownership table hoàn tất
  - contract tests chạy trong CI

---

## 14) Review checklist (dùng cho PR)

- [ ] PR không thêm include LVGL ngoài `sx_ui/sx_lvgl.h`
- [ ] PR không thêm call `lv_*` ngoài UI task
- [ ] PR giữ đúng hướng data/event → UI render
- [ ] PR thêm/đổi event ID có cập nhật taxonomy/range
- [ ] PR đụng state có cập nhật dirty_mask/version
- [ ] PR không tạo vòng phụ thuộc CMake

---

## 15) Changelog (v1.2 → v1.3)

- **NEW:** Section 7.5 “LVGL include wrapper” + compile-time guard
- **NEW:** Section 11 Capacity/Performance requirements (ngưỡng đo được)
- **NEW:** Section 12 Architecture compliance tests (contract + CI smoke)
- **UPGRADE:** Quality gates rõ “gates vs thresholds”
- **UPGRADE:** Phase status gắn DoD theo P0/P1/P2 rõ ràng

---

## Appendix A — Quick Do / Don’t

### DO
- Service: xử lý domain, phát event, cập nhật state.
- UI: render theo snapshot + mask, phát `UI_INPUT`.
- Luôn thêm metrics khi thêm backpressure policy.

### DON’T
- “Fix nhanh” bằng gọi LVGL trong service.
- Include `lvgl.h` ở bất kỳ component nào ngoài `sx_ui/sx_lvgl.h`.
- Bỏ qua dirty_mask rồi gọi render full-screen liên tục.
