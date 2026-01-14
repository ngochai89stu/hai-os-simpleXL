# API Catalog: sx_services – Storage Part 2: SD Service (FAT Filesystem)

## Tổng quan

`sx_sd_service` cung cấp SD card mounting và file operations sử dụng FAT filesystem (ESP-IDF `esp_vfs_fat`). Service:
- Mount SD card qua SPI (shared với LCD)
- Cung cấp file I/O helpers (read, get size, list files)
- Quản lý SPI bus lock để share với LCD
- Unmount SD card khi stop

---

## 1. sx_sd_service.h / sx_sd_service.c

### A) Vai Trò File

**sx_sd_service** là SD card service. File này:
- Mount SD card qua SPI mode (ESP-IDF `esp_vfs_fat_sdspi_mount`)
- Cung cấp file I/O helpers (read, get size, list files)
- Quản lý SPI bus lock để share với LCD (SPI3_HOST)
- Unmount SD card khi stop

**Dependencies trực tiếp:**
```c
// sx_sd_service.c:1-17
#include "sx_sd_service.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "sdmmc_cmd.h"
#include "sx_spi_bus_manager.h"
```

### B) Public API

```c
// sx_sd_service.h:24-42
esp_err_t sx_sd_service_init(const sx_sd_config_t *cfg);
esp_err_t sx_sd_service_start(void);
esp_err_t sx_sd_service_stop(void);
bool sx_sd_is_mounted(void);
const char *sx_sd_get_mount_point(void);
esp_err_t sx_sd_read_file(const char *path, void *out_buf, size_t buf_size, size_t *out_read);
esp_err_t sx_sd_get_file_size(const char *path, size_t *out_size);
esp_err_t sx_sd_list_files(const char *dir_path, sx_sd_file_entry_t *entries, size_t max_count, size_t *out_count);
```

**Contract:**

**`sx_sd_service_init()`**
- **Input**: `cfg` (SD config: mount_point, SPI host, GPIO pins)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: SPI bus đã được init (bởi LCD)
- **Post-conditions**: Config đã được stored, service ready
- **Error model**: 
  - `ESP_OK`: Thành công hoặc đã init (idempotent)
  - `ESP_ERR_INVALID_ARG`: cfg NULL hoặc mount_point NULL

**`sx_sd_service_start()`**
- **Input**: Không có
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Service đã được init
- **Post-conditions**: SD card đã được mounted tại mount_point
- **Error model**: 
  - `ESP_OK`: Thành công hoặc đã mounted
  - `ESP_FAIL`: Mount failed (card missing, damaged, incompatible)
  - `ESP_ERR_*`: SPI/GPIO initialization failed

**`sx_sd_read_file()`**
- **Input**: `path` (file path), `out_buf` (output buffer), `buf_size` (buffer size), `out_read` (output read count)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: SD card đã được mounted
- **Post-conditions**: File data đã được read vào buffer
- **Error model**: 
  - `ESP_ERR_INVALID_STATE`: SD card not mounted
  - `ESP_ERR_INVALID_ARG`: path/out_buf NULL
  - `ESP_FAIL`: File không mở được hoặc read failed

### C) Data Model

**Static State** (```21:24:components/sx_services/sx_sd_service.c```):
- `s_initialized`: Init flag
- `s_mounted`: Mount flag
- `s_cfg`: SD config (mount_point, SPI host, GPIO pins)
- `s_card`: SD card handle (from `esp_vfs_fat_sdspi_mount`)

**Data Structures** (```15:22:37:41:components/sx_services/sx_sd_service.h```):
```c
typedef struct {
    const char *mount_point; // default: /sdcard
    int spi_host;            // SPI host id (e.g., SPI3_HOST)
    int miso_gpio;
    int mosi_gpio;
    int sclk_gpio;
    int cs_gpio;
} sx_sd_config_t;

typedef struct {
    char name[64];
    bool is_dir;
    size_t size;
} sx_sd_file_entry_t;
```

**Invariants:**
- Mount point: Default "/sdcard" (```13:13:components/sx_services/sx_sd_service.h```)
- SPI bus: Shared với LCD (SPI3_HOST)
- CS pin: Khác với LCD CS (SD CS = 10, LCD CS = 41)

### D) Concurrency

- **Context**: 
  - **Init/Start/Stop**: Chạy từ bootstrap (main task, single-threaded boot)
  - **File operations**: Có thể được gọi từ bất kỳ task nào
- **Thread Safety**: 
  - **SPI bus**: Protected bởi `sx_spi_bus_lock()` trong file operations (```161:170:187:198:components/sx_services/sx_sd_service.c```)
  - **Mount/Unmount**: Protected bởi SPI bus lock (```84:94:components/sx_services/sx_sd_service.c```)
  - **⚠️ RISK**: Nếu nhiều tasks gọi file operations đồng thời, có thể block lâu (SPI bus lock là blocking)

### E) Memory Ownership

- **SD card handle**: 
  - **Owner**: ESP-IDF owns (returned từ `esp_vfs_fat_sdspi_mount`)
  - **Lifetime**: Valid từ mount đến unmount
  - **Cleanup**: Unmount trong `sx_sd_service_stop()` (```126:129:components/sx_services/sx_sd_service.c```)

- **File buffers**: 
  - **Owner**: Caller owns output buffer
  - **Lifetime**: Valid trong suốt `sx_sd_read_file()` call
  - **Usage**: Data được read vào caller's buffer

### F) Side Effects

1. **SPI Bus**: Acquire SPI bus lock trước khi mount/read (```84:94:161:170:187:198:components/sx_services/sx_sd_service.c```)
2. **GPIO**: Configure CS pin as output (```58:61:components/sx_services/sx_sd_service.c```)
3. **FAT Filesystem**: Mount SD card tại mount_point (```92:92:components/sx_services/sx_sd_service.c```)
4. **Assets Service**: Notify assets service khi SD mounted (```102:104:components/sx_assets/sx_assets.c``` - `sx_assets_set_sd_ready()`)

### G) Call Sites

1. **sx_bootstrap_start()** - Init và start SD service (```366:371:components/sx_core/sx_bootstrap.c```)
2. **Audio service** - Read audio files từ SD card
3. **Assets service** - Load RGB565 assets từ SD card
4. **Playlist manager** - List music files từ SD card

### H) Issues/Risks

1. **P1 - SPI Bus Blocking**: File operations acquire SPI bus lock (blocking) → có thể block LCD operations lâu.
   - **Điều kiện**: Read large file trong khi LCD đang update
   - **Cách tái hiện**: Read 10MB file, LCD update bị block
   - **Impact**: LCD update bị block, UI freeze

2. **P1 - No Error Recovery**: Nếu mount failed, service không retry → SD card có thể không available.
   - **Điều kiện**: SD card missing hoặc damaged
   - **Cách tái hiện**: Start service với SD card missing
   - **Impact**: Service start failed, SD card không available

3. **P2 - Path Buffer Overflow**: `make_full_path()` dùng fixed buffer 256 bytes (```159:159:components/sx_services/sx_sd_service.c```), có thể overflow nếu path dài.
   - **Điều kiện**: Path length > 255 characters
   - **Cách tái hiện**: `sx_sd_read_file("very/long/path/.../file.bin", ...)`
   - **Impact**: Path bị truncate, file không tìm thấy

4. **P2 - No File Locking**: Không có file locking mechanism → nếu nhiều tasks read/write cùng file, có thể race condition.
   - **Điều kiện**: Nhiều tasks read/write cùng file
   - **Cách tái hiện**: Task A read file, Task B write file cùng lúc
   - **Impact**: Data corruption hoặc read invalid data

### I) Đề Xuất Cải Thiện

1. **P1**: Thêm timeout cho SPI bus lock (ví dụ: 100ms) để tránh block lâu
2. **P1**: Thêm retry logic cho mount operations
3. **P2**: Validate path length hoặc dùng dynamic buffer
4. **P2**: Thêm file locking mechanism cho concurrent access

---

## Tổng Kết Phần 2

### Điểm Mạnh

1. **SPI Bus Sharing**: Share SPI bus với LCD (tiết kiệm GPIO)
2. **Thread Safety**: File operations protected bởi SPI bus lock
3. **Simple API**: API đơn giản, dễ sử dụng

### Điểm Yếu

1. **SPI Bus Blocking**: File operations block LCD operations
2. **No Error Recovery**: Không có retry logic cho mount
3. **Path Buffer**: Fixed buffer có thể overflow

### Đề Xuất Cải Thiện Tổng Thể

1. **P1**: Fix SPI bus blocking với timeout
2. **P1**: Add retry logic cho mount
3. **P2**: Fix path buffer overflow

---

**Tiếp theo**: Phần 3 sẽ phân tích **sx_assets** (asset loader).
