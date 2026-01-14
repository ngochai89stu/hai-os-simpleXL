# API Catalog: sx_services - Audio Part 3: Equalizer

**Lưu ý**: Đây là phần 3 của Batch 4, phân tích **sx_audio_eq** (10-band equalizer).

## Tổng Quan

**sx_audio_eq** là 10-band equalizer service cung cấp:
- **10-Band EQ**: 10 frequency bands (31Hz, 62Hz, 125Hz, 250Hz, 500Hz, 1kHz, 2kHz, 4kHz, 8kHz, 16kHz)
- **Presets**: Flat, Pop, Rock, Jazz, Classical, Custom
- **Biquad Filters**: Peaking EQ filters với separate history cho left/right channels
- **Settings Persistence**: Save/load EQ settings từ NVS

---

## 1. sx_audio_eq.h / sx_audio_eq.c

### A) Vai Trò File

**sx_audio_eq** là 10-band equalizer service. File này:
- Implement biquad filters cho mỗi EQ band
- Process PCM samples qua cascade của 10 biquad filters
- Quản lý EQ presets và custom settings
- Persist settings vào NVS

**Dependencies trực tiếp:**
```c
// sx_audio_eq.c:1-7
#include "sx_audio_eq.h"
#include "sx_settings_service.h"
#include <math.h>
```

### B) Public API

```c
// sx_audio_eq.h:44-79
esp_err_t sx_audio_eq_init(uint32_t sample_rate_hz);
void sx_audio_eq_deinit(void);
esp_err_t sx_audio_eq_set_band(int band, int16_t gain_db);
int16_t sx_audio_eq_get_band(int band);
esp_err_t sx_audio_eq_set_bands(const int16_t gains[SX_AUDIO_EQ_NUM_BANDS]);
esp_err_t sx_audio_eq_get_bands(int16_t gains[SX_AUDIO_EQ_NUM_BANDS]);
esp_err_t sx_audio_eq_set_preset(sx_audio_eq_preset_t preset);
sx_audio_eq_preset_t sx_audio_eq_get_preset(void);
esp_err_t sx_audio_eq_enable(bool enable);
bool sx_audio_eq_is_enabled(void);
esp_err_t sx_audio_eq_process(int16_t *samples, size_t sample_count);
esp_err_t sx_audio_eq_set_sample_rate(uint32_t sample_rate_hz);
```

**Contract:**

**`sx_audio_eq_init()`**
- **Input**: `sample_rate_hz` (sample rate)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Settings service đã được init
- **Post-conditions**: EQ đã được init, filters đã được tính toán, settings đã được load từ NVS
- **Error model**: 
  - `ESP_OK`: Thành công hoặc đã init (idempotent, update sample rate) (```116:119:components/sx_services/sx_audio_eq.c```)

**`sx_audio_eq_process()`**
- **Input**: `samples` (PCM samples, in-place), `sample_count` (sample count)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: EQ đã được init và enabled
- **Post-conditions**: PCM samples đã được process qua EQ filters (in-place)
- **Error model**: 
  - `ESP_OK`: Thành công hoặc không cần process (not initialized/enabled) (```310:312:components/sx_services/sx_audio_eq.c```)
  - **Note**: Samples được process in-place, không copy

**`sx_audio_eq_set_band()`**
- **Input**: `band` (0-9), `gain_db` (gain in 0.1dB units, -120 to +120)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: EQ đã được init
- **Post-conditions**: Band gain đã được set, filter coefficients đã được update, settings đã được save
- **Error model**: 
  - `ESP_ERR_INVALID_STATE`: Chưa init
  - `ESP_ERR_INVALID_ARG`: Band hoặc gain invalid
  - `ESP_OK`: Thành công

### C) Data Model

**Static State** (```29:47:components/sx_services/sx_audio_eq.c```):
- `s_initialized`: Init flag
- `s_enabled`: Enable flag
- `s_sample_rate`: Current sample rate
- `s_band_gains[SX_AUDIO_EQ_NUM_BANDS]`: Gain cho mỗi band (0.1dB units)
- `s_current_preset`: Current preset
- `s_filters[SX_AUDIO_EQ_NUM_BANDS]`: Biquad filters cho mỗi band

**Data Structures** (```36:44:components/sx_services/sx_audio_eq.c```):
```c
typedef struct {
    float b0, b1, b2;  // Numerator coefficients
    float a1, a2;      // Denominator coefficients (a0 = 1.0)
    // Separate history for left and right channels
    float x1_l, x2_l;  // Left channel input history
    float y1_l, y2_l;  // Left channel output history
    float x1_r, x2_r;  // Right channel input history
    float y1_r, y2_r;  // Right channel output history
} biquad_filter_t;
```

**Presets** (```20:26:components/sx_services/sx_audio_eq.c```):
- `SX_AUDIO_EQ_PRESET_FLAT`: All bands = 0
- `SX_AUDIO_EQ_PRESET_POP`: Pop preset
- `SX_AUDIO_EQ_PRESET_ROCK`: Rock preset
- `SX_AUDIO_EQ_PRESET_JAZZ`: Jazz preset
- `SX_AUDIO_EQ_PRESET_CLASSICAL`: Classical preset
- `SX_AUDIO_EQ_PRESET_CUSTOM`: Custom preset

**Invariants:**
- Gain range: -120 to +120 (0.1dB units) = -12.0dB to +12.0dB
- Sample rate: Phải > 0
- Band count: 10 bands (SX_AUDIO_EQ_NUM_BANDS)
- Filter history: Separate cho left/right channels

### D) Concurrency

- **Context**: 
  - **Init/Deinit**: Chạy từ bootstrap (main task, single-threaded boot)
  - **Set operations**: Có thể được gọi từ bất kỳ task nào (UI, settings)
  - **Process**: Được gọi từ audio playback task (```383:426:462:components/sx_services/sx_audio_service.c```)
- **Thread Safety**: 
  - **Static state**: Không được protect bởi mutex (race condition risk)
  - **Filter processing**: In-place processing, không có shared state trong processing loop
  - **⚠️ RISK**: Set band/preset trong khi đang process có thể gây inconsistent state

### E) Memory Ownership

- **PCM samples**: 
  - **Owner**: Caller owns PCM buffer
  - **Lifetime**: Valid trong suốt `sx_audio_eq_process()` call
  - **Usage**: PCM samples được process in-place (không copy)

- **Filter state**: 
  - **Owner**: sx_audio_eq owns (static array)
  - **Lifetime**: Persistent, valid từ init đến deinit
  - **Cleanup**: Reset trong deinit (```173:173:components/sx_services/sx_audio_eq.c```)

- **Settings**: 
  - **Owner**: NVS owns (persistent storage)
  - **Lifetime**: Persistent across reboots
  - **Usage**: Load trong init, save trong set operations

### F) Side Effects

1. **NVS Settings**: Save/load EQ settings từ NVS (```126:154:199:201:234:236:278:280:298:299:components/sx_services/sx_audio_eq.c```)
   - Save: `eq_preset`, `eq_enabled`, `eq_bands`
   - Load: Load settings trong init

2. **PCM Processing**: Process PCM samples in-place qua cascade của 10 biquad filters (```309:341:components/sx_services/sx_audio_eq.c```)
   - Process left/right channels separately với separate history
   - Clamp output để tránh overflow

3. **Filter Coefficients**: Recalculate filter coefficients khi band gain hoặc sample rate thay đổi (```107:113:194:196:231:231:275:275:358:358:components/sx_services/sx_audio_eq.c```)

### G) Call Sites

1. **sx_audio_service_init()** - Init EQ service (```221:224:components/sx_services/sx_audio_service.c```)
2. **sx_audio_service_feed_pcm()** - Process PCM qua EQ (```770:770:components/sx_services/sx_audio_service.c```)
3. **sx_audio_playback_task()** - Process PCM qua EQ trong playback (```383:426:462:components/sx_services/sx_audio_service.c```)
4. **sx_audio_eq_set_sample_rate()** - Update sample rate khi I2S sample rate thay đổi (```737:737:components/sx_services/sx_audio_service.c```)
5. **UI/Settings** - Set EQ bands/presets từ user settings

### H) Issues/Risks

1. **P1 - Thread Safety**: Static state không được protect bởi mutex, có thể race condition khi set band/preset trong khi đang process.
   - **Điều kiện**: Set band/preset trong khi playback task đang process PCM
   - **Cách tái hiện**: Set band từ UI task trong khi playback task đang process
   - **Impact**: Inconsistent filter state, có thể gây audio glitches

2. **P1 - Settings Commit Overhead**: Mỗi lần set band/preset đều commit settings (```201:201:236:236:280:280:299:299:components/sx_services/sx_audio_eq.c```), có thể tốn thời gian nếu set nhiều bands.
   - **Điều kiện**: Set nhiều bands liên tiếp
   - **Cách tái hiện**: Set tất cả 10 bands từ UI
   - **Impact**: NVS commit overhead, có thể block caller

3. **P2 - Filter History Reset**: Filter history được reset khi sample rate thay đổi (```355:355:components/sx_services/sx_audio_eq.c```), có thể gây audio pop.
   - **Điều kiện**: Sample rate thay đổi trong khi đang play
   - **Cách tái hiện**: Play file với sample rate thay đổi
   - **Impact**: Audio pop do filter history reset

4. **P2 - Clamp Overflow**: Output được clamp để tránh overflow (```331:334:components/sx_services/sx_audio_eq.c```), có thể gây distortion nếu gain quá cao.
   - **Điều kiện**: Set gain quá cao cho nhiều bands
   - **Cách tái hiện**: Set tất cả bands lên +12dB
   - **Impact**: Audio distortion do clipping

5. **P2 - Fixed Q Factor**: Q factor được fix ở 1.0 (```110:110:195:195:components/sx_services/sx_audio_eq.c```), không thể điều chỉnh bandwidth.
   - **Điều kiện**: Cần điều chỉnh bandwidth cho một band
   - **Cách tái hiện**: Set band với Q factor khác
   - **Impact**: Không thể điều chỉnh bandwidth, EQ không linh hoạt

### I) Đề Xuất Cải Thiện

1. **P1**: Thêm mutex để protect static state (set operations và process)
2. **P1**: Batch settings commit (chỉ commit khi set tất cả bands xong)
3. **P2**: Smooth filter history transition khi sample rate thay đổi
4. **P2**: Warning log nếu output bị clamp (gain quá cao)
5. **P2**: Thêm Q factor parameter cho mỗi band

---

## Tổng Kết Phần 3

### Điểm Mạnh

1. **10-Band EQ**: Hỗ trợ 10 frequency bands với biquad filters
2. **Presets**: Có sẵn 5 presets + custom
3. **Settings Persistence**: Save/load settings từ NVS
4. **Separate Channels**: Left/right channels có separate filter history

### Điểm Yếu

1. **Thread Safety**: Static state không được protect
2. **Settings Overhead**: Commit settings mỗi lần set band
3. **Filter History Reset**: Reset history khi sample rate thay đổi có thể gây pop
4. **Fixed Q Factor**: Q factor không thể điều chỉnh

### Đề Xuất Cải Thiện Tổng Thể

1. **P1**: Fix thread safety với mutex
2. **P1**: Optimize settings commit (batch commit)
3. **P2**: Improve filter history transition
4. **P2**: Add Q factor parameter

---

**Tiếp theo**: Phần 4 sẽ phân tích **sx_audio_ducking + crossfade** (ducking và crossfade engine).
