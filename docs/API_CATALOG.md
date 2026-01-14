# API Catalog - Tổng Quan

Tài liệu này cung cấp catalog đầy đủ về API, data model, concurrency, và ownership rules cho từng component trong dự án **hai-os-simplexl**.

## Cấu Trúc Tài Liệu

Mỗi component có một file riêng trong thư mục `docs/API_CATALOG/`:

- **[sx_core](./API_CATALOG/sx_core.md)** - Core system: dispatcher, orchestrator, bootstrap, state management
- **sx_platform** - Hardware abstraction: LCD, touch, SPI, I2C, SD card
- **sx_ui** - UI framework: LVGL integration, screen lifecycle, router
- **sx_services** - Business logic services: audio, network, AI, storage
- **sx_assets** - Asset management: image loading, embedded resources
- **sx_app** - Application entry point

## Mẫu Phân Tích

Mỗi file trong catalog tuân theo mẫu phân tích sau:

### A) Vai Trò File
Mô tả ngắn gọn vai trò của file trong kiến trúc và dependencies trực tiếp.

### B) Public API
- Danh sách hàm public với contract (input/output, pre/post conditions)
- Error model (ESP_ERR_*, NULL, assert) và ý nghĩa

### C) Data Model
- Structs/enums quan trọng, invariant

### D) Concurrency
- Chạy trong task nào? context nào? có ISR không?
- Mutex/queue/timer dùng gì? lock order? có risk deadlock không?

### E) Memory Ownership
- Ai malloc/ai free, lifetime của buffer/event payload

### F) Side Effects
- NVS/SD/network/I2S/LVGL… tác động gì

### G) Call Sites
- Ai gọi API này? gọi ở đâu? (liệt kê 5 call sites quan trọng nhất)

### H) Issues/Risks
- Bug class (leak, race, overflow), điều kiện kích hoạt, cách tái hiện

### I) Đề Xuất Cải Thiện
- Cải thiện nhỏ (không refactor lớn), ưu tiên P0/P1/P2

## Tài Liệu Bổ Sung

- **[CALL_GRAPH.md](./CALL_GRAPH.md)** - Call graph theo module và luồng runtime
- **[OWNERSHIP_THREADING_RULES.md](./OWNERSHIP_THREADING_RULES.md)** - Ownership/free rules, thread context, lock order

## Tiến Độ

- ✅ **Batch 1: sx_core** - Hoàn thành
- ✅ **Batch 2: sx_platform** - Hoàn thành
- ✅ **Batch 3: sx_ui** - Hoàn thành
  - ✅ **Batch 4: sx_services audio** - Hoàn thành
  - ✅ **Batch 5: sx_services network+ai** - Hoàn thành
  - ✅ **Batch 6: storage (nvs/sd/fatfs/spiffs) + assets** - Hoàn thành
    - ✅ Phần 1: sx_settings_service (NVS) - Hoàn thành
    - ✅ Phần 2: sx_sd_service (SD card + FAT) - Hoàn thành
    - ✅ Phần 3: sx_assets (asset loader) - Hoàn thành
    - ✅ Phần 4: sx_playlist_manager + sx_media_metadata - Hoàn thành
    - ✅ Phần 1: sx_wifi_service - Hoàn thành
    - ✅ Phần 2: sx_network_optimizer - Hoàn thành
    - ✅ Phần 3: sx_intent_service - Hoàn thành
    - ✅ Phần 4: sx_stt_service - Hoàn thành
    - ✅ Phần 5: sx_tts_service - Hoàn thành
    - ✅ Phần 6: sx_wake_word_service - Hoàn thành
  - ✅ Phần 1: sx_audio_service (core) - Hoàn thành
  - ✅ Phần 2: sx_audio_router - Hoàn thành
  - ✅ Phần 3: sx_audio_eq - Hoàn thành
  - ✅ Phần 4: sx_audio_ducking + crossfade - Hoàn thành
  - ⏳ Phần 3: sx_audio_eq - Đang chờ
  - ⏳ Phần 4: sx_audio_ducking + crossfade - Đang chờ
- ⏳ **Batch 3: sx_ui** - Đang chờ
- ⏳ **Batch 4: sx_services audio** - Đang chờ
- ⏳ **Batch 5: sx_services network+ai** - Đang chờ
- ⏳ **Batch 6: storage + assets** - Đang chờ


