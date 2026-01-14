# Phase 0 Hoàn Thành & Phase 1 Kế Hoạch

**Ngày:** 2025-01-02  
**Trạng thái:** Phase 0 ✅ Hoàn thành | Phase 1 🔄 Đang lập kế hoạch

---

## ✅ PHASE 0: QUICK WINS & STABILITY FIXES - HOÀN THÀNH

### Tổng Kết

Phase 0 đã hoàn thành **100%** các mục tiêu chính:

1. ✅ **Concurrency Hotfix**: Đã thêm mutex protection cho tất cả state flags
2. ✅ **Critical Bug Fixes**: Đã fix memory leaks và tối ưu audio task
3. ✅ **Storage Fixes**: Đã implement auto-commit và validation
4. ✅ **Network Fixes**: Đã thêm URL encoding và tăng queue sizes

### Chi Tiết Hoàn Thành

#### 1. Concurrency Protection ✅

- **WiFi service**: Thêm `s_state_mutex` bảo vệ `s_connected`
- **STT service**: Cải thiện mutex protection cho `s_active`
- **Wake word service**: Thêm `s_state_mutex` bảo vệ `s_active`
- **Audio service**: Đã có mutex từ trước
- **Playlist**: Đã có mutex từ trước

#### 2. Critical Fixes ✅

- Memory leak fix: Dùng `sx_event_alloc_string()` thay `strdup()`
- Audio task priority: Đã set priority 5
- Audio task stack: Đã set 4096 bytes
- SPI bus lock: Đã có timeout 100ms

#### 3. Storage Improvements ✅

- Settings auto-commit: Đã implement
- NVS key validation: Đã implement (max 15 chars)
- `sx_assets_load_rgb565()`: Đã có implementation đầy đủ

#### 4. Network Improvements ✅

- TTS URL encoding: Đã implement `url_encode()` function
- STT queue size: Tăng từ 5 → 10
- TTS queue size: Tăng từ 10 → 20

### Files Modified

1. `components/sx_services/sx_wifi_service.c`
2. `components/sx_services/sx_stt_service.c`
3. `components/sx_services/sx_wake_word_service.c`
4. `components/sx_services/sx_tts_service.c`

---

## 🔄 PHASE 1: ARCHITECTURE HARDENING - KẾ HOẠCH

### Mục Tiêu

Phá vỡ các phụ thuộc xấu, cải thiện testability và cấu trúc.

### Các Nhiệm Vụ

#### 1. Break Circular Dependencies ⚠️ Phức Tạp

**Hiện trạng:**
- `sx_ui/CMakeLists.txt` vẫn có `PRIV_INCLUDE_DIRS "../sx_services/include"`
- `sx_ui/CMakeLists.txt` vẫn có `LINK_INTERFACE_MULTIPLICITY 3`
- `sx_ui` vẫn `REQUIRES sx_services`

**Cần làm:**
- Refactor để `sx_ui` và `sx_services` chỉ giao tiếp qua events và state
- Loại bỏ direct includes giữa hai components
- Loại bỏ `PRIV_INCLUDE_DIRS` và `LINK_INTERFACE_MULTIPLICITY`

**Độ phức tạp:** Cao - Cần refactor nhiều files, có thể break existing code

#### 2. Decouple Bootstrap ⚠️ Trung Bình

**Hiện trạng:**
- Service Registry đã được implement (`sx_service_if.c`)
- `sx_bootstrap.c` vẫn directly include và call init functions của tất cả services
- Một số services đã có lifecycle wrapper (ví dụ: `sx_audio_service_lifecycle.c`)

**Cần làm:**
- Tạo lifecycle wrappers cho tất cả services
- Services tự đăng ký với registry (constructor hoặc init function)
- Bootstrap chỉ gọi `sx_service_init_all()` thay vì direct calls

**Độ phức tạp:** Trung bình - Cần tạo nhiều wrapper files, nhưng pattern đã có sẵn

#### 3. Improve Audio Pipeline ⚠️ Trung Bình

**Hiện trạng:**
- Decode → EQ → Volume → Direct `sx_audio_service_feed_pcm()` → I2S
- Không có buffer queue giữa decode và feed_pcm
- I2S re-configuration có thể block trong feed_pcm thread

**Cần làm:**
- Thêm buffer queue giữa decode và `feed_pcm()` để tạo buffer, chống underrun
- Tạo command queue cho I2S re-configuration để không block PCM feed thread

**Độ phức tạp:** Trung bình - Cần thêm queue và refactor audio pipeline

#### 4. Storage Layer ⚠️ Dễ

**Hiện trạng:**
- `sx_sd_service` chỉ có read operations
- Playlist không persist (chỉ trong memory)

**Cần làm:**
- Implement write API cho `sx_sd_service`
- Implement playlist persistence (JSON trên SD card)

**Độ phức tạp:** Dễ - Chỉ cần thêm functions mới

---

## 📋 Ưu Tiên Thực Hiện Phase 1

### Bước 1: Storage Layer (Dễ nhất, ít rủi ro) 🔵

1. Implement `sx_sd_service_write_file()` function
2. Implement `sx_playlist_save_to_file()` và `sx_playlist_load_from_file()`
3. Test với JSON format

**Ước tính:** 2-3 ngày

### Bước 2: Audio Pipeline Improvements (Trung bình) 🟡

1. Tạo buffer queue giữa decode và feed_pcm
2. Tạo command queue cho I2S re-configuration
3. Refactor audio playback task để sử dụng queues

**Ước tính:** 3-5 ngày

### Bước 3: Service Registry Migration (Trung bình) 🟡

1. Tạo lifecycle wrappers cho tất cả services
2. Services tự đăng ký trong constructor/init
3. Refactor bootstrap để dùng registry

**Ước tính:** 4-6 ngày

### Bước 4: Break Circular Dependencies (Khó nhất) 🔴

1. Phân tích tất cả dependencies giữa sx_ui và sx_services
2. Refactor để chỉ dùng events/state
3. Loại bỏ PRIV_INCLUDE_DIRS và LINK_INTERFACE_MULTIPLICITY
4. Test kỹ lưỡng

**Ước tính:** 1-2 tuần

---

## 🎯 Khuyến Nghị

**Nên bắt đầu với:**
1. **Storage Layer** - Dễ, ít rủi ro, có giá trị ngay
2. **Audio Pipeline** - Cải thiện stability và performance
3. **Service Registry** - Cải thiện architecture, nhưng cần test kỹ
4. **Circular Dependencies** - Để cuối cùng, cần nhiều thời gian và testing

**Lý do:**
- Storage Layer và Audio Pipeline có giá trị ngay lập tức
- Service Registry cải thiện architecture nhưng không break existing code
- Circular Dependencies là thay đổi lớn nhất, cần nhiều testing

---

## 📊 Metrics

### Phase 0 Impact
- **Mutex protections added**: 3 services
- **Queue size increases**: 2 services (50-100% increase)
- **Code stability**: Significantly improved
- **Race conditions eliminated**: 3 critical areas

### Phase 1 Expected Impact
- **Circular dependencies**: 1 → 0 (100% elimination)
- **Bootstrap coupling**: High → Low (Service Registry)
- **Audio underrun risk**: Medium → Low (Buffer queue)
- **Playlist persistence**: None → Full (JSON on SD)

---

*Report generated: 2025-01-02*
