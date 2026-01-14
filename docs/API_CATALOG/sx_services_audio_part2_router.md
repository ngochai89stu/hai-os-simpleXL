# API Catalog: sx_services - Audio Part 2: Router

**Lưu ý**: Đây là phần 2 của Batch 4, phân tích **sx_audio_router** (audio routing system).

## Tổng Quan

**sx_audio_router** là audio routing service cung cấp:
- **Source-to-Sink Routing**: Route audio từ các sources (SD_MUSIC, RADIO, ONLINE_MUSIC, TTS, EXTERNAL) đến các sinks (I2S, EXTERNAL, BOTH)
- **Route Management**: Enable/disable routes, get/set routing configuration
- **Multi-Sink Support**: Route audio đến nhiều sinks đồng thời (I2S + Bluetooth)

---

## 1. sx_audio_router.h / sx_audio_router.c

### A) Vai Trò File

**sx_audio_router** là audio routing system quản lý việc route audio từ các sources đến các sinks. File này:
- Quản lý routing table (source → sink mapping)
- Enable/disable routes
- Route audio data từ sources đến sinks (I2S, Bluetooth, etc.)
- Hỗ trợ multi-sink routing (BOTH = I2S + EXTERNAL)

**Dependencies trực tiếp:**
```c
// sx_audio_router.c:1-7
#include "sx_audio_router.h"
#include "sx_audio_service.h"
#include "sx_bluetooth_service.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
```

### B) Public API

```c
// sx_audio_router.h:43-84
esp_err_t sx_audio_router_init(void);
void sx_audio_router_deinit(void);
esp_err_t sx_audio_router_set_route(sx_audio_source_t source, sx_audio_sink_t sink);
esp_err_t sx_audio_router_get_route(sx_audio_source_t source, sx_audio_sink_t *sink);
esp_err_t sx_audio_router_enable_route(sx_audio_source_t source, bool enabled);
bool sx_audio_router_is_route_enabled(sx_audio_source_t source);
esp_err_t sx_audio_router_route_audio(sx_audio_source_t source, const int16_t *pcm,
                                      size_t samples, uint32_t sample_rate);
const char *sx_audio_router_source_name(sx_audio_source_t source);
const char *sx_audio_router_sink_name(sx_audio_sink_t sink);
```

**Contract:**

**`sx_audio_router_init()`**
- **Input**: Không có
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: FreeRTOS đã được khởi tạo
- **Post-conditions**: Router đã được init, tất cả routes mặc định là I2S và enabled
- **Error model**: 
  - `ESP_OK`: Thành công hoặc đã init (idempotent)
  - `ESP_ERR_NO_MEM`: Mutex creation failed (```39:42:components/sx_services/sx_audio_router.c```)

**`sx_audio_router_set_route()`**
- **Input**: `source` (audio source), `sink` (audio sink)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Router đã được init
- **Post-conditions**: Route đã được set
- **Error model**: 
  - `ESP_ERR_INVALID_ARG`: Chưa init, source/sink invalid
  - `ESP_ERR_TIMEOUT`: Mutex timeout
  - `ESP_OK`: Thành công

**`sx_audio_router_route_audio()`**
- **Input**: `source` (audio source), `pcm` (PCM data), `samples` (sample count), `sample_rate` (sample rate)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Router đã được init, source valid, pcm != NULL, samples > 0
- **Post-conditions**: Audio đã được route đến sink(s)
- **Error model**: 
  - `ESP_ERR_INVALID_ARG`: Chưa init, source invalid, pcm NULL, samples == 0
  - `ESP_ERR_TIMEOUT`: Mutex timeout
  - `ESP_OK`: Thành công (hoặc route disabled, silently ignore)
  - `ESP_FAIL`: I2S feed failed (nếu sink == I2S only) (```161:163:components/sx_services/sx_audio_router.c```)

### C) Data Model

**Static State** (```12:14:components/sx_services/sx_audio_router.c```):
- `s_initialized`: Init flag
- `s_routes[SX_AUDIO_SOURCE_COUNT]`: Routing table (mỗi source có một route entry)
- `s_mutex`: Mutex để protect routing table

**Data Structures** (```36:40:components/sx_services/sx_audio_router.h```):
```c
typedef struct {
    sx_audio_source_t source;
    sx_audio_sink_t sink;
    bool enabled;
} sx_audio_route_t;
```

**Enums** (```16:33:components/sx_services/sx_audio_router.h```):
- `sx_audio_source_t`: NONE, SD_MUSIC, RADIO, ONLINE_MUSIC, TTS, EXTERNAL
- `sx_audio_sink_t`: NONE, I2S, EXTERNAL, BOTH

**Invariants:**
- Mỗi source có một route entry trong `s_routes[]`
- Default route: Tất cả sources route đến I2S và enabled (```45:49:components/sx_services/sx_audio_router.c```)
- Route disabled: Audio được silently ignored (```152:154:components/sx_services/sx_audio_router.c```)
- BOTH sink: Route đến cả I2S và EXTERNAL (```157:186:components/sx_services/sx_audio_router.c```)

### D) Concurrency

- **Context**: 
  - **Init/Deinit**: Chạy từ bootstrap (main task, single-threaded boot)
  - **Route operations**: Có thể được gọi từ bất kỳ task nào (UI, audio services, network)
  - **Route audio**: Có thể được gọi từ bất kỳ task nào (audio playback tasks, TTS, radio, etc.)
- **Thread Safety**: 
  - **Routing table**: Protected bởi `s_mutex` (```75:81:92:98:108:114:143:150:components/sx_services/sx_audio_router.c```)
  - **Route audio**: Mutex được take để read route config, sau đó release trước khi feed audio (```143:150:components/sx_services/sx_audio_router.c```)
  - **⚠️ RISK**: Mutex timeout là `portMAX_DELAY` (blocking), có thể block caller nếu mutex bị hold lâu

### E) Memory Ownership

- **PCM data**: 
  - **Owner**: Caller owns PCM buffer
  - **Lifetime**: Valid trong suốt `sx_audio_router_route_audio()` call
  - **Usage**: PCM data được pass-through đến sinks (không copy)

- **Routing table**: 
  - **Owner**: sx_audio_router owns (static array)
  - **Lifetime**: Persistent, valid từ init đến deinit
  - **Cleanup**: Deinit trong `sx_audio_router_deinit()` (```56:68:components/sx_services/sx_audio_router.c```)

### F) Side Effects

1. **Audio Service**: Feed PCM data đến I2S sink (```159:163:components/sx_services/sx_audio_router.c```)
   - Gọi `sx_audio_service_feed_pcm()` để feed audio đến I2S

2. **Bluetooth Service**: Feed PCM data đến Bluetooth sink (```169:181:components/sx_services/sx_audio_router.c```)
   - Gọi `sx_bluetooth_service_feed_audio()` nếu Bluetooth enabled và connected
   - Check Bluetooth state trước khi feed (```175:175:components/sx_services/sx_audio_router.c```)

3. **Logging**: Log route changes và audio routing (```83:84:116:117:178:184:components/sx_services/sx_audio_router.c```)

### G) Call Sites

1. **sx_bootstrap_start()** - Init router (từ bootstrap)
2. **Audio services** - Route audio từ các sources (SD music, radio, online music, TTS)
3. **Bluetooth service** - Route audio đến Bluetooth sink
4. **UI/Orchestrator** - Set routes từ user settings
5. **External services** - Route audio từ external sources

### H) Issues/Risks

1. **P1 - Blocking Mutex**: Mutex timeout là `portMAX_DELAY` (blocking) (```75:76:92:93:108:109:143:144:components/sx_services/sx_audio_router.c```), có thể block caller nếu mutex bị hold lâu.
   - **Điều kiện**: Mutex bị hold lâu (deadlock hoặc long operation)
   - **Cách tái hiện**: Hold mutex trong một operation dài, sau đó gọi route operation từ task khác
   - **Impact**: Block caller, có thể gây audio dropouts

2. **P1 - External Sink Failure Silent**: Nếu EXTERNAL sink (Bluetooth) fail, error được log nhưng không return error (```180:180:components/sx_services/sx_audio_router.c```), caller không biết.
   - **Điều kiện**: Bluetooth feed failed nhưng sink là BOTH
   - **Cách tái hiện**: Route audio với sink = BOTH, Bluetooth feed failed
   - **Impact**: Caller không biết external sink failed, audio chỉ play qua I2S

3. **P2 - No Route Validation**: Không validate route configuration (ví dụ: route NONE source, route đến NONE sink), có thể gây confusion.
   - **Điều kiện**: Set route với source = NONE hoặc sink = NONE
   - **Cách tái hiện**: Gọi `sx_audio_router_set_route(SX_AUDIO_SOURCE_NONE, SX_AUDIO_SINK_I2S)`
   - **Impact**: Route được set nhưng không có ý nghĩa

4. **P2 - External Sink Check Race**: Check Bluetooth state và feed audio không atomic (```175:176:components/sx_services/sx_audio_router.c```), có thể Bluetooth disconnect giữa check và feed.
   - **Điều kiện**: Bluetooth disconnect giữa check state và feed audio
   - **Cách tái hiện**: Check Bluetooth state, sau đó disconnect, sau đó feed audio
   - **Impact**: Feed audio đến disconnected Bluetooth, có thể gây error

5. **P2 - No Route Priority**: Nếu nhiều sources route đến cùng sink, không có priority mechanism, có thể conflict.
   - **Điều kiện**: Nhiều sources route đến I2S đồng thời
   - **Cách tái hiện**: Route audio từ nhiều sources đến I2S cùng lúc
   - **Impact**: Audio mixing không được handle, có thể gây audio glitches

### I) Đề Xuất Cải Thiện

1. **P1**: Thêm timeout cho mutex operations (ví dụ: 100ms) để tránh block lâu
2. **P1**: Return error nếu EXTERNAL sink fail và sink không phải BOTH
3. **P2**: Validate route configuration (reject NONE source/sink)
4. **P2**: Atomic check và feed cho external sinks (hoặc handle disconnect gracefully)
5. **P2**: Thêm route priority mechanism để handle multiple sources

---

## Tổng Kết Phần 2

### Điểm Mạnh

1. **Simple API**: API đơn giản, dễ sử dụng
2. **Thread Safety**: Routing table được protect bởi mutex
3. **Multi-Sink Support**: Hỗ trợ route đến nhiều sinks (BOTH)
4. **Flexible Routing**: Có thể enable/disable routes động

### Điểm Yếu

1. **Blocking Mutex**: Mutex timeout là blocking, có thể block caller
2. **Error Handling**: External sink failure không được propagate đúng cách
3. **No Validation**: Không validate route configuration
4. **No Priority**: Không có priority mechanism cho multiple sources

### Đề Xuất Cải Thiện Tổng Thể

1. **P1**: Fix blocking mutex với timeout
2. **P1**: Improve error handling cho external sinks
3. **P2**: Add route validation
4. **P2**: Add priority mechanism cho multiple sources

---

**Tiếp theo**: Phần 3 sẽ phân tích **sx_audio_eq** (10-band equalizer).
