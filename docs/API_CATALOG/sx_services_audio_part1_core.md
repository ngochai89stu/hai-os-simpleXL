# API Catalog: sx_services - Audio Part 1: Core Service

**Lưu ý**: Đây là phần 1 của Batch 4, chỉ phân tích **sx_audio_service** (core playback/recording). Các phần khác (router, eq, ducking, crossfade) sẽ được phân tích trong các phần tiếp theo.

## Tổng Quan

**sx_audio_service** là core audio service cung cấp:
- **I2S Playback**: Play audio files (MP3, FLAC, WAV, PCM) qua I2S
- **I2S Recording**: Record audio từ microphone qua I2S
- **Volume Control**: Software/hardware volume với smooth ramping
- **Position Tracking**: Track playback position và duration
- **Spectrum/FFT**: FFT processing cho spectrum visualization
- **Codec Support**: MP3, FLAC, WAV, PCM decoding

---

## 1. sx_audio_service.h / sx_audio_service.c

### A) Vai Trò File

**sx_audio_service** là core audio playback và recording service. File này:
- Quản lý I2S hardware (TX cho playback, RX cho recording)
- Decode audio files (MP3, FLAC, WAV, PCM)
- Feed PCM data vào I2S pipeline
- Track playback position và duration
- Process FFT cho spectrum visualization
- Quản lý volume với smooth ramping

**Dependencies trực tiếp:**
```c
// sx_audio_service.c:1-29
#include "sx_audio_service.h"
#include "sx_media_metadata.h"
#include "sx_sd_service.h"
#include "sx_audio_eq.h"
#include "sx_audio_crossfade.h"
#include "sx_stt_service.h"
#include "sx_platform_volume.h"
#include "sx_playlist_manager.h"
#include "sx_codec_mp3.h"
#include "sx_codec_flac.h"
#include "sx_codec_detector.h"
#include "sx_audio_power.h"
#include "sx_audio_buffer_pool.h"
#include "driver/i2s_std.h"
```

### B) Public API

```c
// sx_audio_service.h:16-75
esp_err_t sx_audio_service_init(void);
esp_err_t sx_audio_service_start(void);
esp_err_t sx_audio_play_file(const char *file_path);
esp_err_t sx_audio_stop(void);
esp_err_t sx_audio_pause(void);
esp_err_t sx_audio_resume(void);
bool sx_audio_is_playing(void);
esp_err_t sx_audio_start_recording(void);
esp_err_t sx_audio_stop_recording(void);
bool sx_audio_is_recording(void);
esp_err_t sx_audio_start_recording_with_stt(void);
esp_err_t sx_audio_set_recording_callback(sx_audio_recording_callback_t callback);
esp_err_t sx_audio_set_volume(uint8_t volume);
uint8_t sx_audio_get_volume(void);
uint32_t sx_audio_get_position(void);
uint32_t sx_audio_get_duration(void);
esp_err_t sx_audio_seek(uint32_t position);
sx_audio_caps_t sx_audio_get_caps(void);
esp_err_t sx_audio_get_spectrum(uint16_t *bands, size_t band_count);
esp_err_t sx_audio_service_feed_pcm(const int16_t *pcm, size_t sample_count, uint32_t sample_rate_hz);
```

**Contract:**

**`sx_audio_service_init()`**
- **Input**: Không có
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: FreeRTOS đã được khởi tạo
- **Post-conditions**: I2S hardware đã được init, mutexes đã được tạo
- **Error model**: 
  - `ESP_OK`: Thành công hoặc đã init (idempotent)
  - I2S init failed: Return `ESP_OK` nhưng `s_i2s_ready = false` (```240:242:components/sx_services/sx_audio_service.c```)

**`sx_audio_play_file()`**
- **Input**: `file_path` (file path to play)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Audio service đã được init và I2S ready
- **Post-conditions**: Playback task đã được tạo và đang chạy
- **Error model**: 
  - `ESP_ERR_INVALID_STATE`: Chưa init hoặc I2S not ready
  - `ESP_ERR_INVALID_ARG`: file_path là NULL
  - `ESP_FAIL`: File không mở được
  - `ESP_ERR_NO_MEM`: Task creation failed

**`sx_audio_service_feed_pcm()`**
- **Input**: `pcm` (PCM samples), `sample_count`, `sample_rate_hz`
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Audio service đã được init và I2S ready
- **Post-conditions**: PCM data đã được feed vào I2S pipeline
- **Error model**: 
  - `ESP_ERR_INVALID_STATE`: Chưa init, I2S not ready, hoặc pcm/sample_count invalid
  - `ESP_ERR_TIMEOUT`: Feed mutex timeout
  - `ESP_ERR_NO_MEM`: Buffer allocation failed
  - `ESP_FAIL`: I2S write failed

**`sx_audio_set_volume()`**
- **Input**: `volume` (0-100)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Audio service đã được init
- **Post-conditions**: Volume đã được set (hardware hoặc software với ramping)
- **Error model**: 
  - `ESP_OK`: Thành công (hardware hoặc software)
  - `ESP_ERR_NO_MEM`: Volume ramp task creation failed (fallback to immediate)

### C) Data Model

**Static State** (```35:89:components/sx_services/sx_audio_service.c```):
- `s_initialized`: Init flag
- `s_playing`, `s_paused`, `s_recording`: Playback/recording state flags
- `s_volume`, `s_target_volume`, `s_current_volume_factor`: Volume state
- `s_volume_ramping`, `s_volume_ramp_task_handle`: Volume ramping state
- `s_tx_chan`, `s_rx_chan`: I2S channel handles
- `s_i2s_ready`: I2S ready flag
- `s_i2s_std_cfg`: I2S config (kept for re-apply)
- `s_current_sample_rate`: Current sample rate
- `s_playback_file`, `s_current_file`: Playback file handles
- `s_playback_task_handle`, `s_recording_task_handle`: Task handles
- `s_recording_callback`: Recording callback function
- `s_feed_mutex`, `s_position_mutex`, `s_spectrum_mutex`: Mutexes
- `s_feed_pcm_buffer`, `s_feed_pcm_capacity`: Feed buffer
- `s_playback_position_seconds`, `s_track_duration_seconds`, `s_samples_played`: Position tracking
- `s_spectrum_bands[4]`: Spectrum data (Bass, Mid-low, Mid-high, High)
- FFT state (nếu CONFIG_DSP_OPTIMIZED): `s_fft_input`, `s_window`, `s_pcm_buffer`, `s_pcm_buffer_pos`

**Invariants:**
- Chỉ có một playback task chạy tại một thời điểm
- Chỉ có một recording task chạy tại một thời điểm
- Volume range: 0-100, clamped nếu > 100
- Sample rate: Phải match I2S config hoặc I2S sẽ được reconfigure

### D) Concurrency

- **Context**: 
  - **Init/Start**: Chạy từ bootstrap (main task, single-threaded boot)
  - **Playback task**: Chạy trong FreeRTOS task "sx_audio_file" (priority 4, core 0)
  - **Recording task**: Chạy trong FreeRTOS task "sx_audio_rec" (priority 5, core 1)
  - **Volume ramp task**: Chạy trong FreeRTOS task "sx_audio_vol_ramp" (priority 3, core tskNO_AFFINITY)
  - **Feed PCM**: Có thể được gọi từ bất kỳ task nào (network, TTS, etc.)
- **Thread Safety**: 
  - **State flags**: Không được protect bởi mutex (race condition risk)
  - **Feed operations**: Protected bởi `s_feed_mutex` (```747:749:components/sx_services/sx_audio_service.c```)
  - **Position tracking**: Protected bởi `s_position_mutex` (```393:397:components/sx_services/sx_audio_service.c```)
  - **Spectrum data**: Protected bởi `s_spectrum_mutex` (```950:957:components/sx_services/sx_audio_service.c```)
  - **I2S operations**: ESP-IDF I2S driver là thread-safe
  - **⚠️ RISK**: State flags (`s_playing`, `s_paused`, `s_recording`) không được protect

### E) Memory Ownership

- **Playback file**: 
  - **Owner**: Playback task owns file handle
  - **Lifetime**: Valid từ play start đến play stop
  - **Cleanup**: Close trong playback task cleanup (```495:495:components/sx_services/sx_audio_service.c```)

- **PCM buffers**: 
  - **Owner**: Playback/recording tasks own buffers (malloc trong task)
  - **Lifetime**: Valid trong suốt task execution
  - **Cleanup**: Free trong task cleanup (```494:494:components/sx_services/sx_audio_service.c```)

- **Feed PCM buffer**: 
  - **Owner**: sx_audio_service owns (static, managed bởi buffer pool)
  - **Lifetime**: Persistent, reallocated nếu cần lớn hơn (```752:766:components/sx_services/sx_audio_service.c```)
  - **Cleanup**: Managed bởi buffer pool

- **Decode buffers**: 
  - **Owner**: Playback task owns (malloc trong task)
  - **Lifetime**: Valid trong suốt playback
  - **Cleanup**: Free trong task cleanup (```484:486:components/sx_services/sx_audio_service.c```)

### F) Side Effects

1. **I2S Hardware**: Initialize I2S channels (TX cho playback, RX cho recording) (```237:292:components/sx_services/sx_audio_service.c```)
   - **GPIO**: BCLK=15, WS=16, DOUT=7, DIN=6 (từ Kconfig)
   - **Sample rate**: 16kHz default (có thể thay đổi on-the-fly)
   - **DMA**: 6 descriptors, 240 frames per descriptor

2. **FreeRTOS**: Tạo playback task, recording task, volume ramp task (```536:625:components/sx_services/sx_audio_service.c```)

3. **Events**: Post events khi playback state changes (```503:509:components/sx_services/sx_audio_service.c```):
   - `SX_EVT_AUDIO_PLAYBACK_STARTED`
   - `SX_EVT_AUDIO_PLAYBACK_STOPPED`
   - `SX_EVT_AUDIO_PLAYBACK_PAUSED`
   - `SX_EVT_AUDIO_PLAYBACK_RESUMED`
   - `SX_EVT_AUDIO_RECORDING_STARTED`
   - `SX_EVT_AUDIO_RECORDING_STOPPED`

4. **EQ Processing**: Apply EQ trong playback pipeline (```383:426:462:components/sx_services/sx_audio_service.c```)

5. **Crossfade Processing**: Apply crossfade trong feed pipeline (```773:773:components/sx_services/sx_audio_service.c```)

6. **Volume Processing**: Apply volume với logarithmic scaling (```387:430:466:777:components/sx_services/sx_audio_service.c```)

7. **FFT Processing**: Process FFT cho spectrum (nếu CONFIG_DSP_OPTIMIZED) (```961:1010:components/sx_services/sx_audio_service.c```)

8. **STT Integration**: Send audio chunks to STT service khi recording (```854:859:components/sx_services/sx_audio_service.c```)

9. **Power Management**: Notify audio power management khi có activity (```400:443:479:617:components/sx_services/sx_audio_service.c```)

### G) Call Sites

1. **sx_bootstrap_start()** - Init và start audio service (```402:411:components/sx_core/sx_bootstrap.c```)
2. **Playlist manager** - Play next track (từ playlist auto-play)
3. **Music screens** - Play/pause/stop controls (từ UI)
4. **TTS service** - Feed PCM data từ TTS (từ TTS service)
5. **Radio service** - Feed PCM data từ radio stream (từ radio service)
6. **STT service** - Start recording với STT (```659:669:components/sx_services/sx_audio_service.c```)

### H) Issues/Risks

1. **P0 - State Flags Race Condition**: State flags (`s_playing`, `s_paused`, `s_recording`) không được protect bởi mutex, có thể race condition khi nhiều tasks access đồng thời.
   - **Điều kiện**: Nhiều tasks check/set state flags đồng thời
   - **Cách tái hiện**: Check `s_playing` từ UI task trong khi playback task set `s_playing = false`
   - **Impact**: Inconsistent state, có thể play/stop không đúng

2. **P0 - I2S Reconfiguration Blocking**: I2S reconfiguration khi sample rate thay đổi là blocking (```732:740:components/sx_services/sx_audio_service.c```), có thể block feed operation.
   - **Điều kiện**: Sample rate thay đổi trong khi đang feed PCM
   - **Cách tái hiện**: Feed PCM với sample rate khác
   - **Impact**: Block feed operation, có thể gây audio dropouts

3. **P1 - Task Deletion Race**: Task deletion không có proper synchronization (```571:576:components/sx_services/sx_audio_service.c```), có thể delete task trong khi task đang cleanup.
   - **Điều kiện**: Stop được gọi trong khi playback task đang cleanup
   - **Cách tái hiện**: Stop ngay sau khi playback task bắt đầu cleanup
   - **Impact**: Task handle invalid, có thể crash

4. **P1 - Feed Mutex Timeout**: Feed mutex có timeout 0 (non-blocking) (```747:749:components/sx_services/sx_audio_service.c```), có thể drop PCM data nếu mutex không available.
   - **Điều kiện**: Nhiều tasks feed PCM đồng thời
   - **Cách tái hiện**: Feed PCM từ nhiều tasks
   - **Impact**: PCM data bị drop, audio glitches

5. **P1 - Buffer Reallocation**: Feed buffer được reallocated nếu cần lớn hơn (```752:766:components/sx_services/sx_audio_service.c```), có thể tốn thời gian nếu reallocate thường xuyên.
   - **Điều kiện**: Feed PCM với sample_count thay đổi
   - **Cách tái hiện**: Feed PCM với varying sample counts
   - **Impact**: Memory fragmentation, performance overhead

6. **P2 - Position Tracking Accuracy**: Position được tính từ samples played (```882:884:components/sx_services/sx_audio_service.c```), có thể không chính xác nếu sample rate thay đổi.
   - **Điều kiện**: Sample rate thay đổi trong khi playing
   - **Cách tái hiện**: Play file với sample rate thay đổi
   - **Impact**: Position không chính xác

7. **P2 - Spectrum Mock Data**: Spectrum dùng mock data nếu FFT không available (```1024:1034:components/sx_services/sx_audio_service.c```), không phản ánh actual audio.
   - **Điều kiện**: CONFIG_DSP_OPTIMIZED không enabled hoặc FFT buffer chưa đầy
   - **Cách tái hiện**: Get spectrum khi FFT không ready
   - **Impact**: Spectrum visualization không chính xác

### I) Đề Xuất Cải Thiện

1. **P0**: Thêm mutex để protect state flags
2. **P0**: Thêm async I2S reconfiguration (queue-based) để tránh block feed operation
3. **P1**: Thêm proper task synchronization cho task deletion (dùng task notification)
4. **P1**: Thêm queue cho feed operations để tránh drop PCM data
5. **P1**: Pre-allocate feed buffer với size lớn hơn để tránh reallocation
6. **P2**: Fix position tracking để handle sample rate changes
7. **P2**: Document rõ spectrum mock data behavior

---

## Tổng Kết Phần 1

### Điểm Mạnh

1. **Multi-Format Support**: Hỗ trợ MP3, FLAC, WAV, PCM
2. **Volume Ramping**: Smooth volume transitions với ramping task
3. **Hardware/Software Volume**: Fallback mechanism
4. **Position Tracking**: Track playback position và duration
5. **FFT Support**: Real FFT với ESP-DSP (nếu available)

### Điểm Yếu

1. **Thread Safety**: State flags không được protect
2. **Blocking Operations**: I2S reconfiguration blocking
3. **Task Management**: Task deletion không có proper synchronization
4. **Buffer Management**: Feed buffer reallocation có thể tốn thời gian

### Đề Xuất Cải Thiện Tổng Thể

1. **P0**: Fix thread safety cho state flags
2. **P0**: Fix I2S reconfiguration blocking
3. **P1**: Improve task deletion synchronization
4. **P1**: Optimize feed buffer management

---

**Tiếp theo**: Phần 2 sẽ phân tích **sx_audio_router** (audio routing system).
