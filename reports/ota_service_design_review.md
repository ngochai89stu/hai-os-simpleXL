# OTA SERVICE DESIGN REVIEW (sx_ota_full)

> **Mục tiêu:** Review thiết kế OTA service theo kiến trúc SimpleXL (event-driven), đánh giá độ an toàn, độ ổn định, và khả năng tái sử dụng pattern cho các service khác.  
> **Phạm vi:** `components/sx_services/sx_ota_full.cpp/.hpp`  
> **Lưu ý:** Report-only, không thay đổi code.

---

## 1) Tổng quan kiến trúc

### Entry points (Public API)
- `SxOtaFull::checkVersion()`
- `SxOtaFull::startUpgrade(const std::string &url)`
- `SxOtaFull::hasActivation() / getActivationCode() / activate()`

**Path:** `components/sx_services/sx_ota_full.hpp`

### Service style
- Singleton pattern: `SxOtaFull::instance()`
- Sử dụng ESP-IDF OTA APIs + HTTP client
- Kết quả/tiến trình báo về UI bằng **dispatcher events** (`sx_dispatcher_post_event`).

**Path:** `components/sx_services/sx_ota_full.cpp`

---

## 2) Luồng hoạt động chính

### 2.1 OTA Check (POST system info)
**Path:** `components/sx_services/sx_ota_full.cpp`  
**Functions:**
- `buildSystemInfoJson()`
- `httpPostJson()`
- `checkVersion()`

**Điểm tốt**
- Payload JSON có các trường tối thiểu hữu ích: language/app/version/mac/network info.
- Có thêm header để đồng bộ với backend: `Activation-Version`, `Device-Id`, `Client-Id`, `Serial-Number`, `User-Agent`.

**Rủi ro / hạn chế**
- `httpPostJson()` đọc response với buffer 512 bytes loop; OK, nhưng không giới hạn tổng size (có thể bị response quá lớn).
- Chưa thấy explicit TLS config (cert bundle / pinning) trong `esp_http_client_config_t`.

### 2.2 Activation flow
**Path:** `components/sx_services/sx_ota_full.cpp`  
**Functions:** `checkVersion()`, `activate()`, `buildActivationPayload()`

**Điểm tốt**
- Hỗ trợ mã activation + challenge.
- Hỗ trợ HMAC (SOC_HMAC_SUPPORTED) và eFuse user_data cho serial.

**Rủi ro / hạn chế**
- Activation state lưu trong biến member (`activation_*`) không persist; sau reboot có thể mất.
- `ESP_ERR_TIMEOUT` mapping cho HTTP 202 (“waiting for user input”) là hợp lý, nhưng cần chuẩn hóa error mapping dùng chung.

### 2.3 OTA Upgrade
**Path:** `components/sx_services/sx_ota_full.cpp`  
**Function:** `startUpgrade()`

**Điểm tốt**
- Sử dụng chuẩn ESP-IDF:
  - `esp_http_client_open()` / `esp_http_client_read()`
  - `esp_ota_begin()` / `esp_ota_write()` / `esp_ota_end()` / `esp_ota_set_boot_partition()`
- Báo tiến trình bằng event `SX_EVT_OTA_PROGRESS` (arg0: %).
- Báo kết thúc: `SX_EVT_OTA_FINISHED`.

**Rủi ro / hạn chế**
- Timeout HTTP (15s) có thể thấp đối với OTA lớn.
- Chưa thấy cơ chế resume / retry chunk.
- Việc `esp_restart()` (nếu có) cần được điều phối (orchestrator/state).

---

## 3) Event-driven integration (SimpleXL compliance)

### Events được dùng
- `SX_EVT_OTA_ERROR`
- `SX_EVT_OTA_PROGRESS`
- `SX_EVT_OTA_FINISHED`
- `SX_EVT_OTA_ACTIVATION_REQUIRED` (gửi activation_code)

**Path:** `components/sx_services/sx_ota_full.cpp` (nhiều vị trí `sx_dispatcher_post_event`).

**Nhận xét**
- Pattern “service → dispatcher event → UI reacts” phù hợp SimpleXL.
- `sx_event_alloc_string()` được dùng để truyền message string; cần đảm bảo lifecycle/free đúng trong event handler.

---

## 4) Error handling & retry strategy

### Error mapping hiện tại
- `httpPostJson()`:
  - HTTP 200 → `ESP_OK`
  - HTTP 202 → `ESP_ERR_TIMEOUT`
  - Others → `ESP_FAIL`

**Điểm tốt**
- Có phân biệt 202 rõ ràng.

**Cần chuẩn hóa**
- Nên có enum/result struct riêng cho OTA API (ví dụ: `sx_ota_result_t`) thay vì map hết vào `esp_err_t`.
- Cần policy retry:
  - Retry checkVersion nếu timeout
  - Retry startUpgrade nếu mạng gián đoạn

---

## 5) Security checklist (ESP32-S3)

### Đã có dấu hiệu tích cực
- Có đường eFuse đọc serial
- Có SOC_HMAC_SUPPORTED branch

### Thiếu / cần xác minh
- TLS certificate verification:
  - Có set `cert_pem` hoặc dùng `crt_bundle_attach` không?
- Firmware signature verification:
  - `esp_ota_end()` có validate; nhưng cần secure boot + signed image để mạnh hơn.
- Anti-rollback:
  - check `esp_ota_get_app_description()` + `esp_efuse_*` anti-rollback config.

**Đề xuất (pattern-only, chưa code):**
- Chuẩn hóa `esp_http_client_config_t` với TLS bundle.
- Bật secure boot + flash encryption nếu sản phẩm production.

---

## 6) Config update pattern (MQTT/Websocket)

**Path:** `components/sx_services/sx_ota_full.cpp`  
**Functions:** `storeMqttConfigFromJson()`, `storeWebsocketConfigFromJson()`

**Điểm tốt**
- Map JSON keys → settings keys theo bảng mapping.
- Tách thành helper functions.

**Rủi ro**
- Settings được ghi ngay; cần đảm bảo atomic (nếu partial update có thể inconsistent).
- Nên có “versioning” cho config payload.

---

## 7) Recommendation (không đổi behavior hiện tại)

### Mức đề xuất
- **Pattern-only** (không sửa code ngay):
  1) Chuẩn hóa `httpPostJson()` thành helper dùng chung cho các service khác.
  2) Chuẩn hóa event payload pattern: progress + error string.
  3) Chuẩn hóa config-mapping JSON→settings.

### Cấu trúc đề xuất (khi refactor nhẹ)
- `components/sx_services/sx_http_json_client.*` (helper)
- `components/sx_services/sx_config_apply.*` (apply config from JSON)

---

## 8) Kết luận

`SxOtaFull` đã đi đúng hướng kiến trúc SimpleXL: service độc lập, event-driven, UI không bị dính vào OTA logic.  
Điểm cần chú ý chủ yếu là **security (TLS/pinning/signed images)** và **retry/resume** để OTA ổn định hơn trên mạng yếu.







