# TODO/FIXME Summary

> Tổng hợp các TODO/FIXME còn lại trong codebase và kế hoạch xử lý

## Phân Loại

### 🔴 Technical Debt (Cần migrate API)

#### RMT Driver Migration (3 TODO)
- **Files:** `components/sx_services/sx_led_service.c` (lines 15, 34, 164)
- **Vấn đề:** Đang dùng legacy RMT API, cần migrate sang RMT encoder API mới
- **Kế hoạch:** 
  - Chờ ESP-IDF WS2812 encoder implementation
  - Migrate khi API stable
- **Ưu tiên:** Trung bình (legacy API vẫn hoạt động)

#### ADC Migration (3 TODO)
- **Files:** `components/sx_services/sx_power_service.c` (lines 10, 98, 208)
- **Vấn đề:** Cần migrate sang `esp_adc/adc_oneshot.h` và `esp_adc/adc_cali.h`
- **Kế hoạch:**
  - Migrate khi implement battery monitoring feature
  - Cần calibration support cho accurate readings
- **Ưu tiên:** Thấp (chưa có battery monitoring)

#### RTC Clock API (2 TODO)
- **Files:** `components/sx_services/sx_power_service.c` (lines 18, 164)
- **Vấn đề:** Cần dùng `esp_pm` component hoặc RTC clock API mới
- **Kế hoạch:**
  - Migrate khi implement power management features
- **Ưu tiên:** Thấp (chưa có power management)

### 🟡 Future Features (Tính năng tương lai)

#### SD Card Assets Loading (2 TODO)
- **Files:** `components/sx_assets/sx_assets.c` (lines 27, 60)
- **Vấn đề:** Chưa implement load RGB565 từ SD card
- **Kế hoạch:**
  - Phase 3: Implement file I/O cho SD card
  - Buffer management cho RGB565 data
- **Ưu tiên:** Trung bình (có thể dùng flash assets thay thế)

#### Gapless Playback (1 TODO)
- **Files:** `components/sx_services/sx_playlist_manager.c` (line 415)
- **Vấn đề:** Chưa preload audio data cho gapless playback
- **Kế hoạch:**
  - Integrate với audio service để preload next track
  - Buffer management cho pre-decoded audio
- **Ưu tiên:** Trung bình (nice to have)

#### OGG Decoder Support (1 TODO)
- **Files:** `components/sx_services/sx_radio_service.c` (line 830)
- **Vấn đề:** Chỉ support MP3, chưa có OGG decoder
- **Kế hoạch:**
  - Integrate esp-opus hoặc OGG decoder library
- **Ưu tiên:** Thấp (MP3 đủ dùng)

#### ID3v2 APIC Parsing (1 TODO)
- **Files:** `components/sx_services/sx_sd_music_service.c` (line 471)
- **Vấn đề:** Chưa parse full ID3v2 APIC frame cho cover art
- **Kế hoạch:**
  - Implement ID3v2 tag parser
  - Extract APIC frame với MIME type detection
- **Ưu tiên:** Thấp (cover art không critical)

## Tổng Kết

### Thống Kê
- **Tổng số TODO:** 14
- **Technical Debt:** 8 (RMT: 3, ADC: 3, RTC: 2)
- **Future Features:** 6 (SD Assets: 2, Gapless: 1, OGG: 1, ID3v2: 1, WebSocket auth: 1 - đã fix)

### Kế Hoạch Xử Lý

#### Ngay lập tức (Đã làm)
- ✅ Cải thiện comments cho rõ ràng hơn
- ✅ Phân loại TODO theo mức độ ưu tiên

#### Ngắn hạn (Có thể làm)
- 🔄 SD Card Assets Loading (nếu cần)
- 🔄 Gapless Playback (nếu cần)

#### Dài hạn (Khi có thời gian)
- 🔄 RMT Driver Migration (chờ ESP-IDF)
- 🔄 ADC Migration (khi implement battery monitoring)
- 🔄 OGG Decoder (nếu cần support OGG format)
- 🔄 ID3v2 APIC Parsing (nếu cần cover art)

## Lưu Ý

- Tất cả TODO đều có comment rõ ràng về kế hoạch
- Không có TODO nào là critical bug
- Có thể tiếp tục phát triển mà không cần fix ngay
- Technical debt sẽ được xử lý khi migrate API

---

*Cập nhật: Sau khi cải thiện comments và phân loại TODO*











