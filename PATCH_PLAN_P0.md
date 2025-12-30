# PATCH_PLAN_P0 — Kế hoạch patch các rủi ro P0

> Mục tiêu: mô tả thay đổi code chính xác nhưng **chưa code**.

---

### P0-01 — Router gọi `on_hide()` 2 lần (double-callback)

- **File:** `components/sx_ui/ui_router.c`
- **Hàm:** `ui_router_navigate_to()`
- **Thay đổi:**
  - Xóa block `if (s_current_screen != SCREEN_ID_MAX && s_current_screen != screen_id) { ... }` ở đầu hàm.
  - Giữ lại block gọi `on_hide()` bên trong `if (lvgl_port_lock(0)) { ... }`.

### P0-02 — Kỷ luật LVGL lock không nhất quán (nested lock / gọi ngoài lock)

- **File 1:** `components/sx_ui/ui_router.c`
- **Hàm:** `ui_router_navigate_to()`
- **Thay đổi:**
  - **Xóa** toàn bộ `lvgl_port_lock(0)` và `lvgl_port_unlock()` bên trong hàm này. Router sẽ không tự lock nữa.

- **File 2:** `components/sx_ui/sx_ui_task.c`
- **Hàm:** `sx_ui_task()`
- **Thay đổi:**
  - Trong main loop, bao quanh đoạn `if (target_screen != last_screen) { ... }` bằng `lvgl_port_lock(0)`.
  - Trong main loop, bao quanh đoạn `if (state_changed) { ... callbacks->on_update(&state); ... }` bằng `lvgl_port_lock(0)`.

- **File 3:** `components/sx_ui/screens/screen_*.c` (tất cả)
- **Hàm:** `on_create`, `on_show`, `on_hide`, `on_destroy`, và các timer callback (ví dụ `boot_timer_cb`)
- **Thay đổi:**
  - **Xóa** toàn bộ `lvgl_port_lock(0)` và `lvgl_port_unlock()` trong các hàm này.
  - **Lưu ý:** `boot_timer_cb` gọi `ui_router_navigate_to()`. Vì `lv_timer_handler()` đã chạy trong lock ở UI task, nên `boot_timer_cb` cũng sẽ chạy trong lock → an toàn.

### P0-03 — Dispatcher post event không block → drop event khi queue đầy

- **File:** `components/sx_core/sx_dispatcher.c`
- **Thay đổi:**
  - Thêm `static uint32_t s_evt_drop_count = 0;`
  - Sửa `sx_dispatcher_post_event()`:
    - Nếu `xQueueSend(..., 0)` trả `pdFALSE`, thì `s_evt_drop_count++` và `ESP_LOGW(...)` (có thể rate-limit log).

### P0-04 — Resource leak khi display init fail path

- **File:** `components/sx_platform/sx_platform.c`
- **Hàm:** `sx_platform_display_init()`
- **Thay đổi:**
  - Thêm `goto fail;` trên các nhánh `esp_lcd_new_panel_io_spi` và `esp_lcd_new_panel_st7796` fail.
  - Tạo một label `fail:` ở cuối hàm, trước `return handles;`.
  - Trong `fail:`, thêm logic cleanup:
    - `if (io_handle) esp_lcd_panel_io_del(io_handle);`
    - `spi_bus_free(LCD_HOST_ID);`
    - `ledc_stop(...)`
    - set `handles.io_handle = NULL; handles.panel_handle = NULL;`

### P0-05 — Double-handle event trong orchestrator (chatbot audio channel)

- **File:** `components/sx_core/sx_orchestrator.c`
- **Hàm:** `sx_orchestrator_task()`
- **Thay đổi:**
  - Gom các nhánh `if (evt.type == ...)` thành một chuỗi `if-else if` duy nhất.
  - Tìm và xóa nhánh xử lý `SX_EVT_CHATBOT_AUDIO_CHANNEL_OPENED` bị lặp.

### P0-06 — String pool size nhỏ (8 slots)

- **File:** `components/sx_core/include/sx_event_string_pool.h`
- **Thay đổi:**
  - Tăng `#define SX_EVENT_STRING_POOL_SIZE` từ 8 lên 32 (hoặc 64) để giảm fallback malloc.


