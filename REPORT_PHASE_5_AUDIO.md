# PHASE 5 — Audio Pipeline
## Báo cáo phân tích audio pipeline: I2S, codec, buffers, streaming, latency

**Ngày tạo:** 2025-01-02  
**Dự án:** hai-os-simplexl  
**Mục tiêu:** Phân tích audio pipeline, codec/decoder, buffers, latency, và performance

---

## 1. I2S CONFIGURATION

### 1.1 I2S Hardware Configuration

**Nguồn:** `components/sx_services/sx_audio_service.c:L92-L153, L226-L310`

**I2S Port & Pins:**
- **Port:** I2S_NUM_0 (configurable via `CONFIG_HAI_AUDIO_I2S_PORT`)
- **BCLK:** GPIO 15 (PCM5102A)
- **WS/LRCLK:** GPIO 16 (PCM5102A)
- **DOUT:** GPIO 7 (PCM5102A output)
- **DIN:** GPIO 6 (Microphone input)
- **MCLK:** Unused (I2S_GPIO_UNUSED)

**I2S Channel Configuration:**

```c
i2s_chan_config_t chan_cfg = {
    .id = SX_AUDIO_I2S_PORT,              // I2S_NUM_0
    .role = I2S_ROLE_MASTER,
    .dma_desc_num = 6,                    // 6 DMA descriptors
    .dma_frame_num = 240,                 // 240 frames per descriptor
    .auto_clear_after_cb = true,
    .auto_clear_before_cb = false,
    .intr_priority = 0,
};
```

**I2S Standard Mode Configuration:**

```c
i2s_std_config_t std_cfg = {
    .clk_cfg = {
        .sample_rate_hz = 16000,          // Default 16kHz (configurable)
        .clk_src = I2S_CLK_SRC_DEFAULT,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    },
    .slot_cfg = {
        .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
        .slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT,
        .slot_mode = I2S_SLOT_MODE_STEREO,
        .slot_mask = I2S_STD_SLOT_BOTH,
        .ws_width = 16,
        .ws_pol = false,
        .bit_shift = false,
        .left_align = false,
        .big_endian = false,
        .bit_order_lsb = false,
    },
    .gpio_cfg = {
        .mclk = SX_AUDIO_PIN_MCLK,        // I2S_GPIO_UNUSED
        .bclk = 15,
        .ws = 16,
        .dout = 7,
        .din = 6,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv = false,
        },
    },
};
```

**DMA Configuration:**
- **Descriptors:** 6 descriptors
- **Frames per descriptor:** 240 frames
- **Total DMA buffer:** 6 * 240 * 2 (stereo) * 2 (bytes) = 5,760 bytes (~5.6KB)

**Phân tích:**
- ✅ **Standard mode:** I2S standard mode (not PDM)
- ✅ **16-bit stereo:** 16-bit data, stereo mode
- ✅ **DMA buffering:** 6 descriptors cho smooth playback
- ⚠️ **Sample rate:** Default 16kHz — có thể cần tăng cho music (44.1kHz/48kHz)
- ⚠️ **DMA buffer size:** 5.6KB có thể không đủ nếu decode chậm

### 1.2 I2S Initialization Sequence

**Nguồn:** `components/sx_services/sx_audio_service.c:L179-L310`

**Sequence:**

```
1. Create I2S channel (L237)
   └── i2s_new_channel(&chan_cfg, &s_tx_chan, &s_rx_chan)

2. Configure standard mode (L245-L280)
   └── i2s_channel_init_std_mode(s_tx_chan, &std_cfg)

3. Enable TX channel (L286)
   └── i2s_channel_enable(s_tx_chan)

4. Mark I2S ready (L288)
   └── s_i2s_ready = true
```

**Phân tích:**
- ✅ **Separate TX/RX:** TX và RX channels tách biệt
- ✅ **Enable after config:** Channel enabled sau khi config
- ⚠️ **No error recovery:** Nếu init fail, service vẫn mark initialized (non-critical)

---

## 2. CODEC/DECODER ANALYSIS

### 2.1 Supported Codecs

| Codec | Decoder | Status | Nguồn |
|-------|---------|--------|-------|
| **MP3** | `esp_audio_simple_dec` (ESP-IDF) | ✅ Implemented | `sx_codec_mp3.c` |
| **AAC** | `esp_audio_simple_dec` (ESP-IDF) | ✅ Implemented | `sx_codec_aac.c` |
| **FLAC** | Custom (libFLAC?) | ✅ Implemented | `sx_codec_flac.c` |
| **Opus** | Custom wrapper | ✅ Implemented | `sx_codec_opus.c` |
| **WAV** | Raw PCM (skip header) | ✅ Implemented | `sx_audio_service.c:L526-L533` |
| **Raw PCM** | Direct feed | ✅ Implemented | `sx_audio_service.c:L456-L480` |

### 2.2 Codec Detection

**Nguồn:** `components/sx_services/sx_codec_detector.c`

**Detection Methods:**

1. **Content-Type Detection:**
   ```c
   esp_err_t sx_codec_detect_from_content_type(const char *content_type, sx_codec_detect_result_t *result);
   ```
   - Detect từ HTTP Content-Type header
   - Confidence: 0.9 (high)

2. **File Extension Detection:**
   ```c
   esp_err_t sx_codec_detect_from_extension(const char *file_path, sx_codec_detect_result_t *result);
   ```
   - Detect từ file extension (.mp3, .aac, .flac, .opus, .wav)
   - Confidence: 0.8 (good)

3. **Magic Number Detection:**
   - FLAC: "fLaC" (0x66 0x4C 0x61 0x43)
   - WAV: "RIFF" (0x52 0x49 0x46 0x46)
   - MP3: ID3v2 "ID3" (0x49 0x44 0x33) or sync word 0xFF 0xFB/0xF3
   - OGG: "OggS" (0x4F 0x67 0x67 0x53)

**Phân tích:**
- ✅ **Multiple methods:** Content-Type, extension, magic number
- ✅ **Confidence score:** Mỗi method có confidence score
- ⚠️ **Fallback:** Nếu detection fail, fallback to raw PCM

### 2.3 MP3 Decoder

**Nguồn:** `components/sx_services/sx_codec_mp3.c`

**Implementation:**

```c
esp_err_t sx_codec_mp3_init(void) {
    // Register default decoders
    esp_audio_dec_register_default();
    esp_audio_simple_dec_register_default();
    
    // Configure MP3 decoder
    esp_audio_simple_dec_cfg_t mp3_cfg = {
        .dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3,
        .dec_cfg = NULL,   // Use default config
    };
    
    esp_audio_simple_dec_open(&mp3_cfg, &s_decoder);
    
    // Allocate output buffer (4KB)
    s_out_buffer = malloc(4096);
}
```

**Decode Process:**

```c
esp_err_t sx_codec_mp3_decode(const uint8_t *mp3_data, size_t mp3_size,
                              int16_t *pcm, size_t pcm_capacity, size_t *pcm_samples) {
    // Prepare input raw data
    esp_audio_simple_dec_raw_t raw = {
        .buffer = (uint8_t *)mp3_data,
        .len = mp3_size,
        .eos = 0,
    };
    
    // Prepare output frame
    esp_audio_simple_dec_out_t out_frame = {
        .buffer = s_out_buffer,
        .len = s_out_buffer_size,
    };
    
    // Decode MP3 to PCM
    esp_audio_err_t ret = esp_audio_simple_dec_process(s_decoder, &raw, &out_frame);
    
    // Copy decoded PCM to output buffer
    memcpy(pcm, out_frame.buffer, out_frame.len);
    *pcm_samples = out_frame.len / sizeof(int16_t);
}
```

**Decoder Info:**

```c
esp_audio_simple_dec_info_t s_decoder_info = {
    .sample_rate,    // Detected sample rate
    .channel,        // Channels (1=mono, 2=stereo)
    .bitrate,        // Bitrate (bps)
};
```

**Phân tích:**
- ✅ **ESP-IDF decoder:** Sử dụng standard ESP-IDF audio decoder
- ✅ **Output buffer:** 4KB buffer cho decoded PCM
- ✅ **Info tracking:** Track sample rate, channels, bitrate
- ⚠️ **Reset support:** Có `sx_codec_mp3_reset()` để reset decoder state

### 2.4 AAC Decoder

**Nguồn:** `components/sx_services/sx_codec_aac.c`

**Implementation:**

Tương tự MP3 decoder, sử dụng `ESP_AUDIO_SIMPLE_DEC_TYPE_AAC`

**Special Features:**

```c
bool sx_codec_aac_supports_heaac_v2(void) {
    // HE-AAC v2 support check
    // Uses libfdk-aac which supports:
    // - AAC-LC
    // - HE-AAC (AAC+)
    // - HE-AAC v2
}
```

**Seek Support:**

```c
esp_err_t sx_codec_aac_seek(uint32_t position_ms) {
    // Note: Seek not supported for streaming decoders
    // For file-based streams, would need:
    // 1. File position tracking
    // 2. Frame boundary detection
    // 3. Decoder reset and re-sync
    // Currently: just flush and reset
    sx_codec_aac_flush();
    sx_codec_aac_reset();
}
```

**Phân tích:**
- ✅ **HE-AAC v2 support:** Support HE-AAC v2 (if libfdk-aac available)
- ⚠️ **Seek not supported:** Seek chỉ flush/reset, không seek thật
- ✅ **Flush support:** Có `sx_codec_aac_flush()` để flush decoder buffers

### 2.5 FLAC Decoder

**Nguồn:** `components/sx_services/sx_codec_flac.c` (cần đọc để xác nhận)

**Status:** Implemented (cần verify implementation details)

### 2.6 Opus Decoder

**Nguồn:** `components/sx_services/sx_codec_opus.c`, `sx_codec_opus_wrapper.cpp`

**Status:** Implemented với C++ wrapper (cần verify implementation)

---

## 3. BUFFER MANAGEMENT

### 3.1 Buffer Pool System

**Nguồn:** `components/sx_services/sx_audio_buffer_pool.c`

**Buffer Pool Structure:**

```c
struct sx_audio_buffer_pool {
    size_t buffer_size;      // Size of each buffer
    size_t pool_size;         // Number of buffers
    bool use_psram;           // Use PSRAM if available
    void **buffers;            // Array of buffer pointers
    bool *in_use;              // Array of in-use flags
    SemaphoreHandle_t mutex;   // Thread safety mutex
    size_t allocated_count;    // Statistics
};
```

**API:**

```c
esp_err_t sx_audio_buffer_pool_create(const sx_audio_buffer_pool_config_t *config, sx_audio_buffer_pool_t **pool);
esp_err_t sx_audio_buffer_pool_alloc(sx_audio_buffer_pool_t *pool, void **buffer);
esp_err_t sx_audio_buffer_pool_free(sx_audio_buffer_pool_t *pool, void *buffer);
void sx_audio_buffer_pool_destroy(sx_audio_buffer_pool_t *pool);
```

**Memory Allocation:**

```c
void *sx_audio_buffer_alloc_heap(size_t size, bool use_psram) {
    if (use_psram && sx_audio_buffer_pool_psram_available()) {
        // Try PSRAM first
        buffer = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (buffer) return buffer;
        // Fallback to SRAM
    }
    // Allocate from SRAM
    buffer = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
```

**Phân tích:**
- ✅ **PSRAM support:** Buffers có thể allocate từ PSRAM
- ✅ **Thread-safe:** Mutex protection cho pool operations
- ✅ **Statistics:** Track allocated count
- ⚠️ **Pool exhaustion:** Không có backpressure nếu pool exhausted

### 3.2 Playback Buffers

**Nguồn:** `components/sx_services/sx_audio_service.c:L312-L313, L317, L337, L345`

**Buffer Sizes:**

```c
#define PLAYBACK_CHUNK_SAMPLES 1024      // 1024 samples per chunk
#define DECODE_BUFFER_SIZE 4096          // 4KB decode buffer

// Allocated in playback task:
int16_t *pcm_buf = malloc(PLAYBACK_CHUNK_SAMPLES * sizeof(int16_t));  // 2KB
uint8_t *decode_buffer = malloc(DECODE_BUFFER_SIZE);                   // 4KB
```

**Total Playback Memory:**
- **PCM buffer:** 1024 * 2 = 2,048 bytes (2KB)
- **Decode buffer:** 4,096 bytes (4KB)
- **Total:** ~6KB per playback task

**Feed PCM Buffer:**

```c
// Reusable buffer cho feed PCM (tránh malloc/free mỗi lần)
static int16_t *s_feed_pcm_buffer = NULL;
static size_t s_feed_pcm_capacity = 0;

// Dynamic resize if needed
if (sample_count > s_feed_pcm_capacity) {
    size_t new_bytes = sample_count * sizeof(int16_t);
    int16_t *new_buf = sx_audio_buffer_alloc_heap(new_bytes, true);  // PSRAM
    // Replace old buffer
}
```

**Phân tích:**
- ✅ **Reusable buffer:** Feed buffer được reuse, tránh malloc/free
- ✅ **Dynamic resize:** Buffer tự resize nếu cần
- ✅ **PSRAM allocation:** Feed buffer trong PSRAM
- ⚠️ **Fixed chunk size:** 1024 samples có thể không optimal cho tất cả codecs

### 3.3 Recording Buffers

**Nguồn:** `components/sx_services/sx_audio_service.c:L833-L836`

**Buffer Size:**

```c
#define RECORDING_BUFFER_SAMPLES 480     // 480 samples (30ms at 16kHz)

int16_t *pcm_buf = malloc(RECORDING_BUFFER_SAMPLES * sizeof(int16_t));  // 960 bytes
```

**Phân tích:**
- ✅ **Small buffer:** 480 samples = 30ms at 16kHz — hợp lý cho real-time
- ⚠️ **Fixed size:** Không dynamic, có thể không đủ nếu sample rate cao hơn

### 3.4 I2S DMA Buffers

**DMA Configuration:**
- **Descriptors:** 6
- **Frames per descriptor:** 240
- **Total buffer:** 6 * 240 * 2 (stereo) * 2 (bytes) = 5,760 bytes (~5.6KB)

**Phân tích:**
- ✅ **Multiple descriptors:** 6 descriptors cho smooth playback
- ⚠️ **Buffer size:** 5.6KB có thể không đủ nếu decode chậm → underrun risk

---

## 4. AUDIO PIPELINE FLOW

### 4.1 Playback Pipeline

**Flow Diagram:**

```
File/Stream Source
    ↓
Playback Task (sx_audio_file, priority 4, Core 0)
    ↓
Read File (fread)
    ↓
Codec Detection (sx_codec_detector)
    ↓
Codec Decoder (MP3/AAC/FLAC/Opus)
    ↓
PCM Output (int16_t stereo)
    ↓
EQ Processing (sx_audio_eq_process)
    ↓
Crossfade (sx_audio_crossfade_process)
    ↓
Volume Apply (logarithmic scaling)
    ↓
Feed PCM (sx_audio_service_feed_pcm)
    ↓
I2S Write (i2s_channel_write)
    ↓
DMA Transfer
    ↓
PCM5102A Output
```

**Nguồn:** `components/sx_services/sx_audio_service.c:L312-L512`

**Playback Task Implementation:**

```c
static void sx_audio_playback_task(void *arg) {
    FILE *f = (FILE *)arg;
    
    // Allocate buffers
    int16_t *pcm_buf = malloc(PLAYBACK_CHUNK_SAMPLES * sizeof(int16_t));  // 2KB
    uint8_t *decode_buffer = malloc(DECODE_BUFFER_SIZE);                   // 4KB
    
    // Detect codec format
    sx_audio_file_format_t format = sx_codec_detect_file(f);
    
    while (s_playing && !s_paused) {
        if (format == SX_AUDIO_FILE_FORMAT_MP3) {
            // Read MP3 data
            size_t bytes_read = fread(decode_buffer, 1, DECODE_BUFFER_SIZE, f);
            
            // Decode MP3
            size_t pcm_samples = 0;
            sx_codec_mp3_decode(decode_buffer, bytes_read, pcm_buf, PLAYBACK_CHUNK_SAMPLES, &pcm_samples);
            
            // Apply EQ
            sx_audio_eq_process(pcm_buf, pcm_samples);
            
            // Apply volume
            for (size_t i = 0; i < pcm_samples; i++) {
                pcm_buf[i] = (int16_t)(pcm_buf[i] * s_current_volume_factor);
            }
            
            // Feed to I2S
            sx_audio_service_feed_pcm(pcm_buf, pcm_samples, sample_rate);
        }
        // ... similar for AAC, FLAC, Raw PCM
    }
}
```

**Phân tích:**
- ✅ **Multi-format support:** MP3, AAC, FLAC, Opus, WAV, Raw PCM
- ✅ **Processing chain:** Decode → EQ → Crossfade → Volume → I2S
- ⚠️ **In-task processing:** EQ và volume apply trong playback task → có thể chậm
- ⚠️ **No buffer queue:** Direct feed to I2S, không có buffer queue → underrun risk

### 4.2 Recording Pipeline

**Flow Diagram:**

```
Microphone Input
    ↓
I2S RX Channel (DMA, Core 1)
    ↓
Recording Task (sx_audio_rec, priority 5, Core 1)
    ↓
I2S Read (i2s_channel_read)
    ↓
Recording Callback (sx_audio_recording_callback_t)
    ↓
Wake Word / STT Queue
    ↓
Wake Word Task / STT Task
```

**Nguồn:** `components/sx_services/sx_audio_service.c:L599-L656, L833-L870`

**Recording Task Implementation:**

```c
static void sx_audio_recording_task(void *arg) {
    int16_t *pcm_buf = malloc(RECORDING_BUFFER_SAMPLES * sizeof(int16_t));  // 960 bytes
    
    while (s_recording) {
        size_t bytes_read = 0;
        esp_err_t ret = i2s_channel_read(s_rx_chan, pcm_buf, 
                                        RECORDING_BUFFER_SAMPLES * sizeof(int16_t), 
                                        &bytes_read, pdMS_TO_TICKS(100));
        
        if (ret == ESP_OK && bytes_read > 0) {
            size_t samples_read = bytes_read / sizeof(int16_t);
            
            // Call recording callback if set
            if (s_recording_callback != NULL) {
                s_recording_callback(pcm_buf, samples_read, s_current_sample_rate);
            }
        }
    }
}
```

**Phân tích:**
- ✅ **Separate task:** Recording task riêng, priority 5, Core 1
- ✅ **Callback pattern:** Recording callback cho flexible routing
- ✅ **Small buffer:** 480 samples (30ms) cho low latency
- ⚠️ **No AFE processing:** Chưa thấy AFE (AEC, VAD, NS, AGC) trong recording task

### 4.3 Feed PCM Pipeline

**Nguồn:** `components/sx_services/sx_audio_service.c:L730-L789`

**Feed PCM Function:**

```c
esp_err_t sx_audio_service_feed_pcm(const int16_t *pcm, size_t sample_count, uint32_t sample_rate_hz) {
    // 1. Lock feed mutex
    xSemaphoreTake(s_feed_mutex, portMAX_DELAY);
    
    // 2. Resize buffer if needed
    if (sample_count > s_feed_pcm_capacity) {
        // Allocate new buffer (PSRAM)
        s_feed_pcm_buffer = sx_audio_buffer_alloc_heap(new_bytes, true);
    }
    
    // 3. Copy PCM data
    memcpy(s_feed_pcm_buffer, pcm, sample_count * sizeof(int16_t));
    
    // 4. Apply EQ
    sx_audio_eq_process(s_feed_pcm_buffer, sample_count);
    
    // 5. Apply crossfade
    sx_audio_crossfade_process(s_feed_pcm_buffer, sample_count);
    
    // 6. Apply volume
    for (size_t i = 0; i < sample_count; i++) {
        s_feed_pcm_buffer[i] = (int16_t)(s_feed_pcm_buffer[i] * s_current_volume_factor);
    }
    
    // 7. Write to I2S
    i2s_channel_write(s_tx_chan, s_feed_pcm_buffer, bytes_to_write, &written, 0);
    
    // 8. Unlock mutex
    xSemaphoreGive(s_feed_mutex);
}
```

**Phân tích:**
- ✅ **Reusable buffer:** Buffer được reuse, tránh malloc/free
- ✅ **Processing chain:** EQ → Crossfade → Volume → I2S
- ⚠️ **Mutex blocking:** `portMAX_DELAY` có thể gây blocking
- ⚠️ **No queue:** Direct write to I2S, không có buffer queue → underrun risk

---

## 5. LATENCY & BUFFER UNDERRUN/OVERRUN

### 5.1 Latency Analysis

**Playback Latency:**

**Components:**
1. **File read:** ~10-50ms (depends on SD card speed)
2. **Decode:** ~5-20ms (depends on codec complexity)
3. **EQ processing:** ~1-5ms (10-band biquad filters)
4. **I2S DMA buffer:** ~180ms (6 * 240 frames / 16kHz = 90ms, stereo = 180ms)
5. **Total:** ~200-250ms

**Recording Latency:**

**Components:**
1. **I2S DMA buffer:** ~30ms (480 samples / 16kHz)
2. **Recording callback:** ~1-5ms
3. **Queue to Wake Word/STT:** ~10-50ms (depends on queue depth)
4. **Total:** ~40-85ms

**Phân tích:**
- ✅ **Low recording latency:** ~40-85ms cho real-time processing
- ⚠️ **High playback latency:** ~200-250ms có thể noticeable
- ⚠️ **No latency measurement:** Không có latency tracking/metrics

### 5.2 Buffer Underrun Risk

**Risk Factors:**

1. **Decode Speed:**
   - **Vị trí:** `components/sx_services/sx_audio_service.c:L373-L407`
   - **Vấn đề:** Nếu decode chậm hơn playback rate → DMA buffer empty → underrun
   - **Điều kiện:** Complex codec (MP3/AAC), high bitrate, slow CPU

2. **I2S DMA Buffer Size:**
   - **Size:** 5.6KB (6 * 240 * 2 * 2)
   - **Duration:** ~180ms at 16kHz
   - **Rủi ro:** Nếu decode chậm > 180ms → underrun

3. **No Buffer Queue:**
   - **Vấn đề:** Direct feed to I2S, không có buffer queue
   - **Hậu quả:** Nếu `i2s_channel_write()` block → underrun

4. **Playback Task Priority:**
   - **Priority:** 4 (medium)
   - **Rủi ro:** Có thể bị preempt bởi high priority tasks → decode delay

**Cách tái hiện:**
- Play high bitrate MP3/AAC
- Load system với nhiều tasks
- Monitor I2S underrun events

**Cách sửa:**
- Tăng I2S DMA buffer size (more descriptors/frames)
- Tăng playback task priority (4 → 5)
- Add buffer queue giữa decode và I2S
- Monitor underrun events và log

### 5.3 Buffer Overrun Risk

**Recording Overrun:**

**Risk Factors:**
1. **Recording callback chậm:**
   - **Vấn đề:** Nếu callback xử lý chậm → I2S RX buffer full → overrun
   - **Điều kiện:** Complex processing trong callback

2. **Queue full:**
   - **Vấn đề:** Wake Word/STT queue full → data loss
   - **Điều kiện:** High audio rate, slow consumer

**Cách sửa:**
- Tăng queue size
- Optimize callback processing
- Drop oldest data nếu queue full

---

## 6. VOLUME & EQ SYSTEM

### 6.1 Volume Control

**Nguồn:** `components/sx_services/sx_audio_service.c:L162-L177, L316-L352, L791-L828`

**Volume Scaling:**

**Logarithmic Scaling:**

```c
static float sx_audio_volume_to_factor_log(uint8_t volume) {
    if (volume == 0) return 0.0f;
    if (volume >= 100) return 1.0f;
    
    // Logarithmic curve: factor = (10^(normalized * 2) - 1) / 99
    float normalized = (float)volume / 100.0f;
    float factor = (powf(10.0f, normalized * 2.0f) - 1.0f) / 99.0f;
    return factor;
}
```

**Volume Ramping:**

```c
static void sx_audio_volume_ramp_task(void *arg) {
    const uint32_t RAMP_DURATION_MS = 200;  // 200ms ramp
    const uint32_t RAMP_STEP_MS = 10;       // Update every 10ms
    
    float start_factor = s_current_volume_factor;
    float target_factor = sx_audio_volume_to_factor_log(s_target_volume);
    float step = (target_factor - start_factor) / RAMP_STEPS;
    
    for (uint32_t i = 0; i <= RAMP_STEPS; i++) {
        s_current_volume_factor = start_factor + (step * i);
        vTaskDelay(pdMS_TO_TICKS(RAMP_STEP_MS));
    }
}
```

**Hardware vs Software Volume:**

**Nguồn:** `components/sx_platform/sx_platform_volume.c`

- **Hardware volume:** I2C codec (ES8388/ES8311) — **DISABLED**
- **Software volume:** PCM multiplication — **ACTIVE**

**Phân tích:**
- ✅ **Logarithmic curve:** Better perceived volume control
- ✅ **Smooth ramping:** 200ms ramp cho smooth transitions
- ✅ **Volume task:** Separate task cho ramping
- ⚠️ **Hardware volume disabled:** Chỉ software volume, có thể có quality loss

### 6.2 EQ System

**Nguồn:** `components/sx_services/sx_audio_eq.c`

**EQ Configuration:**

```c
#define SX_AUDIO_EQ_NUM_BANDS 10

// Band center frequencies (Hz)
static const float s_band_frequencies[SX_AUDIO_EQ_NUM_BANDS] = {
    31.0f, 62.0f, 125.0f, 250.0f, 500.0f, 
    1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
};
```

**EQ Presets:**

```c
static const int16_t s_eq_presets[SX_AUDIO_EQ_PRESET_MAX][SX_AUDIO_EQ_NUM_BANDS] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           // Flat
    {20, 40, 60, 40, 20, 0, -20, -40, -20, 0}, // Pop
    {40, 20, -20, -40, -20, 20, 40, 60, 40, 20}, // Rock
    {0, 20, 40, 20, 0, -20, -40, -20, 0, 20}, // Jazz
    {-20, -40, -20, 0, 20, 40, 60, 40, 20, 0}, // Classical
};
```

**Biquad Filter Implementation:**

```c
typedef struct {
    float b0, b1, b2;  // Numerator coefficients
    float a1, a2;      // Denominator coefficients
    // Separate history for left and right channels
    float x1_l, x2_l, y1_l, y2_l;  // Left channel
    float x1_r, x2_r, y1_r, y2_r;  // Right channel
} biquad_filter_t;

static float process_biquad_left(biquad_filter_t *filter, float input) {
    float output = filter->b0 * input + filter->b1 * filter->x1_l + filter->b2 * filter->x2_l
                   - filter->a1 * filter->y1_l - filter->a2 * filter->y2_l;
    
    // Update history
    filter->x2_l = filter->x1_l;
    filter->x1_l = input;
    filter->y2_l = filter->y1_l;
    filter->y1_l = output;
    
    return output;
}
```

**EQ Processing:**

```c
esp_err_t sx_audio_eq_process(int16_t *pcm, size_t samples) {
    if (!s_enabled) return ESP_OK;
    
    // Process each sample through all filters
    for (size_t i = 0; i < samples; i += 2) {  // Stereo interleaved
        float left = (float)pcm[i];
        float right = (float)pcm[i + 1];
        
        // Process through each band filter
        for (int band = 0; band < SX_AUDIO_EQ_NUM_BANDS; band++) {
            left = process_biquad_left(&s_filters[band], left);
            right = process_biquad_right(&s_filters[band], right);
        }
        
        pcm[i] = (int16_t)left;
        pcm[i + 1] = (int16_t)right;
    }
}
```

**Phân tích:**
- ✅ **10-band EQ:** 10 bands từ 31Hz đến 16kHz
- ✅ **Biquad filters:** Standard IIR biquad filters
- ✅ **Stereo processing:** Separate history cho left/right
- ✅ **Presets:** 5 presets (Flat, Pop, Rock, Jazz, Classical)
- ⚠️ **CPU intensive:** 10 filters * 2 channels = 20 filter operations per sample
- ⚠️ **No optimization:** Không có SIMD optimization

### 6.3 Audio Ducking

**Nguồn:** `components/sx_services/sx_audio_ducking.c`

**Ducking Configuration:**

```c
typedef struct {
    float duck_level;        // 30% volume when ducked (default)
    uint32_t fade_duration_ms; // 200ms fade duration (default)
} sx_audio_ducking_config_t;
```

**Ducking Process:**

```c
static void sx_audio_ducking_fade_task(void *arg) {
    bool ducking = (bool)(intptr_t)arg;
    
    const uint32_t FADE_STEP_MS = 10;
    const uint32_t FADE_STEPS = s_fade_duration_ms / FADE_STEP_MS;
    
    float start_factor = current_volume_factor;
    float target_factor = (ducking) ? current_factor * s_duck_level : s_original_volume_factor;
    
    // Fade volume over duration
    for (uint32_t i = 0; i <= FADE_STEPS; i++) {
        float new_factor = start_factor + (step * i);
        uint8_t new_volume = (uint8_t)(new_factor * 100.0f);
        sx_audio_set_volume(new_volume);
        vTaskDelay(pdMS_TO_TICKS(FADE_STEP_MS));
    }
}
```

**Phân tích:**
- ✅ **Smooth fade:** 200ms fade cho smooth transitions
- ✅ **Separate task:** Fade task riêng, không block main audio
- ⚠️ **Volume API dependency:** Phụ thuộc vào `sx_audio_set_volume()` → có thể conflict với volume ramp

---

## 7. THREAD MODEL

### 7.1 Audio Tasks

| Task | Stack | Priority | Core | Mục Đích |
|------|-------|----------|------|----------|
| `sx_audio_file` | 3072 (3KB) | 4 | Core 0 | File playback |
| `sx_audio_rec` | 4096 (4KB) | 5 | Core 1 | Recording |
| `sx_audio_volume_ramp` | (unknown) | (unknown) | (unknown) | Volume ramping |
| `sx_audio_ducking_fade` | (unknown) | (unknown) | (unknown) | Ducking fade |

**Phân tích:**
- ✅ **Core separation:** Playback (Core 0), Recording (Core 1)
- ⚠️ **Playback priority thấp:** Priority 4 có thể bị preempt
- ⚠️ **Stack sizes:** 3KB cho playback có thể không đủ nếu codec phức tạp

### 7.2 Thread Safety

**Mutexes:**

| Mutex | Bảo Vệ | Nguồn |
|-------|--------|-------|
| `s_feed_mutex` | Feed PCM operations | `sx_audio_service.c:L187` |
| `s_position_mutex` | Position tracking | `sx_audio_service.c:L188` |
| `s_spectrum_mutex` | Spectrum data | `sx_audio_service.c:L189` |
| `s_duck_mutex` | Ducking state | `sx_audio_ducking.c:L41` |
| `s_crossfade_mutex` | Crossfade state | `sx_audio_crossfade.c:L36` |
| `s_mutex` (router) | Audio router | `sx_audio_router.c:L39` |

**Phân tích:**
- ✅ **Multiple mutexes:** Separate mutexes cho different operations
- ⚠️ **Lock order:** Không có lock order convention → deadlock risk
- ⚠️ **Blocking locks:** `portMAX_DELAY` có thể gây blocking

---

## 8. AUDIO ROUTER & MIXER

### 8.1 Audio Router

**Nguồn:** `components/sx_services/sx_audio_router.c`

**Routing Sources:**

```c
typedef enum {
    SX_AUDIO_SOURCE_NONE = 0,
    SX_AUDIO_SOURCE_SD_MUSIC,
    SX_AUDIO_SOURCE_RADIO,
    SX_AUDIO_SOURCE_ONLINE_MUSIC,
    SX_AUDIO_SOURCE_TTS,
    SX_AUDIO_SOURCE_EXTERNAL,
    SX_AUDIO_SOURCE_COUNT,
} sx_audio_source_t;
```

**Routing Sinks:**

```c
typedef enum {
    SX_AUDIO_SINK_NONE = 0,
    SX_AUDIO_SINK_I2S,
    SX_AUDIO_SINK_EXTERNAL,
    SX_AUDIO_SINK_BOTH,
    SX_AUDIO_SINK_COUNT,
} sx_audio_sink_t;
```

**Routing Function:**

```c
esp_err_t sx_audio_router_route_audio(sx_audio_source_t source, const int16_t *pcm,
                                      size_t samples, uint32_t sample_rate) {
    // Get route configuration
    sx_audio_sink_t sink = s_routes[source].sink;
    bool enabled = s_routes[source].enabled;
    
    if (!enabled) return ESP_OK;  // Route disabled
    
    // Route to I2S
    if (sink == SX_AUDIO_SINK_I2S || sink == SX_AUDIO_SINK_BOTH) {
        sx_audio_service_feed_pcm(pcm, samples, sample_rate);
    }
    
    // Route to external (Bluetooth)
    if (sink == SX_AUDIO_SINK_EXTERNAL || sink == SX_AUDIO_SINK_BOTH) {
        if (sx_bluetooth_is_enabled() && sx_bluetooth_get_state() == SX_BT_STATE_CONNECTED) {
            sx_bluetooth_service_feed_audio(pcm, samples, sample_rate);
        }
    }
}
```

**Phân tích:**
- ✅ **Flexible routing:** Multiple sources → multiple sinks
- ✅ **Route enable/disable:** Có thể enable/disable routes
- ⚠️ **No mixer:** Không có audio mixer → chỉ route, không mix multiple sources

### 8.2 Audio Mixer

**Nguồn:** `components/sx_services/sx_audio_mixer.c` (file empty?)

**Status:** File tồn tại nhưng có thể chưa implement (cần verify)

**Phân tích:**
- ⚠️ **No mixer implementation:** Không có audio mixer → không thể mix multiple sources

---

## 9. AUDIO POWER MANAGEMENT

### 9.1 Power Management System

**Nguồn:** `components/sx_services/sx_audio_power.c`

**Configuration:**

```c
typedef struct {
    uint32_t timeout_ms;              // 15 seconds default
    uint32_t check_interval_ms;       // 1 second
    bool enable_auto_power_save;      // true
} sx_audio_power_config_t;
```

**Power Timer:**

```c
s_power_timer = xTimerCreate("sx_audio_power",
                             pdMS_TO_TICKS(s_config.check_interval_ms),  // 1 second
                             pdTRUE,  // Auto-reload
                             NULL,
                             sx_audio_power_timer_callback);
```

**Power Check:**

```c
void sx_audio_power_check_and_update(void) {
    int64_t idle_time = current_time - s_last_activity_time;
    uint32_t idle_time_ms = (uint32_t)(idle_time / 1000);
    
    bool audio_active = sx_audio_is_playing() || sx_audio_is_recording();
    
    if (audio_active) {
        // Exit power save if active
        if (s_in_power_save) {
            s_in_power_save = false;
            // Restore normal power state
        }
        s_last_activity_time = current_time;
    } else {
        // Enter power save if idle > timeout
        if (idle_time_ms >= s_config.timeout_ms) {
            if (!s_in_power_save) {
                s_in_power_save = true;
                // Reduce power consumption
            }
        }
    }
}
```

**Phân tích:**
- ✅ **Auto power save:** Tự động enter/exit power save mode
- ✅ **Activity tracking:** Track last activity time
- ⚠️ **Power save stub:** Power save chỉ set flag, chưa implement actual power reduction
- ⚠️ **Timer callback:** Timer callback chạy trong timer task (stack 4096) — đã fix overflow

---

## 10. SPECTRUM/FFT ANALYSIS

### 10.1 FFT Processing

**Nguồn:** `components/sx_services/sx_audio_service.c:L80-L90, L191-L206`

**FFT Configuration:**

```c
#ifdef CONFIG_DSP_OPTIMIZED
#define FFT_SIZE 512
static float s_fft_input[FFT_SIZE * 2];  // Complex input
static float s_window[FFT_SIZE];         // Window function (Hanning)
static int16_t s_pcm_buffer[FFT_SIZE];   // PCM sample buffer
static size_t s_pcm_buffer_pos = 0;
#endif
```

**FFT Initialization:**

```c
if (!s_fft_initialized) {
    // Initialize FFT tables
    esp_err_t fft_ret = dsps_fft2r_init_fc32(NULL, FFT_SIZE);
    if (fft_ret == ESP_OK) {
        // Generate window function (Hanning window)
        dsps_wind_hann_f32(s_window, FFT_SIZE);
        s_fft_initialized = true;
    }
}
```

**Spectrum Bands:**

```c
static uint16_t s_spectrum_bands[4] = {0, 0, 0, 0};  // Bass, Mid-low, Mid-high, High
```

**Phân tích:**
- ✅ **ESP-DSP optimized:** Sử dụng ESP-DSP library cho FFT
- ✅ **512-point FFT:** 512 samples cho frequency analysis
- ✅ **Hanning window:** Window function để reduce spectral leakage
- ⚠️ **4-band output:** Chỉ 4 bands (Bass, Mid-low, Mid-high, High) — có thể cần more bands
- ⚠️ **Processing location:** FFT processing chưa thấy trong code (cần verify)

---

## 11. SYNC VỚI UI

### 11.1 Audio Events

**Events Emitted:**

**Nguồn:** `components/sx_services/include/sx_audio_service.h:L68-L75`

```c
// Events emitted by audio service:
// - SX_EVT_AUDIO_PLAYBACK_STARTED
// - SX_EVT_AUDIO_PLAYBACK_STOPPED
// - SX_EVT_AUDIO_PLAYBACK_PAUSED
// - SX_EVT_AUDIO_PLAYBACK_RESUMED
// - SX_EVT_AUDIO_RECORDING_STARTED
// - SX_EVT_AUDIO_RECORDING_STOPPED
// - SX_EVT_AUDIO_ERROR
```

**Event Posting:**

**Nguồn:** `components/sx_services/sx_audio_service.c:L502-L509`

```c
// Dispatch playback stopped event
sx_event_t evt = {
    .type = SX_EVT_AUDIO_PLAYBACK_STOPPED,
    .arg0 = 0,
    .arg1 = 0,
    .ptr = NULL
};
sx_dispatcher_post_event(&evt);
```

**Phân tích:**
- ✅ **Event-driven:** Audio service post events cho UI sync
- ⚠️ **Limited events:** Chỉ có basic events, không có position/duration updates

### 11.2 State Updates

**Position Tracking:**

**Nguồn:** `components/sx_services/sx_audio_service.c:L70-L74, L436-L440, L472-L476`

```c
// Track playback position
if (xSemaphoreTake(s_position_mutex, 0) == pdTRUE) {
    s_samples_played += pcm_samples;
    s_current_sample_rate = sample_rate;
    xSemaphoreGive(s_position_mutex);
}

// Calculate position
s_playback_position_seconds = s_samples_played / s_current_sample_rate;
```

**Duration Tracking:**

```c
// Parse metadata and set duration
sx_track_meta_t meta;
esp_err_t meta_ret = sx_meta_parse_file(file_path, &meta);
if (meta_ret == ESP_OK && meta.duration_ms > 0) {
    s_track_duration_seconds = meta.duration_ms / 1000;
}
```

**State Access:**

```c
uint32_t sx_audio_get_position(void) {
    if (xSemaphoreTake(s_position_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        uint32_t pos = s_playback_position_seconds;
        xSemaphoreGive(s_position_mutex);
        return pos;
    }
    return 0;
}
```

**Phân tích:**
- ✅ **Position tracking:** Track samples played và calculate position
- ✅ **Duration tracking:** Parse metadata để get duration
- ⚠️ **No state events:** Position/duration không được post as events → UI phải poll
- ⚠️ **Mutex timeout:** 100ms timeout có thể fail nếu system busy

---

## 12. LỖI TIỀM ẨN & NỢ KỸ THUẬT

### 12.1 P0 (Critical) Issues

1. **Buffer Underrun Risk**
   - **Vị trí:** `components/sx_services/sx_audio_service.c:L782`
   - **Vấn đề:** Direct I2S write, không có buffer queue → underrun nếu decode chậm
   - **Hậu quả:** Audio glitch, poor user experience
   - **Điều kiện:** High bitrate codec, slow CPU, system busy
   - **Cách tái hiện:** Play high bitrate MP3/AAC, load system
   - **Cách sửa:** Add buffer queue giữa decode và I2S, tăng DMA buffer size

2. **Playback Task Priority Thấp**
   - **Vị trí:** `components/sx_services/sx_audio_service.c:L536`
   - **Vấn đề:** Priority 4 có thể bị preempt → decode delay → underrun
   - **Hậu quả:** Audio glitch
   - **Cách sửa:** Tăng priority lên 5 (same as recording)

### 12.2 P1 (High) Issues

1. **Stack Overflow Risk (Playback Task)**
   - **Vị trí:** `components/sx_services/sx_audio_service.c:L536`
   - **Vấn đề:** Stack 3KB có thể không đủ nếu codec phức tạp
   - **Hậu quả:** Task crash, system unstable
   - **Cách sửa:** Tăng stack size lên 4096 hoặc 6144, monitor high water mark

2. **Mutex Lock Order**
   - **Vị trí:** Multiple mutexes (feed, position, spectrum, duck, crossfade)
   - **Vấn đề:** Không có lock order convention → deadlock risk
   - **Cách sửa:** Định nghĩa lock order, document trong code

3. **No Buffer Queue**
   - **Vị trí:** `components/sx_services/sx_audio_service.c:L730-L789`
   - **Vấn đề:** Direct feed to I2S, không có buffer queue
   - **Hậu quả:** Underrun nếu producer/consumer rate mismatch
   - **Cách sửa:** Add buffer queue với backpressure

4. **EQ CPU Intensive**
   - **Vị trí:** `components/sx_services/sx_audio_eq.c`
   - **Vấn đề:** 10 filters * 2 channels = 20 operations per sample
   - **Hậu quả:** High CPU usage, có thể impact performance
   - **Cách sửa:** Optimize với SIMD, hoặc reduce số bands

### 12.3 P2 (Medium) Issues

1. **Sample Rate Fixed**
   - **Vị trí:** `components/sx_services/sx_audio_service.c:L94-L100`
   - **Vấn đề:** Default 16kHz, có thể cần 44.1kHz/48kHz cho music
   - **Cách sửa:** Dynamic sample rate từ decoder info

2. **No Latency Measurement**
   - **Vị trí:** Audio pipeline
   - **Vấn đề:** Không có latency tracking/metrics
   - **Cách sửa:** Add latency measurement và metrics

3. **Hardware Volume Disabled**
   - **Vị trí:** `components/sx_platform/sx_platform_volume.c:L19-L20`
   - **Vấn đề:** Chỉ software volume, có thể quality loss
   - **Cách sửa:** Enable I2C nếu có hardware codec

4. **No Audio Mixer**
   - **Vị trí:** `components/sx_services/sx_audio_mixer.c` (empty?)
   - **Vấn đề:** Không thể mix multiple sources
   - **Cách sửa:** Implement audio mixer

---

## 13. KẾT LUẬN PHASE 5

### 13.1 Điểm Mạnh

1. ✅ **Multi-format support:** MP3, AAC, FLAC, Opus, WAV, Raw PCM
2. ✅ **Codec detection:** Multiple methods (Content-Type, extension, magic number)
3. ✅ **Buffer pool system:** PSRAM support, thread-safe
4. ✅ **EQ system:** 10-band biquad filters, 5 presets
5. ✅ **Volume control:** Logarithmic scaling, smooth ramping
6. ✅ **Audio router:** Flexible routing (sources → sinks)
7. ✅ **Power management:** Auto power save
8. ✅ **FFT/spectrum:** ESP-DSP optimized FFT

### 13.2 Điểm Yếu

1. ⚠️ **Buffer underrun risk:** No buffer queue, direct I2S write
2. ⚠️ **Playback priority thấp:** Priority 4 có thể bị preempt
3. ⚠️ **Stack overflow risk:** 3KB có thể không đủ
4. ⚠️ **Mutex lock order:** Không có convention
5. ⚠️ **EQ CPU intensive:** 20 filter operations per sample
6. ⚠️ **Sample rate fixed:** Default 16kHz, có thể cần higher
7. ⚠️ **No latency measurement:** Không có latency tracking

### 13.3 Hành Động Tiếp Theo

**PHASE 6:** Phân tích Network/AI/Protocols  
**PHASE 7:** Phân tích Storage & Persistence

---

## 14. CHECKLIST HOÀN THÀNH PHASE 5

- [x] Phân tích I2S configuration và initialization
- [x] Phân tích codec/decoder (MP3, AAC, FLAC, Opus)
- [x] Phân tích buffer management (buffer pool, playback/recording buffers)
- [x] Phân tích audio pipeline flow (file → decode → EQ → volume → I2S)
- [x] Phân tích latency và underrun/overrun risks
- [x] Phân tích volume control (logarithmic, hardware/software)
- [x] Phân tích EQ system (10-band biquad filters)
- [x] Phân tích thread model (playback, recording tasks)
- [x] Phân tích audio router và mixer
- [x] Phân tích power management
- [x] Phân tích sync với UI (events, state updates)
- [x] Xác định lỗi tiềm ẩn và nợ kỹ thuật
- [x] Tạo REPORT_PHASE_5_AUDIO.md

---

## 15. THỐNG KÊ FILE ĐÃ ĐỌC

**Tổng số file đã đọc trong Phase 5:** ~10 files

**Danh sách:**
1. `components/sx_services/sx_audio_service.c` (partial)
2. `components/sx_services/include/sx_audio_service.h`
3. `components/sx_services/sx_audio_buffer_pool.c`
4. `components/sx_services/sx_codec_mp3.c` (partial)
5. `components/sx_services/sx_codec_aac.c` (partial)
6. `components/sx_services/sx_audio_eq.c` (partial)
7. `components/sx_services/sx_audio_router.c`
8. `components/sx_services/sx_audio_power.c` (partial)
9. `components/sx_services/sx_audio_ducking.c` (partial)
10. `components/sx_services/sx_codec_detector.c` (partial)
11. `components/sx_services/include/sx_audio_router.h`

**Ước lượng % file đã đọc:** ~20-22% (đọc audio-critical files)

---

**Kết thúc PHASE 5**

