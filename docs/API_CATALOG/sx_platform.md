# API Catalog: sx_platform

Component **sx_platform** cung cấp hardware abstraction layer (HAL) cho display, touch, SPI bus, và volume control.

## Tổng Quan Component

**sx_platform** cung cấp:
- **Display Initialization**: LCD panel initialization với nhiều driver support (ST7796, ST7789, ILI9341)
- **Touch Initialization**: Capacitive touch panel (FT5x06) qua I2C
- **SPI Bus Manager**: Mutex-based locking cho shared SPI bus (LCD + SD card)
- **Volume Control**: Hardware volume control cho codec chips (ES8388, ES8311) hoặc software fallback

---

## 1. sx_platform.h / sx_platform.c

### A) Vai Trò File

**sx_platform** là HAL chính cho display và touch hardware. File này:
- Khởi tạo LCD panel với nhiều driver support (ST7796, ST7789, ILI9341)
- Khởi tạo touch controller (FT5x06) qua I2C
- Quản lý backlight PWM (LEDC)
- Cung cấp brightness control API

**Dependencies trực tiếp:**
```c
// sx_platform.c:1-11
#include "sx_platform.h"
#include <esp_log.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7796.h"
#include "esp_lcd_touch_ft5x06.h"
```

### B) Public API

```c
// sx_platform.h:24-38
sx_display_handles_t sx_platform_display_init(void);
esp_err_t sx_platform_touch_init(sx_touch_handles_t *touch_handles);
esp_err_t sx_platform_set_brightness(uint8_t percent);
uint8_t sx_platform_get_brightness(void);
esp_err_t sx_platform_get_screen_size(uint16_t *width, uint16_t *height);
```

**Contract:**

**`sx_platform_display_init()`**
- **Input**: Không có
- **Output**: `sx_display_handles_t` với `panel_handle` và `io_handle` (NULL nếu failed)
- **Pre-conditions**: GPIO pins đã được config đúng trong Kconfig
- **Post-conditions**: LCD panel đã được init và sẵn sàng sử dụng
- **Error model**: 
  - Return handles với `panel_handle == NULL` nếu failed
  - SPI bus được init nhưng có thể leak nếu panel IO creation failed (```192:196:components/sx_platform/sx_platform.c```)

**`sx_platform_touch_init()`**
- **Input**: `touch_handles` pointer (output)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Touch enabled trong Kconfig (`CONFIG_HAI_TOUCH_ENABLE`)
- **Post-conditions**: Touch controller đã được init và sẵn sàng sử dụng
- **Error model**: 
  - `ESP_ERR_INVALID_ARG`: touch_handles là NULL
  - `ESP_OK`: Touch disabled trong Kconfig (no-op)
  - I2C bus được cleanup nếu touch init failed (```453:456:components/sx_platform/sx_platform.c```)

**`sx_platform_set_brightness()`**
- **Input**: `percent` (0-100)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Backlight đã được init (hoặc fallback GPIO)
- **Post-conditions**: Brightness đã được set
- **Error model**: 
  - `ESP_OK`: Thành công hoặc fallback GPIO
  - `ESP_ERR_*`: LEDC error (nếu PWM init)

**`sx_platform_get_brightness()`**
- **Input**: Không có
- **Output**: Current brightness (0-100)
- **Pre-conditions**: Không có
- **Post-conditions**: Không có
- **Error model**: Luôn return value (0-100)

**`sx_platform_get_screen_size()`**
- **Input**: `width`, `height` pointers (output)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Không có
- **Post-conditions**: Screen size đã được set
- **Error model**: `ESP_ERR_INVALID_ARG` nếu width/height là NULL

### C) Data Model

```c
// sx_platform.h:13-16
typedef struct {
    esp_lcd_panel_io_handle_t io_handle;
    esp_lcd_panel_handle_t panel_handle;
} sx_display_handles_t;
```

```c
// sx_platform.h:19-21
typedef struct {
    esp_lcd_touch_handle_t touch_handle;
} sx_touch_handles_t;
```

**Static State** (```108:109:components/sx_platform/sx_platform.c```):
- `s_backlight_initialized`: Backlight init flag
- `s_current_brightness`: Current brightness (0-100)
- `s_touch_i2c_bus_handle`: I2C bus handle cho touch (```368:368:components/sx_platform/sx_platform.c```)

**Invariants:**
- Display handles: `panel_handle != NULL` nếu init thành công
- Touch handles: `touch_handle != NULL` nếu init thành công và enabled
- Brightness: 0-100 range, clamped nếu > 100

### D) Concurrency

- **Context**: 
  - **Init functions**: Chạy từ `sx_bootstrap_start()` (main task, single-threaded boot)
  - **Brightness control**: Có thể được gọi từ bất kỳ task nào
- **Thread Safety**: 
  - **Init functions**: Không thread-safe (chỉ được gọi một lần tại boot)
  - **Brightness control**: Thread-safe (LEDC driver là thread-safe, static state được protect bởi LEDC)
- **Blocking**: 
  - Display init: ~500ms (panel reset, init commands với delays)
  - Touch init: ~100ms (I2C bus init, touch reset)

### E) Memory Ownership

- **Display handles**: 
  - **Owner**: Caller owns handles struct
  - **Lifetime**: Valid trong suốt lifetime của panel (managed bởi ESP-IDF)
  - **Cleanup**: ESP-IDF tự cleanup khi deinit (không có explicit deinit API)

- **Touch handles**: 
  - **Owner**: Caller owns handles struct
  - **Lifetime**: Valid trong suốt lifetime của touch controller
  - **Cleanup**: ESP-IDF tự cleanup khi deinit (không có explicit deinit API)

- **I2C bus handle**: 
  - **Owner**: sx_platform owns (static)
  - **Lifetime**: Persistent sau khi init
  - **Cleanup**: Cleanup khi touch init failed (```453:456:components/sx_platform/sx_platform.c```)

### F) Side Effects

1. **SPI Bus**: Initialize SPI3 bus cho LCD (```176:176:components/sx_platform/sx_platform.c```)
   - **Shared với SD card**: MISO pin (12) được share (```171:171:components/sx_platform/sx_platform.c```)
   - **Max transfer size**: `LCD_H_RES * LCD_V_RES * sizeof(uint16_t)` (```174:174:components/sx_platform/sx_platform.c```)

2. **I2C Bus**: Initialize I2C bus 1 cho touch controller (```391:415:components/sx_platform/sx_platform.c```)
   - **Port**: I2C_NUM_1 (```74:74:components/sx_platform/sx_platform.c```)
   - **Frequency**: 400kHz (```75:75:components/sx_platform/sx_platform.c```)
   - **GPIO**: SDA=8, SCL=11 (```70:71:components/sx_platform/sx_platform.c```)

3. **LEDC (PWM)**: Initialize LEDC timer và channel cho backlight (```139:157:components/sx_platform/sx_platform.c```)
   - **Timer**: LEDC_TIMER_0, 13-bit resolution, 5kHz frequency
   - **Channel**: LEDC_CHANNEL_0
   - **GPIO**: LCD_PIN_NUM_BK_LIGHT (từ Kconfig)

4. **GPIO**: Configure GPIO pins cho LCD (CS, DC, RST) và touch (INT, RST)

### G) Call Sites

1. **sx_bootstrap_start()** - Init display (```138:143:components/sx_core/sx_bootstrap.c```)
2. **sx_bootstrap_start()** - Init touch (```157:174:components/sx_core/sx_bootstrap.c```)
3. **sx_bootstrap_start()** - Set brightness từ settings (```230:240:components/sx_core/sx_bootstrap.c```)
4. **sx_ui_start()** - Pass display/touch handles to UI (từ UI component)
5. **Settings service** - Set brightness khi user thay đổi (từ settings screen)

### H) Issues/Risks

1. **P0 - SPI Bus Leak**: Nếu panel IO creation failed, SPI bus vẫn được init nhưng không được cleanup (```192:196:components/sx_platform/sx_platform.c```).
   - **Điều kiện**: `esp_lcd_new_panel_io_spi()` failed
   - **Cách tái hiện**: Invalid SPI config hoặc hardware issue
   - **Impact**: SPI bus resource leak, có thể ảnh hưởng đến SD card

2. **P1 - Touch I2C Bus Leak**: Nếu touch init failed sau khi I2C bus đã được init, I2C bus được cleanup (```453:456:components/sx_platform/sx_platform.c```) nhưng nếu touch IO creation failed trước đó, I2C bus có thể leak.
   - **Điều kiện**: `esp_lcd_new_panel_io_i2c()` failed sau khi I2C bus init
   - **Cách tái hiện**: Invalid I2C config hoặc hardware issue
   - **Impact**: I2C bus resource leak

3. **P1 - Shared SPI Bus**: LCD và SD card share SPI bus (SPI3), cần mutex để sync access.
   - **Điều kiện**: LCD và SD card access đồng thời
   - **Cách tái hiện**: Access LCD và SD card từ different tasks
   - **Impact**: SPI bus conflict, data corruption

4. **P2 - Brightness Fallback**: Nếu PWM init failed, fallback về GPIO on/off (```318:327:components/sx_platform/sx_platform.c```).
   - **Điều kiện**: LEDC init failed
   - **Cách tái hiện**: LEDC resource conflict
   - **Impact**: Brightness chỉ có on/off, không có smooth control

5. **P2 - LCD Driver Auto-Detection**: Custom LCD mode dùng heuristic để detect driver (```247:277:components/sx_platform/sx_platform.c```), có thể không chính xác.
   - **Điều kiện**: Custom LCD với resolution không match heuristic
   - **Cách tái hiện**: Custom LCD với resolution lạ
   - **Impact**: Wrong driver được chọn, display không hoạt động đúng

### I) Đề Xuất Cải Thiện

1. **P0**: Cleanup SPI bus nếu panel IO creation failed
2. **P1**: Đảm bảo I2C bus cleanup trong mọi error path
3. **P1**: Document rõ SPI bus sharing và mutex usage
4. **P2**: Thêm error handling cho brightness fallback
5. **P2**: Thêm explicit LCD driver selection cho custom LCD mode

---

## 2. sx_spi_bus_manager.h / sx_spi_bus_manager.c

### A) Vai Trò File

**sx_spi_bus_manager** cung cấp mutex-based locking cho shared SPI bus. LCD và SD card share SPI3 bus, cần sync access để tránh conflict.

**Dependencies trực tiếp:**
```c
// sx_spi_bus_manager.c:1-5
#include "sx_spi_bus_manager.h"
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
```

### B) Public API

```c
// sx_spi_bus_manager.h:17-35
esp_err_t sx_spi_bus_manager_init(void);
void sx_spi_bus_lock(void);
void sx_spi_bus_unlock(void);
```

**Contract:**

**`sx_spi_bus_manager_init()`**
- **Input**: Không có
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: FreeRTOS đã được khởi tạo
- **Post-conditions**: SPI bus mutex đã được tạo
- **Error model**: 
  - `ESP_OK`: Thành công hoặc đã init (idempotent)
  - `ESP_ERR_NO_MEM`: Mutex creation failed

**`sx_spi_bus_lock()`**
- **Input**: Không có
- **Output**: Không có (void)
- **Pre-conditions**: SPI bus manager đã được init (hoặc tự init nếu chưa)
- **Post-conditions**: SPI bus đã được lock (exclusive access)
- **Error model**: 
  - Blocking call (portMAX_DELAY) - sẽ block cho đến khi có lock
  - Log error nếu mutex take failed (```37:39:components/sx_platform/sx_spi_bus_manager.c```)

**`sx_spi_bus_unlock()`**
- **Input**: Không có
- **Output**: Không có (void)
- **Pre-conditions**: SPI bus đã được lock
- **Post-conditions**: SPI bus đã được unlock
- **Error model**: 
  - Log warning nếu chưa init (```43:46:components/sx_platform/sx_spi_bus_manager.c```)
  - Log error nếu mutex give failed (```48:50:components/sx_platform/sx_spi_bus_manager.c```)

### C) Data Model

**Static State** (```9:10:components/sx_platform/sx_spi_bus_manager.c```):
- `s_spi_bus_mutex`: FreeRTOS mutex handle
- `s_initialized`: Init flag

**Invariants:**
- Mutex chỉ được tạo một lần (idempotent init)
- Lock/unlock phải được paired (caller responsibility)

### D) Concurrency

- **Context**: Bất kỳ task nào có thể lock/unlock SPI bus
- **Thread Safety**: 
  - Mutex-based locking - thread-safe
  - Blocking lock (portMAX_DELAY) - sẽ block cho đến khi có lock
- **Lock Order**: 
  - Chỉ có một mutex - không có lock order issue
  - **⚠️ RISK**: Nếu lock được hold lâu, có thể block other tasks

### E) Memory Ownership

- **Mutex**: 
  - **Owner**: sx_spi_bus_manager owns mutex
  - **Lifetime**: Persistent sau khi init
  - **Cleanup**: Không có explicit cleanup (mutex tồn tại trong suốt lifetime của system)

### F) Side Effects

1. **FreeRTOS**: Tạo mutex để sync SPI bus access
2. **Blocking**: Lock operation có thể block tasks nếu SPI bus đang được sử dụng

### G) Call Sites

1. **sx_bootstrap_start()** - Init SPI bus manager (```184:189:components/sx_core/sx_bootstrap.c```)
2. **SD card service** - Lock/unlock SPI bus khi access SD card (từ sx_sd_service.c)
3. **LCD operations** - Lock/unlock SPI bus khi access LCD (nếu cần, từ UI task)

### H) Issues/Risks

1. **P0 - Deadlock Risk**: Nếu một task lock SPI bus và bị block (ví dụ: wait for event), other tasks sẽ bị block indefinitely.
   - **Điều kiện**: Task lock SPI bus và block trong critical section
   - **Cách tái hiện**: Lock SPI bus, wait for event/queue trong critical section
   - **Impact**: Deadlock, system hang

2. **P1 - Lock/Unlock Mismatch**: Nếu caller không unlock sau lock, SPI bus sẽ bị lock forever.
   - **Điều kiện**: Missing unlock call hoặc early return
   - **Cách tái hiện**: Lock SPI bus, return early without unlock
   - **Impact**: SPI bus locked forever, other tasks blocked

3. **P2 - No Timeout**: Lock operation không có timeout, sẽ block indefinitely.
   - **Điều kiện**: SPI bus bị lock bởi hung task
   - **Cách tái hiện**: Task lock SPI bus và hang
   - **Impact**: System hang, không có recovery

### I) Đề Xuất Cải Thiện

1. **P0**: Thêm timeout cho lock operation (ví dụ: 5 giây)
2. **P1**: Thêm RAII pattern hoặc guard class để đảm bảo unlock
3. **P2**: Thêm watchdog để detect hung locks
4. **P2**: Document rõ lock/unlock pairing requirements

---

## 3. sx_platform_volume.h / sx_platform_volume.c

### A) Vai Trò File

**sx_platform_volume** cung cấp hardware volume control cho codec chips (ES8388, ES8311) hoặc software fallback (PCM5102A).

**Dependencies trực tiếp:**
```c
// sx_platform_volume.c:1-6
#include "sx_platform_volume.h"
#include <esp_log.h>
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
```

### B) Public API

```c
// sx_platform_volume.h:17-29
sx_hw_codec_chip_t sx_platform_detect_codec(void);
esp_err_t sx_platform_hw_volume_init(void);
esp_err_t sx_platform_hw_volume_set(uint8_t volume);
uint8_t sx_platform_hw_volume_get(void);
bool sx_platform_hw_volume_available(void);
```

**Contract:**

**`sx_platform_detect_codec()`**
- **Input**: Không có
- **Output**: Detected codec chip type
- **Pre-conditions**: I2C bus available (hoặc disabled)
- **Post-conditions**: Codec type đã được detect và cached
- **Error model**: 
  - Return `SX_HW_CODEC_PCM5102A` nếu I2C disabled hoặc no codec detected
  - Cached result - subsequent calls return cached value

**`sx_platform_hw_volume_init()`**
- **Input**: Không có
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Không có
- **Post-conditions**: Hardware volume đã được init (nếu available)
- **Error model**: 
  - `ESP_OK`: Thành công hoặc đã init (idempotent)
  - Software volume được dùng nếu hardware không available

**`sx_platform_hw_volume_set()`**
- **Input**: `volume` (0-100)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Hardware volume đã được init và available
- **Post-conditions**: Hardware volume đã được set
- **Error model**: 
  - `ESP_ERR_INVALID_STATE`: Chưa init hoặc hardware không available
  - `ESP_ERR_*`: I2C write error

**`sx_platform_hw_volume_get()`**
- **Input**: Không có
- **Output**: Current volume (0-100)
- **Pre-conditions**: Không có
- **Post-conditions**: Không có
- **Error model**: Luôn return value (0-100)

**`sx_platform_hw_volume_available()`**
- **Input**: Không có
- **Output**: `true` nếu hardware volume available, `false` nếu software only
- **Pre-conditions**: Không có
- **Post-conditions**: Không có
- **Error model**: Luôn return boolean

### C) Data Model

```c
// sx_platform_volume.c:12-14
static bool s_initialized = false;
static sx_hw_codec_chip_t s_detected_codec = SX_HW_CODEC_NONE;
static uint8_t s_current_volume = 50;
static bool s_hw_volume_available = false;
```

**Static State** (```11:14:components/sx_platform/sx_platform_volume.c```):
- `s_initialized`: Init flag
- `s_detected_codec`: Cached codec type
- `s_current_volume`: Current volume (0-100)
- `s_hw_volume_available`: Hardware volume availability flag
- `s_i2c_bus_handle`: I2C bus handle (```24:24:components/sx_platform/sx_platform_volume.c```)
- `s_i2c_dev_es8388`, `s_i2c_dev_es8311`: I2C device handles (```26:27:components/sx_platform/sx_platform_volume.c```)

**Invariants:**
- Codec detection chỉ chạy một lần (cached)
- Volume range: 0-100, clamped nếu > 100
- I2C disabled: GPIO 22/21 không sử dụng (```19:20:components/sx_platform/sx_platform_volume.c```)

### D) Concurrency

- **Context**: 
  - **Init/detect**: Chạy từ bootstrap hoặc audio service init
  - **Volume set/get**: Có thể được gọi từ bất kỳ task nào
- **Thread Safety**: 
  - **Static state**: Không được protect bởi mutex
  - **I2C operations**: ESP-IDF I2C driver là thread-safe
  - **⚠️ RISK**: Concurrent volume set có thể race condition

### E) Memory Ownership

- **I2C bus handle**: 
  - **Owner**: sx_platform_volume owns (static)
  - **Lifetime**: Persistent sau khi init
  - **Cleanup**: Không có explicit cleanup

- **I2C device handles**: 
  - **Owner**: sx_platform_volume owns (static)
  - **Lifetime**: Persistent sau khi init
  - **Cleanup**: Không có explicit cleanup

### F) Side Effects

1. **I2C Bus**: Initialize I2C bus 0 cho codec chips (```54:73:components/sx_platform/sx_platform_volume.c```)
   - **⚠️ DISABLED**: GPIO 22/21 không sử dụng (```19:20:components/sx_platform/sx_platform_volume.c```)
   - **Frequency**: 100kHz (```21:21:components/sx_platform/sx_platform_volume.c```)

2. **Codec Registers**: Write volume registers cho ES8388/ES8311 (```250:278:components/sx_platform/sx_platform_volume.c```)

### G) Call Sites

1. **Audio service** - Detect codec và init hardware volume (từ sx_audio_service.c)
2. **Audio service** - Set/get volume khi user thay đổi (từ audio control)
3. **Settings service** - Restore volume từ settings (từ settings screen)

### H) Issues/Risks

1. **P1 - I2C Disabled**: I2C hardware volume đã bị disable (GPIO 22/21 không sử dụng), chỉ dùng software volume.
   - **Điều kiện**: I2C pins không được config
   - **Cách tái hiện**: Check code - I2C pins = -1
   - **Impact**: Không có hardware volume control, chỉ có software volume

2. **P2 - Thread Safety**: Static state không được protect bởi mutex, concurrent volume set có thể race condition.
   - **Điều kiện**: Nhiều tasks set volume đồng thời
   - **Cách tái hiện**: Set volume từ nhiều tasks
   - **Impact**: Volume có thể không nhất quán

3. **P2 - Codec Detection Cache**: Codec detection chỉ chạy một lần, nếu codec thay đổi (hot-plug), không được detect lại.
   - **Điều kiện**: Codec thay đổi sau khi detect
   - **Cách tái hiện**: Hot-plug codec (không phổ biến)
   - **Impact**: Wrong codec type, volume control không hoạt động

### I) Đề Xuất Cải Thiện

1. **P1**: Document rõ I2C disabled và software volume fallback
2. **P2**: Thêm mutex để protect static state
3. **P2**: Thêm codec re-detection API (nếu cần)

---

## Tổng Kết Component

### Điểm Mạnh

1. **Multi-Driver Support**: Hỗ trợ nhiều LCD drivers (ST7796, ST7789, ILI9341)
2. **SPI Bus Sharing**: Mutex-based locking cho shared SPI bus
3. **Hardware Abstraction**: Clean HAL interface, hide implementation details
4. **Error Handling**: Fallback mechanisms (GPIO brightness, software volume)

### Điểm Yếu

1. **Resource Leaks**: SPI/I2C bus có thể leak nếu init failed
2. **Thread Safety**: Một số APIs không thread-safe (volume control)
3. **Lock Management**: SPI bus lock không có timeout, risk deadlock
4. **I2C Disabled**: Hardware volume I2C đã bị disable, chỉ dùng software

### Đề Xuất Cải Thiện Tổng Thể

1. **P0**: Fix SPI bus leak khi panel IO creation failed
2. **P1**: Thêm timeout cho SPI bus lock
3. **P1**: Thêm mutex cho volume control static state
4. **P2**: Document rõ I2C disabled và software volume fallback
5. **P2**: Thêm explicit cleanup APIs cho resources
