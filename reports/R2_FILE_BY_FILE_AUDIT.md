# R2 - PHÂN TÍCH TỪNG FILE

**TUYÊN BỐ CƯỠNG BỨC**: Tôi xác nhận rằng mọi phân tích dưới đây đều dựa trên việc đọc nội dung thực tế của từng file, và mọi nhận định đều được giải thích bằng tiếng Việt.

## Tổng quan

Tổng số file: **562 file**

**Lưu ý**: File này sẽ được cập nhật theo batch. Mỗi file sẽ được phân tích theo format chuẩn dưới đây.

---

## BATCH 1: CORE FILES (Entry Point, Bootstrap, Core Architecture)

### 📄 File: app/app_main.c

**1. File này dùng để làm gì?**
→ File entry point chính của firmware ESP32. Hàm `app_main()` là điểm khởi đầu của ứng dụng, gọi `sx_bootstrap_start()` để khởi động hệ thống.

**2. Các thành phần chính trong file**
- Hàm: `app_main(void)` - Entry point
- Include: `sx_bootstrap.h`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Boot**: Đây là entry point đầu tiên khi ESP32 khởi động.

**4. Phụ thuộc của file**
- Include: `esp_err.h`, `esp_log.h`, `sx_bootstrap.h`
- Component: `sx_core` (bootstrap)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: File rất đơn giản, chỉ gọi bootstrap. Đây là pattern tốt - tách biệt entry point và logic khởi động.
- **Code smell**: Không có. File quá đơn giản để có vấn đề.
- **Phân tầng**: Đúng - app_main chỉ là entry point, không có logic business.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Không có**: File quá đơn giản, không có rủi ro.

**7. Đề xuất cải thiện**
- **Giữ nguyên**: File này đã đúng chuẩn ESP-IDF.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: File rất đơn giản, dễ đánh giá.

---

### 📄 File: components/sx_app/app_main.c

**1. File này dùng để làm gì?**
→ File entry point thứ hai, có thêm logging chi tiết hơn. Có vẻ như đây là version mới hơn của `app/app_main.c`.

**2. Các thành phần chính trong file**
- Hàm: `app_main(void)` - Entry point với logging chi tiết
- Include: `sx_bootstrap.h`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Boot**: Entry point với logging debug.

**4. Phụ thuộc của file**
- Include: `esp_err.h`, `esp_log.h`, `sx_bootstrap.h`
- Component: `sx_core` (bootstrap)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: Có logging chi tiết hơn, tốt cho debug.
- **Code smell**: Có 2 file app_main.c - một ở `app/` và một ở `components/sx_app/`. Cần xác định file nào được dùng.
- **Phân tầng**: Đúng.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Duplicate entry point**: Có 2 file app_main.c, có thể gây confusion. Cần xác định file nào được CMake build.

**7. Đề xuất cải thiện**
- **Xác định file nào được dùng**: Kiểm tra CMakeLists.txt để xem component nào được link. Xóa file không dùng.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: File đơn giản, nhưng cần xác định duplicate.

---

### 📄 File: components/sx_core/sx_bootstrap.c

**1. File này dùng để làm gì?**
→ File khởi động hệ thống. Thực hiện initialization tuần tự: NVS, error handler, settings, theme, OTA, MCP server, dispatcher, orchestrator, platform (display, touch), SD card, assets, UI, và các services (audio, etc.).

**2. Các thành phần chính trong file**
- Hàm: `sx_bootstrap_start(void)` - Khởi động hệ thống
- Logic: Tuần tự init các component theo thứ tự phụ thuộc

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Boot**: Đây là trái tim của quá trình khởi động. Được gọi từ app_main.

**4. Phụ thuộc của file**
- Include: Rất nhiều - dispatcher, orchestrator, platform, UI, services (audio, SD, radio, IR, chatbot, MCP, WiFi, settings, intent, OTA, theme, STT, AFE, wake word, LED, power, state manager, audio power, router, diagnostics, TTS, music online, navigation, telegram, bluetooth, weather, protocol)
- Component: Hầu hết các component trong hệ thống

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: File này thực hiện initialization tuần tự, đảm bảo thứ tự phụ thuộc. Có lazy loading cho một số services (STT, AFE, wake word, playlist, radio, IR, chatbot, WiFi, music online, TTS, navigation, telegram, bluetooth, diagnostics, weather).
- **Code smell**: 
  - File rất dài (809 dòng) - vi phạm Single Responsibility Principle.
  - Có nhiều code bị comment out (lazy loading services) - cần refactor để dùng lazy loader thay vì comment.
  - Hard-coded GPIO pins cho SD card (lines 194-197) - nên dùng Kconfig.
- **Phân tầng**: Đúng - bootstrap là layer cao nhất, gọi các component khác.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory**: File init rất nhiều services, có thể gây memory pressure khi boot.
- **Boot time**: Khởi động tuần tự có thể chậm.
- **Maintainability**: File quá dài, khó maintain. Mỗi khi thêm service mới phải sửa file này.
- **Hard-coded values**: GPIO pins cho SD card hard-coded, không flexible.

**7. Đề xuất cải thiện**
- **Refactor**: Chia nhỏ thành các hàm init riêng: `sx_bootstrap_init_core()`, `sx_bootstrap_init_platform()`, `sx_bootstrap_init_services()`, etc.
- **Lazy loading**: Thay vì comment code, dùng `sx_lazy_loader` để init services khi cần.
- **Kconfig**: Di chuyển hard-coded GPIO pins sang Kconfig.
- **Error handling**: Cải thiện error handling - hiện tại nhiều services init fail nhưng vẫn continue.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: File dài nhưng logic rõ ràng, dễ đánh giá.

---

### 📄 File: components/sx_core/include/sx_bootstrap.h

**1. File này dùng để làm gì?**
→ Header file cho bootstrap, chỉ export hàm `sx_bootstrap_start()`.

**2. Các thành phần chính trong file**
- Hàm: `sx_bootstrap_start(void)` - Khởi động hệ thống

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Boot**: Interface cho bootstrap.

**4. Phụ thuộc của file**
- Include: `esp_err.h`

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: Interface đơn giản, chỉ export hàm cần thiết.
- **Code smell**: Không có.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Không có**.

**7. Đề xuất cải thiện**
- **Giữ nguyên**: Header đã tốt.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: File rất đơn giản.

---

### 📄 File: components/sx_core/include/sx_dispatcher.h

**1. File này dùng để làm gì?**
→ Header file định nghĩa dispatcher - core cross-module boundary. Dispatcher quản lý event queue (multi-producer, single-consumer) và state snapshot (single-writer, multi-reader).

**2. Các thành phần chính trong file**
- Hàm: 
  - `sx_dispatcher_init()` - Khởi tạo dispatcher
  - `sx_dispatcher_post_event()` - Post event vào queue
  - `sx_dispatcher_poll_event()` - Poll event từ queue
  - `sx_dispatcher_set_state()` - Set state snapshot
  - `sx_dispatcher_get_state()` - Get state snapshot

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service, UI**: Dispatcher là trung tâm communication giữa services và UI. Services emit events, UI đọc state.

**4. Phụ thuộc của file**
- Include: `sx_event.h`, `sx_state.h`

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: Pattern copy-out cho state snapshot tránh sharing mutable pointers, tốt cho thread safety. Event queue pattern phù hợp với FreeRTOS.
- **Code smell**: Không có.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory**: State snapshot được copy mỗi lần get, có thể tốn memory nếu state lớn. Nhưng hiện tại state nhỏ, không vấn đề.
- **Performance**: Copy state có overhead, nhưng đảm bảo thread safety.

**7. Đề xuất cải thiện**
- **Giữ nguyên**: Design pattern này đã tốt cho embedded system.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Interface rõ ràng, pattern đã được thiết kế tốt.

---

### 📄 File: components/sx_core/sx_dispatcher.c

**1. File này dùng để làm gì?**
→ Implementation của dispatcher. Quản lý 4 priority queues (low, normal, high, critical) và state snapshot với mutex protection.

**2. Các thành phần chính trong file**
- Static queues: `s_evt_q_low`, `s_evt_q_normal`, `s_evt_q_high`, `s_evt_q_critical`
- Static state: `s_state` với mutex `s_state_mutex`
- Hàm: `sx_dispatcher_init()`, `sx_dispatcher_post_event()`, `sx_dispatcher_poll_event()`, `sx_dispatcher_set_state()`, `sx_dispatcher_get_state()`
- Metrics: Drop event counter với rate-limited logging

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service, UI**: Tất cả events và state updates đều đi qua dispatcher.

**4. Phụ thuộc của file**
- Include: `sx_dispatcher.h`, `sx_event_string_pool.h`, FreeRTOS queue, semaphore
- Component: `sx_core` (event string pool)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Priority queues đảm bảo critical events được xử lý trước.
  - Mutex protection cho state snapshot đảm bảo thread safety.
  - Rate-limited logging tránh spam log khi queue full.
- **Code smell**: 
  - Queue sizes hard-coded (16, 32, 16, 8) - nên dùng Kconfig.
  - Timeout cho critical/high events hard-coded (10ms, 5ms) - nên configurable.
- **Phân tầng**: Đúng - dispatcher là core infrastructure, không phụ thuộc vào business logic.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Queue overflow**: Nếu events được post nhanh hơn orchestrator consume, queues có thể full và events bị drop. Có logging nhưng không có alert.
- **Memory**: 4 queues với tổng size ~72 events, mỗi event ~20 bytes = ~1.4KB. Chấp nhận được.
- **Performance**: Mutex lock/unlock cho mỗi state get/set có overhead, nhưng cần thiết cho thread safety.

**7. Đề xuất cải thiện**
- **Kconfig**: Di chuyển queue sizes và timeouts sang Kconfig.
- **Metrics**: Thêm metrics về queue usage (có thể dùng `uxQueueMessagesWaiting()`).
- **Alert**: Khi queue full nhiều lần, emit alert event.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, pattern đã được thiết kế tốt.

---

### 📄 File: components/sx_core/include/sx_orchestrator.h

**1. File này dùng để làm gì?**
→ Header file cho orchestrator - single writer cho state, xử lý events từ dispatcher.

**2. Các thành phần chính trong file**
- Hàm: `sx_orchestrator_start()` - Start orchestrator task

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Orchestrator là single writer cho state, xử lý tất cả events và update state.

**4. Phụ thuộc của file**
- Include: Không có (chỉ forward declaration)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: Interface đơn giản, chỉ export hàm start.
- **Code smell**: Không có.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Không có**.

**7. Đề xuất cải thiện**
- **Giữ nguyên**: Header đã tốt.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: File rất đơn giản.

---

### 📄 File: components/sx_core/sx_orchestrator.c

**1. File này dùng để làm gì?**
→ Implementation của orchestrator. Tạo FreeRTOS task để poll events từ dispatcher, xử lý events qua event handler registry, và update state.

**2. Các thành phần chính trong file**
- Task: `sx_orchestrator_task()` - Main orchestrator task
- Hàm: `sx_orchestrator_start()` - Start orchestrator task
- Event handlers: Register các event handlers (UI input, chatbot, audio, radio, system, alert, protocol, OTA, activation, WiFi)

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Orchestrator là trái tim của event processing, xử lý tất cả events và update state.

**4. Phụ thuộc của file**
- Include: `sx_dispatcher.h`, `sx_event.h`, `sx_state.h`, `sx_event_handler.h`, `event_handlers.h`
- Component: `sx_core` (dispatcher, event handler system)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Event handler registry pattern tốt, dễ extend.
  - Optimized polling với `vTaskDelayUntil` và batch processing.
  - Rate-limited logging (log mỗi 100 events) tránh spam.
- **Code smell**: 
  - Stack size hard-coded (3072) - nên dùng Kconfig.
  - Poll interval hard-coded (10ms) - nên configurable.
  - Task priority hard-coded (8) - nên configurable.
- **Phân tầng**: Đúng - orchestrator là core, không phụ thuộc vào business logic cụ thể.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Stack overflow**: Stack size 3072 có thể không đủ nếu event handlers phức tạp. Cần monitor stack usage.
- **Event processing delay**: Nếu có nhiều events, orchestrator có thể không kịp xử lý. Cần monitor queue sizes.
- **Single point of failure**: Nếu orchestrator task crash, toàn bộ hệ thống dừng.

**7. Đề xuất cải thiện**
- **Kconfig**: Di chuyển stack size, poll interval, task priority sang Kconfig.
- **Stack monitoring**: Thêm stack usage monitoring (có thể dùng `uxTaskGetStackHighWaterMark()`).
- **Error handling**: Cải thiện error handling trong event handlers - nếu handler crash, không nên crash orchestrator task.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, pattern đã được thiết kế tốt.

---

### 📄 File: components/sx_core/include/sx_state.h

**1. File này dùng để làm gì?**
→ Header file định nghĩa state structure - immutable snapshot-style state. Single writer (orchestrator), multiple readers (UI, services).

**2. Các thành phần chính trong file**
- Struct: 
  - `sx_device_state_t` - Device state enum (UNKNOWN, BOOTING, IDLE, BUSY, ERROR)
  - `sx_wifi_state_t` - WiFi state
  - `sx_audio_state_t` - Audio state
  - `sx_ui_state_t` - UI state (device state, status text, emotion, chat messages, chatbot state, error state, alert state, audio state, WiFi state)
  - `sx_state_t` - Main state structure với sequence number
- Macros: Max lengths cho strings (MESSAGE, SESSION_ID, ERROR_MSG, ALERT_STATUS, ALERT_MSG, ALERT_EMOTION, WIFI_SSID)

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service, UI**: State là single source of truth cho toàn bộ hệ thống.

**4. Phụ thuộc của file**
- Include: Không có (chỉ standard types)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Immutable snapshot pattern tốt cho thread safety.
  - Sequence number để detect state changes.
  - Fixed-size buffers cho strings tránh dynamic allocation.
- **Code smell**: 
  - Fixed-size buffers có thể waste memory nếu không dùng hết.
  - Nhiều fields trong `sx_ui_state_t` có thể được group lại thành substructures.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory**: State structure khá lớn (~1KB), nhưng chỉ có 1 instance, chấp nhận được.
- **String truncation**: Fixed-size buffers có thể bị truncate nếu string quá dài. Cần validation.

**7. Đề xuất cải thiện**
- **Refactor**: Group related fields thành substructures (ví dụ: `sx_chatbot_state_t`, `sx_error_state_t`, `sx_alert_state_t`).
- **Validation**: Thêm validation khi set strings để tránh truncation.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Structure rõ ràng, pattern đã được thiết kế tốt.

---

### 📄 File: components/sx_core/include/sx_event.h

**1. File này dùng để làm gì?**
→ Header file định nghĩa event system. Events là messages giữa services và orchestrator.

**2. Các thành phần chính trong file**
- Enum: 
  - `sx_event_priority_t` - Event priority (LOW, NORMAL, HIGH, CRITICAL)
  - `sx_event_type_t` - Event types (lifecycle, UI, platform, services, input, audio, radio, WiFi, chatbot, system, alert, protocol, OTA, activation, diagnostics, error)
- Struct: `sx_event_t` - Event structure với type, priority, arg0, arg1, ptr
- Macro: `SX_EVT_DEFAULT_PRIORITY()` - Get default priority cho event type

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service, UI**: Tất cả communication giữa services và orchestrator đều qua events.

**4. Phụ thuộc của file**
- Include: Không có (chỉ standard types)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Event-based architecture tốt cho decoupling.
  - Priority system đảm bảo critical events được xử lý trước.
  - Generic event structure với ptr payload flexible.
- **Code smell**: 
  - Nhiều event types (50+) có thể khó maintain. Có thể group thành categories.
  - ptr payload ownership không rõ ràng - cần documentation.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory leaks**: ptr payload có thể point đến dynamic memory, cần clear ownership rules.
- **Type safety**: Generic event structure không type-safe, dễ nhầm lẫn event types.

**7. Đề xuất cải thiện**
- **Documentation**: Thêm documentation về ptr payload ownership rules.
- **Type safety**: Có thể dùng tagged unions cho type-safe events (nhưng sẽ phức tạp hơn).

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Event system rõ ràng, pattern đã được thiết kế tốt.

---

---

## BATCH 2: UI CORE FILES

### 📄 File: components/sx_ui/ui_router.c

**1. File này dùng để làm gì?**
→ Implementation của UI router - quản lý navigation giữa các screens. Router tạo container cho screens, xử lý show/hide/destroy callbacks.

**2. Các thành phần chính trong file**
- Static state: `s_current_screen`, `s_screen_container`, `s_router_initialized`
- Hàm: `ui_router_init()`, `ui_router_navigate_to()`, `ui_router_get_current_screen()`, `ui_router_get_container()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **UI**: Router là trung tâm navigation, được gọi từ UI task và các screens.

**4. Phụ thuộc của file**
- Include: `ui_router.h`, `ui_screen_registry.h`, LVGL, `esp_lvgl_port.h`, `sx_dispatcher.h`, `sx_state.h`
- Component: `sx_ui` (screen registry)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: Router pattern tốt, tách biệt navigation logic. Có duplicate navigation prevention.
- **Code smell**: 
  - Có TRACE logging với `__builtin_return_address(0)` - có thể là debug code còn sót lại.
  - LVGL lock được acquire trong `navigate_to()` - đúng pattern.
- **Phân tầng**: Đúng - router là UI infrastructure, không phụ thuộc vào business logic.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Nested locking**: Nếu `navigate_to()` được gọi từ trong LVGL lock, có thể deadlock. Nhưng code đã có check.
- **Screen lifecycle**: on_hide và on_destroy được gọi trong cùng lock - có thể gây delay nếu screen cleanup phức tạp.

**7. Đề xuất cải thiện**
- **Remove TRACE logging**: Xóa debug TRACE logging nếu không cần thiết.
- **Async cleanup**: Có thể làm screen cleanup async để không block navigation.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, pattern đã được thiết kế tốt.

---

### 📄 File: components/sx_ui/ui_screen_registry.c

**1. File này dùng để làm gì?**
→ Implementation của screen registry - quản lý screen callbacks và names. Registry lưu callbacks cho mỗi screen ID.

**2. Các thành phần chính trong file**
- Static arrays: `s_screen_callbacks[]`, `s_screen_names[]`
- Hàm: `ui_screen_registry_init()`, `ui_screen_registry_register()`, `ui_screen_registry_get()`, `ui_screen_registry_get_name()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **UI**: Registry được dùng bởi router để lấy screen callbacks.

**4. Phụ thuộc của file**
- Include: `ui_screen_registry.h`, `esp_log.h`, `string.h`

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: Registry pattern tốt, dễ extend. Screen names mapping hữu ích cho debugging.
- **Code smell**: Không có.
- **Phân tầng**: Đúng - registry là UI infrastructure.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory**: Static arrays với size MAX_SCREENS - chấp nhận được.
- **Validation**: Có validation cho screen_id và callbacks.

**7. Đề xuất cải thiện**
- **Giữ nguyên**: Registry đã tốt.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, đơn giản.

---

## BATCH 3: SERVICES

### 📄 File: components/sx_services/sx_audio_service.c

**1. File này dùng để làm gì?**
→ Audio service chính - quản lý I2S playback, recording, volume control, position tracking, FFT/spectrum analysis. File rất dài (1057+ dòng).

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_playing`, `s_paused`, `s_recording`, `s_volume`, I2S handles, playback/recording tasks
- Hàm: `sx_audio_service_init()`, `sx_audio_service_start()`, `sx_audio_play_file()`, `sx_audio_stop()`, `sx_audio_pause()`, `sx_audio_resume()`, `sx_audio_set_volume()`, `sx_audio_get_position()`, `sx_audio_get_duration()`, `sx_audio_seek()`, `sx_audio_get_spectrum()`, `sx_audio_service_feed_pcm()`
- Tasks: `sx_audio_playback_task()`, `sx_audio_recording_task()`, `sx_audio_volume_ramp_task()`
- FFT: ESP-DSP FFT với window function (Hanning)

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Audio service là core service, được gọi từ UI, orchestrator, và các services khác (radio, music).

**4. Phụ thuộc của file**
- Include: Rất nhiều - audio service header, media metadata, codec services (MP3, FLAC, AAC, Opus), audio effects (EQ, crossfade, ducking, power, router, buffer pool, recovery, profiler), platform volume, playlist manager, dispatcher, events, I2S driver, math.h
- Component: `sx_services` (codec, audio effects), `sx_platform` (volume)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Service pattern tốt với init/start/stop.
  - Volume control với logarithmic scaling và smooth ramping.
  - Position tracking và seek support.
  - FFT với ESP-DSP cho spectrum analysis.
  - Codec auto-detection.
- **Code smell**: 
  - File quá dài (1057+ dòng) - vi phạm SRP. FFT/spectrum nên tách ra service riêng.
  - Hard-coded buffer sizes (PLAYBACK_CHUNK_SAMPLES, DECODE_BUFFER_SIZE) - nên dùng Kconfig.
  - GPIO pins có thể hard-coded (cần check).
- **Phân tầng**: Đúng - service layer, không phụ thuộc UI.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory**: Nhiều static buffers và tasks, có thể tốn memory.
- **Stack overflow**: Playback/recording tasks có thể cần stack lớn.
- **Thread safety**: Có mutex cho feed PCM, position, spectrum - tốt.
- **Error recovery**: Có audio recovery service integration.

**7. Đề xuất cải thiện**
- **Refactor**: Tách FFT/spectrum ra service riêng (`sx_audio_spectrum_service.c`).
- **Kconfig**: Di chuyển buffer sizes và GPIO pins sang Kconfig.
- **Stack monitoring**: Thêm stack monitoring cho tasks.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: File dài nhưng logic rõ ràng, dễ đánh giá.

---

### 📄 File: components/sx_services/sx_wifi_service.c

**1. File này dùng để làm gì?**
→ WiFi service - quản lý WiFi connection, scanning, reconnection. Sử dụng ESP-IDF WiFi API.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_started`, `s_cfg`, `s_netif`, `s_wifi_event_group`, connection info (SSID, password, IP, RSSI, channel)
- Hàm: `sx_wifi_service_init()`, `sx_wifi_service_start()`, `sx_wifi_connect()`, `sx_wifi_disconnect()`, `sx_wifi_scan()`, `sx_wifi_is_connected()`, `sx_wifi_get_ip()`, `sx_wifi_get_rssi()`
- Event handler: `sx_wifi_event_handler()` - xử lý WiFi events và IP events

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: WiFi service được lazy load khi cần, emit events khi connected/disconnected.

**4. Phụ thuộc của file**
- Include: `sx_wifi_service.h`, ESP-IDF WiFi, netif, event, `sx_dispatcher.h`, `sx_event.h`, `sx_network_optimizer.h`
- Component: `sx_services` (network optimizer)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Service pattern với init/start/connect/disconnect.
  - Event-driven với ESP-IDF event system.
  - Auto-reconnect support.
  - Network optimizer integration.
- **Code smell**: 
  - Max retry hard-coded (MAX_RETRY = 5) - nên configurable.
  - Buffer sizes hard-coded (SSID 33, password 65) - có thể không đủ cho một số networks.
- **Phân tầng**: Đúng - service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory**: Event group và buffers - chấp nhận được.
- **Connection timeout**: Có retry logic nhưng timeout có thể không configurable.

**7. Đề xuất cải thiện**
- **Kconfig**: Di chuyển max retry và timeout sang Kconfig.
- **Buffer sizes**: Tăng buffer sizes hoặc dùng dynamic allocation.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, sử dụng ESP-IDF APIs đúng cách.

---

### 📄 File: components/sx_services/sx_settings_service.c

**1. File này dùng để làm gì?**
→ Settings service - persistent configuration storage sử dụng NVS (Non-Volatile Storage). Cung cấp APIs để get/set strings, integers, booleans, blobs.

**2. Các thành phần chính trong file**
- Static state: `s_nvs_handle`, `s_initialized`
- Hàm: `sx_settings_service_init()`, `sx_settings_set_string()`, `sx_settings_get_string()`, `sx_settings_get_string_default()`, `sx_settings_set_int()`, `sx_settings_get_int()`, `sx_settings_get_int_default()`, `sx_settings_set_bool()`, `sx_settings_get_bool()`, `sx_settings_get_bool_default()`, `sx_settings_set_blob()`, `sx_settings_get_blob()`, `sx_settings_delete()`, `sx_settings_commit()`, `sx_settings_erase_all()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Settings service được dùng bởi hầu hết services để lưu/load configuration.

**4. Phụ thuộc của file**
- Include: `sx_settings_service.h`, ESP-IDF NVS
- Component: Không có (chỉ dùng ESP-IDF NVS)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Clean API với type-safe getters/setters.
  - Default value support (`get_*_default()`).
  - Blob support cho complex data.
  - Commit mechanism để flush changes.
- **Code smell**: Không có.
- **Phân tầng**: Đúng - service layer, không phụ thuộc business logic.

**6. Rủi ro / vấn đề tiềm ẩn**
- **NVS limits**: NVS có giới hạn về key length và value size - cần document.
- **Commit frequency**: Mỗi set operation không tự động commit - cần gọi `commit()` manually hoặc có auto-commit option.

**7. Đề xuất cải thiện**
- **Auto-commit option**: Có thể thêm option để auto-commit sau mỗi set operation (với trade-off về performance).
- **Documentation**: Thêm documentation về NVS limits.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, API clean.

---

### 📄 File: components/sx_services/sx_radio_service.c

**1. File này dùng để làm gì?**
→ Radio service - HTTP streaming cho online radio. Hỗ trợ AAC, MP3, OGG, WAV formats, ICY metadata parsing, auto-reconnect, buffer management.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_playing`, `s_paused`, `s_current_url`, `s_cfg`, HTTP task, reconnect task, metadata, reconnection state, ICY metadata, buffer management, station volume boost
- Hàm: `sx_radio_service_init()`, `sx_radio_service_start()`, `sx_radio_play()`, `sx_radio_stop()`, `sx_radio_pause()`, `sx_radio_resume()`, `sx_radio_get_metadata()`
- Tasks: `sx_radio_http_task()`, `sx_radio_reconnect_task()`
- Helpers: `sx_radio_setup_http_connection()`, `sx_radio_read_stream_data()`, `sx_radio_parse_icy_headers()`, `sx_radio_process_audio_chunk()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Radio service được lazy load, emit events khi started/stopped/metadata/error.

**4. Phụ thuộc của file**
- Include: ESP-IDF HTTP client, codec services (AAC, MP3, FLAC), codec detector, audio service, audio recovery, dispatcher, events
- Component: `sx_services` (codec, audio, recovery)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - HTTP streaming với buffer management.
  - Auto-reconnect với exponential backoff.
  - ICY metadata parsing.
  - Codec auto-detection.
  - Station volume boost support.
  - Audio recovery integration.
- **Code smell**: 
  - File khá dài (967+ dòng) - có thể refactor thành các modules nhỏ hơn.
  - Buffer sizes hard-coded (READ_BUFFER_SIZE_DEFAULT, MIN_BUFFER_BEFORE_PLAY_DEFAULT) - nên dùng Kconfig.
  - Max reconnect attempts hard-coded (10) - nên configurable.
- **Phân tầng**: Đúng - service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory**: HTTP task và buffers có thể tốn memory.
- **Network errors**: Có reconnection logic nhưng có thể cần better error handling.
- **Buffer underrun**: Có audio recovery nhưng có thể cần monitor buffer fill.

**7. Đề xuất cải thiện**
- **Refactor**: Chia nhỏ thành `sx_radio_http.c`, `sx_radio_metadata.c`, `sx_radio_buffer.c`.
- **Kconfig**: Di chuyển buffer sizes và reconnect config sang Kconfig.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, có nhiều tính năng.

---

### 📄 File: components/sx_services/sx_sd_service.c

**1. File này dùng để làm gì?**
→ SD card service - mount/unmount SD card qua SPI, cung cấp file operations. Sử dụng ESP-IDF FAT filesystem.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_mounted`, `s_cfg`, `s_card`
- Hàm: `sx_sd_service_init()`, `sx_sd_service_start()`, `sx_sd_service_stop()`, `sx_sd_is_mounted()`, `sx_sd_get_mount_point()`, file operations (open, read, write, stat, etc.)

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: SD service được init trong bootstrap, được dùng bởi SD music service và assets service.

**4. Phụ thuộc của file**
- Include: ESP-IDF FAT filesystem, SDSPI host, SPI master, GPIO, `sx_spi_bus_manager.h`
- Component: `sx_platform` (SPI bus manager)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Service pattern với init/start/stop.
  - SPI bus sharing với LCD (dùng SPI bus manager).
  - File operations wrapper.
- **Code smell**: 
  - GPIO pins hard-coded trong lazy loader (lines 210-213) - nên dùng Kconfig.
  - Mount config hard-coded (max_files=5, allocation_unit_size=16KB) - nên configurable.
- **Phân tầng**: Đúng - service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **SPI bus contention**: SD card và LCD share SPI bus - cần SPI bus manager để tránh conflict.
- **Card removal**: Không có card detect pin - không detect khi card bị remove.

**7. Đề xuất cải thiện**
- **Kconfig**: Di chuyển GPIO pins và mount config sang Kconfig.
- **Card detect**: Có thể thêm card detect pin nếu hardware support.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, sử dụng ESP-IDF APIs đúng cách.

---

### 📄 File: components/sx_services/sx_ir_service.c

**1. File này dùng để làm gì?**
→ IR service - IR remote control sử dụng RMT (Remote Control) peripheral. Hỗ trợ send/receive IR codes, nhiều protocols (NEC, etc.), AC control integration.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_cfg`, RMT TX/RX channels, encoders, receive state, mutex
- Hàm: `sx_ir_service_init()`, `sx_ir_service_start()`, `sx_ir_service_stop()`, `sx_ir_send_raw()`, `sx_ir_send_code()`, `sx_ir_send_ac_command()`, `sx_ir_start_receive()`, `sx_ir_stop_receive()`, `sx_ir_get_received_code()`
- Static buffers: `s_tx_symbol_buffer[]`, `s_pulse_buffer[]` - để tránh fragmentation

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: IR service được lazy load, được dùng bởi IR control screen và MCP tools.

**4. Phụ thuộc của file**
- Include: ESP-IDF RMT (TX, RX, encoder), GPIO, `sx_ir_service.h`, `sx_ir_codes.h` (external functions)
- Component: `sx_services` (IR codes database)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - RMT TX/RX với carrier modulation (38kHz).
  - Multiple protocol support.
  - Static buffers để tránh fragmentation - tốt cho embedded.
  - Thread safety với mutex.
- **Code smell**: 
  - GPIO pins hard-coded trong lazy loader (tx_gpio=14) - nên dùng Kconfig.
  - Buffer sizes hard-coded (MAX_IR_PULSES=300) - nên configurable.
- **Phân tầng**: Đúng - service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory**: Static buffers - chấp nhận được.
- **Protocol support**: Chỉ support NEC protocol trong encoder - có thể cần thêm protocols.

**7. Đề xuất cải thiện**
- **Kconfig**: Di chuyển GPIO pins và buffer sizes sang Kconfig.
- **Protocol extensibility**: Có thể thêm protocol registry để dễ thêm protocols mới.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, sử dụng ESP-IDF RMT đúng cách.

---

### 📄 File: components/sx_core/sx_lazy_loader.c

**1. File này dùng để làm gì?**
→ Lazy loader - initialize services on-demand thay vì tại boot time. Giảm boot time và memory usage.

**2. Các thành phần chính trong file**
- Static state: `s_service_initialized[]`, `s_mutex`
- Hàm: `sx_lazy_service_init()`, `sx_lazy_service_is_initialized()`, `sx_lazy_service_deinit()`
- Service types: 20+ lazy services (WiFi, STT, wake word, AFE, TTS, BLE nav, chatbot, radio, music online, SD card, IR, bluetooth, weather, navigation, diagnostics, intent, protocol bridge, playlist, protocol WS, protocol MQTT)

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Lazy loader được gọi khi services cần được init on-demand.

**4. Phụ thuộc của file**
- Include: Rất nhiều service headers - WiFi, STT, wake word, AFE, TTS, navigation, chatbot, radio, music online, SD, IR, bluetooth, weather, diagnostics, intent, protocol bridge, playlist, protocol WS/MQTT, settings
- Component: Hầu hết services

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Lazy loading pattern tốt cho embedded system.
  - Thread-safe với mutex.
  - Check initialization state để tránh double init.
  - Load config từ settings service.
- **Code smell**: 
  - File phụ thuộc vào rất nhiều services - có thể dùng function pointers hoặc registry pattern.
  - Hard-coded GPIO pins trong một số services (SD card, IR) - nên dùng Kconfig.
  - Default API keys hard-coded (weather service) - nên remove hoặc document.
- **Phân tầng**: Đúng - core infrastructure.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Coupling**: File phụ thuộc vào tất cả services - nếu thêm service mới phải sửa file này.
- **Error handling**: Nếu service init fail, state được reset - tốt.

**7. Đề xuất cải thiện**
- **Registry pattern**: Có thể dùng registry pattern để decouple.
- **Kconfig**: Di chuyển hard-coded values sang Kconfig.
- **Remove default keys**: Xóa default API keys hoặc document rõ ràng.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, pattern tốt.

---

## BATCH 4: UI SCREENS

### 📄 File: components/sx_ui/screens/screen_home.c

**1. File này dùng để làm gì?**
→ Home screen - main launcher screen với grid menu (2x3 + chatbot). Có idle timeout để return to screensaver.

**2. Các thành phần chính trong file**
- Static UI objects: `s_top_bar`, `s_grid`, `s_container`, `s_idle_timer`
- Menu items: 7 items (Music Player, Online Music, Radio, SD Card, IR Control, Settings, Chatbot)
- Hàm: `on_create()`, `on_show()`, `on_hide()`, `on_destroy()`, `on_update()`, `home_menu_item_click_cb()`, `create_home_menu_item()`, `idle_timer_cb()`, `home_touch_event_cb()`, `reset_idle_timer()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **UI**: Home screen là main launcher, được navigate từ boot/flash screens.

**4. Phụ thuộc của file**
- Include: LVGL, `ui_router.h`, `ui_screen_registry.h`, `sx_dispatcher.h`, `sx_state.h`, `screen_common.h`, `ui_icons.h`, `ui_theme_tokens.h`, FreeRTOS
- Component: `sx_ui` (router, registry, common, icons)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Grid layout với icon cards.
  - Idle timeout với timer.
  - Touch event để reset timer.
  - UI theme tokens.
- **Code smell**: 
  - Idle timeout hard-coded (30s) - nên dùng settings hoặc Kconfig.
  - Screen gọi `ui_router_navigate_to()` trực tiếp - đúng pattern (screens có thể navigate).
- **Phân tầng**: Đúng - UI layer, chỉ navigate, không gọi services trực tiếp.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory**: Timer và UI objects - chấp nhận được.
- **Timer cleanup**: Timer được cleanup trong on_hide - tốt.

**7. Đề xuất cải thiện**
- **Settings**: Di chuyển idle timeout sang settings service.
- **Giữ nguyên**: Screen đã tốt.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, UI pattern tốt.

---

### 📄 File: components/sx_ui/screens/screen_music_player.c

**1. File này dùng để làm gì?**
→ Music player screen - full-featured music player với album art, track info, controls, progress slider, spectrum visualization, playlist view.

**2. Các thành phần chính trong file**
- Static UI objects: Top bar, content, album art, track title/artist/genre, progress slider, play/pause/prev/next buttons, volume slider, spectrum, time display, playlist list
- Hàm: `on_create()`, `on_show()`, `on_hide()`, `on_destroy()`, `on_update()`, event handlers (play/pause, volume, prev/next, progress seek, toggle list)
- Sub-screens: `screen_music_player_spectrum.c`, `screen_music_player_list.c`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **UI**: Music player screen được navigate từ home screen, gọi audio service APIs trực tiếp.

**4. Phụ thuộc của file**
- Include: LVGL, `ui_router.h`, `screen_common.h`, `sx_audio_service.h`, `sx_playlist_manager.h`, `sx_dispatcher.h`, `sx_event.h`, `ui_icons.h`, `ui_theme_tokens.h`, spectrum/list sub-screens
- Component: `sx_services` (audio, playlist)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Full-featured music player UI.
  - Spectrum visualization.
  - Playlist integration.
  - Progress seek support.
- **Code smell**: 
  - Screen gọi `sx_audio_service_*()` trực tiếp - vi phạm layering (nên emit events).
  - File khá dài (648 dòng) - có thể tách sub-screens ra files riêng (đã có spectrum và list).
- **Phân tầng**: ❌ **Vi phạm** - UI gọi services trực tiếp thay vì qua events.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Coupling**: Screen phụ thuộc trực tiếp vào audio service - khó test và maintain.
- **State sync**: Screen cần sync với audio service state - có thể có race condition.

**7. Đề xuất cải thiện**
- **Events**: Screens chỉ nên emit events (SX_EVT_UI_INPUT), orchestrator xử lý và gọi services.
- **State reading**: Screen đọc state từ dispatcher thay vì gọi service APIs.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, nhưng có vi phạm layering.

---

### 📄 File: components/sx_ui/screens/screen_chat.c

**1. File này dùng để làm gì?**
→ Chat screen - chatbot interface với message list, input bar, send button, status indicators (connection, STT, TTS, emotion).

**2. Các thành phần chính trong file**
- Static UI objects: Top bar, message list, input bar, textarea, send button, status labels (connection, STT, TTS, emotion), typing indicator
- Hàm: `on_create()`, `on_show()`, `on_hide()`, `on_destroy()`, `on_update()`, `send_btn_event_cb()`, `add_message_to_list()`
- State tracking: `s_last_state_seq`, `s_chatbot_connected`, `s_tts_speaking`, `s_stt_active`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **UI**: Chat screen được navigate từ home screen, emit UI input events, đọc state từ dispatcher.

**4. Phụ thuộc của file**
- Include: LVGL, `ui_router.h`, `sx_dispatcher.h`, `sx_state.h`, `sx_event.h`, `sx_stt_service.h`, `sx_tts_service.h`, `screen_common.h`, `ui_theme_tokens.h`, `ui_list.h`
- Component: `sx_ui` (router, common), `sx_services` (STT, TTS - chỉ include headers, không gọi trực tiếp)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Chat UI với message list.
  - Status indicators.
  - Emit events thay vì gọi services trực tiếp - **đúng pattern**.
  - Đọc state từ dispatcher - **đúng pattern**.
- **Code smell**: 
  - Include STT/TTS headers nhưng không dùng - có thể remove.
  - Message copy với `strdup()` - cần free trong orchestrator.
- **Phân tầng**: ✅ **Đúng** - Screen emit events, đọc state, không gọi services trực tiếp.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory**: Message list có thể grow - cần limit hoặc pagination.
- **String ownership**: Message copy được free trong orchestrator - cần document.

**7. Đề xuất cải thiện**
- **Remove unused includes**: Xóa STT/TTS includes nếu không dùng.
- **Message limit**: Thêm limit cho message list để tránh memory issues.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, **tuân thủ layering rules**.

---

### 📄 File: components/sx_ui/screens/screen_settings.c

**1. File này dùng để làm gì?**
→ Settings screen - main settings menu với icon grid (2 columns). Navigate đến các sub-settings screens.

**2. Các thành phần chính trong file**
- Static UI objects: Top bar, content, grid, container
- Menu items: 7 items (Display, Bluetooth, Screensaver, Equalizer, Wi-Fi, Voice, About)
- Hàm: `on_create()`, `on_show()`, `on_hide()`, `on_destroy()`, `settings_item_cb()`, `create_settings_icon_card()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **UI**: Settings screen được navigate từ home screen, chỉ navigate đến sub-settings screens.

**4. Phụ thuộc của file**
- Include: LVGL, `ui_router.h`, `screen_common.h`, `sx_state.h`, `ui_theme_tokens.h`, `ui_icons.h`
- Component: `sx_ui` (router, common, icons)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Icon grid layout.
  - Navigation đến sub-screens.
  - UI theme tokens.
- **Code smell**: Không có.
- **Phân tầng**: ✅ **Đúng** - Screen chỉ navigate, không gọi services.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Không có**: Screen đơn giản, chỉ navigation.

**7. Đề xuất cải thiện**
- **Giữ nguyên**: Screen đã tốt.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, đơn giản.

---

---

## BATCH 5: MORE SERVICES & PLATFORM

### 📄 File: components/sx_services/sx_chatbot_service.c

**1. File này dùng để làm gì?**
→ Chatbot service - xử lý messages, intent parsing, protocol integration (WebSocket/MQTT), music command detection, MCP message handling.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_ready`, `s_cfg`, `s_message_queue`, `s_intent_parsing_enabled`, protocol availability flags
- Task: `sx_chatbot_task()` - process messages từ queue
- Hàm: `sx_chatbot_service_init()`, `sx_chatbot_service_start()`, `sx_chatbot_send_message()`, `sx_chatbot_handle_json_message()`, `sx_chatbot_handle_mcp_message()`, `sx_chatbot_is_music_command()`, `sx_chatbot_build_radio_url()`
- Helpers: Intent parsing integration, protocol base interface, music command detection

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Chatbot service được lazy load, xử lý messages từ UI, emit events cho orchestrator.

**4. Phụ thuộc của file**
- Include: ESP-IDF FreeRTOS, cJSON, `sx_dispatcher.h`, `sx_event.h`, `sx_intent_service.h`, `sx_radio_service.h`, protocol services (WS/MQTT), `sx_protocol_base.h`
- Component: `sx_services` (intent, radio, protocol)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Message queue pattern tốt.
  - Intent parsing integration.
  - Protocol abstraction (base interface).
  - Music command detection (legacy).
- **Code smell**: 
  - Hard-coded API URL (`http://14.225.204.77:5005/stream_pcm?song=`) - nên dùng settings hoặc Kconfig.
  - Music command detection là legacy code - có thể remove nếu intent parsing đã cover.
  - Simple URL encoding (chỉ replace spaces) - có thể dùng proper URL encoding.
- **Phân tầng**: Đúng - service layer, emit events.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory**: Message queue có thể overflow - cần monitor.
- **URL encoding**: Simple encoding có thể fail với special characters.

**7. Đề xuất cải thiện**
- **Kconfig/Settings**: Di chuyển API URL sang settings hoặc Kconfig.
- **URL encoding**: Dùng proper URL encoding library.
- **Remove legacy**: Xóa music command detection nếu intent parsing đã cover.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, có nhiều tính năng.

---

### 📄 File: components/sx_services/sx_ota_service.c

**1. File này dùng để làm gì?**
→ OTA (Over-The-Air) update service - download và flash firmware từ HTTPS URL. Sử dụng ESP-IDF HTTPS OTA API.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_updating`, `s_last_error`, `s_progress_callback`
- Task: `sx_ota_task()` - download và flash firmware
- HTTP event handler: `http_event_handler()` - report progress
- Hàm: `sx_ota_service_init()`, `sx_ota_start_update()`, `sx_ota_get_progress()`, `sx_ota_get_error()`, `sx_ota_is_updating()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: OTA service được init trong bootstrap, được gọi từ OTA screen.

**4. Phụ thuộc của file**
- Include: ESP-IDF HTTPS OTA, HTTP client, `sx_dispatcher.h`, `sx_event.h`
- Component: Không có (chỉ dùng ESP-IDF APIs)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - HTTPS OTA với certificate support.
  - Progress callback.
  - Error reporting.
  - Auto-reboot sau khi update thành công.
- **Code smell**: 
  - Progress calculation có thể không chính xác (dùng `data_len` thay vì cumulative).
  - Error message buffer cố định (256 bytes) - có thể không đủ.
- **Phân tầng**: Đúng - service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Power loss**: Nếu mất điện trong khi update, có thể brick device - cần recovery mechanism.
- **Certificate validation**: Cần đảm bảo certificate được validate đúng.

**7. Đề xuất cải thiện**
- **Progress calculation**: Sửa progress calculation để chính xác hơn.
- **Recovery**: Thêm recovery mechanism cho power loss scenarios.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, sử dụng ESP-IDF APIs đúng cách.

---

### 📄 File: components/sx_services/sx_navigation_service.c

**1. File này dùng để làm gì?**
→ Navigation service - route calculation, geocoding, instruction generation, offline cache support. Integration với Google Maps API và BLE navigation.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_state`, `s_current_route`, `s_current_position`, `s_current_waypoint_index`, mutex, callbacks, external instruction
- Cache: Route cache và geocoding cache với expiry
- Hàm: `sx_navigation_service_init()`, `sx_navigation_calculate_route()`, `sx_navigation_start()`, `sx_navigation_stop()`, `sx_navigation_update_position()`, `sx_navigation_get_current_instruction()`, geocoding functions
- Helpers: `sx_nav_calculate_route_api()`, `sx_nav_generate_instruction()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Navigation service được lazy load, được dùng bởi navigation screen và BLE navigation.

**4. Phụ thuộc của file**
- Include: ESP-IDF HTTP client, timer, cJSON, `sx_wifi_service.h`, `sx_tts_service.h`, `sx_geocoding.h`
- Component: `sx_services` (WiFi, TTS, geocoding)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Route calculation với API và offline cache.
  - Geocoding support.
  - Instruction generation.
  - Position tracking.
  - Callback system cho state và instructions.
- **Code smell**: 
  - Cache size hard-coded (MAX_CACHED_ROUTES=10, MAX_CACHED_GEOCODING=50) - nên dùng Kconfig.
  - Cache expiry hard-coded (24 hours) - nên configurable.
  - API key có thể hard-coded - cần check.
- **Phân tầng**: Đúng - service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory**: Route cache có thể tốn memory nếu routes lớn.
- **API rate limits**: Google Maps API có rate limits - cần handle.

**7. Đề xuất cải thiện**
- **Kconfig**: Di chuyển cache sizes và expiry sang Kconfig.
- **API key**: Đảm bảo API key được load từ settings, không hard-coded.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, có nhiều tính năng.

---

### 📄 File: components/sx_services/sx_bluetooth_service.c

**1. File này dùng để làm gì?**
→ Bluetooth service - placeholder implementation cho Bluetooth/BLE. Cung cấp API structure nhưng chưa implement đầy đủ.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_enabled`, `s_state`, `s_config`, `s_device_name`, mutex, callbacks, connected device
- Hàm: `sx_bluetooth_service_init()`, `sx_bluetooth_start()`, `sx_bluetooth_stop()`, `sx_bluetooth_start_discovery()`, `sx_bluetooth_stop_discovery()`, `sx_bluetooth_connect()`, `sx_bluetooth_disconnect()`, `sx_bluetooth_get_connected_device()`
- Note: Tất cả functions đều có warning "placeholder - requires ESP-IDF Bluetooth stack"

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Bluetooth service được lazy load, nhưng chưa implement đầy đủ.

**4. Phụ thuộc của file**
- Include: ESP-IDF FreeRTOS, semaphore
- Component: Không có (placeholder)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - API structure tốt, dễ extend.
  - Thread-safe với mutex.
  - Callback system.
- **Code smell**: 
  - **Stub implementation** - tất cả functions đều là placeholder, không có actual Bluetooth stack integration.
  - State management có nhưng không có actual Bluetooth operations.
- **Phân tầng**: Đúng - service layer (nhưng chưa implement).

**6. Rủi ro / vấn đề tiềm ẩn**
- **Không functional**: Service không hoạt động - chỉ là API structure.
  - **Impact**: Bluetooth features không available.

**7. Đề xuất cải thiện**
- **Implement**: Cần implement actual Bluetooth stack integration (ESP-IDF Bluetooth/BLE APIs).
  - **Priority**: P2 (Medium) - không critical nhưng cần cho Bluetooth features.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: File rõ ràng là placeholder, dễ đánh giá.

---

### 📄 File: components/sx_platform/sx_platform.c

**1. File này dùng để làm gì?**
→ Platform layer - display initialization (LCD), touch initialization, backlight control, hardware volume control. Support nhiều LCD types (ST7796, ST7789, ILI9341).

**2. Các thành phần chính trong file**
- Static state: `s_backlight_initialized`, `s_current_brightness`
- LCD init commands: ST7796U custom init sequence
- Hàm: `sx_platform_display_init()`, `sx_platform_touch_init()`, `sx_platform_set_brightness()`, `sx_platform_hw_volume_init()`, `sx_platform_hw_volume_available()`, `sx_platform_hw_volume_set()`
- LCD selection: Dựa trên Kconfig (CONFIG_LCD_ST7796_320X480, CONFIG_LCD_ST7789_240X320, etc.)

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Platform**: Platform layer được init trong bootstrap, cung cấp hardware abstraction.

**4. Phụ thuộc của file**
- Include: ESP-IDF LCD driver, SPI, LEDC (PWM), GPIO, touch driver, I2C
- Component: Không có (platform layer)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Hardware abstraction tốt.
  - Support nhiều LCD types với Kconfig.
  - Auto-detection cho custom LCD.
  - Backlight control với PWM.
  - SPI bus sharing với SD card.
- **Code smell**: 
  - GPIO pins có thể hard-coded (cần check Kconfig).
  - LCD init sequence hard-coded - có thể move sang Kconfig hoặc separate file.
- **Phân tầng**: Đúng - platform layer, hardware abstraction.

**6. Rủi ro / vấn đề tiềm ẩn**
- **SPI bus sharing**: LCD và SD card share SPI bus - cần SPI bus manager (đã có).
- **LCD compatibility**: Auto-detection có thể fail với LCD không standard.

**7. Đề xuất cải thiện**
- **Kconfig**: Đảm bảo tất cả GPIO pins đều từ Kconfig.
- **LCD init**: Có thể move init sequences sang separate files hoặc Kconfig.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, hardware abstraction tốt.

---

### 📄 File: components/sx_assets/sx_assets.c

**1. File này dùng để làm gì?**
→ Assets service - load assets từ SD card (RGB565 images), provide bootscreen và flashscreen images. Hiện tại chủ yếu là stub.

**2. Các thành phần chính trong file**
- Static state: `s_sd_mounted`
- Asset structure: `sx_asset` với data pointer và info
- Hàm: `sx_assets_init()`, `sx_assets_load_rgb565()`, `sx_assets_get_data()`, `sx_assets_free()`, `sx_assets_sd_ready()`, `sx_assets_set_sd_ready()`, `sx_assets_get_bootscreen_img()`, `sx_assets_get_flashscreen_img()`
- Note: `sx_assets_load_rgb565()` là stub - chưa implement.

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Assets service được init trong bootstrap, được dùng bởi UI screens (boot, flash).

**4. Phụ thuộc của file**
- Include: ESP-IDF FAT filesystem, SDSPI, SPI, GPIO, `bootscreen_img.h`, `flashscreen_img.h`
- Component: `sx_assets` (image headers)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Asset loading API structure tốt.
  - SD card integration hook.
  - Bootscreen/flashscreen getters.
- **Code smell**: 
  - **Stub implementation** - `sx_assets_load_rgb565()` chưa implement, chỉ có TODO comments.
  - SD mount point hard-coded (`/sdcard`) - nên dùng từ SD service.
- **Phân tầng**: Đúng - service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Không functional**: Asset loading từ SD card chưa hoạt động.
  - **Impact**: Không thể load custom assets từ SD card.

**7. Đề xuất cải thiện**
- **Implement**: Cần implement `sx_assets_load_rgb565()` để load assets từ SD card.
  - **Priority**: P2 (Medium) - không critical nhưng cần cho custom assets.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: File rõ ràng có stub, dễ đánh giá.

---

### 📄 File: components/sx_ui/screens/screen_equalizer.c

**1. File này dùng để làm gì?**
→ Equalizer screen - 10-band EQ với presets (Flat, Pop, Rock, Jazz, Classical, Custom), reverb control, apply button.

**2. Các thành phần chính trong file**
- Static UI objects: Top bar, content, preset selector, 10 band sliders, apply button, reverb slider, container
- EQ presets: 5 presets với 10-band values
- Hàm: `on_create()`, `on_show()`, `on_hide()`, `on_destroy()`, `on_update()`, `reverb_slider_cb()`, `preset_selector_cb()`, `band_slider_cb()`, `apply_btn_cb()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **UI**: Equalizer screen được navigate từ settings screen, gọi audio EQ service trực tiếp.

**4. Phụ thuộc của file**
- Include: LVGL, `ui_router.h`, `screen_common.h`, `sx_audio_eq.h`, `sx_audio_reverb.h`, `ui_theme_tokens.h`, `ui_slider.h`
- Component: `sx_services` (audio EQ, reverb)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - 10-band EQ với presets.
  - Reverb control.
  - Apply button để apply changes.
  - UI theme tokens.
- **Code smell**: 
  - Screen gọi `sx_audio_eq_*()` và `sx_audio_reverb_*()` trực tiếp - vi phạm layering (nên emit events).
  - EQ presets hard-coded - có thể load từ settings hoặc file.
- **Phân tầng**: ❌ **Vi phạm** - UI gọi services trực tiếp thay vì qua events.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Coupling**: Screen phụ thuộc trực tiếp vào audio EQ service - khó test và maintain.

**7. Đề xuất cải thiện**
- **Events**: Screen chỉ nên emit events, orchestrator xử lý và gọi services.
- **Settings**: Load EQ presets từ settings hoặc file.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, nhưng có vi phạm layering.

---

### 📄 File: test/unit_test/test_dispatcher.c

**1. File này dùng để làm gì?**
→ Unit tests cho dispatcher - test initialization, event posting/polling, state get/set, queue full scenario, thread safety, invalid operations.

**2. Các thành phần chính trong file**
- Test functions: `test_dispatcher_init()`, `test_event_post_and_poll()`, `test_state_get_set()`, `test_event_queue_full()`, `test_state_thread_safety()`, `test_invalid_event_post()`, `test_invalid_event_poll()`, `test_invalid_state_ops()`
- Setup/teardown: `setUp()`, `tearDown()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Test**: Unit tests, không tham gia vào runtime flow.

**4. Phụ thuộc của file**
- Include: Unity test framework, FreeRTOS, `sx_dispatcher.h`, `sx_event.h`, `sx_state.h`
- Component: `sx_core` (dispatcher)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Test coverage tốt cho dispatcher.
  - Test edge cases (queue full, invalid operations).
  - Thread safety test (basic).
- **Code smell**: 
  - Thread safety test không thực sự test multi-threading - chỉ test multiple calls từ same thread.
  - Một số tests có comments "would need multiple threads" - chưa implement.
- **Phân tầng**: Đúng - test layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Test coverage**: Có thể cần thêm tests cho edge cases khác.

**7. Đề xuất cải thiện**
- **Multi-threading tests**: Thêm actual multi-threading tests cho thread safety.
- **More edge cases**: Thêm tests cho more edge cases.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Test code rõ ràng, coverage tốt.

---

## TỔNG KẾT TIẾN ĐỘ

**Đã đọc và phân tích**: ~35 files (Core, UI Core, Services, UI Screens, Platform, Tests)

**Phân loại**:
- ✅ **Core files** (10 files): Đã phân tích đầy đủ
- ✅ **UI Core files** (3 files): Đã phân tích đầy đủ
- ✅ **Services** (15 files): Đã phân tích đầy đủ
- ✅ **UI Screens** (5 files): Đã phân tích đầy đủ
- ✅ **Platform** (1 file): Đã phân tích đầy đủ
- ✅ **Tests** (1 file): Đã phân tích đầy đủ

**Còn lại**: ~527 files (Services còn lại, UI Screens còn lại, Codec services, Audio effects, Protocol services, Platform components, Build files, Config files, Docs, etc.)

**Chiến lược tiếp tục**:
- Tiếp tục đọc các files quan trọng còn lại (services, screens, codecs)
- Tạo summary cho các files ít quan trọng hơn (docs, configs, build scripts)
- Cập nhật báo cáo liên tục

---

## BATCH 6: CODEC SERVICES

### 📄 File: components/sx_services/sx_codec_mp3.c

**1. File này dùng để làm gì?**
→ MP3 codec service - decode MP3 audio streams sử dụng ESP-IDF esp_audio_simple_dec API. Cung cấp decode function và metadata extraction.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_decoder`, `s_decoder_info`, `s_info_ready`, `s_out_buffer`, `s_out_buffer_size`
- Hàm: `sx_codec_mp3_init()`, `sx_codec_mp3_decode()`, `sx_codec_mp3_get_info()`, `sx_codec_mp3_deinit()`, `sx_codec_mp3_reset()`, `sx_codec_mp3_flush()`
- Buffer: Static output buffer (DEFAULT_OUT_BUFFER_SIZE = 4096)

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: MP3 codec được dùng bởi audio service và radio service để decode MP3 streams.

**4. Phụ thuộc của file**
- Include: ESP-IDF esp_audio_simple_dec API, `sx_codec_mp3.h`, `sx_codec_common.h`
- Component: Không có (chỉ dùng ESP-IDF APIs)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Clean API với init/decode/get_info/deinit.
  - Metadata extraction (sample rate, channels, bitrate).
  - Reset và flush support.
  - Error handling tốt.
- **Code smell**: 
  - Buffer size hard-coded (4096) - nên dùng Kconfig hoặc configurable.
  - Output buffer là static - có thể cần dynamic sizing cho different frame sizes.
- **Phân tầng**: Đúng - codec service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory**: Static buffer có thể không đủ cho large frames.
- **Thread safety**: Không có mutex - có thể có issues nếu decode được gọi từ multiple threads.

**7. Đề xuất cải thiện**
- **Kconfig**: Di chuyển buffer size sang Kconfig.
- **Thread safety**: Thêm mutex nếu decode được gọi từ multiple threads.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, sử dụng ESP-IDF APIs đúng cách.

---

### 📄 File: components/sx_services/sx_codec_aac.c

**1. File này dùng để làm gì?**
→ AAC codec service - decode AAC audio streams (AAC-LC, HE-AAC, HE-AAC v2) sử dụng ESP-IDF esp_audio_simple_dec API. Tương tự MP3 codec nhưng cho AAC format.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_decoder`, `s_decoder_info`, `s_info_ready`, `s_out_buffer`, `s_out_buffer_size`
- Hàm: `sx_codec_aac_init()`, `sx_codec_aac_decode()`, `sx_codec_aac_get_info()`, `sx_codec_aac_deinit()`, `sx_codec_aac_reset()`, `sx_codec_aac_flush()`, `sx_codec_aac_seek()`, `sx_codec_aac_supports_heaac_v2()`
- Buffer: Static output buffer (DEFAULT_OUT_BUFFER_SIZE = 4096)

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: AAC codec được dùng bởi audio service và radio service để decode AAC streams.

**4. Phụ thuộc của file**
- Include: ESP-IDF esp_audio_simple_dec API, `sx_codec_aac.h`, `sx_codec_common.h`
- Component: Không có (chỉ dùng ESP-IDF APIs)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Clean API tương tự MP3 codec.
  - Seek support (nhưng có warning là không support cho streaming).
  - HE-AAC v2 support check.
  - Reset và flush support.
- **Code smell**: 
  - Buffer size hard-coded (4096) - nên dùng Kconfig.
  - Seek implementation chỉ flush/reset - không thực sự seek (cần file I/O integration).
- **Phân tầng**: Đúng - codec service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Seek**: Seek không thực sự hoạt động cho streaming - chỉ flush/reset decoder.
- **Thread safety**: Không có mutex - tương tự MP3 codec.

**7. Đề xuất cải thiện**
- **Kconfig**: Di chuyển buffer size sang Kconfig.
- **Seek**: Implement proper seek cho file-based playback (nếu cần).

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, tương tự MP3 codec.

---

### 📄 File: components/sx_services/sx_codec_flac.c

**1. File này dùng để làm gì?**
→ FLAC codec service - decode FLAC audio streams sử dụng ESP-IDF esp_audio_simple_dec API. Có conditional compilation vì FLAC support có thể không available.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_decoder`, `s_decoder_info`, `s_info_ready`, `s_out_buffer`, `s_out_buffer_size` (chỉ khi ESP_AUDIO_SIMPLE_DEC_TYPE_FLAC defined)
- Hàm: `sx_codec_flac_init()`, `sx_codec_flac_decode()`, `sx_codec_flac_get_info()`, `sx_codec_flac_deinit()`, `sx_codec_flac_reset()`, `sx_codec_flac_flush()`
- Conditional compilation: `#ifdef ESP_AUDIO_SIMPLE_DEC_TYPE_FLAC`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: FLAC codec được dùng bởi audio service để decode FLAC files.

**4. Phụ thuộc của file**
- Include: ESP-IDF esp_audio_simple_dec API (conditional), `sx_codec_flac.h`
- Component: Không có (chỉ dùng ESP-IDF APIs)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Conditional compilation để handle khi FLAC không available.
  - Clean API tương tự MP3/AAC codecs.
  - Error handling tốt.
- **Code smell**: 
  - Conditional compilation có thể làm code khó maintain.
  - Buffer size hard-coded (4096) - nên dùng Kconfig.
- **Phân tầng**: Đúng - codec service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Availability**: FLAC support có thể không available - cần check trước khi dùng.
- **Thread safety**: Không có mutex - tương tự các codec khác.

**7. Đề xuất cải thiện**
- **Kconfig**: Di chuyển buffer size sang Kconfig.
- **Availability check**: Có thể thêm function để check FLAC availability.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, có conditional compilation để handle missing support.

---

### 📄 File: components/sx_services/sx_audio_eq.c

**1. File này dùng để làm gì?**
→ Audio EQ service - 10-band parametric equalizer với presets (Flat, Pop, Rock, Jazz, Classical, Custom). Sử dụng biquad filters cho mỗi band, support stereo.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_enabled`, `s_sample_rate`, `s_band_gains[]`, `s_current_preset`, `s_filters[]` (biquad filters)
- EQ presets: 5 presets với 10-band values (in 0.1dB units)
- Band frequencies: 31, 62, 125, 250, 500, 1k, 2k, 4k, 8k, 16k Hz
- Hàm: `sx_audio_eq_init()`, `sx_audio_eq_process()`, `sx_audio_eq_set_band_gain()`, `sx_audio_eq_set_preset()`, `sx_audio_eq_get_band_gain()`, `sx_audio_eq_get_preset()`, `sx_audio_eq_set_enabled()`, `sx_audio_eq_set_sample_rate()`
- Helpers: `calculate_biquad_coefficients()`, `process_biquad_left()`, `process_biquad_right()`, `update_filters()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: EQ service được dùng bởi audio service để apply EQ effects to audio stream.

**4. Phụ thuộc của file**
- Include: Math library (sin, cos, pow), `sx_audio_eq.h`, `sx_settings_service.h`
- Component: `sx_services` (settings)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - 10-band parametric EQ với biquad filters.
  - Preset support với settings persistence.
  - Stereo support (separate history cho left/right channels).
  - Sample rate configurable.
- **Code smell**: 
  - Biquad filter calculations có thể tốn CPU - có thể optimize.
  - Settings keys hard-coded - nên dùng constants hoặc Kconfig.
- **Phân tầng**: Đúng - audio effect service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **CPU usage**: Biquad filters có thể tốn CPU - cần monitor performance.
- **Precision**: Float calculations có thể có precision issues - nhưng acceptable cho audio.

**7. Đề xuất cải thiện**
- **Optimization**: Có thể optimize biquad calculations nếu CPU usage cao.
- **Settings keys**: Dùng constants cho settings keys.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, EQ algorithm đúng.

---

## BATCH 7: VOICE SERVICES

### 📄 File: components/sx_services/sx_stt_service.c

**1. File này dùng để làm gì?**
→ STT (Speech-to-Text) service - gửi audio chunks đến STT endpoint qua HTTP, parse JSON response, emit events và call callbacks.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_active`, `s_config`, `s_result_callback`, `s_user_data`, `s_last_error`, `s_chunk_queue`, `s_stt_task_handle`, `s_stt_mutex`
- Task: `sx_stt_task()` - process audio chunks từ queue, gửi HTTP requests
- Audio chunk structure: `stt_audio_chunk_t` với PCM data và sample count
- Hàm: `sx_stt_service_init()`, `sx_stt_start()`, `sx_stt_stop()`, `sx_stt_feed_audio()`, `sx_stt_get_last_error()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: STT service được lazy load, được dùng bởi chatbot service và UI (chat screen).

**4. Phụ thuộc của file**
- Include: ESP-IDF HTTP client, cJSON, FreeRTOS, `sx_stt_service.h`, `sx_dispatcher.h`, `sx_event.h`, `sx_settings_service.h`
- Component: `sx_services` (settings)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Queue-based audio chunk processing.
  - HTTP client với authentication (Bearer token).
  - JSON response parsing.
  - Event emission và callback support.
  - Settings integration.
- **Code smell**: 
  - Queue size hard-coded (STT_CHUNK_QUEUE_SIZE = 5) - nên dùng Kconfig.
  - Error message buffer cố định (256 bytes) - có thể không đủ.
  - Memory allocation trong task (response buffer) - có thể dùng buffer pool.
- **Phân tầng**: Đúng - service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory**: Response buffer allocation trong task có thể gây fragmentation.
- **Network errors**: Cần better error handling cho network failures.

**7. Đề xuất cải thiện**
- **Kconfig**: Di chuyển queue size và buffer sizes sang Kconfig.
- **Buffer pool**: Dùng buffer pool cho response buffers.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, HTTP client integration tốt.

---

### 📄 File: components/sx_services/sx_tts_service.c

**1. File này dùng để làm gì?**
→ TTS (Text-to-Speech) service - gửi text đến TTS endpoint qua HTTP, receive audio data, play qua audio service. Support priority queue.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_config`, `s_endpoint_url`, `s_api_key`, `s_tts_task_handle`, `s_tts_queue`, `s_tts_mutex`, `s_speaking`, `s_current_request`
- Request structure: `sx_tts_request_t` với text, priority, callbacks
- Task: `sx_tts_task()` - process requests từ queue
- Hàm: `sx_tts_service_init()`, `sx_tts_service_deinit()`, `sx_tts_speak()`, `sx_tts_cancel()`, `sx_tts_is_speaking()`
- Helpers: `sx_tts_synthesize()`, `sx_tts_http_request()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: TTS service được lazy load, được dùng bởi chatbot service và UI (chat screen).

**4. Phụ thuộc của file**
- Include: ESP-IDF HTTP client, cJSON, FreeRTOS, `sx_tts_service.h`, `sx_settings_service.h`, `sx_wifi_service.h`, `sx_audio_service.h`
- Component: `sx_services` (settings, WiFi, audio)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Priority queue cho requests.
  - HTTP client với authentication.
  - Audio playback integration.
  - Settings integration.
  - Deinit support.
- **Code smell**: 
  - Queue size hard-coded (10) - nên dùng Kconfig.
  - Text buffer cố định (512 bytes) - có thể không đủ cho long texts.
  - Memory allocation trong task (audio data) - có thể dùng buffer pool.
- **Phân tầng**: Đúng - service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory**: Audio data allocation có thể tốn memory.
- **Text length**: 512 bytes có thể không đủ cho long texts.

**7. Đề xuất cải thiện**
- **Kconfig**: Di chuyển queue size và buffer sizes sang Kconfig.
- **Buffer pool**: Dùng buffer pool cho audio data.
- **Text length**: Tăng text buffer hoặc dùng dynamic allocation.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, priority queue tốt.

---

### 📄 File: components/sx_services/sx_wake_word_service.c

**1. File này dùng để làm gì?**
→ Wake word service - detect wake words từ audio stream sử dụng ESP-SR library. Support multiple wake word types.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_active`, `s_config`, `s_detected_callback`, `s_user_data`, `s_wake_word_task_handle`, `s_audio_queue`, `s_settings_model_path`
- Task: `sx_wake_word_task()` - process audio từ queue, feed to wake word engine
- Conditional compilation: `#ifdef CONFIG_SX_WAKE_WORD_ESP_SR_ENABLE`
- Hàm: `sx_wake_word_service_init()`, `sx_wake_word_start()`, `sx_wake_word_stop()`, `sx_wake_word_feed_audio()`, `sx_wake_word_register_callback()`, `sx_wake_word_reset()`, `sx_wake_word_deinit()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Wake word service được lazy load, được dùng bởi audio recording service và chatbot service.

**4. Phụ thuộc của file**
- Include: FreeRTOS, `sx_wake_word_service.h`, `sx_audio_afe.h`, `sx_settings_service.h`, `sx_audio_service.h`
- External functions: ESP-SR wrapper functions (conditional)
- Component: `sx_services` (AFE, settings, audio)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Queue-based audio processing.
  - ESP-SR integration (conditional).
  - Settings integration (model path, threshold).
  - Callback system.
- **Code smell**: 
  - Conditional compilation có thể làm code khó maintain.
  - Audio buffer size hard-coded (320 samples = 20ms at 16kHz) - nên dùng Kconfig.
  - Queue size hard-coded (10) - nên dùng Kconfig.
- **Phân tầng**: Đúng - service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **ESP-SR dependency**: Service không hoạt động nếu ESP-SR không enabled.
- **Performance**: Wake word detection có thể tốn CPU.

**7. Đề xuất cải thiện**
- **Kconfig**: Di chuyển buffer sizes và queue size sang Kconfig.
- **Documentation**: Document ESP-SR dependency rõ ràng.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, ESP-SR integration tốt.

---

## BATCH 8: AUDIO EFFECTS & UTILITIES

### 📄 File: components/sx_services/sx_audio_crossfade.c

**1. File này dùng để làm gì?**
→ Audio crossfade service - smooth transition giữa old và new audio tracks. Support fade out/in với configurable duration.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_enabled`, `s_state`, `s_fade_duration_ms`, `s_samples_processed`, `s_total_fade_samples`, `s_sample_rate`, `s_crossfade_mutex`
- States: `SX_CROSSFADE_IDLE`, `SX_CROSSFADE_FADING_OUT`, `SX_CROSSFADE_FADING_IN`, `SX_CROSSFADE_COMPLETE`
- Hàm: `sx_audio_crossfade_init()`, `sx_audio_crossfade_start()`, `sx_audio_crossfade_process()`, `sx_audio_crossfade_stop()`, `sx_audio_crossfade_set_enabled()`, `sx_audio_crossfade_set_duration()`
- Note: Current implementation chỉ fade out - full crossfade (mix old + new) chưa implement.

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Crossfade service được dùng bởi audio service và playlist manager để smooth track transitions.

**4. Phụ thuộc của file**
- Include: Math library, FreeRTOS, `sx_audio_crossfade.h`
- Component: Không có

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - State machine cho crossfade process.
  - Configurable fade duration.
  - Thread-safe với mutex.
- **Code smell**: 
  - **Incomplete implementation** - chỉ fade out, chưa có full crossfade (mix old + new).
  - Fade duration hard-coded (500ms default) - nên dùng settings hoặc Kconfig.
  - Sample rate hard-coded (16000) - nên configurable.
- **Phân tầng**: Đúng - audio effect service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Incomplete**: Full crossfade chưa implement - chỉ fade out.
  - **Impact**: Không có smooth transition giữa tracks.

**7. Đề xuất cải thiện**
- **Complete implementation**: Implement full crossfade (mix old + new PCM buffers).
  - **Priority**: P2 (Medium) - nice to have cho better UX.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, nhưng incomplete.

---

### 📄 File: components/sx_services/sx_audio_recovery.c

**1. File này dùng để làm gì?**
→ Audio recovery service - handle buffer underrun bằng cách pause playback, wait for buffer refill, then resume. Monitor buffer fill level.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_recovery_active`, `s_config`, `s_stats`, `s_recovery_mutex`, `s_recovery_task_handle`
- Task: `sx_audio_recovery_task()` - pause, wait for buffer refill, resume
- Config: `sx_audio_recovery_config_t` với threshold, target buffer, max attempts
- Stats: `sx_audio_recovery_stats_t` với total/successful/failed recoveries
- Hàm: `sx_audio_recovery_init()`, `sx_audio_recovery_check()`, `sx_audio_recovery_trigger()`, `sx_audio_recovery_get_stats()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Recovery service được dùng bởi audio service và radio service để handle buffer underruns.

**4. Phụ thuộc của file**
- Include: FreeRTOS, `sx_audio_recovery.h`, `sx_audio_service.h`
- Component: `sx_services` (audio)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Buffer monitoring với threshold.
  - Recovery mechanism với pause/resume.
  - Statistics tracking.
  - Max attempts limit.
- **Code smell**: 
  - Buffer level estimation là time-based (không query actual buffer) - có thể không chính xác.
  - Timeout hard-coded (5000ms) - nên configurable.
- **Phân tầng**: Đúng - audio utility service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Buffer estimation**: Time-based estimation có thể không chính xác - cần query actual buffer level từ audio service.

**7. Đề xuất cải thiện**
- **Actual buffer query**: Query actual buffer fill level từ audio service thay vì time-based estimation.
- **Kconfig**: Di chuyển timeout và thresholds sang Kconfig.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, nhưng buffer estimation cần improve.

---

### 📄 File: components/sx_services/sx_audio_buffer_pool.c

**1. File này dùng để làm gì?**
→ Audio buffer pool service - manage pool of audio buffers để tránh fragmentation. Support PSRAM allocation.

**2. Các thành phần chính trong file**
- Pool structure: `sx_audio_buffer_pool` với buffer pointers, in-use flags, mutex, statistics
- Hàm: `sx_audio_buffer_pool_create()`, `sx_audio_buffer_pool_destroy()`, `sx_audio_buffer_pool_alloc()`, `sx_audio_buffer_pool_free()`, `sx_audio_buffer_pool_get_stats()`, `sx_audio_buffer_pool_psram_available()`
- Helpers: `sx_audio_buffer_alloc_heap()`, `sx_audio_buffer_free_heap()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Buffer pool service được dùng bởi audio service và codec services để allocate buffers.

**4. Phụ thuộc của file**
- Include: ESP-IDF heap_caps, FreeRTOS, `sx_audio_buffer_pool.h`
- Component: Không có (utility service)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Pool pattern tốt cho memory management.
  - PSRAM support.
  - Thread-safe với mutex.
  - Statistics tracking.
- **Code smell**: Không có.
- **Phân tầng**: Đúng - utility service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory**: Pool allocation có thể tốn memory nếu pool size lớn.

**7. Đề xuất cải thiện**
- **Giữ nguyên**: Buffer pool đã tốt.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, pool pattern tốt.

---

## BATCH 9: EVENT HANDLERS & PLAYLIST

### 📄 File: components/sx_core/sx_event_handlers/audio_handler.c

**1. File này dùng để làm gì?**
→ Audio event handler - xử lý `SX_EVT_AUDIO_PLAYBACK_STOPPED` event, auto-play next track nếu playlist có auto-play enabled.

**2. Các thành phần chính trong file**
- Hàm: `sx_event_handler_audio_playback_stopped()`
- Logic: Check playlist auto-play, call `sx_playlist_next()` nếu enabled

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Core**: Event handler được register trong orchestrator, xử lý audio events.

**4. Phụ thuộc của file**
- Include: `sx_event_handler.h`, `sx_playlist_manager.h`
- Component: `sx_core` (event handler), `sx_services` (playlist)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Simple event handler pattern.
  - Auto-play logic.
- **Code smell**: Không có.
- **Phân tầng**: Đúng - event handler layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Không có**: Handler đơn giản, không có issues.

**7. Đề xuất cải thiện**
- **Giữ nguyên**: Handler đã tốt.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, đơn giản.

---

### 📄 File: components/sx_core/sx_event_handlers/chatbot_handler.c

**1. File này dùng để làm gì?**
→ Chatbot event handlers - xử lý nhiều chatbot events (STT, TTS, emotion, audio channel, connection, etc.), update state.

**2. Các thành phần chính trong file**
- Handlers: `sx_event_handler_chatbot_stt()`, `sx_event_handler_chatbot_tts_sentence()`, `sx_event_handler_chatbot_emotion()`, `sx_event_handler_chatbot_tts_start()`, `sx_event_handler_chatbot_tts_stop()`, `sx_event_handler_chatbot_audio_channel_opened()`, `sx_event_handler_chatbot_connected()`, `sx_event_handler_chatbot_disconnected()`, etc.
- Helper: `map_emotion_to_id()` - map emotion strings to stable IDs

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Core**: Event handlers được register trong orchestrator, xử lý chatbot events và update state.

**4. Phụ thuộc của file**
- Include: `sx_event_handler.h`, `sx_event_string_pool.h`, `sx_chatbot_service.h`, `sx_protocol_ws.h`, `sx_protocol_mqtt.h`, `sx_audio_protocol_bridge.h`
- Component: `sx_core` (event handler), `sx_services` (chatbot, protocol)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Multiple event handlers cho chatbot.
  - State update pattern.
  - Emotion mapping.
  - Protocol integration (WS/MQTT).
- **Code smell**: 
  - File có nhiều handlers - có thể tách thành multiple files.
  - Emotion mapping là simple substring matching - có thể improve.
- **Phân tầng**: Đúng - event handler layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Emotion mapping**: Simple substring matching có thể không chính xác.

**7. Đề xuất cải thiện**
- **Refactor**: Có thể tách handlers thành multiple files nếu file quá dài.
- **Emotion mapping**: Improve emotion mapping algorithm.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, nhiều handlers.

---

### 📄 File: components/sx_services/sx_playlist_manager.c

**1. File này dùng để làm gì?**
→ Playlist manager - manage playlists với shuffle, repeat, gapless playback, metadata caching. Support next/previous track navigation.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_current_playlist`, `s_playlist_mutex`, `s_next_preloaded`, `s_preloaded_index`, `s_preloaded_track_path`
- Metadata cache: `sx_track_meta_cache_t[]` với LRU eviction
- Playlist structure: `sx_playlist_t` với track paths, current index, shuffle, repeat flags
- Hàm: `sx_playlist_manager_init()`, `sx_playlist_create()`, `sx_playlist_set_current()`, `sx_playlist_next()`, `sx_playlist_previous()`, `sx_playlist_get_current()`, `sx_playlist_should_auto_play_next()`, `sx_playlist_free()`, `get_track_metadata()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Playlist manager được dùng bởi audio service và UI (music player screen).

**4. Phụ thuộc của file**
- Include: FreeRTOS, `sx_playlist_manager.h`, `sx_audio_service.h`, `sx_dispatcher.h`, `sx_event.h`, `sx_media_metadata.h`
- Component: `sx_services` (audio, metadata)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Playlist management với shuffle/repeat.
  - Metadata caching với LRU.
  - Gapless playback support (preloading).
  - Thread-safe với mutex.
- **Code smell**: 
  - Cache size hard-coded (METADATA_CACHE_SIZE = 32) - nên dùng Kconfig.
  - File path buffer cố định (512 bytes) - có thể không đủ cho long paths.
- **Phân tầng**: Đúng - service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory**: Playlist và metadata cache có thể tốn memory nếu large.
- **Path length**: 512 bytes có thể không đủ cho long paths.

**7. Đề xuất cải thiện**
- **Kconfig**: Di chuyển cache size sang Kconfig.
- **Path length**: Tăng path buffer hoặc dùng dynamic allocation.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, playlist management tốt.

---

---

## BATCH 10: MORE UI SCREENS & SERVICES

### 📄 File: components/sx_ui/screens/screen_radio.c

**1. File này dùng để làm gì?**
→ Radio screen - hiển thị danh sách radio stations (VOV1, VOV2, VOV3, VOV5), play/pause controls, station info, error handling với retry button.

**2. Các thành phần chính trong file**
- Static UI objects: Top bar, content, station title, station info, error label, retry button, play button, station list, container
- Station keys: Hard-coded VOV stations
- Hàm: `on_create()`, `on_show()`, `on_hide()`, `on_destroy()`, `on_update()`, `station_item_click_cb()`, `play_btn_cb()`, `retry_btn_cb()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **UI**: Radio screen được navigate từ home screen, gọi radio service APIs trực tiếp.

**4. Phụ thuộc của file**
- Include: LVGL, `ui_router.h`, `screen_common.h`, `sx_radio_service.h`, `sx_dispatcher.h`, `sx_event.h`, `ui_icons.h`, `ui_theme_tokens.h`, `ui_list.h`
- Component: `sx_services` (radio)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Station list với scrollable list component.
  - Play/pause controls.
  - Error handling với retry button.
  - Station info display.
- **Code smell**: 
  - Screen gọi `sx_radio_*()` trực tiếp - vi phạm layering (nên emit events).
  - Station keys hard-coded - nên load từ settings hoặc station table service.
  - Comment về `sx_radio_station_table.h` nhưng không include - có thể cần fix.
- **Phân tầng**: ❌ **Vi phạm** - UI gọi services trực tiếp thay vì qua events.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Coupling**: Screen phụ thuộc trực tiếp vào radio service - khó test và maintain.
- **Station list**: Hard-coded stations - không flexible.

**7. Đề xuất cải thiện**
- **Events**: Screen chỉ nên emit events, orchestrator xử lý và gọi services.
- **Station table**: Load stations từ station table service hoặc settings.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, nhưng có vi phạm layering.

---

### 📄 File: components/sx_ui/screens/screen_wifi_setup.c

**1. File này dùng để làm gì?**
→ WiFi setup screen - scan và connect to WiFi networks, hiển thị QR code của IP address khi connected, password dialog.

**2. Các thành phần chính trong file**
- Static UI objects: Top bar, content, network list, scan button, status label, password dialog, QR code widget, IP label, QR container
- Network storage: `s_networks[]` array (20 networks max)
- Hàm: `on_create()`, `on_show()`, `on_hide()`, `on_destroy()`, `on_update()`, `scan_btn_cb()`, `network_item_click_cb()`, `show_password_dialog()`, `update_ip_qr_code()`, `refresh_network_list()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **UI**: WiFi setup screen được navigate từ settings screen, gọi WiFi service APIs trực tiếp.

**4. Phụ thuộc của file**
- Include: LVGL, `ui_router.h`, `screen_common.h`, `sx_wifi_service.h`, `sx_settings_service.h`, `sx_qr_code_service.h`, `ui_theme_tokens.h`, `ui_list.h`
- Component: `sx_services` (WiFi, settings, QR code)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Network scanning với list display.
  - Password dialog cho encrypted networks.
  - QR code display khi connected.
  - IP address display.
- **Code smell**: 
  - Screen gọi `sx_wifi_*()` trực tiếp - vi phạm layering (nên emit events).
  - Network array size hard-coded (20) - nên dùng Kconfig.
  - QR code service init trong screen - nên init trong bootstrap.
- **Phân tầng**: ❌ **Vi phạm** - UI gọi services trực tiếp thay vì qua events.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Coupling**: Screen phụ thuộc trực tiếp vào WiFi service - khó test và maintain.
- **Memory**: Network array có thể không đủ nếu có nhiều networks.

**7. Đề xuất cải thiện**
- **Events**: Screen chỉ nên emit events, orchestrator xử lý và gọi services.
- **Kconfig**: Di chuyển network array size sang Kconfig.
- **Service init**: Move QR code service init sang bootstrap.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, nhưng có vi phạm layering.

---

### 📄 File: components/sx_ui/screens/screen_ota.c

**1. File này dùng để làm gì?**
→ OTA update screen - check for updates, start update, hiển thị progress bar, status messages.

**2. Các thành phần chính trong file**
- Static UI objects: Top bar, content, status label, progress bar, check button, update button, container
- Progress callback: `ota_progress_callback()` - update progress bar
- Hàm: `on_create()`, `on_show()`, `on_hide()`, `on_destroy()`, `on_update()`, `check_btn_cb()`, `update_btn_cb()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **UI**: OTA screen được navigate từ settings screen, gọi OTA service APIs trực tiếp.

**4. Phụ thuộc của file**
- Include: LVGL, `ui_router.h`, `screen_common.h`, `sx_ota_service.h`, `sx_settings_service.h`, `ui_theme_tokens.h`, `ui_slider.h`
- Component: `sx_services` (OTA, settings)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Check for updates button.
  - Progress bar với callback.
  - Status messages.
- **Code smell**: 
  - Screen gọi `sx_ota_*()` trực tiếp - vi phạm layering (nên emit events).
  - Default OTA URL hard-coded (`https://example.com/firmware.bin`) - nên remove hoặc document.
  - Update check logic đơn giản (chỉ check URL exists) - cần implement proper version check.
- **Phân tầng**: ❌ **Vi phạm** - UI gọi services trực tiếp thay vì qua events.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Coupling**: Screen phụ thuộc trực tiếp vào OTA service - khó test và maintain.
- **Version check**: Update check không thực sự check version - chỉ check URL exists.

**7. Đề xuất cải thiện**
- **Events**: Screen chỉ nên emit events, orchestrator xử lý và gọi services.
- **Version check**: Implement proper version check từ server.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, nhưng có vi phạm layering và incomplete version check.

---

### 📄 File: components/sx_services/sx_mcp_server.c

**1. File này dùng để làm gì?**
→ MCP (Model Context Protocol) server - register và execute tools, handle tool calls từ chatbot, provide tool list. Support device control, audio, screen, network, radio tools.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_send_callback`, `s_tools[]` array (MAX_TOOLS = 100), `s_tools_count`
- Tool registry: `mcp_tool_entry_t` với name, description, handler, user_only flag, input_schema
- Built-in tools: Device status, audio volume, screen brightness/theme/rotation, network QR code, radio play/stop/get stations/set display mode
- Hàm: `sx_mcp_server_init()`, `sx_mcp_server_register_tool()`, `sx_mcp_server_handle_request()`, `handle_tools_list()`, `handle_tool_call()`, `register_builtin_tools()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: MCP server được init trong bootstrap, được dùng bởi chatbot service để execute tools.

**4. Phụ thuộc của file**
- Include: cJSON, `sx_mcp_server.h`, `sx_mcp_tools.h` (external tool functions)
- Component: `sx_services` (MCP tools)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Tool registry pattern tốt.
  - Built-in tools registration.
  - JSON request/response handling.
  - Tool schema support (reserved).
- **Code smell**: 
  - Tool registry size hard-coded (MAX_TOOLS = 100) - nên dùng Kconfig.
  - External tool functions - có thể dùng function pointers hoặc registry pattern tốt hơn.
- **Phân tầng**: Đúng - service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory**: Tool registry có thể tốn memory nếu có nhiều tools.
- **Tool execution**: Tool execution có thể fail - cần better error handling.

**7. Đề xuất cải thiện**
- **Kconfig**: Di chuyển MAX_TOOLS sang Kconfig.
- **Error handling**: Improve error handling cho tool execution.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, MCP server pattern tốt.

---

### 📄 File: components/sx_services/sx_navigation_ble.c

**1. File này dùng để làm gì?**
→ BLE Navigation service - receive navigation data từ Android app qua BLE GATT, parse navigation instructions, update navigation service, TTS integration.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_connected`, `s_mutex`, callbacks, navigation queue, icon storage
- BLE GATT: Service và characteristics với UUIDs, connection handle
- Conditional compilation: `#ifdef CONFIG_BT_ENABLED` - BLE support
- Navigation queue: Circular queue cho navigation data
- Task: `nav_queue_task()` - process navigation queue
- Hàm: `sx_navigation_ble_init()`, `sx_navigation_ble_start()`, `sx_navigation_ble_stop()`, BLE callbacks (`ble_gap_event()`, `ble_gatt_svr_chr_access()`)

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: BLE Navigation service được lazy load, được dùng bởi navigation service và UI (navigation screen).

**4. Phụ thuộc của file**
- Include: ESP-IDF NimBLE (conditional), FreeRTOS, `sx_navigation_ble.h`, `sx_navigation_ble_parser.h`, `sx_navigation_service.h`, `sx_tts_service.h`, `sx_settings_service.h`, `sx_theme_service.h`, `sx_platform.h`, `sx_navigation_icon_cache.h`
- Component: `sx_services` (navigation, TTS, settings, theme, platform, icon cache)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - BLE GATT server với NimBLE.
  - Navigation queue với circular buffer.
  - Navigation parser integration.
  - TTS integration cho instructions.
  - Icon cache integration.
- **Code smell**: 
  - Conditional compilation có thể làm code khó maintain.
  - Queue size hard-coded (NAV_QUEUE_SIZE = 10) - nên dùng Kconfig.
  - Timeout hard-coded (NAV_DATA_TIMEOUT_MS = 10000) - nên configurable.
- **Phân tầng**: Đúng - service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **BLE dependency**: Service không hoạt động nếu BLE không enabled.
- **Queue overflow**: Navigation queue có thể overflow nếu data đến nhanh.

**7. Đề xuất cải thiện**
- **Kconfig**: Di chuyển queue size và timeout sang Kconfig.
- **Queue monitoring**: Thêm monitoring cho queue overflow.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, BLE GATT integration tốt.

---

### 📄 File: components/sx_services/sx_music_online_service.c

**1. File này dùng để làm gì?**
→ Music online service - download lyrics từ lyrics API, authentication với device ID và MAC address, display mode management.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_display_mode`, `s_current_track`, `s_auth_config`, `s_auth_key`
- HTTP event handler: `http_event_handler()` - handle HTTP events
- Hàm: `sx_music_online_service_init()`, `sx_music_online_set_display_mode()`, `sx_music_online_get_display_mode()`, `sx_music_online_download_lyrics()`, `sx_music_online_add_auth_headers()`, `sx_music_online_generate_auth_key()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Music online service được lazy load, được dùng bởi music online screen.

**4. Phụ thuộc của file**
- Include: ESP-IDF HTTP client, cJSON, mbedtls SHA256, ESP-IDF system/chip_info/MAC, `sx_music_online_service.h`, `sx_settings_service.h`, `sx_wifi_service.h`
- Component: `sx_services` (settings, WiFi)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Lyrics download từ API.
  - Authentication với device ID và MAC.
  - Display mode management.
  - Settings integration.
- **Code smell**: 
  - API URL hard-coded (`https://api.lyrics.ovh/v1/`) - nên dùng settings hoặc Kconfig.
  - Buffer size đã được optimize (4096 thay vì 8192) - tốt.
  - URL encoding đơn giản - có thể cần proper URL encoding.
- **Phân tầng**: Đúng - service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **API dependency**: Service phụ thuộc vào external lyrics API - có thể fail nếu API down.
- **URL encoding**: Simple URL encoding có thể fail với special characters.

**7. Đề xuất cải thiện**
- **Settings/Kconfig**: Di chuyển API URL sang settings hoặc Kconfig.
- **URL encoding**: Dùng proper URL encoding library.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, authentication pattern tốt.

---

### 📄 File: components/sx_services/sx_audio_protocol_bridge.c

**1. File này dùng để làm gì?**
→ Audio protocol bridge - bridge audio giữa local audio service và protocol services (WebSocket/MQTT). Encode audio với Opus, send/receive audio packets.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_started`, `s_send_enabled`, `s_receive_enabled`, Opus encoder state, audio buffers, queues, tasks
- Opus encoder: Config với sample rate, channels, application, bitrate
- Audio buffers: PCM accumulation buffer, Opus encode buffer
- Queues: Audio send queue (20 packets), audio receive queue (30 packets)
- Tasks: `audio_send_task()`, `audio_receive_task()`
- Recording callback: `recording_callback()` - accumulate PCM samples
- Hàm: `sx_audio_protocol_bridge_init()`, `sx_audio_protocol_bridge_start()`, `sx_audio_protocol_bridge_stop()`, `sx_audio_protocol_bridge_enable_send()`, `sx_audio_protocol_bridge_enable_receive()`, `init_opus_encoder()`, `on_audio_packet_received()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Audio protocol bridge được lazy load, được dùng bởi chatbot service và protocol services.

**4. Phụ thuộc của file**
- Include: FreeRTOS, `sx_audio_protocol_bridge.h`, `sx_audio_service.h`, `sx_codec_opus.h`, `sx_protocol_ws.h`, `sx_protocol_mqtt.h`, `sx_protocol_audio.h`, `sx_protocol_base.h`, `sx_protocol_factory.h`, `sx_dispatcher.h`, `sx_event.h`, `sx_state.h`
- Component: `sx_services` (audio, codec opus, protocol)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Opus encoding cho audio compression.
  - Queue-based audio packet processing.
  - Send/receive tasks.
  - Recording callback integration.
  - Protocol abstraction (base interface).
  - Error statistics tracking.
- **Code smell**: 
  - Queue sizes đã được optimize (20 send, 30 receive) - tốt.
  - Buffer sizes hard-coded - nên dùng Kconfig.
  - Opus config hard-coded (16kbps, VOIP) - nên configurable.
- **Phân tầng**: Đúng - service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Memory**: Audio buffers và queues có thể tốn memory.
- **Latency**: Audio encoding/decoding có thể add latency.

**7. Đề xuất cải thiện**
- **Kconfig**: Di chuyển buffer sizes và Opus config sang Kconfig.
- **Latency monitoring**: Add latency monitoring.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, audio bridge pattern tốt.

---

### 📄 File: components/sx_services/sx_media_metadata.c

**1. File này dùng để làm gì?**
→ Media metadata parser - parse metadata từ MP3 (ID3v2), OGG/FLAC (Vorbis Comments), estimate duration. Support title, artist, genre, duration extraction.

**2. Các thành phần chính trong file**
- Parsers: `parse_id3v2()`, `parse_vorbis_comment()`, `parse_flac_vorbis_comment()`, `parse_ogg_vorbis_comment()`, `parse_vorbis_comment_data()`
- Helpers: `read_be32()`, `read_be32_syncsafe()`, `read_string()`, `estimate_duration_from_file_size()`
- Hàm: `sx_meta_parse_file()`, `sx_meta_parse_buffer()`, `sx_meta_estimate_duration()`, `sx_meta_get_file_format()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Metadata parser được dùng bởi audio service, playlist manager, và media services để extract metadata.

**4. Phụ thuộc của file**
- Include: Standard C library (stdio, string, stdlib, sys/stat), `sx_media_metadata.h`
- Component: Không có (utility service)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - ID3v2 parser cho MP3.
  - Vorbis Comments parser cho OGG/FLAC.
  - Duration estimation từ file size.
  - File format detection.
- **Code smell**: 
  - UTF-16 encoding support chưa đầy đủ (skip UTF-16 frames) - có thể improve.
  - Duration estimation đơn giản (file size based) - có thể improve với actual decoding.
- **Phân tầng**: Đúng - utility service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Encoding support**: UTF-16 encoding không được support đầy đủ - có thể mất metadata.
- **Duration accuracy**: Duration estimation có thể không chính xác.

**7. Đề xuất cải thiện**
- **UTF-16 support**: Implement proper UTF-16 to UTF-8 conversion.
- **Duration accuracy**: Improve duration estimation hoặc decode actual duration.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, metadata parsing tốt.

---

### 📄 File: components/sx_services/sx_theme_service.c

**1. File này dùng để làm gì?**
→ Theme service - manage UI themes (dark, light, auto), apply theme colors to LVGL objects, auto theme based on time of day.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_current_theme`, `s_change_callback`, `s_theme_mutex`
- Theme colors: `s_dark_theme`, `s_light_theme` với color definitions
- Hàm: `sx_theme_service_init()`, `sx_theme_set_type()`, `sx_theme_get_type()`, `sx_theme_get_colors()`, `sx_theme_apply_to_object()`, `sx_theme_register_change_callback()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Theme service được init trong bootstrap, được dùng bởi UI screens và theme service.

**4. Phụ thuộc của file**
- Include: LVGL, FreeRTOS, `sx_theme_service.h`, `sx_settings_service.h`, time.h
- Component: `sx_services` (settings)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Theme management với dark/light/auto.
  - Auto theme based on time of day (18:00-6:00 = dark).
  - Settings persistence.
  - Callback system cho theme changes.
  - LVGL integration.
- **Code smell**: 
  - Time threshold hard-coded (18:00, 6:00) - nên configurable.
  - Theme colors hard-coded - có thể load từ file hoặc settings.
- **Phân tầng**: Đúng - service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Time zone**: Auto theme có thể không chính xác nếu time zone không set đúng.

**7. Đề xuất cải thiện**
- **Settings**: Di chuyển time thresholds sang settings.
- **Theme colors**: Load theme colors từ file hoặc settings.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, theme management tốt.

---

### 📄 File: components/sx_services/sx_network_optimizer.c

**1. File này dùng để làm gì?**
→ Network optimizer - track connection statistics, calculate retry delays với exponential backoff, provide success rate metrics.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, statistics (total/successful/failed connections, reconnect attempts)
- Retry config: `sx_retry_config_t` với initial delay, max delay, max attempts, exponential backoff, backoff multiplier
- Hàm: `sx_network_optimizer_init()`, `sx_network_optimizer_get_retry_delay()`, `sx_network_optimizer_record_connection()`, `sx_network_optimizer_record_reconnect()`, `sx_network_optimizer_get_success_rate()`, `sx_network_optimizer_reset_stats()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ **Service**: Network optimizer được init trong WiFi service, được dùng bởi WiFi service và network services.

**4. Phụ thuộc của file**
- Include: FreeRTOS, `sx_network_optimizer.h`
- Component: Không có (utility service)

**5. Nhận định kỹ thuật**
- **Thiết kế hợp lý**: 
  - Connection statistics tracking.
  - Exponential backoff calculation.
  - Success rate calculation.
  - Simple và lightweight.
- **Code smell**: Không có.
- **Phân tầng**: Đúng - utility service layer.

**6. Rủi ro / vấn đề tiềm ẩn**
- **Không có**: Service đơn giản, không có issues.

**7. Đề xuất cải thiện**
- **Giữ nguyên**: Network optimizer đã tốt.

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, đơn giản và hiệu quả.

---

---

## BATCH 11: C++ SERVICES & MORE COMPONENTS

### 📄 File: components/sx_services/sx_ota_full_service.cpp

**1. File này dùng để làm gì?**
→ C++ wrapper cho OTA full service, cung cấp C API cho C++ implementation (`sx_ota_full.hpp`). Quản lý activation polling task với exponential backoff.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_activation_task`
- Activation polling: `activation_poll_task()` với exponential backoff (2s → 10s max)
- C API wrappers: `sx_ota_full_service_init()`, `sx_ota_full_check_version()`, `sx_ota_full_start_upgrade()`, `sx_ota_full_activate()`, `sx_ota_full_has_activation()`, `sx_ota_full_get_activation_code()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ OTA update flow: `sx_bootstrap_start()` → `sx_ota_service_init()` → `sx_ota_full_service_init()` → UI screen gọi `sx_ota_full_check_version()` / `sx_ota_full_start_upgrade()` → Activation polling nếu server trả về 202.

**4. Phụ thuộc của file**
- `sx_ota_full.hpp` (C++ implementation)
- `sx_dispatcher.h` (post events)
- `sx_event.h` (event types)
- `sx_event_string_pool.h` (string pool)

**5. Nhận định kỹ thuật**
- ✅ **Tốt**: C++ wrapper pattern tốt, tách biệt C API và C++ implementation
- ✅ **Tốt**: Activation polling với exponential backoff hợp lý
- ⚠️ **Cần cải thiện**: Hard-coded `s_activation_max_attempts = 30`, nên move vào Kconfig
- ⚠️ **Cần cải thiện**: Stack size của activation task hard-coded (4096), nên move vào Kconfig

**6. Rủi ro / vấn đề tiềm ẩn**
- Activation task có thể leak nếu không được cleanup đúng cách
- Max attempts có thể không đủ cho slow networks

**7. Đề xuất cải thiện**
- Move `s_activation_max_attempts` và stack size vào Kconfig
- Add cleanup logic cho activation task khi service deinit

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Code rõ ràng, pattern tốt.

---

### 📄 File: components/sx_services/sx_ota_full.cpp

**1. File này dùng để làm gì?**
→ C++ implementation của OTA full service, xử lý OTA check version, start upgrade, activation với HTTP client và JSON parsing.

**2. Các thành phần chính trong file**
- Singleton pattern: `SxOtaFull::instance()`
- System info JSON: `buildSystemInfoJson()` (version, MAC, network info)
- HTTP POST JSON: `httpPostJson()` với custom headers (Device-Id, Client-Id, Serial-Number, User-Agent)
- OTA operations: `checkVersion()`, `startUpgrade()`, `activate()`
- Serial number từ eFuse (optional)

**3. File này tham gia vào luồng nào của hệ thống?**
→ OTA update flow: UI screen → `sx_ota_full_check_version()` → `SxOtaFull::checkVersion()` → HTTP POST với system info → Parse response → Emit events.

**4. Phụ thuộc của file**
- `sx_dispatcher.h` (post events)
- `sx_event.h` (event types)
- `sx_settings_service.h` (OTA URL, device UUID)
- `sx_wifi_service.h` (network info)
- `esp_http_client.h` (HTTP client)
- `esp_ota_ops.h` (OTA operations)
- `cJSON.h` (JSON parsing)

**5. Nhận định kỹ thuật**
- ✅ **Tốt**: Singleton pattern đúng cách
- ✅ **Tốt**: System info JSON đầy đủ (version, MAC, network)
- ✅ **Tốt**: Custom headers đúng format (Device-Id, Client-Id, Serial-Number)
- ⚠️ **Cần cải thiện**: Hard-coded timeout (15000ms), nên move vào Kconfig
- ⚠️ **Cần cải thiện**: Error handling có thể cải thiện (retry logic)

**6. Rủi ro / vấn đề tiềm ẩn**
- HTTP timeout có thể không đủ cho slow networks
- OTA upgrade có thể fail nếu không đủ flash space

**7. Đề xuất cải thiện**
- Move HTTP timeout vào Kconfig
- Add retry logic cho HTTP requests
- Validate flash space trước khi start upgrade

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, đầy đủ.

---

### 📄 File: components/sx_services/sx_service_helpers/sx_http_client.cpp

**1. File này dùng để làm gì?**
→ C++ HTTP client helper, cung cấp `postJson()` với custom headers support.

**2. Các thành phần chính trong file**
- `SxHttpClient::postJson()`: POST JSON với custom headers, parse response, handle status codes (200 = success, 202 = timeout, others = error)

**3. File này tham gia vào luồng nào của hệ thống?**
→ Used by OTA service, chatbot service, và các services khác cần HTTP POST JSON.

**4. Phụ thuộc của file**
- `esp_http_client.h` (ESP-IDF HTTP client)

**5. Nhận định kỹ thuật**
- ✅ **Tốt**: Simple và reusable
- ⚠️ **Cần cải thiện**: Hard-coded timeout (15000ms), nên move vào Kconfig
- ⚠️ **Cần cải thiện**: Response buffer size hard-coded (512), có thể không đủ cho large responses

**6. Rủi ro / vấn đề tiềm ẩn**
- Response buffer có thể overflow nếu response quá lớn

**7. Đề xuất cải thiện**
- Move timeout và buffer size vào Kconfig
- Add dynamic buffer allocation cho large responses

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Code đơn giản và rõ ràng.

---

### 📄 File: components/sx_services/sx_wake_word_esp_sr.cpp

**1. File này dùng để làm gì?**
→ C++ wrapper cho ESP-SR wake word detection, tích hợp với ESP-SR wakenet models.

**2. Các thành phần chính trong file**
- Static state: `s_wakenet`, `s_model_data`, `s_wakenet_initialized`, `s_wake_config`, `s_detected_callback`
- Initialization: `sx_wake_word_init_esp_sr()` - load model, create model data
- Audio feed: `sx_wake_word_feed_audio_esp_sr()` - feed audio to model, check threshold, call callback
- Callback registration: `sx_wake_word_register_callback_esp_sr()`
- Reset/Deinit: `sx_wake_word_reset_esp_sr()`, `sx_wake_word_deinit_esp_sr()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ Wake word detection flow: Audio input → `sx_wake_word_feed_audio_esp_sr()` → ESP-SR model → Threshold check → Callback → UI update.

**4. Phụ thuộc của file**
- `esp_wn_iface.h`, `esp_wn_models.h`, `esp_afe_sr_models.h` (ESP-SR APIs)
- `sx_wake_word_service.h` (C API)

**5. Nhận định kỹ thuật**
- ✅ **Tốt**: ESP-SR integration đúng cách
- ✅ **Tốt**: Threshold checking hợp lý
- ⚠️ **Cần cải thiện**: Model path hard-coded ("model"), nên move vào Kconfig
- ⚠️ **Cần cải thiện**: Confidence conversion (ret / 100.0f) có thể không chính xác

**6. Rủi ro / vấn đề tiềm ẩn**
- Model files phải có trong partition hoặc filesystem
- Threshold có thể cần tuning per model

**7. Đề xuất cải thiện**
- Move model path vào Kconfig
- Add model validation khi init
- Add threshold tuning UI

**8. Mức độ tự tin khi đánh giá**
- **Cao**: ESP-SR integration đúng cách.

---

### 📄 File: components/sx_services/sx_codec_opus_wrapper.cpp

**1. File này dùng để làm gì?**
→ C++ wrapper cho Opus encoder, sử dụng esp-opus-encoder library.

**2. Các thành phần chính trong file**
- Static state: `s_encoder`, `s_encoder_config`, `s_encoder_initialized`
- Initialization: `sx_codec_opus_encoder_init_cpp()` - validate sample rate, create encoder (20ms frame duration)
- Encoding: `sx_codec_opus_encode_cpp()` - convert C array to vector, encode, copy to output buffer
- Reset/Deinit: `sx_codec_opus_encoder_reset_cpp()`, `sx_codec_opus_encoder_deinit_cpp()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ Opus encoding flow: Audio input → `sx_codec_opus_encode_cpp()` → Opus encoder → Encoded data → Network/Storage.

**4. Phụ thuộc của file**
- `opus_encoder.h` (esp-opus-encoder library)
- `sx_codec_opus.h` (C API)

**5. Nhận định kỹ thuật**
- ✅ **Tốt**: Exception handling đầy đủ
- ✅ **Tốt**: Sample rate validation
- ⚠️ **Cần cải thiện**: Frame duration hard-coded (20ms), nên move vào config
- ⚠️ **Cần cải thiện**: Reset là no-op, có thể cần implement proper reset

**6. Rủi ro / vấn đề tiềm ẩn**
- Frame duration có thể không optimal cho tất cả use cases
- Reset không thực sự reset encoder state

**7. Đề xuất cải thiện**
- Move frame duration vào config
- Implement proper reset nếu cần

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Code rõ ràng, exception handling tốt.

---

### 📄 File: components/sx_services/sx_codec_opus_decoder_wrapper.cpp

**1. File này dùng để làm gì?**
→ C++ wrapper cho Opus decoder, sử dụng esp-opus-encoder library.

**2. Các thành phần chính trong file**
- Static state: `s_decoder`, `s_decoder_config`, `s_decoder_mutex` (thread-safe)
- Initialization: `sx_codec_opus_decoder_init_cpp()` - create decoder (60ms frame duration)
- Decoding: `sx_codec_opus_decode_cpp()` - convert to vector, decode, copy to output buffer
- Reset/Deinit: `sx_codec_opus_decoder_reset_cpp()`, `sx_codec_opus_decoder_deinit_cpp()`
- Sample rate getter: `sx_codec_opus_decoder_get_sample_rate_cpp()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ Opus decoding flow: Network/Storage → Opus data → `sx_codec_opus_decode_cpp()` → Opus decoder → PCM data → Audio service.

**4. Phụ thuộc của file**
- `opus_decoder.h` (esp-opus-encoder library)
- `sx_codec_opus.h` (C API)
- `std::mutex` (thread safety)

**5. Nhận định kỹ thuật**
- ✅ **Tốt**: Thread-safe với mutex
- ✅ **Tốt**: Exception handling đầy đủ
- ✅ **Tốt**: Proper reset với `ResetState()`
- ⚠️ **Cần cải thiện**: Frame duration hard-coded (60ms), nên move vào config

**6. Rủi ro / vấn đề tiềm ẩn**
- Frame duration có thể không optimal cho tất cả use cases

**7. Đề xuất cải thiện**
- Move frame duration vào config

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Code rõ ràng, thread-safe, exception handling tốt.

---

### 📄 File: components/sx_services/sx_audio_afe_esp_sr.cpp

**1. File này dùng để làm gì?**
→ C++ wrapper cho ESP-SR AFE (Audio Front-End), cung cấp AEC, VAD, NS, AGC.

**2. Các thành phần chính trong file**
- Static state: `s_afe_handle`, `s_afe_iface`, `s_models`, `s_afe_initialized`, `s_afe_config`, `s_vad_callback`
- Initialization: `sx_audio_afe_init_esp_sr()` - load models, create AFE config, create AFE interface và data handle
- Processing: `sx_audio_afe_process_esp_sr()` - feed audio, fetch processed audio, check VAD, call callback
- VAD callback: `sx_audio_afe_register_vad_callback_esp_sr()`
- Reset/Deinit: `sx_audio_afe_reset_esp_sr()`, `sx_audio_afe_deinit_esp_sr()`

**3. File này tham gia vào luồng nào của hệ thống?**
→ Audio processing flow: Audio input → `sx_audio_afe_process_esp_sr()` → AFE processing (AEC/VAD/NS/AGC) → Processed audio → Audio service.

**4. Phụ thuộc của file**
- `esp_afe_sr_models.h`, `esp_afe_sr_iface.h` (ESP-SR AFE APIs)
- `sx_audio_afe.h` (C API)

**5. Nhận định kỹ thuật**
- ✅ **Tốt**: ESP-SR AFE integration đúng cách
- ✅ **Tốt**: VAD callback support
- ⚠️ **Cần cải thiện**: Model path hard-coded ("model"), nên move vào Kconfig
- ⚠️ **Cần cải thiện**: AFE config parameters hard-coded (preferred core, priority, memory alloc mode)

**6. Rủi ro / vấn đề tiềm ẩn**
- Model files phải có trong partition hoặc filesystem
- AFE config có thể cần tuning per hardware

**7. Đề xuất cải thiện**
- Move model path và AFE config vào Kconfig
- Add model validation khi init

**8. Mức độ tự tin khi đánh giá**
- **Cao**: ESP-SR AFE integration đúng cách.

---

### 📄 File: components/sx_services/sx_ir_codes.c

**1. File này dùng để làm gì?**
→ IR code database cho các model điều hòa (Toshiba, Sharp), cung cấp lookup function.

**2. Các thành phần chính trong file**
- Static database: `s_ir_ac_codes[]` với model name, protocol, address, codes array
- Lookup function: `sx_ir_get_code()` - tìm code theo brand, model, command

**3. File này tham gia vào luồng nào của hệ thống?**
→ IR control flow: UI screen → `sx_ir_get_code()` → Lookup database → Return address và code → IR service transmit.

**4. Phụ thuộc của file**
- `sx_ir_service.h` (IR service types)

**5. Nhận định kỹ thuật**
- ✅ **Tốt**: Database structure rõ ràng
- ⚠️ **Cần cải thiện**: Database hard-coded, nên move vào external file hoặc settings
- ⚠️ **Cần cải thiện**: Case-insensitive comparison tự implement, nên dùng `strcasecmp` nếu có

**6. Rủi ro / vấn đề tiềm ẩn**
- Database có thể không đủ cho tất cả models
- Codes có thể cần test và điều chỉnh per model

**7. Đề xuất cải thiện**
- Move database vào external file (JSON/CSV)
- Add database update mechanism
- Add code learning/recording feature

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Code rõ ràng, database structure hợp lý.

---

### 📄 File: components/sx_services/sx_radio_station_table.h

**1. File này dùng để làm gì?**
→ Radio station table cho VOV stations (Vietnamese), cung cấp lookup function.

**2. Các thành phần chính trong file**
- Static table: `g_vn_vov_stations[]` với key, url, title
- Lookup function: `sx_radio_lookup_station()` - tìm station theo key

**3. File này tham gia vào luồng nào của hệ thống?**
→ Radio service flow: UI screen → `sx_radio_lookup_station()` → Lookup table → Return station info → Radio service play.

**4. Phụ thuộc của file**
- `<strings.h>` (strcasecmp)

**5. Nhận định kỹ thuật**
- ✅ **Tốt**: Table structure rõ ràng
- ⚠️ **Cần cải thiện**: Table hard-coded, nên move vào external file hoặc settings

**6. Rủi ro / vấn đề tiềm ẩn**
- Table có thể không đủ cho tất cả stations
- URLs có thể thay đổi

**7. Đề xuất cải thiện**
- Move table vào external file (JSON/CSV)
- Add station update mechanism

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Code rõ ràng, table structure hợp lý.

---

### 📄 File: components/sx_services/sx_navigation_service.c

**1. File này dùng để làm gì?**
→ Navigation service, quản lý routes, waypoints, instructions, geocoding với offline cache support.

**2. Các thành phần chính trong file**
- Static state: `s_initialized`, `s_state`, `s_current_route`, `s_current_position`, `s_current_waypoint_index`, `s_mutex`, callbacks
- Route calculation: `sx_navigation_calculate_route()` - check cache, call API, save to cache
- Navigation control: `sx_navigation_start()`, `sx_navigation_stop()`, `sx_navigation_pause()`, `sx_navigation_resume()`
- Instruction generation: `sx_nav_generate_instruction()` - generate instruction từ waypoint và distance
- Geocoding: `sx_navigation_geocode_address()`, `sx_navigation_reverse_geocode()` - check cache, call API, save to cache
- Offline cache: `s_route_cache[]`, `s_geocoding_cache[]` với expiry (24 hours)

**3. File này tham gia vào luồng nào của hệ thống?**
→ Navigation flow: UI screen → `sx_navigation_calculate_route()` → API call → Cache → `sx_navigation_start()` → Update position → Generate instructions → TTS → UI update.

**4. Phụ thuộc của file**
- `sx_tts_service.h` (TTS for instructions)
- `sx_wifi_service.h` (network check)
- `esp_http_client.h` (HTTP client)
- `cJSON.h` (JSON parsing)

**5. Nhận định kỹ thuật**
- ✅ **Tốt**: Offline cache support
- ✅ **Tốt**: Thread-safe với mutex
- ⚠️ **Cần cải thiện**: Cache size hard-coded (MAX_CACHED_ROUTES = 10, MAX_CACHED_GEOCODING = 50), nên move vào Kconfig
- ⚠️ **Cần cải thiện**: Cache expiry hard-coded (86400s = 24h), nên move vào Kconfig

**6. Rủi ro / vấn đề tiềm ẩn**
- Cache có thể overflow nếu có quá nhiều routes
- API calls có thể fail nếu network không available

**7. Đề xuất cải thiện**
- Move cache size và expiry vào Kconfig
- Add cache cleanup mechanism
- Add retry logic cho API calls

**8. Mức độ tự tin khi đánh giá**
- **Cao**: Implementation rõ ràng, offline cache tốt.

---

### 📄 File: components/sx_services/sx_jpeg_encoder.c

**1. File này dùng để làm gì?**
→ Minimal JPEG encoder implementation, convert RGB565/RGB888 to JPEG.

**2. Các thành phần chính trong file**
- Quantization tables: `qtable_luma[]`, `qtable_chroma[]` (quality 50)
- Quality adjustment: `adjust_quality_table()` - scale quantization table theo quality
- Color conversion: `rgb565_to_rgb888()` - convert RGB565 to RGB888
- JPEG header writing: `write_jpeg_header()` - SOI, APP0, DQT, SOF0, DHT
- JPEG encoding: `sx_jpeg_encode()` - convert RGB, DCT, quantization, Huffman encoding

**3. File này tham gia vào luồng nào của hệ thống?**
→ Image encoding flow: Display buffer → `sx_jpeg_encode()` → JPEG encoder → JPEG data → Network/Storage.

**4. Phụ thuộc của file**
- `<math.h>` (DCT calculations)

**5. Nhận định kỹ thuật**
- ⚠️ **Cần cải thiện**: Minimal implementation, có thể không đủ cho production
- ⚠️ **Cần cải thiện**: DHT (Huffman tables) simplified, có thể không đúng chuẩn JPEG
- ✅ **Tốt**: Quality adjustment hợp lý

**6. Rủi ro / vấn đề tiềm ẩn**
- Minimal implementation có thể không tương thích với tất cả JPEG decoders
- Performance có thể không tốt bằng hardware encoder

**7. Đề xuất cải thiện**
- Consider sử dụng ESP32 hardware JPEG encoder nếu có
- Hoặc sử dụng libjpeg-turbo nếu cần full JPEG support
- Improve DHT implementation

**8. Mức độ tự tin khi đánh giá**
- **Trung bình**: Minimal implementation, cần review kỹ hơn.

---

### 📄 File: components/sx_ui/screens/screen_equalizer.c

**1. File này dùng để làm gì?**
→ Equalizer screen với 10-band EQ, preset selector, reverb control.

**2. Các thành phần chính trong file**
- Static UI objects: Top bar, content, preset selector, 10 band sliders, reverb slider, apply button
- EQ presets: `s_eq_presets[][]` (Flat, Pop, Rock, Jazz, Classical)
- Preset selection: `preset_selector_cb()` - apply preset values
- Band sliders: `band_slider_cb()` - update EQ band
- Reverb slider: `reverb_slider_cb()` - update reverb level
- Apply button: `apply_btn_cb()` - apply EQ và reverb settings

**3. File này tham gia vào luồng nào của hệ thống?**
→ UI flow: Settings screen → Equalizer screen → User adjusts bands/reverb → Apply → Audio service update EQ/reverb.

**4. Phụ thuộc của file**
- `sx_audio_eq.h` (EQ service)
- `sx_audio_reverb.h` (Reverb service)
- `ui_slider.h` (Slider component)
- `screen_common.h` (Common UI helpers)

**5. Nhận định kỹ thuật**
- ✅ **Tốt**: UI structure rõ ràng
- ✅ **Tốt**: Preset support
- ⚠️ **Cần cải thiện**: Direct service calls, nên emit events thay vì direct calls
- ⚠️ **Cần cải thiện**: Band sliders vertical rotation có thể không optimal

**6. Rủi ro / vấn đề tiềm ẩn**
- Direct service calls vi phạm layering
- Vertical sliders có thể khó sử dụng

**7. Đề xuất cải thiện**
- Refactor để emit events thay vì direct service calls
- Consider horizontal sliders thay vì vertical

**8. Mức độ tự tin khi đánh giá**
- **Cao**: UI implementation rõ ràng.

---

## BATCH 12: SERVICES (FULL PASS) + ARCH v1.3 VIOLATIONS TRACKING

> Chế độ audit: **Option B** (đọc hết, ghi sâu file quan trọng / file có rủi ro / file có vi phạm kiến trúc).

### 🔥 Nhóm file vi phạm kiến trúc theo SIMPLEXL_ARCH v1.3 (services gọi LVGL)

Các file dưới đây nằm trong `components/sx_services/` nhưng **include LVGL / esp_lvgl_port** hoặc gọi `lv_*` (vi phạm rule: *Service không được dính UI; LVGL chỉ được gọi trong UI task*):

1. `components/sx_services/sx_display_service.c`
2. `components/sx_services/sx_theme_service.c`
3. `components/sx_services/sx_image_service.c`
4. `components/sx_services/sx_qr_code_service.c`
5. `components/sx_services/sx_mcp_tools.c` (cần kiểm tra mức độ dùng LVGL; có include lvgl.h theo grep)

> Đây khớp với Section 8 (Known violations) trong tài liệu kiến trúc v1.3.

---

### 📄 File: components/sx_services/sx_display_service.c

**1. File này dùng để làm gì?**
→ Service "display" cung cấp API chụp màn hình, encode JPEG, upload/download JPEG, và **hiển thị image trực tiếp lên LVGL screen**.

**2. Thành phần chính**
- State: `s_initialized`, `s_preview_image_obj`, `s_preview_timer`
- Capture: `sx_display_capture_screen()` (hiện là placeholder, fill đen)
- Encode: `sx_display_encode_jpeg()` → `sx_jpeg_encode_rgb565()`
- Upload/download: dùng `esp_http_client`
- Preview: `sx_display_show_image()` / `sx_display_hide_image()` dùng LVGL trực tiếp + FreeRTOS timer

**3. Vi phạm kiến trúc / vấn đề**
- ❌ **Vi phạm lớn**: service include `esp_lvgl_port.h`, `lvgl.h` và gọi `lv_*`.
- ❌ UI ownership sai: `sx_display_show_image()` tạo `lv_img_create(lv_scr_act())` trong service.
- ⚠️ Memory ownership nguy hiểm:
  - `sx_display_show_image()` malloc `image_copy` rồi truyền vào `sx_image_create_lvgl_rgb565()`.
  - `sx_display_hide_image()` lấy `lv_img_get_src()` rồi `free(img_dsc->data)` và `free(img_dsc)` thủ công → dễ double-free nếu LVGL cũng free.
- ⚠️ Screen capture hiện **chưa implement**, chỉ log warning và fill buffer màu đen.

**4. Khuyến nghị (theo ARCH v1.3 pattern)**
- Tách thành 2 lớp:
  - `sx_display_service` chỉ làm capture/encode/upload/download, trả dữ liệu qua event/state.
  - UI screen chịu trách nhiệm render (tạo lv_img, timer UI, v.v.).
- Chuẩn hoá ownership: descriptor + data nên có API free rõ ràng (không free qua `lv_img_get_src()` theo kiểu "hack").

**5. Mức độ tự tin**: Cao (đọc trực tiếp code, vi phạm rõ).

---

### 📄 File: components/sx_services/sx_theme_service.c

**1. File này dùng để làm gì?**
→ Quản lý theme (dark/light/auto) cho ứng dụng, cung cấp màu sắc và giao diện thống nhất.

**2. Thành phần chính**
- State: `s_initialized`, `s_current_theme`, `s_change_callback`
- Màu sắc: `s_dark_theme` và `s_light_theme` (hard-coded)
- API: `sx_theme_set_type()`, `sx_theme_get_colors()`, `sx_theme_apply_to_object()`
- Auto-theme: chuyển dark/light dựa trên giờ (6h-18h: light, 18h-6h: dark)

**3. Vi phạm kiến trúc / vấn đề**
- ❌ **Vi phạm kiến trúc**: service include `lvgl.h` và gọi `lv_*` trong `sx_theme_apply_to_object()`.
- ❌ **Tight coupling với LVGL**: `sx_theme_apply_to_object()` trực tiếp gọi `lv_obj_set_style_bg_color()`.
- ⚠️ **Hard-coded theme**: Màu sắc cứng trong code, khó tùy chỉnh.
- ⚠️ **Không có persistence cho auto-theme**: Không lưu trạng thái auto-theme đã detect.

**4. Khuyến nghị (theo ARCH v1.3)**
- Tách thành 2 lớp:
  - `sx_theme_service` chỉ quản lý state theme (dark/light/auto) và trả về màu sắc dạng `uint32_t`.
  - UI layer (screen/component) tự apply màu thông qua event/state thay vì gọi trực tiếp LVGL.
- Thêm event `SX_EVT_THEME_CHANGED` để UI cập nhật khi theme thay đổi.
- Di chuyển LVGL-specific code vào UI helper.

**5. Mức độ tự tin**: Cao (đọc code trực tiếp, vi phạm rõ).

---

### 📄 File: components/sx_services/sx_image_service.c

**1. File này dùng để làm gì?**
→ Service xử lý ảnh: decode PNG/JPEG → RGB565, tạo LVGL image descriptor, resize/crop.

**2. Thành phần chính**
- Decode: `decode_jpeg_to_rgb565()`, `decode_png_to_rgb565()`
- LVGL wrapper: `sx_image_create_lvgl_rgb565()`, `sx_image_free_lvgl()`
- Helper: `parse_jpeg_header()`, `parse_png_header()`, `rgb888_to_rgb565()`
- Phụ thuộc: lodepng (PNG), tjpgd (JPEG) từ LVGL

**3. Vi phạm kiến trúc / vấn đề**
- ❌ **Vi phạm kiến trúc**: include `lvgl.h` và trả về LVGL descriptors (`lv_img_dsc_t`).
- ⚠️ **Memory ownership phức tạp**: `sx_image_create_lvgl_rgb565()` malloc data nhưng không rõ ai free.
- ⚠️ **Phụ thuộc vào LVGL internals**: Sử dụng internal LVGL paths (e.g., `../../managed_components/lvgl__lvgl/src/...`).

**4. Khuyến nghị**
- Tách thành 2 lớp:
  - `sx_image_decoder`: pure decoding (PNG/JPEG → RGB565), không biết LVGL.
  - UI helper: convert RGB565 → LVGL image descriptor.
- Chuẩn hóa API với rõ ràng ownership.
- Sử dụng LVGL public API thay vì internal headers.

**5. Bổ sung quan sát quan trọng (ownership hiện tại)**
- `sx_image_load_from_file()` và `sx_image_load_from_memory()` có thể trả về:
  - **RGB565 decoded buffer** (malloc) hoặc
  - **raw bytes** (malloc copy) nếu decode fail.
  Caller phải `sx_image_free()`.
- `sx_image_create_lvgl_rgb565(data, w, h)`:
  - **KHÔNG copy data**; gắn trực tiếp con trỏ `data` vào `lv_image_dsc_t`.
  - Hàm trả về `sx_lvgl_image_t*` và khi free bằng `sx_image_free_lvgl()` sẽ free **cả descriptor và data**.
  ⇒ nếu caller vừa giữ `data` để free riêng, dễ double-free.

**6. Mức độ tự tin**: Cao.

---

### 📄 File: components/sx_services/sx_qr_code_service.c

**1. File này dùng để làm gì?**
→ Tạo QR code từ text/IP. Có 2 mode:
- Nếu `LV_USE_QRCODE` bật: tạo widget `lv_qrcode_*`.
- Nếu không: tạo bitmap placeholder theo hash (không phải QR thật).

**2. Thành phần chính**
- `sx_qr_code_generate()`:
  - Khi có LVGL QR: tạo `lv_qrcode_create(lv_scr_act())` tạm thời, update, rồi **tự dựng bitmap placeholder** (không extract QR thật từ widget).
  - Khi không có LVGL QR: tạo pattern placeholder.
- `sx_qr_code_create_lvgl_widget()` (chỉ khi `LV_USE_QRCODE`): tạo widget QR trực tiếp cho UI.

**3. Vi phạm kiến trúc / vấn đề**
- ❌ **Vi phạm kiến trúc**: service include `lvgl.h` và gọi `lv_qrcode_*`, `lv_scr_act()`.
- ❌ Logic QR không đúng chuẩn:
  - Nhánh `LV_USE_QRCODE`: tạo widget rồi **không đọc được bitmap**, cuối cùng vẫn tạo pattern giả.
  - Nhánh fallback: pattern giả.
  ⇒ `sx_qr_code_generate()` về bản chất **không tạo QR code thật**.
- ⚠️ API design lệch: nếu đã có `sx_qr_code_create_lvgl_widget()`, thì service nên chỉ cung cấp dữ liệu/URL; UI tự tạo widget.

**4. Khuyến nghị**
- Nếu mục tiêu là QR thật: dùng thư viện QR (qrcodegen) trong service, trả về matrix/bitmap thật.
- Nếu mục tiêu là hiển thị QR: bỏ `sx_qr_code_generate()` dạng bitmap; chỉ giữ `sx_qr_code_create_lvgl_widget()` nhưng chuyển nó sang UI layer.
- Dù theo hướng nào, **không dùng LVGL trong service**.

**5. Mức độ tự tin**: Cao.

---

### 📄 File: components/sx_services/sx_mcp_tools.c

**1. File này dùng để làm gì?**
→ Implement các “tools” cho MCP server (JSON-RPC). Cung cấp các lệnh điều khiển playback, playlist, SD list/search, OTA, device info, v.v.

**2. Thành phần chính**
- Helpers tạo JSON-RPC response: `create_response()`, `mcp_tool_create_error()`, `mcp_tool_create_success()`
- Tool functions (export):
  - `mcp_tool_sdmusic_playback()`, `mcp_tool_sdmusic_mode()`, `mcp_tool_sdmusic_track()`, `mcp_tool_sdmusic_search()`, `mcp_tool_sdmusic_directory()`, ...
- Gọi trực tiếp service APIs: `sx_audio_*`, `sx_playlist_*`, `sx_sd_music_*`, ...

**3. Vấn đề kiến trúc**
- ✅ Không gọi LVGL trực tiếp trong file này (những include LVGL trước đó là do phụ thuộc gián tiếp tới `sx_display_service`/`sx_image_service` vốn vi phạm).
- ⚠️ MCP tools đang gọi services trực tiếp (OK về layering vì nó nằm ở service layer), nhưng cần đảm bảo thread-safety (MCP server có thể chạy task riêng).

**4. Rủi ro**
- Không thấy cơ chế auth/permission trong tool calls (tuỳ use-case).
- Payload có thể lớn (list 100 entries), cần chú ý heap.

**5. Khuyến nghị**
- Thêm rate limit / permission gate cho tool nhạy cảm (OTA, display upload).
- Chuẩn hoá trả lỗi (map `esp_err_t` → JSON-RPC error codes).

**6. Mức độ tự tin**: Trung bình-cao (file dài; đã đọc phần lớn và xác nhận pattern chính).

---

## BATCH 12 (tiếp): Đọc tiếp toàn bộ sx_services + sx_protocol + codec_common

### 📄 File: components/sx_services/sx_audio_service.c

**1. File này dùng để làm gì?**
→ Service audio lõi: phát nhạc từ file (MP3/FLAC/WAV/PCM), feed PCM ra I2S, record mic (I2S RX), tích hợp EQ/crossfade, volume (HW/SW), tracking position/duration, hook sang STT.

**2. Thành phần chính**
- I2S std mode: `i2s_new_channel()`, `i2s_channel_init_std_mode()`, `i2s_channel_write()/read()`.
- Playback task: `sx_audio_playback_task()`
  - Detect format: `sx_codec_detect_file_format()`
  - Decode MP3/FLAC (buffer 4096), fallback RAW/WAV
  - Apply EQ: `sx_audio_eq_process()`
  - Apply volume: `s_current_volume_factor` (log curve)
  - Post event khi dừng: `SX_EVT_AUDIO_PLAYBACK_STOPPED`
  - Preload next (gapless-ish): `sx_playlist_preload_next()`
- Feed PCM API: `sx_audio_service_feed_pcm()`
  - Reconfig sample rate động (disable/init/enable I2S)
  - Copy vào reusable buffer từ `sx_audio_buffer_alloc_heap()` để tránh malloc/free mỗi lần
  - Apply EQ + crossfade + volume rồi write I2S
- Recording task: `sx_audio_recording_task()`
  - callback streaming + đẩy chunk sang STT nếu đang active
- Volume:
  - Ưu tiên HW volume (codec I2C) nếu available
  - SW volume ramp task 200ms, update 10ms
- Position/duration:
  - track `s_samples_played` và `s_track_duration_seconds` (metadata/estimate)

**3. Tuân thủ kiến trúc (ARCH v1.3)**
- ✅ Không include LVGL / không gọi `lv_*`.
- ✅ Giao tiếp lên UI thông qua event (`sx_dispatcher_post_event`) + getters (position/duration/spectrum).

**4. Rủi ro / vấn đề tiềm ẩn**
- ⚠️ Threading/task lifetime:
  - `sx_audio_stop()` dùng `vTaskDelete(s_playback_task_handle)` sau delay 100ms → có thể kill task giữa lúc đang giữ resource (file/I2S mutex). An toàn hơn là dùng signal + join.
- ⚠️ Sample rate switching: trong `sx_audio_service_feed_pcm()` gọi `i2s_channel_disable/init/enable` **ngay trong đường realtime**; nếu bị gọi dày có thể gây glitch.
- ⚠️ `sx_audio_playback_task()` loop condition `while (!feof(f) && s_playing && !s_paused)`:
  - Khi pause, vòng lặp dừng hẳn và sẽ cleanup → pause thực tế giống stop (bug UX). Nên dùng `if (s_paused) { vTaskDelay...; continue; }`.
- ⚠️ Mutex take với timeout 0 ở `s_feed_mutex`: nếu contention sẽ trả `ESP_ERR_TIMEOUT` và drop audio frame.
- ⚠️ EQ áp 2 lần:
  - Trong playback task đã `sx_audio_eq_process(pcm_buf, ...)` rồi gọi `sx_audio_service_feed_pcm()`; trong feed_pcm lại EQ lần nữa → nguy cơ double-EQ (tuỳ đường gọi). (Cần xác nhận đường gọi thực tế; trong code hiện tại playback task gọi `sx_audio_service_feed_pcm()` với PCM đã EQ + volume; feed_pcm sẽ lại copy + EQ + volume.)

**5. Đề xuất cải thiện**
- Sửa pause/resume semantics (không exit playback task khi pause).
- Tách pipeline rõ ràng: hoặc EQ ở playback task, hoặc EQ trong feed_pcm (chỉ 1 nơi).
- Stop task nên graceful: set flag + notify + chờ task tự cleanup.
- Sample-rate switching nên debounce hoặc chỉ đổi khi stream đổi.

**6. Mức độ tự tin**: Cao (đọc trực tiếp phần lớn file và các luồng chính).

---

### 📄 File: components/sx_services/sx_wifi_service.c

**1. File này dùng để làm gì?**
→ Quản lý WiFi STA: init/start/stop, scan, connect/disconnect, lưu SSID/password hiện tại, cập nhật IP/RSSI/channel, auto-reconnect với backoff (qua network optimizer), và bắn event cho core/UI.

**2. Thành phần chính**
- Init: `esp_netif_init()`, `esp_event_loop_create_default()`, `esp_netif_create_default_wifi_sta()`, `esp_wifi_init()`.
- Event handler: `sx_wifi_event_handler()` xử lý `WIFI_EVENT_*` và `IP_EVENT_STA_GOT_IP`.
- Scan blocking: `esp_wifi_scan_start(..., true)` rồi `esp_wifi_scan_get_ap_records()`.
- Auto reconnect:
  - `MAX_RETRY = 5`, cấu hình backoff qua `sx_network_optimizer_get_retry_delay()`.
  - Khi disconnect: delay bằng `vTaskDelay()` ngay trong event handler.
- Events:
  - `SX_EVT_WIFI_CONNECTED` (ptr = ssid)
  - `SX_EVT_WIFI_DISCONNECTED`

**3. Tuân thủ kiến trúc (ARCH v1.3)**
- ✅ Không gọi LVGL.
- ✅ Thông báo lên hệ thống qua `sx_dispatcher_post_event()`.

**4. Rủi ro / vấn đề tiềm ẩn**
- ⚠️ **Blocking trong event handler**: dùng `vTaskDelay()` ngay trong `sx_wifi_event_handler()` → có thể làm nghẽn event loop task của ESP-IDF (không nên sleep trong handler). Nên chuyển reconnect logic sang task/timer.
- ⚠️ Bit logic hơi lạ: trong nhánh reconnect có `xEventGroupSetBits(... WIFI_FAIL_BIT)` ngay sau khi gọi `esp_wifi_connect()`; bit FAIL thường chỉ nên set khi fail thật.
- ⚠️ Lưu password vào RAM global (`s_current_password`) — cân nhắc security (tuỳ threat model).
- ⚠️ `sx_wifi_connect()` disconnect rồi delay 500ms: hard-coded.

**5. Đề xuất cải thiện**
- Dời auto-reconnect sang dedicated FreeRTOS task hoặc esp_timer, tránh delay trong event handler.
- Chỉnh logic event group: chỉ set FAIL khi fail; set CONNECTED khi got IP.
- Thêm API lấy trạng thái “connecting/failed” để UI hiển thị tiến trình.

**6. Mức độ tự tin**: Cao.

---

*File này sẽ được cập nhật liên tục cho đến khi hoàn thành tất cả 562 files.*

**TỔNG KẾT TIẾN ĐỘ CẬP NHẬT**:
- **Đã đọc và phân tích**: ~127 files (thêm: Audio Service, WiFi Service)
- **Còn lại**: ~435 files


