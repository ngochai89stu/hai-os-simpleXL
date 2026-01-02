#include "sx_audio_service.h"
#include "sx_media_metadata.h"

#include <esp_log.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_sd_service.h"
#include "sx_audio_eq.h"
#include "sx_audio_crossfade.h"
#include "sx_stt_service.h"
#include "sx_platform_volume.h"
#include "sx_playlist_manager.h"
#include "sx_codec_mp3.h"
#include "sx_codec_flac.h"
#include "sx_codec_detector.h"
#include "sx_codec_common.h"
#include "sx_audio_power.h"
#include "sx_audio_buffer_pool.h"
#include "driver/i2s_std.h"
#include <math.h>

static const char *TAG = "sx_audio";

// Phase 4: Audio service state
static bool s_initialized = false;
static bool s_playing = false;
static bool s_paused = false;
static bool s_recording = false;
static uint8_t s_volume = 50; // Default volume: 50%
static uint8_t s_target_volume = 50; // Target volume for ramp
static float s_current_volume_factor = 0.5f; // Current volume factor (0.0-1.0) for smooth ramp
static bool s_volume_ramping = false; // True if volume is currently ramping
static TaskHandle_t s_volume_ramp_task_handle = NULL;

// I2S handles (std mode)
static i2s_chan_handle_t s_tx_chan = NULL;
static i2s_chan_handle_t s_rx_chan = NULL;
static bool s_i2s_ready = false;
static i2s_std_config_t s_i2s_std_cfg = {0};   // Keep last config for re-apply
static uint32_t s_current_sample_rate = 16000; // default

// Playback state
static FILE *s_playback_file = NULL;
static char s_current_file[256] = {0};
static TaskHandle_t s_playback_task_handle = NULL;

// Recording state (reserved for future recording feature)
// static FILE *s_recording_file = NULL;
// static char s_recording_file_path[256] = {0};
static TaskHandle_t s_recording_task_handle = NULL;
static sx_audio_recording_callback_t s_recording_callback = NULL;

// Feed-from-stream mutex
static SemaphoreHandle_t s_feed_mutex = NULL;

// Reusable buffer cho đường feed PCM (tránh malloc/free mỗi lần gọi)
static int16_t *s_feed_pcm_buffer = NULL;
static size_t s_feed_pcm_capacity = 0;

// Phase 1: Hybrid Music Screen - Position and Duration tracking
static uint32_t s_playback_position_seconds = 0;  // Current playback position in seconds
static uint32_t s_track_duration_seconds = 0;     // Total track duration in seconds
static uint32_t s_samples_played = 0;              // Total samples played (for position calculation)
static SemaphoreHandle_t s_position_mutex = NULL;  // Mutex for position tracking

// Phase 1: Hybrid Music Screen - Spectrum/FFT data
static uint16_t s_spectrum_bands[4] = {0, 0, 0, 0};  // Bass, Mid-low, Mid-high, High
static SemaphoreHandle_t s_spectrum_mutex = NULL;

// Phase 5: FFT processing state (for real FFT with ESP-DSP)
#ifdef CONFIG_DSP_OPTIMIZED
#include "dsps_fft2r.h"
#include "dsps_wind.h"
#define FFT_SIZE 512
static float s_fft_input[FFT_SIZE * 2];  // Complex input (real + imag)
static float s_window[FFT_SIZE];         // Window function
static bool s_fft_initialized = false;
static int16_t s_pcm_buffer[FFT_SIZE];   // PCM sample buffer for FFT
static size_t s_pcm_buffer_pos = 0;      // Current position in buffer
#endif

// Cấu hình audio/I2S: đọc từ Kconfig (Kconfig.projbuild)
// Use SX_AUDIO config if available, otherwise fallback to HAI_AUDIO or defaults
#ifdef CONFIG_SX_AUDIO_SAMPLE_RATE
#define SX_AUDIO_SAMPLE_RATE    CONFIG_SX_AUDIO_SAMPLE_RATE
#elif defined(CONFIG_HAI_AUDIO_SAMPLE_RATE)
#define SX_AUDIO_SAMPLE_RATE    CONFIG_HAI_AUDIO_SAMPLE_RATE
#else
#define SX_AUDIO_SAMPLE_RATE    16000
#endif

#ifdef CONFIG_HAI_AUDIO_I2S_PORT
#define SX_AUDIO_I2S_PORT       CONFIG_HAI_AUDIO_I2S_PORT
#else
#define SX_AUDIO_I2S_PORT       0
#endif

#define SX_AUDIO_BITS           I2S_DATA_BIT_WIDTH_16BIT

#ifdef CONFIG_HAI_AUDIO_DMA_DESC_NUM
#define SX_AUDIO_DMA_DESC_NUM   CONFIG_HAI_AUDIO_DMA_DESC_NUM
#else
#define SX_AUDIO_DMA_DESC_NUM   6
#endif

#ifdef CONFIG_HAI_AUDIO_DMA_FRAME_NUM
#define SX_AUDIO_DMA_FRAME_NUM  CONFIG_HAI_AUDIO_DMA_FRAME_NUM
#else
#define SX_AUDIO_DMA_FRAME_NUM  240
#endif

// Hardware pin mapping - cấu hình qua Kconfig, mặc định theo PCM5102A board hiện tại
// PCM5102A Audio Output: GPIO 7 (DOUT), GPIO 15 (BCLK), GPIO 16 (WS)
// Microphone Input: GPIO 4, 5, 6 (cần xác nhận chi tiết từng chân)
#ifdef CONFIG_HAI_AUDIO_PIN_MCLK
#define SX_AUDIO_PIN_MCLK       ((CONFIG_HAI_AUDIO_PIN_MCLK < 0) ? I2S_GPIO_UNUSED : CONFIG_HAI_AUDIO_PIN_MCLK)
#else
#define SX_AUDIO_PIN_MCLK       I2S_GPIO_UNUSED
#endif

#ifdef CONFIG_HAI_AUDIO_PIN_BCLK
#define SX_AUDIO_PIN_BCLK       CONFIG_HAI_AUDIO_PIN_BCLK
#else
#define SX_AUDIO_PIN_BCLK       15
#endif

#ifdef CONFIG_HAI_AUDIO_PIN_WS
#define SX_AUDIO_PIN_WS         CONFIG_HAI_AUDIO_PIN_WS
#else
#define SX_AUDIO_PIN_WS         16
#endif

#ifdef CONFIG_HAI_AUDIO_PIN_DOUT
#define SX_AUDIO_PIN_DOUT       CONFIG_HAI_AUDIO_PIN_DOUT
#else
#define SX_AUDIO_PIN_DOUT       7
#endif

#ifdef CONFIG_HAI_AUDIO_PIN_DIN
#define SX_AUDIO_PIN_DIN        CONFIG_HAI_AUDIO_PIN_DIN
#else
#define SX_AUDIO_PIN_DIN        6
#endif

// Audio service task
static void sx_audio_task(void *arg);
static void sx_audio_playback_task(void *arg);
static void sx_audio_recording_task(void *arg);
static void sx_audio_volume_ramp_task(void *arg);

// Logarithmic volume scaling (maps 0-100 to 0.0-1.0 with logarithmic curve)
static float sx_audio_volume_to_factor_log(uint8_t volume) {
    if (volume == 0) return 0.0f;
    if (volume >= 100) return 1.0f;
    // Logarithmic curve: factor = 10^((volume - 100) / 33.33)
    // This gives a nice logarithmic curve from 0 to 100
    float normalized = (float)volume / 100.0f;
    // Use exponential curve for better perceived volume control
    // factor = (10^(normalized * 2) - 1) / 99
    float factor = (powf(10.0f, normalized * 2.0f) - 1.0f) / 99.0f;
    return factor;
}

// Linear volume scaling (fallback, reserved for future use)
// static float sx_audio_volume_to_factor_linear(uint8_t volume) {
//     return (float)volume / 100.0f;
// }

esp_err_t sx_audio_service_init(void) {
    if (s_initialized) {
        ESP_LOGW(TAG, "Audio service already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing audio service...");

    s_feed_mutex = xSemaphoreCreateMutex();
    s_position_mutex = xSemaphoreCreateMutex();
    s_spectrum_mutex = xSemaphoreCreateMutex();
    
    // Phase 5: Initialize FFT if ESP-DSP is available
#ifdef CONFIG_DSP_OPTIMIZED
    if (!s_fft_initialized) {
        // Initialize FFT tables
        esp_err_t fft_ret = dsps_fft2r_init_fc32(NULL, FFT_SIZE);
        if (fft_ret == ESP_OK) {
            // Generate window function (Hanning window)
            dsps_wind_hann_f32(s_window, FFT_SIZE);
            s_fft_initialized = true;
            s_pcm_buffer_pos = 0;
            ESP_LOGI(TAG, "FFT initialized (size=%d)", FFT_SIZE);
        } else {
            ESP_LOGW(TAG, "FFT initialization failed: %s", esp_err_to_name(fft_ret));
        }
    }
#endif
    
    // Phase 5: Initialize hardware volume control
    esp_err_t hw_vol_ret = sx_platform_hw_volume_init();
    if (hw_vol_ret != ESP_OK) {
        ESP_LOGW(TAG, "Hardware volume init failed (non-critical): %s", esp_err_to_name(hw_vol_ret));
    } else {
        if (sx_platform_hw_volume_available()) {
            ESP_LOGI(TAG, "Hardware volume control available");
        } else {
            ESP_LOGI(TAG, "Using software volume control");
        }
    }
    
    // Initialize EQ service
    esp_err_t eq_ret = sx_audio_eq_init(SX_AUDIO_SAMPLE_RATE);
    if (eq_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize EQ service: %s", esp_err_to_name(eq_ret));
    }

    // Initialize I2S hardware (std mode)
    i2s_chan_config_t chan_cfg = {
        .id = SX_AUDIO_I2S_PORT,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = SX_AUDIO_DMA_DESC_NUM,
        .dma_frame_num = SX_AUDIO_DMA_FRAME_NUM,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };

    esp_err_t ret = i2s_new_channel(&chan_cfg, &s_tx_chan, &s_rx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(ret));
        s_initialized = true;
        s_i2s_ready = false;
        return ESP_OK; // non-critical
    }

    s_i2s_std_cfg = (i2s_std_config_t){
        .clk_cfg = {
            .sample_rate_hz = SX_AUDIO_SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = SX_AUDIO_BITS,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = SX_AUDIO_BITS,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = true,
            .big_endian = false,
            .bit_order_lsb = false,
        },
        .gpio_cfg = {
            .mclk = SX_AUDIO_PIN_MCLK,
            .bclk = SX_AUDIO_PIN_BCLK,
            .ws = SX_AUDIO_PIN_WS,
            .dout = SX_AUDIO_PIN_DOUT,
            .din = SX_AUDIO_PIN_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(s_tx_chan, &s_i2s_std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode failed: %s", esp_err_to_name(ret));
        s_initialized = true;
        s_i2s_ready = false;
        return ESP_OK; // non-critical
    }

    ret = i2s_channel_enable(s_tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable (tx) failed: %s", esp_err_to_name(ret));
        s_initialized = true;
        s_i2s_ready = false;
        return ESP_OK; // non-critical
    }

    s_initialized = true;
    s_i2s_ready = true;
    ESP_LOGI(TAG, "Audio service initialized (I2S ready)");
    return ESP_OK;
}

esp_err_t sx_audio_service_start(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Optional background task - currently unused
    (void)sx_audio_task;

    ESP_LOGI(TAG, "Audio service started");
    return ESP_OK;
}

#define PLAYBACK_CHUNK_SAMPLES 1024
#define DECODE_BUFFER_SIZE 4096

static void sx_audio_playback_task(void *arg) {
    FILE *f = (FILE *)arg;
    int16_t *pcm_buf = malloc(PLAYBACK_CHUNK_SAMPLES * sizeof(int16_t));
    if (!pcm_buf) {
        ESP_LOGE(TAG, "No mem for pcm buffer");
        fclose(f);
        vTaskDelete(NULL);
        return;
    }
    
    // Detect file format
    sx_audio_file_format_t format = sx_codec_detect_file_format(s_current_file, f);
    fseek(f, 0, SEEK_SET);
    
    // Initialize decoders if needed
    bool mp3_decoder_initialized = false;
    bool flac_decoder_initialized = false;
    uint8_t *decode_buffer = NULL;
    
    if (format == SX_AUDIO_FILE_FORMAT_MP3) {
        if (sx_codec_mp3_init() == ESP_OK) {
            mp3_decoder_initialized = true;
            decode_buffer = (uint8_t *)malloc(DECODE_BUFFER_SIZE);
        } else {
            ESP_LOGE(TAG, "Failed to initialize MP3 decoder");
            format = SX_AUDIO_FILE_FORMAT_UNKNOWN; // Fallback to raw
        }
    } else if (format == SX_AUDIO_FILE_FORMAT_FLAC) {
        if (sx_codec_flac_init() == ESP_OK) {
            flac_decoder_initialized = true;
            decode_buffer = (uint8_t *)malloc(DECODE_BUFFER_SIZE);
        } else {
            ESP_LOGE(TAG, "Failed to initialize FLAC decoder");
            format = SX_AUDIO_FILE_FORMAT_UNKNOWN; // Fallback to raw
        }
    }
    
    // Skip WAV header if needed
    if (format == SX_AUDIO_FILE_FORMAT_WAV) {
        uint8_t hdr[44];
        if (fread(hdr, 1, 44, f) == 44 && sx_codec_is_wav_header(hdr)) {
            // Header already skipped
        } else {
            fseek(f, 0, SEEK_SET);
        }
    }
    
    uint32_t sample_rate = SX_AUDIO_SAMPLE_RATE;
    
    while (!feof(f) && s_playing && !s_paused) {
        if (format == SX_AUDIO_FILE_FORMAT_MP3 && mp3_decoder_initialized && decode_buffer) {
            // Read MP3 data
            size_t bytes_read = fread(decode_buffer, 1, DECODE_BUFFER_SIZE, f);
            if (bytes_read == 0) break;
            
            // Decode MP3
            size_t pcm_samples = 0;
            esp_err_t decode_ret = sx_codec_mp3_decode(decode_buffer, bytes_read,
                                                       pcm_buf, PLAYBACK_CHUNK_SAMPLES,
                                                       &pcm_samples);
            if (decode_ret == ESP_OK && pcm_samples > 0) {
                // Get sample rate from decoder
                sx_mp3_decoder_info_t info = {0};
                if (sx_codec_mp3_get_info(&info) == ESP_OK && info.info_ready) {
                    sample_rate = info.sample_rate_hz;
                }
                
                // Apply EQ
                sx_audio_eq_process(pcm_buf, pcm_samples);
                
                // Apply volume
                for (size_t i = 0; i < pcm_samples; i++) {
                    pcm_buf[i] = (int16_t)(pcm_buf[i] * s_current_volume_factor);
                }
                
                sx_audio_service_feed_pcm(pcm_buf, pcm_samples, sample_rate);
                
                // Phase 1: Track playback position
                if (s_position_mutex != NULL && xSemaphoreTake(s_position_mutex, 0) == pdTRUE) {
                    s_samples_played += pcm_samples;
                    s_current_sample_rate = sample_rate;
                    xSemaphoreGive(s_position_mutex);
                }
                
                // Notify audio power management
                sx_audio_power_notify_activity();
            } else if (decode_ret == ESP_ERR_NOT_FINISHED) {
                // Need more input, continue reading
                continue;
            } else {
                // Decode error, try to continue
                ESP_LOGW(TAG, "MP3 decode error: %s", esp_err_to_name(decode_ret));
            }
        } else if (format == SX_AUDIO_FILE_FORMAT_FLAC && flac_decoder_initialized && decode_buffer) {
            // Read FLAC data
            size_t bytes_read = fread(decode_buffer, 1, DECODE_BUFFER_SIZE, f);
            if (bytes_read == 0) break;
            
            // Decode FLAC
            size_t pcm_samples = 0;
            esp_err_t decode_ret = sx_codec_flac_decode(decode_buffer, bytes_read,
                                                        pcm_buf, PLAYBACK_CHUNK_SAMPLES,
                                                        &pcm_samples);
            if (decode_ret == ESP_OK && pcm_samples > 0) {
                // Get sample rate from decoder
                sx_flac_decoder_info_t info = {0};
                if (sx_codec_flac_get_info(&info) == ESP_OK && info.info_ready) {
                    sample_rate = info.sample_rate_hz;
                }
                
                // Apply EQ
                sx_audio_eq_process(pcm_buf, pcm_samples);
                
                // Apply volume
                for (size_t i = 0; i < pcm_samples; i++) {
                    pcm_buf[i] = (int16_t)(pcm_buf[i] * s_current_volume_factor);
                }
                
                sx_audio_service_feed_pcm(pcm_buf, pcm_samples, sample_rate);
                
                // Phase 1: Track playback position
                if (s_position_mutex != NULL && xSemaphoreTake(s_position_mutex, 0) == pdTRUE) {
                    s_samples_played += pcm_samples;
                    s_current_sample_rate = sample_rate;
                    xSemaphoreGive(s_position_mutex);
                }
                
                // Notify audio power management
                sx_audio_power_notify_activity();
            } else if (decode_ret == ESP_ERR_NOT_FINISHED || decode_ret == ESP_ERR_NOT_SUPPORTED) {
                // Need more input or not supported, continue reading
                if (decode_ret == ESP_ERR_NOT_SUPPORTED) {
                    ESP_LOGW(TAG, "FLAC decoder not supported, falling back to raw PCM");
                    format = SX_AUDIO_FILE_FORMAT_RAW_PCM;
                    fseek(f, 0, SEEK_SET);
                }
                continue;
            } else {
                // Decode error, try to continue
                ESP_LOGW(TAG, "FLAC decode error: %s", esp_err_to_name(decode_ret));
            }
        } else {
            // Raw PCM or WAV (header already skipped)
            size_t read = fread(pcm_buf, sizeof(int16_t), PLAYBACK_CHUNK_SAMPLES, f);
            if (read == 0) break;
            
            // Apply EQ before feeding to I2S
            sx_audio_eq_process(pcm_buf, read);
            
            // Apply volume with logarithmic scaling
            for (size_t i = 0; i < read; i++) {
                pcm_buf[i] = (int16_t)(pcm_buf[i] * s_current_volume_factor);
            }
            
            sx_audio_service_feed_pcm(pcm_buf, read, sample_rate);
            
            // Phase 1: Track playback position
            if (s_position_mutex != NULL && xSemaphoreTake(s_position_mutex, 0) == pdTRUE) {
                s_samples_played += read;
                s_current_sample_rate = sample_rate;
                xSemaphoreGive(s_position_mutex);
            }
            
            // Notify audio power management
            sx_audio_power_notify_activity();
        }
    }
    
    // Cleanup
    if (decode_buffer) {
        free(decode_buffer);
    }
    if (mp3_decoder_initialized) {
        sx_codec_mp3_reset();
    }
    if (flac_decoder_initialized) {
        sx_codec_flac_reset();
    }
    
    free(pcm_buf);
    fclose(f);
    s_playing = false;
    s_playback_task_handle = NULL;
    
    // Phase 5: Gapless playback - preload next track before stopping
    sx_playlist_preload_next();
    
    // Dispatch playback stopped event for auto-play next track
    sx_event_t evt = {
        .type = SX_EVT_AUDIO_PLAYBACK_STOPPED,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = NULL
    };
    sx_dispatcher_post_event(&evt);
    
    vTaskDelete(NULL);
}

esp_err_t sx_audio_play_file(const char *file_path) {
    if (!s_initialized || !s_i2s_ready) return ESP_ERR_INVALID_STATE;
    if (!file_path) return ESP_ERR_INVALID_ARG;
    if (s_playing) {
        sx_audio_stop();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    FILE *f = fopen(file_path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "failed to open %s", file_path);
        return ESP_FAIL;
    }
    // Skip WAV header if present
    uint8_t hdr[44];
    size_t r = fread(hdr, 1, 44, f);
    if (r==44 && sx_codec_is_wav_header(hdr)) {
        ESP_LOGI(TAG, "WAV header detected – skipping 44 bytes");
    } else {
        fseek(f, 0, SEEK_SET); // raw pcm
    }
    s_playing = true;
    // Tối ưu: Giảm stack size từ 4096 xuống 3072 để tiết kiệm memory
    BaseType_t ret = xTaskCreatePinnedToCore(sx_audio_playback_task, "sx_audio_file", 3072, f, 4, &s_playback_task_handle, 0);
    if (ret!=pdPASS) {
        fclose(f);
        s_playing = false;
        return ESP_ERR_NO_MEM;
    }
    strncpy(s_current_file, file_path, sizeof(s_current_file)-1);
    
    // Phase 5: Parse metadata and set duration
    sx_track_meta_t meta;
    esp_err_t meta_ret = sx_meta_parse_file(file_path, &meta);
    if (meta_ret == ESP_OK && meta.duration_ms > 0) {
        if (xSemaphoreTake(s_position_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            s_track_duration_seconds = meta.duration_ms / 1000;
            xSemaphoreGive(s_position_mutex);
            ESP_LOGI(TAG, "Set track duration: %lu seconds from metadata", (unsigned long)s_track_duration_seconds);
        }
    } else {
        // Try duration estimate
        uint32_t est_duration_ms = sx_meta_estimate_duration(file_path);
        if (est_duration_ms > 0) {
            if (xSemaphoreTake(s_position_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                s_track_duration_seconds = est_duration_ms / 1000;
                xSemaphoreGive(s_position_mutex);
                ESP_LOGI(TAG, "Set track duration: %lu seconds (estimated)", (unsigned long)s_track_duration_seconds);
            }
        }
    }
    
    return ESP_OK;
}

esp_err_t sx_audio_stop(void) {
    s_playing = false;
    s_paused = false;
    if (s_playback_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (s_playback_task_handle != NULL) {
            vTaskDelete(s_playback_task_handle);
            s_playback_task_handle = NULL;
        }
    }
    if (s_playback_file != NULL) {
        fclose(s_playback_file);
        s_playback_file = NULL;
    }
    return ESP_OK;
}

esp_err_t sx_audio_pause(void) {
    s_paused = true;
    return ESP_OK;
}

esp_err_t sx_audio_resume(void) {
    s_paused = false;
    return ESP_OK;
}

bool sx_audio_is_playing(void) {
    return s_playing && !s_paused;
}

esp_err_t sx_audio_start_recording(void) {
    if (!s_initialized || !s_i2s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_recording) {
        return ESP_OK;
    }
    if (s_rx_chan == NULL) {
        ESP_LOGE(TAG, "RX channel not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = i2s_channel_enable(s_rx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable RX channel: %s", esp_err_to_name(ret));
        return ret;
    }
    s_recording = true;
    // Notify audio power management
    sx_audio_power_notify_activity();
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        sx_audio_recording_task,
        "sx_audio_rec",
        4096,
        NULL,
        5,
        &s_recording_task_handle,
        1
    );
    if (task_ret != pdPASS) {
        s_recording = false;
        i2s_channel_disable(s_rx_chan);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Recording started");
    return ESP_OK;
}

esp_err_t sx_audio_stop_recording(void) {
    if (!s_recording) {
        return ESP_OK;
    }
    s_recording = false;
    if (s_recording_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (s_recording_task_handle != NULL) {
            vTaskDelete(s_recording_task_handle);
            s_recording_task_handle = NULL;
        }
    }
    if (s_rx_chan != NULL) {
        i2s_channel_disable(s_rx_chan);
    }
    ESP_LOGI(TAG, "Recording stopped");
    return ESP_OK;
}

bool sx_audio_is_recording(void) {
    return s_recording;
}

esp_err_t sx_audio_start_recording_with_stt(void) {
    // Start STT session first
    esp_err_t ret = sx_stt_start_session(NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to start STT session: %s", esp_err_to_name(ret));
        // Continue with recording even if STT fails
    }
    
    // Start recording
    return sx_audio_start_recording();
}

esp_err_t sx_audio_set_recording_callback(sx_audio_recording_callback_t callback) {
    s_recording_callback = callback;
    ESP_LOGI(TAG, "Recording callback %s", callback ? "set" : "cleared");
    return ESP_OK;
}

esp_err_t sx_audio_set_volume(uint8_t volume) {
    if (volume > 100) volume = 100;
    
    // Phase 5: Try hardware volume first, fallback to software
    if (sx_platform_hw_volume_available()) {
        esp_err_t ret = sx_platform_hw_volume_set(volume);
        if (ret == ESP_OK) {
            ESP_LOGD(TAG, "Volume set via hardware: %d%%", volume);
            s_target_volume = volume;
            s_volume = volume;
            s_current_volume_factor = sx_audio_volume_to_factor_log(volume);
            return ESP_OK;
        }
        // Fall through to software volume if hardware fails
    }
    
    s_target_volume = volume;
    
    // If volume ramp task is not running, start it
    if (s_volume_ramp_task_handle == NULL) {
        s_volume_ramping = true;
        BaseType_t ret = xTaskCreatePinnedToCore(
            sx_audio_volume_ramp_task,
            "sx_audio_vol_ramp",
            2048,
            NULL,
            3,
            &s_volume_ramp_task_handle,
            tskNO_AFFINITY
        );
        if (ret != pdPASS) {
            // Fallback: set volume immediately
            s_volume = volume;
            s_current_volume_factor = sx_audio_volume_to_factor_log(volume);
            ESP_LOGW(TAG, "Failed to create volume ramp task, setting volume immediately");
            return ESP_ERR_NO_MEM;
        }
    }
    
    return ESP_OK;
}

uint8_t sx_audio_get_volume(void) {
    return s_volume;
}

// Feed raw PCM samples (signed 16-bit stereo interleaved) into I2S pipeline.
esp_err_t sx_audio_service_feed_pcm(const int16_t *pcm, size_t sample_count, uint32_t sample_rate_hz) {
    if (!s_initialized || !s_i2s_ready || pcm == NULL || sample_count == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    // If sample rate changed, reconfigure I2S on-the-fly (blocking)
    if (sample_rate_hz != s_current_sample_rate) {
        s_i2s_std_cfg.clk_cfg.sample_rate_hz = sample_rate_hz;
        i2s_channel_disable(s_tx_chan);
        i2s_channel_init_std_mode(s_tx_chan, &s_i2s_std_cfg);
        i2s_channel_enable(s_tx_chan);
        s_current_sample_rate = sample_rate_hz;
        // Update EQ sample rate
        sx_audio_eq_set_sample_rate(sample_rate_hz);
        // Update crossfade sample rate
        sx_audio_crossfade_set_sample_rate(sample_rate_hz);
        ESP_LOGI(TAG, "I2S sample rate updated to %u Hz", sample_rate_hz);
    }

    if (s_tx_chan == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_feed_mutex && xSemaphoreTake(s_feed_mutex, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    // Bảo đảm buffer dùng lại đủ lớn cho đợt feed hiện tại
    if (sample_count > s_feed_pcm_capacity) {
        size_t new_bytes = sample_count * sizeof(int16_t);
        int16_t *new_buf = (int16_t *)sx_audio_buffer_alloc_heap(new_bytes, true);
        if (new_buf == NULL) {
            if (s_feed_mutex) {
                xSemaphoreGive(s_feed_mutex);
            }
            return ESP_ERR_NO_MEM;
        }
        if (s_feed_pcm_buffer) {
            sx_audio_buffer_free_heap(s_feed_pcm_buffer);
        }
        s_feed_pcm_buffer = new_buf;
        s_feed_pcm_capacity = sample_count;
    }

    // Copy sang buffer dùng lại rồi xử lý in-place
    memcpy(s_feed_pcm_buffer, pcm, sample_count * sizeof(int16_t));
    sx_audio_eq_process(s_feed_pcm_buffer, sample_count);
    
    // Apply crossfade if active
    sx_audio_crossfade_process(s_feed_pcm_buffer, sample_count);
    
    // Apply volume with logarithmic scaling
    for (size_t i = 0; i < sample_count; i++) {
        s_feed_pcm_buffer[i] = (int16_t)(s_feed_pcm_buffer[i] * s_current_volume_factor);
    }

    size_t bytes_to_write = sample_count * sizeof(int16_t);
    size_t written = 0;
    esp_err_t ret = i2s_channel_write(s_tx_chan, s_feed_pcm_buffer, bytes_to_write, &written, 0);

    if (s_feed_mutex) {
        xSemaphoreGive(s_feed_mutex);
    }

    return (ret == ESP_OK && written == bytes_to_write) ? ESP_OK : ESP_FAIL;
}

// Volume ramp task - smooth transition between volume levels
static void sx_audio_volume_ramp_task(void *arg) {
    (void)arg;
    
    const uint32_t RAMP_DURATION_MS = 200; // 200ms ramp duration
    const uint32_t RAMP_STEP_MS = 10;      // Update every 10ms
    const uint32_t RAMP_STEPS = RAMP_DURATION_MS / RAMP_STEP_MS;
    
    float start_factor = s_current_volume_factor;
    float target_factor = sx_audio_volume_to_factor_log(s_target_volume);
    float step = (target_factor - start_factor) / RAMP_STEPS;
    
    for (uint32_t i = 0; i <= RAMP_STEPS; i++) {
        s_current_volume_factor = start_factor + (step * i);
        if (s_current_volume_factor < 0.0f) s_current_volume_factor = 0.0f;
        if (s_current_volume_factor > 1.0f) s_current_volume_factor = 1.0f;
        
        vTaskDelay(pdMS_TO_TICKS(RAMP_STEP_MS));
        
        // Check if target changed during ramp
        if (s_target_volume != s_volume) {
            // Restart ramp with new target
            start_factor = s_current_volume_factor;
            target_factor = sx_audio_volume_to_factor_log(s_target_volume);
            step = (target_factor - start_factor) / RAMP_STEPS;
            i = 0; // Reset counter
        }
    }
    
    // Finalize volume
    s_volume = s_target_volume;
    s_current_volume_factor = target_factor;
    s_volume_ramping = false;
    s_volume_ramp_task_handle = NULL;
    
    ESP_LOGD(TAG, "Volume ramp complete: %d%% (factor: %.3f)", s_volume, s_current_volume_factor);
    vTaskDelete(NULL);
}

// Unused placeholders
static void sx_audio_task(void *arg) { (void)arg; vTaskDelete(NULL); }

#define RECORDING_BUFFER_SAMPLES 480
static void sx_audio_recording_task(void *arg) {
    (void)arg;
    int16_t *pcm_buf = malloc(RECORDING_BUFFER_SAMPLES * sizeof(int16_t));
    if (!pcm_buf) {
        ESP_LOGE(TAG, "No mem for recording buffer");
        s_recording = false;
        vTaskDelete(NULL);
        return;
    }
    while (s_recording) {
        size_t bytes_read = 0;
        esp_err_t ret = i2s_channel_read(s_rx_chan, pcm_buf, RECORDING_BUFFER_SAMPLES * sizeof(int16_t), &bytes_read, pdMS_TO_TICKS(100));
        if (ret == ESP_OK && bytes_read > 0) {
            size_t samples_read = bytes_read / sizeof(int16_t);
            // Call recording callback if set (for audio streaming)
            if (s_recording_callback != NULL) {
                s_recording_callback(pcm_buf, samples_read, SX_AUDIO_SAMPLE_RATE);
            }
            
            // Send to STT endpoint if STT is active
            if (sx_stt_is_active()) {
                esp_err_t stt_ret = sx_stt_send_audio_chunk(pcm_buf, samples_read);
                if (stt_ret != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to send audio chunk to STT: %s", esp_err_to_name(stt_ret));
                }
            }
            
            ESP_LOGD(TAG, "Recorded %zu samples", samples_read);
        } else if (ret != ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "Recording read error: %s", esp_err_to_name(ret));
        }
    }
    free(pcm_buf);
    s_recording_task_handle = NULL;
    vTaskDelete(NULL);
}

// Phase 1: Hybrid Music Screen - Position and Duration functions
uint32_t sx_audio_get_position(void) {
    if (s_position_mutex == NULL) {
        return 0;
    }
    
    if (xSemaphoreTake(s_position_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return s_playback_position_seconds;  // Return last known value
    }
    
    // Calculate position from samples played
    if (s_current_sample_rate > 0 && s_samples_played > 0) {
        s_playback_position_seconds = s_samples_played / s_current_sample_rate;
    }
    
    uint32_t position = s_playback_position_seconds;
    xSemaphoreGive(s_position_mutex);
    
    return position;
}

uint32_t sx_audio_get_duration(void) {
    if (s_position_mutex == NULL) {
        return 0;
    }
    
    if (xSemaphoreTake(s_position_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return s_track_duration_seconds;  // Return last known value
    }
    
    uint32_t duration = s_track_duration_seconds;
    xSemaphoreGive(s_position_mutex);
    
    return duration;
}

// Phase 5: Audio capabilities
sx_audio_caps_t sx_audio_get_caps(void) {
    sx_audio_caps_t caps = {0};
    // Phase 5: Seek not yet implemented, set to false
    caps.seek_supported = false;
    return caps;
}

esp_err_t sx_audio_seek(uint32_t position) {
    // Phase 5: Check if seek is supported
    sx_audio_caps_t caps = sx_audio_get_caps();
    if (!caps.seek_supported) {
        ESP_LOGW(TAG, "Seek not supported");
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    // TODO: Implement seek functionality when decoder supports it
    // This requires:
    // 1. Stop current playback
    // 2. Reopen file and seek decoder to position
    // 3. Resume playback from new position
    // 4. Update s_playback_position_seconds
    
    ESP_LOGW(TAG, "Seek not yet implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

// Phase 5: FFT Spectrum Processing
// Note: Real FFT implementation requires ESP-DSP library
// For now, provide framework with basic mock data that responds to audio activity

// Phase 1: Hybrid Music Screen - Spectrum/FFT functions
esp_err_t sx_audio_get_spectrum(uint16_t *bands, size_t band_count) {
    if (bands == NULL || band_count < 4) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (s_spectrum_mutex == NULL) {
        // Initialize with zeros if not ready
        bands[0] = bands[1] = bands[2] = bands[3] = 0;
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_spectrum_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        // Return last known values
        bands[0] = s_spectrum_bands[0];
        bands[1] = s_spectrum_bands[1];
        bands[2] = s_spectrum_bands[2];
        bands[3] = s_spectrum_bands[3];
        return ESP_OK;
    }
    
    // Phase 5: Process spectrum data
    if (s_playing && !s_paused) {
#ifdef CONFIG_DSP_OPTIMIZED
        // Use real FFT if available and buffer is full
        if (s_fft_initialized && s_pcm_buffer_pos >= FFT_SIZE) {
            // Prepare input: convert int16 PCM to float, apply window
            for (int i = 0; i < FFT_SIZE; i++) {
                s_fft_input[i * 2] = (float)s_pcm_buffer[i] / 32768.0f * s_window[i]; // Real
                s_fft_input[i * 2 + 1] = 0.0f; // Imaginary
            }
            
            // Perform FFT
            dsps_fft2r_fc32(s_fft_input, FFT_SIZE);
            
            // Calculate magnitude and frequency bands
            // FFT output: [real0, imag0, real1, imag1, ...]
            // Frequency bins: bin[i] = i * sample_rate / FFT_SIZE
            
            float bass_energy = 0.0f;
            float midlow_energy = 0.0f;
            float midhigh_energy = 0.0f;
            float high_energy = 0.0f;
            
            uint32_t sample_rate = s_current_sample_rate > 0 ? s_current_sample_rate : 44100;
            float bin_width = (float)sample_rate / FFT_SIZE;
            
            // Calculate energy in each frequency band
            for (int i = 1; i < FFT_SIZE / 2; i++) { // Skip DC component
                float real = s_fft_input[i * 2];
                float imag = s_fft_input[i * 2 + 1];
                float magnitude = sqrtf(real * real + imag * imag);
                float freq = i * bin_width;
                
                if (freq < 250.0f) {
                    bass_energy += magnitude;
                } else if (freq < 500.0f) {
                    midlow_energy += magnitude;
                } else if (freq < 2000.0f) {
                    midhigh_energy += magnitude;
                } else {
                    high_energy += magnitude;
                }
            }
            
            // Normalize to 0-255 range (log scale for better visualization)
            s_spectrum_bands[0] = (uint16_t)(fminf(255.0f, bass_energy * 0.1f));
            s_spectrum_bands[1] = (uint16_t)(fminf(255.0f, midlow_energy * 0.1f));
            s_spectrum_bands[2] = (uint16_t)(fminf(255.0f, midhigh_energy * 0.1f));
            s_spectrum_bands[3] = (uint16_t)(fminf(255.0f, high_energy * 0.1f));
            
            // Reset buffer for next FFT
            s_pcm_buffer_pos = 0;
        } else {
            // Buffer not full yet, use mock data
            static uint32_t s_spectrum_counter = 0;
            s_spectrum_counter++;
            s_spectrum_bands[0] = (uint16_t)(128 + 127 * sin(s_spectrum_counter * 0.1));
            s_spectrum_bands[1] = (uint16_t)(128 + 100 * sin(s_spectrum_counter * 0.15));
            s_spectrum_bands[2] = (uint16_t)(128 + 80 * sin(s_spectrum_counter * 0.2));
            s_spectrum_bands[3] = (uint16_t)(128 + 60 * sin(s_spectrum_counter * 0.25));
            for (int i = 0; i < 4; i++) {
                if (s_spectrum_bands[i] > 255) s_spectrum_bands[i] = 255;
            }
        }
#else
        // ESP-DSP not available, use mock data
        static uint32_t s_spectrum_counter = 0;
        s_spectrum_counter++;
        s_spectrum_bands[0] = (uint16_t)(128 + 127 * sin(s_spectrum_counter * 0.1));
        s_spectrum_bands[1] = (uint16_t)(128 + 100 * sin(s_spectrum_counter * 0.15));
        s_spectrum_bands[2] = (uint16_t)(128 + 80 * sin(s_spectrum_counter * 0.2));
        s_spectrum_bands[3] = (uint16_t)(128 + 60 * sin(s_spectrum_counter * 0.25));
        for (int i = 0; i < 4; i++) {
            if (s_spectrum_bands[i] > 255) s_spectrum_bands[i] = 255;
        }
#endif
    } else {
        // Not playing, fade out
        for (int i = 0; i < 4; i++) {
            if (s_spectrum_bands[i] > 0) {
                s_spectrum_bands[i] = s_spectrum_bands[i] * 9 / 10; // Fade out
            }
        }
    }
    
    // Copy spectrum data
    bands[0] = s_spectrum_bands[0];  // Bass
    bands[1] = s_spectrum_bands[1];  // Mid-low
    bands[2] = s_spectrum_bands[2];  // Mid-high
    bands[3] = s_spectrum_bands[3];  // High
    
    xSemaphoreGive(s_spectrum_mutex);
    
    // TODO: Implement actual FFT processing
    // For now, return simulated data (will be updated when FFT is implemented)
    return ESP_OK;
}
