# SIMPLEXL_ARCH v1.1 (hai-os-simplexl)

Tài liệu này định nghĩa **các quy tắc kiến trúc “không thương lượng” (non‑negotiable)** cho repo, đồng thời bổ sung các mục **cưỡng bức (enforcement)** + **đặc tả vận hành (event/state/lifecycle)** + **quality gates** để tiến tới mức firmware thương mại.

> Nguyên tắc cốt lõi: **giữ SimpleXL “simple” bằng kỷ luật kỹ thuật**: ranh giới tầng rõ, một chiều; state/event có hợp đồng; vi phạm rule thì *fail build/CI*.

---

## 0) Non‑negotiables (KHÔNG ĐƯỢC VI PHẠM)

1. **Chỉ UI task** được phép gọi **bất kỳ LVGL API** nào.
2. **Services không bao giờ include UI headers**.
3. UI ↔ services **chỉ giao tiếp qua _events_ và _state snapshots_**.
4. **Orchestrator** là **single source of truth** cho state (single writer).
5. Không mang “legacy UI”/“legacy build graph” vào repo.

---

## 1) Goals

- Không legacy UI, không legacy build graph.
- Một UI owner task cho **mọi LVGL calls**.
- Services không include UI headers.
- UI ↔ services chỉ qua **events** và **state snapshots**.
- Kiến trúc đủ chặt để scale: nhiều service + 30+ screens + assets (SD) mà vẫn ổn định.

---

## 2) Component boundaries (ranh giới component)

### `sx_core`
- Owns: `sx_event`, `sx_state`, `sx_dispatcher`, `sx_bootstrap`, `sx_orchestrator`.
- **Single writer** cho `sx_state_t`.
- Consumer duy nhất của event queue.
- Là nơi duy nhất được phép “điều phối” giữa UI ↔ services ↔ platform.

### `sx_ui`
- Owns: UI task (LVGL init + render loop).
- Chỉ **đọc** `sx_state_t` snapshot.
- Chỉ emit **`SX_EVT_UI_INPUT`** (UI input events).
- Forbidden:
  - include service headers (bất kỳ header nội bộ service).
  - gọi trực tiếp driver/HAL.

### `sx_platform`
- Owns: LCD/touch/backlight/SD drivers + board/pinmap.
- Forbidden:
  - LVGL calls **trừ** các integration points được UI task gọi (vd: flush_cb, touch_read_cb).

### `sx_services`
- Owns: audio/wifi/ir/mcp/chatbot (và các domain service khác).
- Emit events và expose **service APIs chỉ cho `sx_core`** (không expose cho UI).
- Forbidden:
  - include `sx_ui/*`
  - gọi LVGL
  - trực tiếp can thiệp state UI (chỉ phát event + cung cấp API nội bộ cho core)

---

## 3) Dispatch model

### 3.1 Event queue
- **multi‑producer / single‑consumer**
  - Producers: UI + services
  - Consumer: orchestrator (`sx_core`)

### 3.2 State snapshot
- **single‑writer / multi‑reader**
  - Writer: orchestrator
  - Readers: UI (và tùy chọn services, nhưng chỉ đọc)

---

## 4) Ownership rules

- **Only UI task may call LVGL APIs.**
- Orchestrator là **single source of truth** cho state.
- Không có “global state vô chủ”. Nếu có state, phải thuộc:
  - `sx_state_t` (owned by orchestrator) hoặc
  - state nội bộ service (owned by service) nhưng publish ra `sx_state_t` thông qua orchestrator.

---

## 5) Enforcement (CƯỠNG BỨC RULES)

> Mục tiêu: Rules không chỉ nằm trên giấy; **vi phạm = fail build/CI**.

### 5.1 Dependency fence (IDF/CMake)
- `sx_services` **không được** depend `sx_ui` (REQUIRES/PRIV_REQUIRES).
- `sx_ui` **không được** depend trực tiếp `sx_services`.
- `sx_platform` **không được** depend `sx_ui` theo chiều gọi LVGL (UI gọi platform integration points, không ngược lại).

**Nguyên tắc include:**
- UI chỉ include **public** headers của `sx_core` (vd `sx_state.h`, `sx_event.h`).
- Services chỉ include **public** headers của `sx_core` + private nội bộ service.
- Public headers = nằm trong `include/` (hoặc convention tương đương) và không kéo theo header “nội tạng”.

### 5.2 Forbidden include checks (CI/script)
Tạo check (script) để fail nếu:
- `sx_services/**` có `#include "sx_ui/..."` hoặc include header UI.
- `sx_ui/**` include header service (ngoại trừ một “API façade” nếu bạn có, ví dụ `sx_services_api.h` do `sx_core` cung cấp).
- `sx_platform/**` gọi `lv_*` trực tiếp (trừ file glue được whitelist theo rule).

### 5.3 LVGL call firewall (Debug build)
- UI task set “thread identity” (vd: `sx_ui_set_ui_thread()`).
- Mọi entrypoint UI glue (screen manager, widgets factory) gọi `SX_ASSERT_UI_THREAD()` trước khi thao tác LVGL.
- (Tuỳ chọn nâng cao) wrap một số API LVGL thường dùng bằng macro/wrapper để assert thread.

---

## 6) Event spec (ĐẶC TẢ EVENT)

### 6.1 Event taxonomy
Chuẩn hoá event theo **domain** để tránh “event rác”:
- `SX_EVT_UI_INPUT` (UI → core)
- `SX_EVT_SYS_*` (system: boot, lowmem, watchdog, …)
- `SX_EVT_NET_*`
- `SX_EVT_AUDIO_*`
- `SX_EVT_IR_*`
- `SX_EVT_NAV_*`
- `SX_EVT_MCP_*` / `SX_EVT_CHAT_*` (nếu có)

### 6.2 Payload rules (an toàn bộ nhớ)
- Payload nên là **POD** (plain data) hoặc handle/id, tránh truyền pointer sống dài.
- Nếu bắt buộc dùng pointer/buffer:
  - phải ghi rõ ownership (ai alloc, ai free)
  - ưu tiên copy vào buffer owned by core hoặc dùng refcount.

### 6.3 Priority + backpressure
- Tối thiểu 2 mức priority: `REALTIME` và `NORMAL`.
- Queue full policy bắt buộc khai báo:
  - Event nào **drop** (ví dụ spam slider/volume)  
  - Event nào **coalesce** (giữ event cuối)  
  - Event nào **never drop** (vd: power, stop audio, fatal)

### 6.4 Observability cho event
- Counters: produced, consumed, dropped, coalesced theo loại event.
- Watermark queue: max depth theo thời gian.

---

## 7) State snapshot spec (ĐẶC TẢ STATE)

### 7.1 Single source of truth
- `sx_state_t` là state “public” duy nhất mà UI đọc.
- State nội bộ service không được UI chạm trực tiếp.

### 7.2 Versioning + dirty flags
- `sx_state_t` có:
  - `uint32_t version`
  - `uint32_t dirty_mask` (bit theo domain hoặc theo screen)
- UI chỉ re-render khi:
  - `version` đổi, hoặc
  - dirty mask có bit liên quan

### 7.3 Double-buffer (khuyến nghị)
- Orchestrator ghi vào buffer “back”.
- Khi commit snapshot: atomic swap pointer → UI đọc “front”.
- UI đọc snapshot theo cơ chế “read-only view”, không mutate.

### 7.4 Budget & stability
- `sx_state_t` phải **nhỏ, ổn định**, tránh nhét blob lớn.
- Asset/bitmap không nằm trong state; state chỉ giữ **id/uri/handle**.

---

## 8) Lifecycle contract (HỢP ĐỒNG VÒNG ĐỜI)

### 8.1 Services lifecycle
Mỗi service tuân thủ interface (conceptual):
- `init(cfg)`
- `start()`
- `handle_event(evt)` *(nếu service nhận event qua core)*
- `stop()`
- `deinit()`

**Quy tắc:**
- Orchestrator gọi lifecycle theo thứ tự chuẩn.
- Service không tự spawn các “luồng bí mật” ngoài kiểm soát core (trừ khi đăng ký rõ).

### 8.2 UI lifecycle (screens)
Mỗi screen có hợp đồng tối thiểu:
- `create()` / `destroy()`
- `on_enter(prev)` / `on_exit(next)`
- `on_state_change(snapshot, dirty_mask)`
- `on_ui_event(ui_evt)` (nếu có)

**Quy tắc:**
- Mọi LVGL object được tạo trong `create()` và giải phóng trong `destroy()`.
- Cache asset phải có policy rõ: preload/lazy + release-on-exit (hoặc LRU).

### 8.3 Resource ownership table
Phải có bảng ownership cho:
- audio buffers
- image cache
- sd handles / i2s handles / http client handles
- tasks/queues/timers

---

## 9) Observability (LOG/TRACE/METRICS)

- Log tag theo module: `SX_CORE`, `SX_UI`, `SX_PLATFORM`, `SX_SVC_AUDIO`, `SX_SVC_NET`, …
- “Event trace” ring buffer (tuỳ chọn): ghi lại (ts, type, src, dst, result) để debug luồng.
- Metrics tối thiểu:
  - free heap, free psram
  - queue depth watermark
  - UI frame time (nếu đo được)
  - audio underrun count (nếu có)

---

## 10) Quality gates (Definition of Done – đo được)

Mỗi phase/PR phải tuân thủ các “gates” tối thiểu (ví dụ):
- Boot clean, không assert, không watchdog reset.
- Heap/PSRAM watermark không vượt ngân sách.
- Event queue không overflow trong scenario chuẩn.
- UI input latency dưới ngưỡng mục tiêu.
- Không có forbidden include / forbidden dependency.

> Các con số (ms/KB) nên được chốt theo phần cứng thật của bạn.

---

## 11) Testing strategy (tối thiểu nhưng hiệu quả)

- Unit tests (host) cho:
  - dispatcher, event routing, reducers/state transitions
- Contract tests cho:
  - event schema (payload size, forbidden pointer)
  - state schema (version/dirty logic)
- Static checks:
  - forbidden include/deps
  - (tuỳ chọn) clang-format/lint rules

---

## 12) Security & robustness (mức thương mại)

- Input validation cho event payload từ UI/network.
- Timeout/retry/backoff chuẩn cho network.
- Chế độ “safe mode”:
  - nếu lowmem hoặc service crash → degrade gracefully (tắt module phụ, UI vẫn sống).
- Quy tắc xử lý lỗi:
  - error codes thống nhất
  - không “silent fail”

---

## 13) Phase status (cập nhật)

- Phase 0: PASS (build clean, boots).
- Phase 1: event/state/dispatcher wired; UI task exists and reads state.
- Phase 2: LVGL UI task + screen lifecycle contract + state-driven rendering.
- Phase 3: platform drivers hardened + board/pinmap sealed + SD policy.
- Phase 4: services modular + event spec enforced + ownership table complete.
- Phase 5+: productization gates (metrics, stability, update strategy, release hygiene).

---

## Appendix A — Quick “Do / Don’t”

**DO**
- UI chỉ emit `SX_EVT_UI_INPUT`.
- Services chỉ emit domain events + expose APIs cho core.
- Orchestrator commit state snapshots với version/dirty.

**DON’T**
- UI include services header.
- Services include UI header.
- Platform gọi LVGL trực tiếp.
- Bất kỳ task nào khác UI task gọi LVGL.

