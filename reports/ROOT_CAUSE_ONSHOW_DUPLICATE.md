# ROOT CAUSE ANALYSIS: FlashScreen::onShow() Called Twice

## 🎯 TÓM TẮT

**FlashScreen::onShow() bị gọi 2 lần vì có 2 nguồn độc lập cùng gọi `ui_router_navigate_to(SCREEN_ID_FLASH)`:**

1. **Timer callback** (`screen_boot.c:boot_timer_cb`) - gọi sau 3 giây
2. **Main UI loop** (`sx_ui_task.c:main loop`) - kiểm tra thời gian và gọi sau khi `boot_elapsed >= 3000ms`

## 📋 CHUỖI GỌI (CALL CHAIN)

### Lần 1: Timer Callback (4741ms)
```
screen_boot.c:103-112
  └─ boot_timer_cb(lv_timer_t *timer)
      └─ ui_router_navigate_to(SCREEN_ID_FLASH)  [line 108]
          └─ ui_router.c:49-126
              └─ callbacks->on_show()  [line 118]
                  └─ screen_flash.c:77-85
                      └─ FlashScreen::onShow() ✅ Lần 1
```

### Lần 2: Main UI Loop (4921ms, cách 160ms)
```
sx_ui_task.c:175-226
  └─ Main UI loop (for loop)
      └─ Check boot_elapsed >= 3000ms  [line 197]
          └─ Set target_screen = SCREEN_ID_FLASH  [line 203]
              └─ ui_router_navigate_to(target_screen)  [line 222]
                  └─ ui_router.c:49-126
                      └─ callbacks->on_show()  [line 118]
                          └─ screen_flash.c:77-85
                              └─ FlashScreen::onShow() ❌ Lần 2 (DUPLICATE)
```

## 🔍 PHÂN TÍCH CHI TIẾT

### 1️⃣ screen_boot.c - Timer Callback

**File:** `components/sx_ui/screens/screen_boot.c`

**Function:** `boot_timer_cb()` (line 103-112)

```c
static void boot_timer_cb(lv_timer_t *timer) {
    ESP_LOGI(TAG, "Boot screen timer expired, navigating to flash screen");
    if (lvgl_port_lock(0)) {
        ui_router_navigate_to(SCREEN_ID_FLASH);  // ← GỌI LẦN 1
        lvgl_port_unlock();
    }
}
```

**Đăng ký:** Trong `on_show()` (line 122)
```c
s_boot_timer = lv_timer_create(boot_timer_cb, BOOTSCREEN_DISPLAY_TIME_MS, NULL);
```

**Hủy:** Trong `on_hide()` (line 140) và `on_destroy()` (line 154)

**Vấn đề:** Timer callback chạy **SAU KHI** BootScreen đã bị destroy, nhưng vẫn gọi navigate vì:
- Timer được tạo trong `on_show()` với delay 3000ms
- Timer callback chạy trong LVGL task context (không phải UI task)
- Khi timer callback chạy, BootScreen có thể đã bị destroy, nhưng callback vẫn còn sống

### 2️⃣ sx_ui_task.c - Main UI Loop

**File:** `components/sx_ui/sx_ui_task.c`

**Function:** Main loop (line 175-226)

**Logic:**
```c
for (;;) {
    sx_dispatcher_get_state(&state);
    ui_screen_id_t target_screen = SCREEN_ID_BOOT;
    
    if (state.ui.device_state == SX_DEV_IDLE) {
        if (!boot_shown) {
            boot_start_time = xTaskGetTickCount();
            boot_shown = true;
        } else {
            uint32_t boot_elapsed = xTaskGetTickCount() - boot_start_time;
            if (boot_elapsed < pdMS_TO_TICKS(3000)) {
                target_screen = SCREEN_ID_BOOT;
            } else {
                // ← SAU 3 GIÂY, SET TARGET = FLASH
                if (!flash_shown) {
                    target_screen = SCREEN_ID_FLASH;  // ← GỌI LẦN 2
                    flash_start_time = xTaskGetTickCount();
                    flash_shown = true;
                }
            }
        }
    }
    
    // Navigate if screen changed
    if (target_screen != last_screen) {
        ui_router_navigate_to(target_screen);  // ← GỌI LẦN 2
        last_screen = target_screen;
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));  // Delay 100ms giữa các vòng lặp
}
```

**Vấn đề:** Main loop kiểm tra `boot_elapsed >= 3000ms` **SAU KHI** timer callback đã navigate, dẫn đến:
- Timer callback navigate lần 1 (4741ms)
- Main loop kiểm tra sau 1-2 vòng lặp (4921ms, cách 160ms)
- Main loop thấy `boot_elapsed >= 3000ms` → navigate lần 2

### 3️⃣ ui_router.c - Navigation Logic

**File:** `components/sx_ui/ui_router.c`

**Function:** `ui_router_navigate_to()` (line 49-126)

**Vấn đề:** Router không có cơ chế chặn duplicate navigation khi:
- Lần 1: Timer callback gọi → `s_current_screen = SCREEN_ID_FLASH` → `on_show()` được gọi
- Lần 2: Main loop gọi (cách 160ms) → Router check `s_current_screen == screen_id` (line 77) → **NHƯNG** check này chỉ hoạt động nếu cả 2 lần gọi xảy ra trong cùng một context

**Race condition:** 
- Timer callback chạy trong LVGL task context
- Main loop chạy trong UI task context
- Có thể có race condition giữa 2 context này

## 🎯 NGUYÊN NHÂN GỐC (ROOT CAUSE)

**FlashScreen::onShow() bị gọi lần 2 vì:**

1. **Timer callback** (`boot_timer_cb`) và **Main UI loop** đều có logic navigate đến FLASH screen sau 3 giây
2. **Timer callback chạy trước** (4741ms) → navigate lần 1
3. **Main loop kiểm tra sau** (4921ms, sau 1-2 vòng lặp 100ms) → thấy `boot_elapsed >= 3000ms` → navigate lần 2
4. **Router không có cơ chế chặn duplicate** khi 2 lần gọi xảy ra từ 2 context khác nhau (LVGL task vs UI task)

## 📊 TIMELINE

```
Time    Event
─────────────────────────────────────────────────────────
0ms     BootScreen::onShow() → Timer created (3000ms delay)
3000ms  Timer callback scheduled (LVGL task)
4741ms  Timer callback FIRED → navigate_to(FLASH) → onShow() lần 1 ✅
4741ms  BootScreen::onDestroy() → Timer deleted
4921ms  Main loop check: boot_elapsed >= 3000ms → navigate_to(FLASH) → onShow() lần 2 ❌
```

## 🔧 VÌ SAO CALLBACK TỒN TẠI SAU DESTROY?

**Timer callback không bị hủy kịp:**

1. Timer được tạo trong `on_show()` với delay 3000ms
2. Timer callback chạy trong LVGL task context (không phải UI task)
3. Khi timer callback chạy (4741ms), BootScreen có thể đã bị destroy, nhưng:
   - Timer callback đã được schedule trong LVGL timer queue
   - LVGL timer queue không bị ảnh hưởng bởi screen lifecycle
   - Callback vẫn chạy và gọi navigate

**Tuy nhiên, trong code hiện tại:**
- `on_hide()` xóa timer (line 140)
- `on_destroy()` cũng xóa timer (line 154)
- **NHƯNG** timer callback có thể đã được schedule trước khi `on_hide()` được gọi

## 📝 KẾT LUẬN

**Root cause:** Có 2 nguồn độc lập cùng navigate đến FLASH screen:
1. Timer callback (`boot_timer_cb`) - gọi từ LVGL task context
2. Main UI loop - gọi từ UI task context

**Vì sao callback tồn tại sau destroy:**
- Timer callback được schedule trong LVGL timer queue
- Khi timer callback chạy, BootScreen đã bị destroy, nhưng callback vẫn chạy vì đã được schedule
- Main loop cũng kiểm tra thời gian và navigate, dẫn đến duplicate

**File + Function cụ thể:**
- `components/sx_ui/screens/screen_boot.c:103` - `boot_timer_cb()`
- `components/sx_ui/sx_ui_task.c:203` - Main loop set `target_screen = SCREEN_ID_FLASH`
- `components/sx_ui/sx_ui_task.c:222` - Main loop gọi `ui_router_navigate_to(target_screen)`






















