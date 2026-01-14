# API Catalog: sx_services - Audio Part 4: Ducking & Crossfade

**Lưu ý**: Đây là phần 4 của Batch 4, phân tích **sx_audio_ducking** và **sx_audio_crossfade** (ducking và crossfade engine).

## Tổng Quan

**sx_audio_ducking** là audio ducking service để giảm volume khi Assistant nói.
**sx_audio_crossfade** là crossfade engine để smooth transition giữa các audio tracks.

---

## 1. sx_audio_ducking.h / sx_audio_ducking.c

### A) Vai Trò File

**sx_audio_ducking** là audio ducking manager để giảm volume khi Assistant nói. File này:
- Duck audio (giảm volume) khi Assistant bắt đầu nói
- Restore audio (khôi phục volume) khi Assistant kết thúc
- Smooth fade transitions với fade task
- Quản lý duck level và fade duration

**Dependencies trực tiếp:**
```c
// sx_audio_ducking.c:1-8
#include "sx_audio_ducking.h"
#include "sx_audio_service.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
```

### B) Public API

```c
// sx_audio_ducking.h:20-37
esp_err_t sx_audio_ducking_init(const sx_audio_ducking_config_t *config);
esp_err_t sx_audio_duck(void);
esp_err_t sx_audio_restore(void);
bool sx_audio_is_ducked(void);
esp_err_t sx_audio_ducking_set_level(float duck_level);
float sx_audio_ducking_get_level(void);
```

**Contract:**

**`sx_audio_ducking_init()`**
- **Input**: `config` (ducking config, có thể NULL để dùng default)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: FreeRTOS đã được khởi tạo
- **Post-conditions**: Ducking service đã được init, mutex đã được tạo
- **Error model**: 
  - `ESP_OK`: Thành công hoặc đã init (idempotent)
  - `ESP_ERR_NO_MEM`: Mutex creation failed (```42:45:components/sx_services/sx_audio_ducking.c```)

**`sx_audio_duck()`**
- **Input**: Không có
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Ducking service đã được init
- **Post-conditions**: Fade task đã được tạo để duck audio
- **Error model**: 
  - `ESP_ERR_INVALID_STATE`: Chưa init
  - `ESP_ERR_TIMEOUT`: Mutex timeout
  - `ESP_ERR_NO_MEM`: Task creation failed
  - `ESP_OK`: Thành công hoặc đã ducked (idempotent) (```114:117:components/sx_services/sx_audio_ducking.c```)

**`sx_audio_restore()`**
- **Input**: Không có
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Ducking service đã được init
- **Post-conditions**: Fade task đã được tạo để restore audio
- **Error model**: 
  - `ESP_ERR_INVALID_STATE`: Chưa init
  - `ESP_ERR_TIMEOUT`: Mutex timeout
  - `ESP_ERR_NO_MEM`: Task creation failed
  - `ESP_OK`: Thành công hoặc không ducked (idempotent) (```149:152:components/sx_services/sx_audio_ducking.c```)

### C) Data Model

**Static State** (```13:18:components/sx_services/sx_audio_ducking.c```):
- `s_initialized`: Init flag
- `s_ducked`: Duck state flag
- `s_duck_level`: Duck level (0.0 = mute, 1.0 = no ducking), default 0.3
- `s_fade_duration_ms`: Fade duration (ms), default 200ms
- `s_original_volume_factor`: Original volume factor trước khi duck
- `s_duck_mutex`: Mutex để protect state

**Data Structures** (```14:17:components/sx_services/sx_audio_ducking.h```):
```c
typedef struct {
    float duck_level;      // Volume level when ducked (0.0 = mute, 1.0 = no ducking)
    uint32_t fade_duration_ms; // Fade duration in milliseconds
} sx_audio_ducking_config_t;
```

**Invariants:**
- Duck level: 0.0-1.0 (clamped)
- Fade duration: 10-1000ms (clamped)
- Original volume: Stored trước khi duck, restored sau khi restore

### D) Concurrency

- **Context**: 
  - **Init**: Chạy từ bootstrap (main task, single-threaded boot)
  - **Duck/Restore**: Có thể được gọi từ bất kỳ task nào (TTS, orchestrator)
  - **Fade task**: Chạy trong FreeRTOS task "sx_audio_duck" hoặc "sx_audio_restore" (priority 3, core tskNO_AFFINITY)
- **Thread Safety**: 
  - **State**: Protected bởi `s_duck_mutex` (```119:121:154:156:components/sx_services/sx_audio_ducking.c```)
  - **Fade task**: Mỗi lần duck/restore tạo một task mới (```124:132:159:167:components/sx_services/sx_audio_ducking.c```)
  - **⚠️ RISK**: Nếu duck/restore được gọi liên tiếp, có thể tạo nhiều fade tasks đồng thời

### E) Memory Ownership

- **Fade tasks**: 
  - **Owner**: FreeRTOS owns (created trong duck/restore)
  - **Lifetime**: Valid từ task creation đến task deletion (sau khi fade complete)
  - **Cleanup**: Task tự delete sau khi fade complete (```106:106:components/sx_services/sx_audio_ducking.c```)

- **State**: 
  - **Owner**: sx_audio_ducking owns (static)
  - **Lifetime**: Persistent, valid từ init đến deinit
  - **Cleanup**: Reset trong deinit (nếu có)

### F) Side Effects

1. **Audio Service**: Set volume qua `sx_audio_set_volume()` trong fade task (```92:92:components/sx_services/sx_audio_ducking.c```)
   - Volume được set mỗi 10ms trong fade duration

2. **Volume Ramping**: Ducking sử dụng volume ramping của audio service, có thể conflict với volume ramp task của audio service.

### G) Call Sites

1. **sx_bootstrap_start()** - Init ducking service (từ bootstrap)
2. **TTS service** - Duck audio khi TTS bắt đầu, restore khi TTS kết thúc
3. **Orchestrator** - Duck/restore từ events

### H) Issues/Risks

1. **P1 - Multiple Fade Tasks**: Nếu duck/restore được gọi liên tiếp, có thể tạo nhiều fade tasks đồng thời, gây conflict.
   - **Điều kiện**: Duck/restore được gọi liên tiếp trước khi fade task complete
   - **Cách tái hiện**: Gọi duck, sau đó restore ngay lập tức
   - **Impact**: Nhiều fade tasks chạy đồng thời, volume có thể không đúng

2. **P1 - Volume Ramping Conflict**: Ducking sử dụng `sx_audio_set_volume()` có thể conflict với volume ramp task của audio service.
   - **Điều kiện**: Duck/restore trong khi volume ramp task đang chạy
   - **Cách tái hiện**: Set volume từ UI, sau đó duck ngay lập tức
   - **Impact**: Volume ramp conflict, volume có thể không đúng

3. **P2 - Original Volume Loss**: Original volume được store trong fade task (```75:75:components/sx_services/sx_audio_ducking.c```), có thể mất nếu fade task bị cancel.
   - **Điều kiện**: Fade task bị cancel hoặc crash
   - **Cách tái hiện**: Duck, sau đó cancel fade task
   - **Impact**: Original volume mất, restore không đúng

4. **P2 - Volume Conversion Accuracy**: Volume được convert từ percentage sang factor và ngược lại (```67:67:89:90:components/sx_services/sx_audio_ducking.c```), có thể mất accuracy.
   - **Điều kiện**: Volume có giá trị không tròn (ví dụ: 33%)
   - **Cách tái hiện**: Set volume 33%, sau đó duck
   - **Impact**: Volume conversion mất accuracy, restore không chính xác

### I) Đề Xuất Cải Thiện

1. **P1**: Cancel existing fade task trước khi tạo task mới
2. **P1**: Coordinate với audio service volume ramp task
3. **P2**: Store original volume trong static state thay vì fade task
4. **P2**: Sử dụng volume factor trực tiếp thay vì convert percentage

---

## 2. sx_audio_crossfade.h / sx_audio_crossfade.c

### A) Vai Trò File

**sx_audio_crossfade** là crossfade engine để smooth transition giữa các audio tracks. File này:
- Start crossfade giữa old và new tracks
- Process PCM samples với fade in/out
- Quản lý crossfade state và progress

**Dependencies trực tiếp:**
```c
// sx_audio_crossfade.c:1-8
#include "sx_audio_crossfade.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
```

### B) Public API

```c
// sx_audio_crossfade.h:28-60
esp_err_t sx_audio_crossfade_init(const sx_audio_crossfade_config_t *config);
esp_err_t sx_audio_crossfade_start(const int16_t *old_pcm, const int16_t *new_pcm, size_t sample_count);
bool sx_audio_crossfade_process(int16_t *pcm, size_t sample_count);
bool sx_audio_crossfade_is_active(void);
sx_audio_crossfade_state_t sx_audio_crossfade_get_state(void);
esp_err_t sx_audio_crossfade_stop(void);
esp_err_t sx_audio_crossfade_set_enabled(bool enabled);
bool sx_audio_crossfade_is_enabled(void);
void sx_audio_crossfade_set_sample_rate(uint32_t sample_rate);
```

**Contract:**

**`sx_audio_crossfade_init()`**
- **Input**: `config` (crossfade config, có thể NULL để dùng default)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: FreeRTOS đã được khởi tạo
- **Post-conditions**: Crossfade engine đã được init, mutex đã được tạo
- **Error model**: 
  - `ESP_OK`: Thành công hoặc đã init (idempotent)
  - `ESP_ERR_NO_MEM`: Mutex creation failed (```36:40:components/sx_services/sx_audio_crossfade.c```)

**`sx_audio_crossfade_start()`**
- **Input**: `old_pcm` (old track PCM), `new_pcm` (new track PCM), `sample_count` (sample count)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Crossfade engine đã được init và enabled
- **Post-conditions**: Crossfade đã được start, state = FADING_OUT
- **Error model**: 
  - `ESP_ERR_INVALID_STATE`: Chưa init hoặc disabled
  - `ESP_ERR_INVALID_ARG`: old_pcm/new_pcm NULL hoặc sample_count == 0
  - `ESP_ERR_TIMEOUT`: Mutex timeout
  - `ESP_OK`: Thành công

**`sx_audio_crossfade_process()`**
- **Input**: `pcm` (PCM samples, in-place), `sample_count` (sample count)
- **Output**: `true` nếu crossfade vẫn active, `false` nếu complete
- **Pre-conditions**: Crossfade đã được start
- **Post-conditions**: PCM samples đã được process với crossfade (in-place)
- **Error model**: 
  - `false`: Không cần process (not initialized/enabled/idle) hoặc complete
  - `true`: Crossfade vẫn active

### C) Data Model

**Static State** (```13:20:components/sx_services/sx_audio_crossfade.c```):
- `s_initialized`: Init flag
- `s_enabled`: Enable flag
- `s_state`: Crossfade state (IDLE, FADING_OUT, FADING_IN, COMPLETE)
- `s_fade_duration_ms`: Fade duration (ms), default 500ms
- `s_samples_processed`: Samples đã được process
- `s_total_fade_samples`: Total fade samples
- `s_sample_rate`: Current sample rate
- `s_crossfade_mutex`: Mutex để protect state

**Data Structures** (```14:17:components/sx_services/sx_audio_crossfade.h```):
```c
typedef struct {
    uint32_t fade_duration_ms; // Crossfade duration in milliseconds
    bool enabled;              // Enable/disable crossfade
} sx_audio_crossfade_config_t;
```

**Enums** (```20:25:components/sx_services/sx_audio_crossfade.h```):
- `SX_CROSSFADE_IDLE`: No crossfade in progress
- `SX_CROSSFADE_FADING_OUT`: Fading out old track
- `SX_CROSSFADE_FADING_IN`: Fading in new track
- `SX_CROSSFADE_COMPLETE`: Crossfade complete

**Invariants:**
- Fade duration: 10-2000ms (clamped)
- Sample rate: Phải > 0
- Total fade samples: Calculated từ sample rate và fade duration

### D) Concurrency

- **Context**: 
  - **Init**: Chạy từ bootstrap (main task, single-threaded boot)
  - **Start/Stop**: Có thể được gọi từ bất kỳ task nào (playlist manager, orchestrator)
  - **Process**: Được gọi từ audio playback task (```773:773:components/sx_services/sx_audio_service.c```)
- **Thread Safety**: 
  - **State**: Protected bởi `s_crossfade_mutex` (```61:63:91:94:175:177:components/sx_services/sx_audio_crossfade.c```)
  - **Process**: Mutex timeout là 0 (non-blocking) (```91:94:components/sx_services/sx_audio_crossfade.c```), skip nếu mutex busy
  - **⚠️ RISK**: Nếu process được gọi từ nhiều tasks, có thể skip crossfade nếu mutex busy

### E) Memory Ownership

- **PCM samples**: 
  - **Owner**: Caller owns PCM buffer
  - **Lifetime**: Valid trong suốt `sx_audio_crossfade_process()` call
  - **Usage**: PCM samples được process in-place (không copy)

- **State**: 
  - **Owner**: sx_audio_crossfade owns (static)
  - **Lifetime**: Persistent, valid từ init đến deinit
  - **Cleanup**: Reset trong stop (```179:181:components/sx_services/sx_audio_crossfade.c```)

### F) Side Effects

1. **PCM Processing**: Process PCM samples in-place với fade in/out (```122:140:components/sx_services/sx_audio_crossfade.c```)
   - Apply fade out cho old track (giảm gain từ 1.0 xuống 0.0)
   - Apply fade in cho new track (tăng gain từ 0.5 lên 1.0)
   - **Note**: Hiện tại chỉ có một PCM buffer, chỉ apply fade out (```123:125:components/sx_services/sx_audio_crossfade.c```)

### G) Call Sites

1. **sx_bootstrap_start()** - Init crossfade engine (từ bootstrap)
2. **sx_audio_service_feed_pcm()** - Process PCM qua crossfade (```773:773:components/sx_services/sx_audio_service.c```)
3. **Playlist manager** - Start crossfade khi chuyển track
4. **sx_audio_crossfade_set_sample_rate()** - Update sample rate khi I2S sample rate thay đổi (```739:739:components/sx_services/sx_audio_service.c```)

### H) Issues/Risks

1. **P1 - Incomplete Implementation**: Crossfade chỉ apply fade out cho một PCM buffer (```123:125:components/sx_services/sx_audio_crossfade.c```), không mix old và new tracks.
   - **Điều kiện**: Start crossfade với old_pcm và new_pcm
   - **Cách tái hiện**: Start crossfade, sau đó process PCM
   - **Impact**: Crossfade không hoạt động đúng, chỉ fade out old track

2. **P1 - Mutex Timeout Skip**: Process mutex timeout là 0 (non-blocking) (```91:94:components/sx_services/sx_audio_crossfade.c```), có thể skip crossfade nếu mutex busy.
   - **Điều kiện**: Process được gọi trong khi start/stop đang hold mutex
   - **Cách tái hiện**: Start crossfade, sau đó process ngay lập tức
   - **Impact**: Crossfade bị skip, audio không smooth

3. **P2 - State Transition Logic**: State transition logic phức tạp (```106:139:components/sx_services/sx_audio_crossfade.c```), có thể gây confusion.
   - **Điều kiện**: State transition trong process loop
   - **Cách tái hiện**: Process PCM với state transitions
   - **Impact**: State có thể không đúng, crossfade không smooth

4. **P2 - Sample Rate Change**: Sample rate có thể thay đổi trong khi crossfade đang active, `s_total_fade_samples` không được update.
   - **Điều kiện**: Sample rate thay đổi trong khi crossfade active
   - **Cách tái hiện**: Start crossfade, sau đó change sample rate
   - **Impact**: Fade duration không đúng, crossfade không smooth

### I) Đề Xuất Cải Thiện

1. **P1**: Implement full crossfade với mixing old và new tracks
2. **P1**: Thêm timeout cho process mutex (ví dụ: 10ms) để tránh skip
3. **P2**: Simplify state transition logic
4. **P2**: Update `s_total_fade_samples` khi sample rate thay đổi

---

## Tổng Kết Phần 4

### Điểm Mạnh

1. **Ducking**: Smooth fade transitions với fade task
2. **Crossfade**: Framework cho crossfade engine
3. **Thread Safety**: State được protect bởi mutex

### Điểm Yếu

1. **Ducking**: Multiple fade tasks có thể conflict
2. **Crossfade**: Incomplete implementation (chỉ fade out, không mix)
3. **Error Handling**: Mutex timeout có thể skip operations

### Đề Xuất Cải Thiện Tổng Thể

1. **P1**: Fix ducking multiple fade tasks
2. **P1**: Complete crossfade implementation
3. **P2**: Improve error handling và timeout

---

**Hoàn thành Batch 4**: Đã phân tích đầy đủ 4 phần của audio services (core, router, eq, ducking + crossfade).
