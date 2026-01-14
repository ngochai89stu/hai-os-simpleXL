# API Catalog: sx_services – Storage Part 3: Assets Loader

## Tổng quan

`sx_assets` cung cấp asset loading từ SD card (RGB565 images). Service:
- Load RGB565 assets từ SD card
- Quản lý asset handles và memory
- Expose embedded images (bootscreen, flashscreen)
- Tuân theo SIMPLEXL_ARCH v1.3 (không include LVGL)

---

## 1. sx_assets.h / sx_assets.c

### A) Vai Trò File

**sx_assets** là asset loader service. File này:
- Load RGB565 assets từ SD card (stub implementation)
- Quản lý asset handles và memory
- Expose embedded images (bootscreen, flashscreen) qua raw data structure
- Tuân theo SIMPLEXL_ARCH v1.3 - không include LVGL (sx_ui sẽ wrap thành lv_img_dsc_t)

**Dependencies trực tiếp:**
```c
// sx_assets.c:1-10
#include "sx_assets.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "driver/gpio.h"
```

### B) Public API

```c
// sx_assets.h:36-62
esp_err_t sx_assets_init(void);
sx_asset_handle_t sx_assets_load_rgb565(const char *path, sx_asset_info_t *info);
const uint16_t* sx_assets_get_data(sx_asset_handle_t handle);
void sx_assets_free(sx_asset_handle_t handle);
bool sx_assets_sd_ready(void);
void sx_assets_set_sd_ready(bool ready);
const sx_embedded_img_data_t* sx_assets_get_bootscreen_data(void);
const sx_embedded_img_data_t* sx_assets_get_flashscreen_data(void);
```

**Contract:**

**`sx_assets_init()`**
- **Input**: Không có
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: SD service đã được init (mount handled bởi sx_sd_service)
- **Post-conditions**: Service ready (stub implementation)
- **Error model**: 
  - `ESP_OK`: Thành công hoặc đã init (idempotent)

**`sx_assets_load_rgb565()`**
- **Input**: `path` (asset path từ SD root), `info` (output asset info)
- **Output**: Asset handle nếu thành công, NULL nếu failed
- **Pre-conditions**: SD card đã được mounted
- **Post-conditions**: Asset đã được load vào memory (stub: return NULL)
- **Error model**: 
  - `NULL`: SD card not mounted, path invalid, hoặc load failed
  - **Note**: Hiện tại là stub implementation (```49:75:components/sx_assets/sx_assets.c```)

**`sx_assets_get_bootscreen_data()`**
- **Input**: Không có
- **Output**: Raw embedded image data structure
- **Pre-conditions**: Service đã được init
- **Post-conditions**: Return embedded bootscreen data (stub: data = NULL)
- **Error model**: 
  - **Note**: Stub implementation, actual data access từ sx_ui component (```112:124:components/sx_assets/sx_assets.c```)

### C) Data Model

**Static State** (```22:22:components/sx_assets/sx_assets.c```):
- `s_sd_mounted`: SD mount flag (set bởi `sx_assets_set_sd_ready()`)

**Data Structures** (```17:35:components/sx_assets/sx_assets.h```):
```c
typedef struct sx_asset* sx_asset_handle_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t size_bytes;  // Size in bytes (width * height * 2 for RGB565)
} sx_asset_info_t;

typedef struct {
    const uint8_t *data;      // Raw pixel data (RGB565)
    uint16_t width;
    uint16_t height;
    uint32_t data_size;       // Size in bytes
    uint8_t color_format;    // Format code (LV_COLOR_FORMAT_RGB565 = 0x0B)
} sx_embedded_img_data_t;

struct sx_asset {
    uint16_t *data;
    sx_asset_info_t info;
};
```

**Invariants:**
- Asset handle: Opaque pointer, valid từ load đến free
- RGB565 format: 2 bytes per pixel
- Embedded images: Access từ sx_ui component (không thể access từ sx_assets vì generated files include lvgl.h)

### D) Concurrency

- **Context**: 
  - **Init**: Chạy từ bootstrap (main task, single-threaded boot)
  - **Load/Free operations**: Có thể được gọi từ bất kỳ task nào (UI task)
- **Thread Safety**: 
  - **SD mount flag**: Không được protect bởi mutex (race condition risk)
  - **Asset handles**: Không được protect (stub implementation)
  - **⚠️ RISK**: Nếu nhiều tasks load/free assets đồng thời, có thể race condition

### E) Memory Ownership

- **Asset handles**: 
  - **Owner**: Caller owns asset handle
  - **Lifetime**: Valid từ load đến free
  - **Cleanup**: Free trong `sx_assets_free()` (```85:94:components/sx_assets/sx_assets.c```)

- **Asset data**: 
  - **Owner**: Asset handle owns (malloc trong load)
  - **Lifetime**: Valid trong suốt lifetime của handle
  - **Cleanup**: Free trong `sx_assets_free()` (```90:92:components/sx_assets/sx_assets.c```)

### F) Side Effects

1. **SD Card**: Read asset files từ SD card (stub: not implemented)
2. **Memory**: Allocate memory cho asset data (stub: not implemented)
3. **SD Service Integration**: SD service gọi `sx_assets_set_sd_ready()` khi mount (```102:104:components/sx_assets/sx_assets.c```)

### G) Call Sites

1. **sx_bootstrap_start()** - Init assets service (```372:376:components/sx_core/sx_bootstrap.c```)
2. **SD service** - Set SD ready state (```sx_assets_set_sd_ready(true)```)
3. **UI screens** - Load assets cho display (stub: not implemented)

### H) Issues/Risks

1. **P0 - Stub Implementation**: `sx_assets_load_rgb565()` là stub (return NULL) → assets không thể load từ SD card.
   - **Điều kiện**: Gọi `sx_assets_load_rgb565()` với valid path
   - **Cách tái hiện**: Load asset từ SD card
   - **Impact**: Asset không được load, UI không hiển thị assets

2. **P1 - No Thread Safety**: SD mount flag và asset handles không được protect bởi mutex.
   - **Điều kiện**: Nhiều tasks load/free assets đồng thời
   - **Cách tái hiện**: Load assets từ nhiều tasks
   - **Impact**: Race condition, có thể crash hoặc memory leak

3. **P2 - Embedded Images Stub**: `sx_assets_get_bootscreen_data()` return stub (data = NULL) → embedded images không accessible từ sx_assets.
   - **Điều kiện**: Gọi `sx_assets_get_bootscreen_data()`
   - **Cách tái hiện**: Get bootscreen data
   - **Impact**: Embedded images không accessible (phải access từ sx_ui component)

### I) Đề Xuất Cải Thiện

1. **P0**: Implement `sx_assets_load_rgb565()` để load assets từ SD card
2. **P1**: Thêm mutex để protect SD mount flag và asset handles
3. **P2**: Document rõ embedded images access pattern (sx_ui component)

---

## Tổng Kết Phần 3

### Điểm Mạnh

1. **Architecture Compliance**: Tuân theo SIMPLEXL_ARCH v1.3 (không include LVGL)
2. **SD Integration**: Tích hợp với SD service

### Điểm Yếu

1. **Stub Implementation**: Load function chưa được implement
2. **No Thread Safety**: Không có mutex protection
3. **Embedded Images**: Stub implementation

### Đề Xuất Cải Thiện Tổng Thể

1. **P0**: Implement asset loading từ SD card
2. **P1**: Add mutex protection
3. **P2**: Document embedded images access

---

**Tiếp theo**: Phần 4 sẽ phân tích **sx_playlist_manager + sx_media_metadata** (playlist và metadata parsing).
