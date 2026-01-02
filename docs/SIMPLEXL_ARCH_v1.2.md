# SIMPLEXL_ARCH v1.2 (Implementation-first)

> **Mục tiêu của v1.2:** biến “rules trên giấy” thành **rules có thể cưỡng bức** (compile/CI/runtime), đồng thời “đóng hợp đồng” cho **event/state/lifecycle** để tránh hiểu sai và chống thoái hoá kiến trúc theo thời gian.  
> v1.2 được viết dựa trên các gap đã được chỉ ra ở v1.1: *enforcement chưa implement*, *4 LVGL violations còn tồn tại*, *event taxonomy chưa map vào code*, *state thiếu version/dirty_mask/double-buffer*, *lifecycle chưa có interface chuẩn*, *quality gates chưa có ngưỡng và chưa đưa vào CI*.

---

## 0) Non‑negotiables (KHÔNG THƯƠNG LƯỢNG)

1. **Chỉ UI task** được gọi **bất kỳ LVGL API** nào (bao gồm `lv_*`, `esp_lvgl_port_*`, các helper/wrapper có gọi LVGL).
2. **Services không bao giờ include UI headers** và **không include LVGL headers**.
3. UI ↔ services **chỉ giao tiếp qua _events_ và _state snapshots_**.
4. **Orchestrator** là **single source of truth** cho state (single writer).
5. **Violations = fail build/CI** (ít nhất ở Debug/CI profile).

---

## 1) Architectural shape (hình dạng kiến trúc)

```
+-------------------------------+
|            sx_ui              |  (UI task, LVGL only here)
|  - screens/widgets            |
|  - reads sx_state snapshot    |
|  - emits only UI_INPUT events |
+---------------+---------------+
                |
                v  events (UI_INPUT) + read-only state
+---------------+---------------+
|            sx_core            |  (dispatcher/orchestrator)
|  - event queues + registry    |
|  - reducers/handlers          |
|  - commits state snapshots    |
+-------+---------------+-------+
        |               |
        v               v
+-------+-----+   +-----+-------+
| sx_services |   | sx_platform  |
| (audio/net) |   | (drivers)    |
| emits events|   | no LVGL calls|
+-------------+   +-------------+
```

---

## 2) Component boundaries (ranh giới + public API)

### 2.1 Quy ước “public header”
- Mỗi component chỉ expose các header public trong:
  - `components/<comp>/include/`
- Code nội bộ nằm trong:
  - `components/<comp>/src/` (hoặc tương đương) và **KHÔNG được include trực tiếp từ component khác**.

### 2.2 `sx_core`
**Owns**
- `sx_event` (queue + post API + registry)
- `sx_state` (snapshot)
- `sx_dispatcher` / `sx_orchestrator`
- `sx_metrics` (optional nhưng khuyến nghị)

**Public headers (được phép UI/services include)**
- `sx_event.h`, `sx_state.h`, `sx_types.h`, `sx_metrics.h` (nếu có)

### 2.3 `sx_ui`
**Owns**
- UI task + LVGL init + render loop
- Screen manager
- Asset cache policy (UI-owned)

**Allowed includes**
- `sx_core/include/*` (read-only state + UI_INPUT event post wrapper)
- `sx_platform/include/*` (chỉ integration points, không gọi LVGL từ platform)

**Forbidden**
- include `sx_services/*`
- include `lvgl.h` từ ngoài “UI-only LVGL boundary” (xem Section 5.3)

### 2.4 `sx_services`
**Owns**
- audio, net, ir, mcp/chatbot, …

**Allowed includes**
- `sx_core/include/*` (event post)
- private headers của chính service

**Forbidden**
- include `sx_ui/*`
- include `lvgl.h` / `esp_lvgl_port.h` / bất kỳ header kéo theo LVGL
- gọi `lv_*`

### 2.5 `sx_platform`
**Owns**
- driver LCD/touch/backlight/SD + board/pinmap

**Forbidden**
- gọi `lv_*` (trừ các glue callback bắt buộc do UI task đăng ký và gọi)

---

## 3) Dispatch model (mô hình luồng)

### 3.1 Event system
- Multi‑producer / single‑consumer
  - Producers: UI + services
  - Consumer: `sx_orchestrator` (core)

### 3.2 State snapshot
- Single‑writer / multi‑reader
  - Writer: orchestrator
  - Readers: UI (và optional readers khác nhưng chỉ read-only)

---

## 4) Event spec v1.2 (đặc tả + mapping vào code)

### 4.1 Taxonomy (bắt buộc)
Event ID phải theo domain:

- `SX_EVT_UI_INPUT_*`
- `SX_EVT_SYS_*`
- `SX_EVT_NET_*`
- `SX_EVT_AUDIO_*`
- `SX_EVT_IR_*`
- `SX_EVT_NAV_*`
- `SX_EVT_MCP_*` / `SX_EVT_CHAT_*` (nếu có)

**Yêu cầu implement**
- `sx_event.h` phải có enum/defines theo taxonomy (không dùng “một đống event lẫn lộn”).
- Mọi event phải có:
  - `type`
  - `priority`
  - `payload_size`
  - `payload` (POD hoặc handle)

### 4.2 Priority (chuẩn hoá theo code thực tế)
Nếu code hiện có 4 mức (Critical/High/Normal/Low), v1.2 **chốt hợp đồng**:

- `CRITICAL`: never drop
- `HIGH`: drop theo policy
- `NORMAL`: drop/coalesce khi queue full
- `LOW`: drop first

### 4.3 Backpressure policy (bắt buộc có)
Chốt policy theo loại event:

- **Never drop**: power, stop audio, fatal, watchdog-recovery, mount fail, …
- **Coalesce**: slider/volume/progress (giữ event cuối)
- **Drop when busy**: telemetry/log spam, periodic refresh không quan trọng

**Yêu cầu implement**
- `sx_event_post()` phải hỗ trợ:
  - coalesce theo (type + key)
  - drop theo priority khi queue gần full
- `sx_metrics` phải có counters:
  - produced/consumed/dropped/coalesced per type
  - watermark per queue

### 4.4 Payload rules (an toàn bộ nhớ)
- Default: POD (copy vào event)
- Nếu payload là pointer:
  - phải dùng handle/refcount, hoặc copy sang buffer owned-by-core
  - ghi rõ ownership

---

## 5) State snapshot spec v1.2 (đặc tả + mapping vào code)

### 5.1 `sx_state_t` bắt buộc có:
- `uint32_t version;`
- `uint32_t dirty_mask;`

### 5.2 Dirty mask scheme (gợi ý)
Bit theo domain:

- `DIRTY_UI` / `DIRTY_AUDIO` / `DIRTY_NET` / `DIRTY_IR` / `DIRTY_NAV` / …

### 5.3 Double-buffer (khuyến nghị mạnh)
- Orchestrator ghi vào back buffer
- Commit: atomic swap pointer
- UI đọc “front” snapshot (read-only)

### 5.4 Budget rule
- `sx_state_t` không chứa blob lớn (bitmap/audio)
- chỉ chứa id/handle/uri

---

## 6) Lifecycle contract v1.2 (đặc tả + interface bắt buộc)

### 6.1 Service interface (bắt buộc)
Tạo header public: `components/sx_services/include/sx_service_if.h`

```c
typedef struct {
  const char* name;

  esp_err_t (*init)(void* ctx);
  esp_err_t (*start)(void* ctx);
  esp_err_t (*stop)(void* ctx);
  esp_err_t (*deinit)(void* ctx);

  // optional: allow core to route events into service
  esp_err_t (*handle_event)(void* ctx, const sx_event_t* evt);
} sx_service_vtbl_t;
```

**Rule:** Orchestrator là nơi duy nhất gọi lifecycle.

### 6.2 Screen interface (bắt buộc)
Tạo header public: `components/sx_ui/include/sx_screen_if.h`

```c
typedef struct {
  const char* name;

  void (*create)(void);
  void (*destroy)(void);

  void (*on_enter)(const char* from);
  void (*on_exit)(const char* to);

  void (*on_state_change)(const sx_state_t* st, uint32_t dirty_mask);
  void (*on_ui_event)(const sx_event_t* ui_evt); // only UI_INPUT
} sx_screen_vtbl_t;
```

**Rule:** Mọi LVGL object phải create/destroy đúng vòng đời.

### 6.3 Resource ownership table (bắt buộc có file)
Tạo file: `docs/SIMPLEXL_RESOURCE_OWNERSHIP.md`
- audio buffers
- image cache
- sd handles / i2s / http client
- tasks/queues/timers

---

## 7) Enforcement v1.2 (cụ thể hoá “vi phạm = fail build/CI”)

> v1.2 chốt “mechanism”, không chỉ mô tả.

### 7.1 Dependency fence (CMake) — MUST
- `sx_services` **không được** `REQUIRES sx_ui`
- `sx_ui` **không được** `REQUIRES sx_services`

(Đây là hàng rào “graph-level”, không đủ một mình, nhưng bắt buộc.)

### 7.2 Forbidden include checks (script) — MUST
Tạo: `scripts/check_forbidden_includes.sh`

- Fail nếu:
  - `components/sx_services/**` include `sx_ui/*` hoặc `lvgl*.h` hoặc `esp_lvgl_port.h`
  - `components/sx_platform/**` gọi `lv_` (trừ whitelist)
  - `components/sx_ui/**` include header nội bộ service

**Whitelist policy**
- Cho phép whitelist theo file pattern trong script (vd: `*lvgl_port*` nếu là glue do UI gọi).

### 7.3 LVGL call firewall (runtime) — MUST (Debug/CI)
Tạo: `components/sx_ui/include/sx_lvgl_guard.h`

```c
extern TaskHandle_t g_sx_ui_task;

static inline void sx_ui_set_ui_task_handle(TaskHandle_t h) { g_sx_ui_task = h; }

#define SX_ASSERT_UI_THREAD() do { \
  TaskHandle_t cur = xTaskGetCurrentTaskHandle(); \
  if (cur != g_sx_ui_task) { \
    ESP_LOGE("SX_LVGL", "LVGL call from non-UI task! cur=%p ui=%p", cur, g_sx_ui_task); \
    assert(0); \
  } \
} while(0)

#define SX_LVGL_CALL(stmt) do { SX_ASSERT_UI_THREAD(); stmt; } while(0)
```

**Rule:** Mọi “entrypoint” UI thao tác LVGL phải dùng `SX_LVGL_CALL(...)` hoặc gọi `SX_ASSERT_UI_THREAD()`.

### 7.4 Static analysis (khuyến nghị, nhưng v1.2 đưa vào kế hoạch)
- `clang-tidy`/`cppcheck` rule: detect `lv_` usage ngoài `sx_ui`
- Có thể chạy trong CI (optional P1)

---

## 8) Known violations (tình trạng v1.1 → yêu cầu v1.2)

v1.2 quy định: **Release profile yêu cầu 0 violation**.  
Các violation đang được báo cáo ở v1.1 cần xử lý theo pattern “service → core data / UI render”:

- `sx_display_service` (include LVGL + call LVGL)
- `sx_theme_service` (include LVGL + call LVGL)
- `sx_image_service` (include LVGL + LVGL types/descriptors)
- `sx_qr_code_service` (include LVGL headers)

**Refactor pattern bắt buộc**
- Service chỉ chuẩn bị dữ liệu (theme state, image metadata, QR payload)
- UI nhận event/state và render bằng LVGL trong UI task

---

## 9) Observability v1.2 (từ “concept” sang “mechanism”)

### 9.1 Metrics collection mechanism — MUST (tối thiểu)
Tạo `sx_metrics.c` trong `sx_core`:
- API:
  - `sx_metrics_inc(counter_id)`
  - `sx_metrics_set_gauge(gauge_id, value)`
  - `sx_metrics_dump()` (log snapshot)

### 9.2 Required metrics
- heap free, psram free
- queue depth per priority + watermark
- dropped/coalesced events
- (optional) UI frame time

---

## 10) Quality gates v1.2 (đưa vào CI + chốt ngưỡng)

### 10.1 Gates bắt buộc (CI)
- `check_forbidden_includes.sh` PASS
- build debug PASS
- (optional) unit tests PASS

### 10.2 Ngưỡng (placeholder phải được chốt theo board thật)
Trong v1.2, mọi ngưỡng phải nằm trong file:
- `docs/SIMPLEXL_QUALITY_GATES.md`

Ví dụ cấu trúc:
- Boot: không assert, không watchdog reset
- Heap/PSRAM: min free watermark
- Event queue: không overflow trong scenario chuẩn
- UI latency: max ms (đo bằng timestamp + metrics)

---

## 11) Phase status v1.2 (đồng bộ với code, có completion criteria)

- **Phase 0**: Build clean + boots (PASS)
- **Phase 1**: Event/state/dispatcher wired; UI task exists; UI reads state (PASS)
- **Phase 2**: Screen lifecycle interface + state-driven rendering (REQUIRED)
- **Phase 3**: Platform hardened + pinmap sealed + SD policy (REQUIRED)
- **Phase 4**: Services modular + no LVGL in services + event taxonomy implemented (REQUIRED)
- **Phase 5**: CI gates + metrics + release hygiene (REQUIRED)

Mỗi phase phải có:
- DoD (Definition of Done)
- Script/CI check (nếu có)
- Owner

---

## 12) Review checklist (dùng cho PR)

PR chỉ được merge nếu:

- [ ] Không thêm include forbidden
- [ ] Không thêm LVGL call ngoài UI task
- [ ] Event theo taxonomy + payload rules
- [ ] State update qua orchestrator + dirty_mask/version
- [ ] Screen/service lifecycle tuân thủ interface
- [ ] CI gates PASS

---

## 13) Changelog (v1.1 → v1.2)

- **Enforcement**: từ “đề xuất” → chốt cơ chế + file paths cụ thể (script + runtime guard).
- **Event**: taxonomy + priority + backpressure trở thành hợp đồng bắt buộc.
- **State**: bắt buộc `version` + `dirty_mask`; double-buffer khuyến nghị mạnh.
- **Lifecycle**: thêm interface chuẩn cho services + screens.
- **Quality gates**: tách file ngưỡng riêng để chốt theo phần cứng thật; đưa gates vào CI.
- **Violations**: định nghĩa “release = 0 violation” + pattern refactor.

---

## Appendix A — Quick Do / Don’t

**DO**
- UI chỉ post `SX_EVT_UI_INPUT_*` qua wrapper.
- Services emit domain events; không chạm LVGL.
- Orchestrator commit snapshot (version/dirty_mask).

**DON’T**
- Services include `lvgl.h`/`esp_lvgl_port.h`.
- Platform gọi `lv_*`.
- UI include service headers.
- “Fix nhanh” bằng gọi LVGL trong service.
