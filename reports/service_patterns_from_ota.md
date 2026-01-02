# SERVICE PATTERNS RÚT RA TỪ OTA (sx_ota_full) ĐỂ DÙNG CHO TOÀN OS

> **Mục tiêu:** Trích các pattern có giá trị dùng chung từ OTA service để các service khác (Weather, Navigation, Chatbot, Settings sync…) dùng lại **mà không phụ thuộc OTA**, giữ kiến trúc SimpleXL.

---

## 1) Pattern: HTTP JSON Client Wrapper

### Mô tả
Một wrapper chuẩn cho HTTP POST/GET JSON:
- Set headers chuẩn (User-Agent, Device-Id, Accept-Language…)
- Set timeout
- Perform request
- Read response body
- Map HTTP status → result

### Ví dụ trong code
- **Path:** `components/sx_services/sx_ota_full.cpp`
- **Function:** `esp_err_t SxOtaFull::httpPostJson(const std::string &url, const std::string &body, std::string &out_response)`

### Vì sao tốt cho OS
- Nhiều service có HTTP JSON calls (Weather, Geocoding, OTA, Chatbot cloud, telemetry).
- Tránh copy/paste esp_http_client setup.

### Đề xuất tái cấu trúc nhẹ (pattern-only)
- **Vị trí mới đề xuất:** `components/sx_services/sx_http_json_client.c/.h`
- **API tối thiểu:**
  ```c
  typedef struct {
      const char *url;
      uint32_t timeout_ms;
      const char *content_type; // default: application/json
  } sx_http_json_cfg_t;

  esp_err_t sx_http_post_json(const sx_http_json_cfg_t *cfg,
                             const char *body,
                             char *resp_buf, size_t resp_buf_len,
                             int *http_status);
  ```

### Mức độ tái sử dụng
- **B — tách nhẹ**

---

## 2) Pattern: Service → UI bằng Dispatcher Events

### Mô tả
Service không gọi UI trực tiếp; thay vào đó:
- Post event progress
- Post event error
- Post event finished

### Ví dụ trong code
- **Path:** `components/sx_services/sx_ota_full.cpp`
- **Events:** `SX_EVT_OTA_PROGRESS`, `SX_EVT_OTA_ERROR`, `SX_EVT_OTA_FINISHED`

### Vì sao tốt cho OS
- UI thread an toàn, không bị block.
- Dễ unify flow cho nhiều service async (OTA, download, voice upload, sync).

### Đề xuất chuẩn hóa
- Tạo một “job/event” contract dùng chung:
  - job_id
  - progress (0-100)
  - status (running/failed/success)
  - message string (optional)

### Mức độ tái sử dụng
- **C — pattern-only** (vì event enum sẽ khác cho từng service)

---

## 3) Pattern: JSON → Settings Mapping

### Mô tả
Nhận JSON config từ server rồi map vào settings keys (flat key-value).

### Ví dụ trong code
- **Path:** `components/sx_services/sx_ota_full.cpp`
- **Function:** `storeMqttConfigFromJson(cJSON *mqtt)`
- **Function:** `storeWebsocketConfigFromJson(cJSON *ws)`

### Vì sao tốt cho OS
- Nhiều service cần apply config từ backend (MQTT endpoint, WS endpoint, feature flags).
- Tránh viết lại mapping cho từng module.

### Đề xuất tái cấu trúc nhẹ
- **Vị trí mới đề xuất:** `components/sx_services/sx_config_apply.c/.h`
- **API tối thiểu:**
  ```c
  typedef struct {
      const char *json_key;
      const char *settings_key;
  } sx_cfg_map_t;

  esp_err_t sx_apply_string_map_from_json(cJSON *obj, const sx_cfg_map_t *map, size_t map_len);
  ```

### Mức độ tái sử dụng
- **B — tách nhẹ**

---

## 4) Pattern: System Info Payload (Device Identity)

### Mô tả
Tạo payload JSON mô tả thiết bị để gửi lên server:
- app/version
- mac
- network status
- optional serial/eFuse

### Ví dụ trong code
- **Path:** `components/sx_services/sx_ota_full.cpp`
- **Function:** `std::string buildSystemInfoJson()`

### Vì sao tốt cho OS
- Dùng chung cho:
  - telemetry/diagnostics
  - registration
  - feature gating
  - activation

### Đề xuất tái cấu trúc nhẹ
- **Vị trí mới đề xuất:** `components/sx_services/sx_device_identity.c/.h`
- **API tối thiểu:**
  ```c
  esp_err_t sx_device_identity_build_json(char *out, size_t out_len);
  ```

### Mức độ tái sử dụng
- **B — tách nhẹ**

---

## 5) Pattern: Activation workflow (HTTP 202 → waiting)

### Mô tả
Server trả HTTP 202 để báo “đang chờ user input”, client chuyển sang UI flow.

### Ví dụ trong code
- **Path:** `components/sx_services/sx_ota_full.cpp`
- **Function:** `httpPostJson()` (status == 202 → `ESP_ERR_TIMEOUT`)

### Vì sao tốt cho OS
- Có thể áp dụng cho:
  - device pairing
  - login/OTP
  - permissions

### Đề xuất
- Chuẩn hóa result struct thay vì ép vào `ESP_ERR_TIMEOUT`.

### Mức độ tái sử dụng
- **C — pattern-only**

---

## Kết luận

Top pattern nên chuẩn hóa toàn OS (ưu tiên):
1) HTTP JSON Client Wrapper
2) JSON → Settings Mapping
3) Service → UI bằng Dispatcher Events
4) System Info Payload builder
5) Activation workflow (202 waiting)







