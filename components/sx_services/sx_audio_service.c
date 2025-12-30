#include "sx_audio_service.h"

#include <esp_log.h>
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

// TODO: move to Kconfig/board config
#define SX_AUDIO_I2S_PORT       I2S_NUM_0
#define SX_AUDIO_SAMPLE_RATE    16000
#define SX_AUDIO_BITS           I2S_DATA_BIT_WIDTH_16BIT
#define SX_AUDIO_DMA_DESC_NUM   6
#define SX_AUDIO_DMA_FRAME_NUM  240

// Hardware pin mapping - Xác nhận với hardware thực tế
// PCM5102A Audio Output: GPIO 7 (DOUT), GPIO 15 (BCLK), GPIO 16 (WS)
// Microphone Input: GPIO 4, 5, 6 (cần xác nhận chi tiết từng chân)
#define SX_AUDIO_PIN_MCLK       I2S_GPIO_UNUSED  // PCM5102A không cần MCLK
#define SX_AUDIO_PIN_BCLK       15  // PCM5102A Bit Clock
#define SX_AUDIO_PIN_WS         16  // PCM5102A Word Select (LRCLK)
#define SX_AUDIO_PIN_DOUT       7   // PCM5102A Data Output (Speaker/Headphone)
#define SX_AUDIO_PIN_DIN        6   // Microphone Data Input (GPIO 4,5 có thể là control pins)

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

    // Apply EQ (in-place processing, so we need a copy)
    int16_t *pcm_processed = (int16_t *)malloc(sample_count * sizeof(int16_t));
    if (pcm_processed == NULL) {
        if (s_feed_mutex) {
            xSemaphoreGive(s_feed_mutex);
        }
        return ESP_ERR_NO_MEM;
    }
    memcpy(pcm_processed, pcm, sample_count * sizeof(int16_t));
    sx_audio_eq_process(pcm_processed, sample_count);
    
    // Apply crossfade if active
    sx_audio_crossfade_process(pcm_processed, sample_count);
    
    // Apply volume with logarithmic scaling
    for (size_t i = 0; i < sample_count; i++) {
        pcm_processed[i] = (int16_t)(pcm_processed[i] * s_current_volume_factor);
    }

    size_t bytes_to_write = sample_count * sizeof(int16_t);
    size_t written = 0;
    esp_err_t ret = i2s_channel_write(s_tx_chan, pcm_processed, bytes_to_write, &written, 0);
    
    free(pcm_processed);

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
