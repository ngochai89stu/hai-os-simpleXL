# API Catalog: sx_services – Storage Part 1: Settings Service (NVS)

## Tổng quan

`sx_settings_service` cung cấp persistent configuration storage sử dụng ESP-IDF NVS (Non-Volatile Storage). Service:
- Wrap NVS APIs với namespace "sx_settings"
- Hỗ trợ string, int, bool, blob
- Cung cấp get với default value
- Commit changes để flush vào flash

---

## 1. sx_settings_service.h / sx_settings_service.c

### A) Vai Trò File

**sx_settings_service** là persistent settings storage service. File này:
- Wrap ESP-IDF NVS APIs với namespace "sx_settings"
- Cung cấp type-safe APIs cho string/int/bool/blob
- Hỗ trợ default values cho get operations
- Quản lý NVS handle lifecycle

**Dependencies trực tiếp:**
```c
// sx_settings_service.c:1-6
#include "sx_settings_service.h"
#include "nvs_flash.h"
#include "nvs.h"
```

### B) Public API

```c
// sx_settings_service.h:14-43
esp_err_t sx_settings_service_init(void);
esp_err_t sx_settings_set_string(const char *key, const char *value);
esp_err_t sx_settings_get_string(const char *key, char *value, size_t max_len);
esp_err_t sx_settings_get_string_default(const char *key, char *value, size_t max_len, const char *default_value);
esp_err_t sx_settings_set_int(const char *key, int32_t value);
esp_err_t sx_settings_get_int(const char *key, int32_t *value);
esp_err_t sx_settings_get_int_default(const char *key, int32_t *value, int32_t default_value);
esp_err_t sx_settings_set_bool(const char *key, bool value);
esp_err_t sx_settings_get_bool(const char *key, bool *value);
esp_err_t sx_settings_get_bool_default(const char *key, bool *value, bool default_value);
esp_err_t sx_settings_set_blob(const char *key, const void *value, size_t len);
esp_err_t sx_settings_get_blob(const char *key, void *value, size_t *len);
esp_err_t sx_settings_get_blob_size(const char *key, size_t *len);
esp_err_t sx_settings_delete(const char *key);
esp_err_t sx_settings_commit(void);
esp_err_t sx_settings_erase_all(void);
```

**Contract:**

**`sx_settings_service_init()`**
- **Input**: Không có
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: NVS flash đã được init (`nvs_flash_init()`)
- **Post-conditions**: NVS handle đã được mở, service ready
- **Error model**: 
  - `ESP_OK`: Thành công hoặc đã init (idempotent)
  - `ESP_ERR_*`: NVS open failed (```20:24:components/sx_services/sx_settings_service.c```)

**`sx_settings_set_string()`**
- **Input**: `key` (setting key), `value` (string value)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Service đã được init
- **Post-conditions**: Setting đã được set (chưa commit)
- **Error model**: 
  - `ESP_ERR_INVALID_ARG`: Chưa init, key/value NULL
  - `ESP_ERR_*`: NVS set failed

**`sx_settings_commit()`**
- **Input**: Không có
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Service đã được init
- **Post-conditions**: Changes đã được flush vào flash
- **Error model**: 
  - `ESP_OK`: Thành công
  - `ESP_ERR_*`: NVS commit failed

### C) Data Model

**Static State** (```11:12:components/sx_services/sx_settings_service.c```):
- `s_nvs_handle`: NVS handle (opened trong init)
- `s_initialized`: Init flag

**Namespace**: "sx_settings" (```9:9:components/sx_services/sx_settings_service.c```)

**Invariants:**
- NVS handle phải valid sau khi init thành công
- Changes phải được commit để persist vào flash
- Key length: NVS key max 15 characters (ESP-IDF limit)

### D) Concurrency

- **Context**: 
  - **Init**: Chạy từ bootstrap (main task, single-threaded boot)
  - **Get/Set operations**: Có thể được gọi từ bất kỳ task nào
- **Thread Safety**: 
  - **NVS operations**: ESP-IDF NVS APIs là thread-safe (internal mutex)
  - **⚠️ RISK**: Nếu nhiều tasks set/commit đồng thời, có thể race condition (ESP-IDF NVS có internal protection nhưng không guarantee atomicity cho multiple operations)

### E) Memory Ownership

- **NVS handle**: 
  - **Owner**: sx_settings_service owns (static)
  - **Lifetime**: Persistent sau khi init
  - **Cleanup**: Không có explicit cleanup (handle tồn tại trong suốt lifetime của system)

- **String values**: 
  - **Owner**: Caller owns input string (copied vào NVS)
  - **Lifetime**: Valid trong suốt `sx_settings_set_string()` call
  - **Storage**: NVS stores copy của string

- **Blob values**: 
  - **Owner**: Caller owns input blob (copied vào NVS)
  - **Lifetime**: Valid trong suốt `sx_settings_set_blob()` call
  - **Storage**: NVS stores copy của blob

### F) Side Effects

1. **NVS Flash**: Read/write operations access NVS flash partition
2. **Flash Wear**: Mỗi commit ghi vào flash → wear leveling được handle bởi ESP-IDF
3. **Logging**: Log errors qua `ESP_LOG*`

### G) Call Sites

1. **sx_bootstrap_start()** - Init settings service (```67:71:components/sx_core/sx_bootstrap.c```)
2. **EQ service** - Load/save EQ presets và band gains (```126:154:199:201:234:236:278:280:298:299:components/sx_services/sx_audio_eq.c```)
3. **WiFi service** - Load WiFi config (nếu có)
4. **STT/TTS services** - Load endpoint URLs và API keys (```165:180:48:56:components/sx_services/sx_stt_service.c```)
5. **Wake word service** - Load model path và threshold (```99:111:components/sx_services/sx_wake_word_service.c```)

### H) Issues/Risks

1. **P1 - No Transaction Support**: Không có transaction support → nếu set nhiều settings và commit fail, một số settings có thể đã được set.
   - **Điều kiện**: Set nhiều settings, sau đó commit fail
   - **Cách tái hiện**: Set 10 settings, sau đó commit fail (flash full)
   - **Impact**: Một số settings đã được set, một số chưa → inconsistent state

2. **P1 - Key Length Limit**: NVS key max 15 characters (ESP-IDF limit), không có validation.
   - **Điều kiện**: Set key với length > 15
   - **Cách tái hiện**: `sx_settings_set_string("very_long_key_name_that_exceeds_limit", "value")`
   - **Impact**: Key bị truncate hoặc error, setting không được lưu

3. **P2 - No Type Validation**: Không validate type khi get → có thể get int với get_string (sẽ fail nhưng không có clear error).
   - **Điều kiện**: Set int, sau đó get với get_string
   - **Cách tái hiện**: `sx_settings_set_int("key", 123)`, sau đó `sx_settings_get_string("key", ...)`
   - **Impact**: Get fail với unclear error message

4. **P2 - Blob Size Mismatch**: `sx_settings_get_blob()` yêu cầu caller provide buffer size, nếu size không đủ sẽ return `ESP_ERR_INVALID_SIZE` nhưng không clear.
   - **Điều kiện**: Get blob với buffer size nhỏ hơn actual size
   - **Cách tái hiện**: `sx_settings_get_blob("key", buffer, &size)` với size = 10, actual size = 100
   - **Impact**: Return `ESP_ERR_INVALID_SIZE`, caller phải call `get_blob_size()` trước

### I) Đề Xuất Cải Thiện

1. **P1**: Thêm transaction support (begin/commit/rollback)
2. **P1**: Validate key length trong set operations
3. **P2**: Thêm type metadata để validate type khi get
4. **P2**: Improve error messages cho blob size mismatch

---

## Tổng Kết Phần 1

### Điểm Mạnh

1. **Simple API**: API đơn giản, dễ sử dụng
2. **Type Safety**: Type-safe APIs cho string/int/bool/blob
3. **Default Values**: Hỗ trợ default values cho get operations
4. **Thread Safety**: ESP-IDF NVS có internal thread safety

### Điểm Yếu

1. **No Transaction**: Không có transaction support
2. **Key Validation**: Không validate key length
3. **Type Validation**: Không validate type khi get

### Đề Xuất Cải Thiện Tổng Thể

1. **P1**: Add transaction support
2. **P1**: Validate key length
3. **P2**: Add type metadata

---

**Tiếp theo**: Phần 2 sẽ phân tích **sx_sd_service** (SD card + FAT filesystem).
