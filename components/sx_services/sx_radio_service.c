#include "sx_radio_service.h"

#include <esp_log.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_http_client.h"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_audio_service.h"
#include "sx_codec_aac.h"
#include "sx_codec_mp3.h"
#include "sx_codec_flac.h"
#include "sx_codec_detector.h"
#include "sx_audio_recovery.h"
#include "freertos/timers.h"

// Buffer monitoring state for recovery
static uint32_t s_buffer_fill_samples = 0;
static uint32_t s_buffer_sample_rate = 44100; // Default
static uint32_t s_last_buffer_check_time = 0;
// Note: BUFFER_CHECK_INTERVAL_MS reserved for future buffer monitoring implementation
// static const uint32_t BUFFER_CHECK_INTERVAL_MS = 100;

static const char *TAG = "sx_radio";

// Radio service state
static bool s_initialized = false;
static bool s_playing = false;
static bool s_paused = false;
static bool s_stop_requested = false;
static char s_current_url[512] = {0};
static sx_radio_config_t s_cfg = {0};

// HTTP streaming task
static TaskHandle_t s_http_task_handle = NULL;
static TaskHandle_t s_reconnect_task_handle = NULL;

// Metadata (defined in header)
static sx_radio_metadata_t s_metadata = {0};

// Reconnection state
static uint32_t s_reconnect_attempts = 0;
static uint32_t s_max_reconnect_attempts = 10;
static bool s_auto_reconnect = true;

// ICY metadata
static bool s_icy_metadata_enabled = false;
static uint32_t s_icy_metadata_interval = 0;
static uint32_t s_bytes_since_metadata = 0;

// Constants
#define READ_BUFFER_SIZE_DEFAULT 4096
#define MIN_BUFFER_BEFORE_PLAY_DEFAULT 2048
#define DEFAULT_RECONNECT_DELAY_MS 1000
#define MAX_RECONNECT_DELAY_MS 30000

// Buffer management
static size_t s_buffer_size = READ_BUFFER_SIZE_DEFAULT;
static size_t s_min_buffer_before_play = MIN_BUFFER_BEFORE_PLAY_DEFAULT;
static size_t s_buffered_bytes = 0;
static size_t s_buffer_capacity = 0;

// Network buffer improvements (Phase 5)
// - Dynamic buffer sizing based on network conditions
// - Buffer fill monitoring
// - Adaptive buffering strategy
static uint32_t s_buffer_fill_target_ms = 500; // Target buffer fill in milliseconds
static uint32_t s_buffer_fill_current_ms = 0;  // Current buffer fill in milliseconds
static bool s_buffer_ready = false;            // Buffer ready for playback

// Display mode
static sx_radio_display_mode_t s_display_mode = SX_RADIO_DISPLAY_INFO;

// Audio format detected from Content-Type
typedef enum {
    SX_RADIO_FORMAT_UNKNOWN = 0,
    SX_RADIO_FORMAT_AAC,
    SX_RADIO_FORMAT_MP3,
    SX_RADIO_FORMAT_OGG,
    SX_RADIO_FORMAT_WAV,
} sx_radio_audio_format_t;

static sx_radio_audio_format_t s_detected_format = SX_RADIO_FORMAT_UNKNOWN;

// Per-station volume boost configuration
typedef struct {
    char station_key[64];
    uint8_t volume_boost;  // 0-200, 100 = no boost
} sx_station_volume_t;

#define MAX_STATION_VOLUMES 32
static sx_station_volume_t s_station_volumes[MAX_STATION_VOLUMES];
static size_t s_station_volume_count = 0;
static uint8_t s_current_station_volume_boost = 100; // Default: no boost

// MP3 decoder state
static bool s_mp3_decoder_initialized = false;

// Forward declarations
static void sx_radio_http_task(void *arg);
static void sx_radio_reconnect_task(void *arg);
static esp_err_t sx_radio_setup_http_connection(const char *url, esp_http_client_handle_t *client);
static esp_err_t sx_radio_read_stream_data(esp_http_client_handle_t client);
static void sx_radio_parse_icy_headers(esp_http_client_handle_t client);
static void sx_radio_process_audio_chunk(const uint8_t *data, size_t size);
static bool sx_radio_should_reconnect(void);
static uint32_t sx_radio_get_reconnect_delay(void);
static void sx_radio_reset_reconnect_state(void);
static void sx_radio_publish_started(void);
static void sx_radio_publish_stopped(void);
static void sx_radio_publish_error(const char *error);
static void sx_radio_publish_metadata(const sx_radio_metadata_t *metadata);

esp_err_t sx_radio_service_init(const sx_radio_config_t *cfg) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    if (cfg != NULL) {
        s_cfg = *cfg;
        s_buffer_size = (cfg->buffer_size > 0) ? cfg->buffer_size : READ_BUFFER_SIZE_DEFAULT;
        s_min_buffer_before_play = (cfg->min_buffer_before_play > 0) ? cfg->min_buffer_before_play : MIN_BUFFER_BEFORE_PLAY_DEFAULT;
    } else {
        memset(&s_cfg, 0, sizeof(s_cfg));
        s_buffer_size = READ_BUFFER_SIZE_DEFAULT;
        s_min_buffer_before_play = MIN_BUFFER_BEFORE_PLAY_DEFAULT;
    }
    
    // Initialize AAC decoder
    esp_err_t codec_ret = sx_codec_aac_init();
    if (codec_ret != ESP_OK) {
        ESP_LOGW(TAG, "AAC codec init failed (non-critical): %s", esp_err_to_name(codec_ret));
    }
    
    // Phase 5: Initialize audio recovery manager
    sx_audio_recovery_config_t recovery_cfg = {
        .buffer_underrun_threshold_ms = 100,
        .recovery_buffer_target_ms = 500,
        .max_recovery_attempts = 3,
    };
    esp_err_t recovery_ret = sx_audio_recovery_init(&recovery_cfg);
    if (recovery_ret != ESP_OK) {
        ESP_LOGW(TAG, "Audio recovery init failed (non-critical): %s", esp_err_to_name(recovery_ret));
    }
    
    s_initialized = true;
    s_auto_reconnect = true;
    s_max_reconnect_attempts = 10;
    
    ESP_LOGI(TAG, "Radio service initialized (online streaming)");
    return ESP_OK;
}

esp_err_t sx_radio_service_start(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Task will be created when streaming starts
    ESP_LOGI(TAG, "Radio service started");
    return ESP_OK;
}

esp_err_t sx_radio_service_stop(void) {
    if (s_http_task_handle != NULL) {
        s_stop_requested = true;
        vTaskDelay(pdMS_TO_TICKS(500));
        if (s_http_task_handle != NULL) {
            vTaskDelete(s_http_task_handle);
            s_http_task_handle = NULL;
        }
    }
    
    if (s_reconnect_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (s_reconnect_task_handle != NULL) {
            vTaskDelete(s_reconnect_task_handle);
            s_reconnect_task_handle = NULL;
        }
    }
    
    s_playing = false;
    s_paused = false;
    return ESP_OK;
}

esp_err_t sx_radio_pause(void) {
    if (!s_playing) {
        return ESP_ERR_INVALID_STATE;
    }
    s_paused = true;
    ESP_LOGI(TAG, "Paused streaming");
    return ESP_OK;
}

esp_err_t sx_radio_resume(void) {
    if (!s_playing) {
        return ESP_ERR_INVALID_STATE;
    }
    s_paused = false;
    ESP_LOGI(TAG, "Resumed streaming");
    return ESP_OK;
}

bool sx_radio_is_paused(void) {
    return s_paused;
}

const char *sx_radio_get_current_url(void) {
    return s_current_url;
}

const sx_radio_metadata_t *sx_radio_get_metadata(void) {
    return &s_metadata;
}

#include "sx_radio_station_table.h"

esp_err_t sx_radio_play_station(const char *station_id) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (station_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Stop current playback if any
    if (s_playing) {
        sx_radio_stop_playback();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    // station_id can be either:
    // - Station key (e.g. "VOV3")
    // - Full URL (http/https)
    const sx_radio_station_t *st = sx_radio_lookup_station(station_id);
    const char *url = (st != NULL) ? st->url : station_id;

    strncpy(s_current_url, url, sizeof(s_current_url) - 1);
    s_current_url[sizeof(s_current_url) - 1] = '\0';

    // Apply per-station volume boost if configured
    s_current_station_volume_boost = 100; // Default: no boost
    if (st != NULL) {
        // Look up volume boost for this station
        for (size_t i = 0; i < s_station_volume_count; i++) {
            if (strcmp(s_station_volumes[i].station_key, station_id) == 0) {
                s_current_station_volume_boost = s_station_volumes[i].volume_boost;
                ESP_LOGI(TAG, "Applied volume boost %d%% for station %s", s_current_station_volume_boost, station_id);
                break;
            }
        }
    }

    // If known station, seed metadata title for UI
    if (st != NULL) {
        strncpy(s_metadata.stream_title, st->title, sizeof(s_metadata.stream_title) - 1);
        s_metadata.stream_title[sizeof(s_metadata.stream_title) - 1] = '\0';
        sx_radio_publish_metadata(&s_metadata);
    }
    
    // Reset decoder when starting new stream
    sx_codec_aac_reset();
    if (s_mp3_decoder_initialized) {
        sx_codec_mp3_reset();
    }
    
    s_stop_requested = false;
    s_paused = false;
    sx_radio_reset_reconnect_state();
    
    // Create HTTP streaming task
    BaseType_t ret = xTaskCreatePinnedToCore(
        sx_radio_http_task,
        "sx_radio_http",
        8192,
        NULL,
        3,
        &s_http_task_handle,
        0
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create HTTP task");
        return ESP_ERR_NO_MEM;
    }
    
    s_playing = true;
    sx_radio_publish_started();
    
    ESP_LOGI(TAG, "Started streaming: %s -> %s", station_id, s_current_url);
    return ESP_OK;
}

esp_err_t sx_radio_play_url(const char *url) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (url == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Stop current playback if any
    if (s_playing) {
        sx_radio_stop_playback();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    strncpy(s_current_url, url, sizeof(s_current_url) - 1);
    s_current_url[sizeof(s_current_url) - 1] = '\0';
    
    // Reset decoder when starting new stream
    sx_codec_aac_reset();
    if (s_mp3_decoder_initialized) {
        sx_codec_mp3_reset();
    }
    
    s_stop_requested = false;
    s_paused = false;
    sx_radio_reset_reconnect_state();
    
    // Create HTTP streaming task
    BaseType_t ret = xTaskCreatePinnedToCore(
        sx_radio_http_task,
        "sx_radio_http",
        8192,
        NULL,
        3,
        &s_http_task_handle,
        0
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create HTTP task");
        return ESP_ERR_NO_MEM;
    }
    
    s_playing = true;
    sx_radio_publish_started();
    
    ESP_LOGI(TAG, "Started streaming from URL: %s", url);
    return ESP_OK;
}

esp_err_t sx_radio_stop_playback(void) {
    if (!s_playing) {
        return ESP_OK;
    }
    
    s_stop_requested = true;
    s_playing = false;
    s_paused = false;
    
    // Wait for HTTP task to finish
    if (s_http_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(500));
        if (s_http_task_handle != NULL) {
            vTaskDelete(s_http_task_handle);
            s_http_task_handle = NULL;
        }
    }
    
    // Wait for reconnect task to finish
    if (s_reconnect_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (s_reconnect_task_handle != NULL) {
            vTaskDelete(s_reconnect_task_handle);
            s_reconnect_task_handle = NULL;
        }
    }
    
    sx_radio_publish_stopped();
    ESP_LOGI(TAG, "Stopped streaming");
    return ESP_OK;
}

bool sx_radio_is_playing(void) {
    return s_playing;
}

// HTTP streaming task implementation
static void sx_radio_http_task(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "HTTP read task started");
    
    char url[512];
    strncpy(url, s_current_url, sizeof(url) - 1);
    url[sizeof(url) - 1] = '\0';
    
    sx_radio_reset_reconnect_state();
    
    while (!s_stop_requested) {
        // Setup HTTP connection
        esp_http_client_handle_t client = NULL;
        esp_err_t ret = sx_radio_setup_http_connection(url, &client);
        if (ret != ESP_OK || client == NULL) {
            if (sx_radio_should_reconnect()) {
                sx_radio_reconnect_task(NULL);
                continue;
            } else {
                break;
            }
        }
        
        // Read and process stream data
        sx_radio_read_stream_data(client);
        
        // Cleanup
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        
        // If connection failed and auto-reconnect is enabled, try to reconnect
        if (s_auto_reconnect && !s_stop_requested && sx_radio_should_reconnect()) {
            sx_radio_reconnect_task(NULL);
            continue;
        }
        
        if (!s_stop_requested) {
            break;
        }
    }
    
    s_playing = false;
    ESP_LOGI(TAG, "HTTP read task ended");
    vTaskDelete(NULL);
}

static esp_err_t sx_radio_setup_http_connection(const char *url, esp_http_client_handle_t *client) {
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 10000,
        .buffer_size = s_buffer_size,
        .skip_cert_common_name_check = true,
    };
    
    *client = esp_http_client_init(&config);
    if (*client == NULL) {
        ESP_LOGE(TAG, "Failed to create HTTP client");
        sx_radio_publish_error("Failed to create HTTP client");
        return ESP_FAIL;
    }
    
    // Open connection
    esp_err_t err = esp_http_client_open(*client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(*client);
        *client = NULL;
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        sx_radio_publish_error("Failed to open HTTP connection");
        return err;
    }
    
    // Fetch headers
    int content_length = esp_http_client_fetch_headers(*client);
    if (content_length < 0) {
        ESP_LOGW(TAG, "Failed to fetch headers or content-length unknown");
    }
    
    // Parse ICY headers
    sx_radio_parse_icy_headers(*client);
    
    // Reset reconnect state on successful connection
    sx_radio_reset_reconnect_state();
    
    return ESP_OK;
}

static void sx_radio_parse_icy_headers(esp_http_client_handle_t client) {
    char *header_value = NULL;
    
    // Check for ICY metadata interval
    if (esp_http_client_get_header(client, "icy-metaint", &header_value) == ESP_OK && header_value != NULL) {
        s_icy_metadata_enabled = true;
        s_icy_metadata_interval = (uint32_t)atoi(header_value);
        ESP_LOGI(TAG, "ICY metadata enabled, interval: %lu bytes", (unsigned long)s_icy_metadata_interval);
    } else {
        s_icy_metadata_enabled = false;
        s_icy_metadata_interval = 0;
    }
    
    // Extract other ICY headers
    if (esp_http_client_get_header(client, "icy-name", &header_value) == ESP_OK && header_value != NULL) {
        strncpy(s_metadata.stream_title, header_value, sizeof(s_metadata.stream_title) - 1);
        s_metadata.stream_title[sizeof(s_metadata.stream_title) - 1] = '\0';
        ESP_LOGI(TAG, "Stream title: %s", s_metadata.stream_title);
    }
    
    if (esp_http_client_get_header(client, "icy-url", &header_value) == ESP_OK && header_value != NULL) {
        strncpy(s_metadata.stream_url, header_value, sizeof(s_metadata.stream_url) - 1);
        s_metadata.stream_url[sizeof(s_metadata.stream_url) - 1] = '\0';
    }
    
    // Use Smart Codec Engine to detect format
    sx_codec_detect_result_t detect_result = {0};
    bool detected = false;
    
    if (esp_http_client_get_header(client, "Content-Type", &header_value) == ESP_OK && header_value != NULL) {
        strncpy(s_metadata.content_type, header_value, sizeof(s_metadata.content_type) - 1);
        s_metadata.content_type[sizeof(s_metadata.content_type) - 1] = '\0';
        
        // Use smart codec detector
        if (sx_codec_detect_from_content_type(header_value, &detect_result) == ESP_OK) {
            detected = true;
            ESP_LOGI(TAG, "Detected audio format: %s (confidence: %.2f)", 
                     detect_result.detected_format, detect_result.confidence);
        }
    }
    
    // Map detected codec type to radio format
    if (detected) {
        switch (detect_result.codec_type) {
            case SX_CODEC_TYPE_AAC:
                s_detected_format = SX_RADIO_FORMAT_AAC;
                break;
            case SX_CODEC_TYPE_MP3:
                s_detected_format = SX_RADIO_FORMAT_MP3;
                break;
            case SX_CODEC_TYPE_FLAC:
                s_detected_format = SX_RADIO_FORMAT_UNKNOWN; // FLAC not yet supported in radio
                ESP_LOGW(TAG, "FLAC format detected but not yet supported in radio streaming");
                break;
            case SX_CODEC_TYPE_OGG:
            case SX_CODEC_TYPE_OPUS:
                s_detected_format = SX_RADIO_FORMAT_OGG;
                ESP_LOGW(TAG, "OGG/Opus format detected but not yet supported");
                s_detected_format = SX_RADIO_FORMAT_UNKNOWN;
                break;
            case SX_CODEC_TYPE_WAV:
                s_detected_format = SX_RADIO_FORMAT_WAV;
                ESP_LOGW(TAG, "WAV format detected but not yet supported");
                s_detected_format = SX_RADIO_FORMAT_UNKNOWN;
                break;
            default:
                s_detected_format = SX_RADIO_FORMAT_UNKNOWN;
                break;
        }
    } else {
        // No Content-Type header - default to AAC
        ESP_LOGW(TAG, "No Content-Type header, defaulting to AAC decoder");
        s_detected_format = SX_RADIO_FORMAT_AAC;
    }
    
    if (esp_http_client_get_header(client, "icy-br", &header_value) == ESP_OK && header_value != NULL) {
        s_metadata.bitrate = atoi(header_value) * 1000; // Convert kbps to bps
    }
    
    sx_radio_publish_metadata(&s_metadata);
    s_bytes_since_metadata = 0;
}

static esp_err_t sx_radio_read_stream_data(esp_http_client_handle_t client) {
    uint8_t *chunk_buffer = malloc(s_buffer_size);
    if (chunk_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate read buffer");
        return ESP_ERR_NO_MEM;
    }
    
    s_buffer_capacity = s_buffer_size;
    s_buffered_bytes = 0;
    
    uint32_t consecutive_errors = 0;
    const uint32_t MAX_CONSECUTIVE_ERRORS = 3;
    
    while (!s_stop_requested && !s_paused) {
        int data_read = esp_http_client_read(client, (char *)chunk_buffer, s_buffer_size);
        
        if (data_read > 0) {
            s_buffered_bytes += data_read;
            
            // Phase 5: Update buffer fill status
            // Estimate buffer fill time (rough estimate based on bitrate)
            // For AAC at 64kbps: ~8KB per second
            if (s_metadata.bitrate > 0) {
                uint32_t bytes_per_second = s_metadata.bitrate / 8;
                if (bytes_per_second > 0) {
                    s_buffer_fill_current_ms = (s_buffered_bytes * 1000) / bytes_per_second;
                }
            }
            
            // Check if buffer is ready for playback
            if (!s_buffer_ready && s_buffer_fill_current_ms >= s_buffer_fill_target_ms) {
                s_buffer_ready = true;
                ESP_LOGI(TAG, "Buffer ready for playback (%lu ms filled)", (unsigned long)s_buffer_fill_current_ms);
            }
            
            // Phase 5: Check for buffer underrun and trigger recovery if needed
            if (s_buffer_ready && s_buffer_fill_current_ms < s_buffer_fill_target_ms / 2) {
                sx_audio_recovery_check(s_buffer_fill_current_ms);
            }
        }
        
        if (data_read < 0) {
            consecutive_errors++;
            ESP_LOGW(TAG, "HTTP read error (consecutive: %lu/%lu): %s",
                     (unsigned long)consecutive_errors,
                     (unsigned long)MAX_CONSECUTIVE_ERRORS,
                     esp_err_to_name(data_read));
            
            if (consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
                ESP_LOGE(TAG, "Too many consecutive read errors, reconnecting...");
                free(chunk_buffer);
                return ESP_FAIL;
            }
            
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        // Reset error counter on successful read
        consecutive_errors = 0;
        
        if (data_read == 0) {
            // End of stream or connection closed
            ESP_LOGI(TAG, "Stream ended or connection closed");
            if (s_auto_reconnect && !s_stop_requested) {
                if (sx_radio_should_reconnect()) {
                    free(chunk_buffer);
                    return ESP_FAIL; // Will reconnect in outer loop
                } else {
                    sx_radio_publish_error("Max reconnect attempts reached");
                    free(chunk_buffer);
                    return ESP_FAIL;
                }
            }
            break;
        }
        
        // Process ICY metadata if enabled
        if (s_icy_metadata_enabled && s_icy_metadata_interval > 0) {
            s_bytes_since_metadata += data_read;
            
            // Check if we need to read metadata block
            if (s_bytes_since_metadata >= s_icy_metadata_interval) {
                // Read metadata length byte
                uint8_t metadata_length_byte = 0;
                int meta_read = esp_http_client_read(client, (char *)&metadata_length_byte, 1);
                if (meta_read == 1) {
                    uint32_t metadata_length = metadata_length_byte * 16; // ICY metadata length is in 16-byte blocks
                    if (metadata_length > 0 && metadata_length < 8192) {
                        char *metadata_buffer = malloc(metadata_length);
                        if (metadata_buffer != NULL) {
                            int meta_data_read = esp_http_client_read(client, metadata_buffer, metadata_length);
                            if (meta_data_read == (int)metadata_length) {
                                // Parse ICY metadata string (e.g., "StreamTitle='Song Title';")
                                char *title_start = strstr(metadata_buffer, "StreamTitle='");
                                if (title_start != NULL) {
                                    title_start += 13; // Skip "StreamTitle='"
                                    char *title_end = strchr(title_start, '\'');
                                    if (title_end != NULL) {
                                        size_t title_len = title_end - title_start;
                                        if (title_len < sizeof(s_metadata.stream_title)) {
                                            strncpy(s_metadata.stream_title, title_start, title_len);
                                            s_metadata.stream_title[title_len] = '\0';
                                            sx_radio_publish_metadata(&s_metadata);
                                            ESP_LOGI(TAG, "ICY metadata: %s", s_metadata.stream_title);
                                        }
                                    }
                                }
                            }
                            free(metadata_buffer);
                        }
                    }
                }
                s_bytes_since_metadata = 0;
            }
        }
        
        // Process audio chunk
        sx_radio_process_audio_chunk(chunk_buffer, data_read);
        
        // Small delay to prevent CPU hogging
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    free(chunk_buffer);
    return ESP_OK;
}

#define AAC_DECODE_BUFFER_SAMPLES 2048
#define MP3_DECODE_BUFFER_SAMPLES 2048
static int16_t s_aac_pcm_buffer[AAC_DECODE_BUFFER_SAMPLES * 2]; // stereo
static int16_t s_mp3_pcm_buffer[MP3_DECODE_BUFFER_SAMPLES * 2]; // stereo

static void sx_radio_process_audio_chunk(const uint8_t *data, size_t size) {
    if (!data || size == 0) return;
    
    // Auto-select decoder based on detected format
    if (s_detected_format == SX_RADIO_FORMAT_AAC || s_detected_format == SX_RADIO_FORMAT_UNKNOWN) {
        // Try AAC decode
        size_t pcm_samples = 0;
        sx_codec_aac_info_t info = {0};
        esp_err_t decode_ret = sx_codec_aac_decode(data, size,
                                                    s_aac_pcm_buffer, AAC_DECODE_BUFFER_SAMPLES,
                                                    &pcm_samples, &info);
        
        if (decode_ret == ESP_OK && pcm_samples > 0) {
            // Feed decoded PCM to audio service (via mixer if available)
            uint32_t sr = (info.sample_rate_hz > 0) ? info.sample_rate_hz : 44100;
            s_buffer_sample_rate = sr; // Update sample rate for buffer monitoring
            
            #ifdef CONFIG_SX_AUDIO_MIXER_ENABLE
            extern esp_err_t sx_audio_mixer_feed(sx_audio_mixer_source_t, const int16_t*, size_t, uint32_t);
            extern sx_audio_mixer_source_t SX_AUDIO_MIXER_SOURCE_RADIO;
            esp_err_t feed_ret = sx_audio_mixer_feed(SX_AUDIO_MIXER_SOURCE_RADIO, s_aac_pcm_buffer, pcm_samples, sr);
            #else
            esp_err_t feed_ret = sx_audio_service_feed_pcm(s_aac_pcm_buffer, pcm_samples, sr);
            #endif
            if (feed_ret != ESP_OK) {
                ESP_LOGW(TAG, "Feed PCM failed: %s (dropping %zu samples)", esp_err_to_name(feed_ret), pcm_samples);
                // On feed failure, reduce buffer estimate
                if (s_buffer_fill_samples > pcm_samples) {
                    s_buffer_fill_samples -= pcm_samples;
                } else {
                    s_buffer_fill_samples = 0;
                }
            } else {
                // Estimate buffer fill: add samples fed, subtract samples consumed
                // Simple estimation: assume consumption rate equals sample rate
                uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                if (s_last_buffer_check_time > 0) {
                    uint32_t elapsed_ms = current_time - s_last_buffer_check_time;
                    uint32_t samples_consumed = (s_buffer_sample_rate * elapsed_ms) / 1000;
                    if (samples_consumed < s_buffer_fill_samples) {
                        s_buffer_fill_samples -= samples_consumed;
                    } else {
                        s_buffer_fill_samples = 0;
                    }
                }
                s_buffer_fill_samples += pcm_samples;
                s_last_buffer_check_time = current_time;
                
                // Check buffer level for recovery
                uint32_t buffer_ms = (s_buffer_fill_samples * 1000) / s_buffer_sample_rate;
                sx_audio_recovery_check(buffer_ms);
            }
        } else if (decode_ret == ESP_ERR_NOT_FINISHED) {
            // Decoder needs more input - this is normal, not an error
            ESP_LOGD(TAG, "AAC decoder needs more input (chunk %zu bytes)", size);
        } else if (decode_ret == ESP_ERR_NOT_SUPPORTED) {
            // Decoder not integrated yet - log once per stream start
            static bool logged_once = false;
            if (!logged_once) {
                ESP_LOGW(TAG, "AAC decoder not integrated - audio will be silent (HTTP+metadata OK)");
                logged_once = true;
            }
        } else {
            ESP_LOGD(TAG, "AAC decode failed: %s (chunk %zu bytes)", esp_err_to_name(decode_ret), size);
        }
    } else if (s_detected_format == SX_RADIO_FORMAT_MP3) {
        // Use MP3 decoder
        if (!s_mp3_decoder_initialized) {
            esp_err_t ret = sx_codec_mp3_init();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to initialize MP3 decoder");
                return;
            }
            s_mp3_decoder_initialized = true;
        }
        
        // Decode MP3
        size_t pcm_samples = 0;
        esp_err_t decode_ret = sx_codec_mp3_decode(
            data,
            size,
            s_mp3_pcm_buffer,
            MP3_DECODE_BUFFER_SAMPLES * 2, // stereo capacity
            &pcm_samples
        );
        
        if (decode_ret == ESP_OK && pcm_samples > 0) {
            // Get decoder info for sample rate
            sx_mp3_decoder_info_t mp3_info = {0};
            uint32_t sr = 44100; // Default
            if (sx_codec_mp3_get_info(&mp3_info) == ESP_OK && mp3_info.info_ready) {
                sr = mp3_info.sample_rate_hz;
                // channels = mp3_info.channels; // Reserved for future use
            }
            
            s_buffer_sample_rate = sr; // Update sample rate for buffer monitoring
            
            // Feed decoded PCM to audio service
            #ifdef CONFIG_SX_AUDIO_MIXER_ENABLE
            extern esp_err_t sx_audio_mixer_feed(sx_audio_mixer_source_t, const int16_t*, size_t, uint32_t);
            extern sx_audio_mixer_source_t SX_AUDIO_MIXER_SOURCE_RADIO;
            esp_err_t feed_ret = sx_audio_mixer_feed(SX_AUDIO_MIXER_SOURCE_RADIO, s_mp3_pcm_buffer, pcm_samples, sr);
            #else
            esp_err_t feed_ret = sx_audio_service_feed_pcm(s_mp3_pcm_buffer, pcm_samples, sr);
            #endif
            
            if (feed_ret != ESP_OK) {
                ESP_LOGW(TAG, "Feed PCM failed: %s (dropping %zu samples)", esp_err_to_name(feed_ret), pcm_samples);
                // On feed failure, reduce buffer estimate
                if (s_buffer_fill_samples > pcm_samples) {
                    s_buffer_fill_samples -= pcm_samples;
                } else {
                    s_buffer_fill_samples = 0;
                }
            } else {
                // Estimate buffer fill: add samples fed, subtract samples consumed
                uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                if (s_last_buffer_check_time > 0) {
                    uint32_t elapsed_ms = current_time - s_last_buffer_check_time;
                    uint32_t samples_consumed = (s_buffer_sample_rate * elapsed_ms) / 1000;
                    if (samples_consumed < s_buffer_fill_samples) {
                        s_buffer_fill_samples -= samples_consumed;
                    } else {
                        s_buffer_fill_samples = 0;
                    }
                }
                s_buffer_fill_samples += pcm_samples;
                s_last_buffer_check_time = current_time;
                
                // Check buffer level for recovery
                uint32_t buffer_ms = (s_buffer_fill_samples * 1000) / s_buffer_sample_rate;
                sx_audio_recovery_check(buffer_ms);
            }
        } else if (decode_ret == ESP_ERR_NOT_FINISHED) {
            // Decoder needs more input - this is normal, not an error
            ESP_LOGD(TAG, "MP3 decoder needs more input (chunk %zu bytes)", size);
        } else {
            ESP_LOGD(TAG, "MP3 decode failed: %s (chunk %zu bytes)", esp_err_to_name(decode_ret), size);
        }
    } else if (s_detected_format == SX_RADIO_FORMAT_OGG) {
        // Future: OGG decoder support
        // Requires: esp-opus or similar OGG decoder library integration
        // Current: Only MP3 format supported
        ESP_LOGW(TAG, "OGG decoder not yet implemented - only MP3 format supported");
    } else {
        ESP_LOGW(TAG, "Unknown audio format, cannot decode");
    }
}

static void sx_radio_reconnect_task(void *arg) {
    (void)arg;
    
    if (!sx_radio_should_reconnect()) {
        ESP_LOGE(TAG, "Max reconnect attempts (%lu) reached", (unsigned long)s_max_reconnect_attempts);
        sx_radio_publish_error("Max reconnect attempts reached");
        return;
    }
    
    s_reconnect_attempts++;
    uint32_t delay_ms = sx_radio_get_reconnect_delay();
    
    ESP_LOGI(TAG, "Reconnecting... (attempt %lu/%lu, delay %lu ms)",
             (unsigned long)s_reconnect_attempts,
             (unsigned long)s_max_reconnect_attempts,
             (unsigned long)delay_ms);
    
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

static bool sx_radio_should_reconnect(void) {
    if (!s_auto_reconnect) {
        return false;
    }
    if (s_max_reconnect_attempts > 0 && s_reconnect_attempts >= s_max_reconnect_attempts) {
        return false;
    }
    return true;
}

static uint32_t sx_radio_get_reconnect_delay(void) {
    // Exponential backoff: 1s, 2s, 4s, 8s, 16s, max 30s
    uint32_t delay = DEFAULT_RECONNECT_DELAY_MS;
    for (uint32_t i = 1; i < s_reconnect_attempts && delay < MAX_RECONNECT_DELAY_MS; i++) {
        delay *= 2;
    }
    return (delay < MAX_RECONNECT_DELAY_MS) ? delay : MAX_RECONNECT_DELAY_MS;
}

static void sx_radio_reset_reconnect_state(void) {
    s_reconnect_attempts = 0;
}

static void sx_radio_publish_started(void) {
    sx_event_t evt = {
        .type = SX_EVT_RADIO_STARTED,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = NULL
    };
    sx_dispatcher_post_event(&evt);
}

static void sx_radio_publish_stopped(void) {
    sx_event_t evt = {
        .type = SX_EVT_RADIO_STOPPED,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = NULL
    };
    sx_dispatcher_post_event(&evt);
}

static void sx_radio_publish_error(const char *error) {
    if (error == NULL) {
        return;
    }
    
    char *error_copy = strdup(error);
    if (error_copy == NULL) {
        return;
    }
    
    sx_event_t evt = {
        .type = SX_EVT_RADIO_ERROR,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = error_copy
    };
    sx_dispatcher_post_event(&evt);
}

static void sx_radio_publish_metadata(const sx_radio_metadata_t *metadata) {
    if (metadata == NULL) {
        return;
    }
    
    // Allocate memory for metadata
    size_t metadata_size = sizeof(sx_radio_metadata_t);
    sx_radio_metadata_t *metadata_copy = malloc(metadata_size);
    if (metadata_copy == NULL) {
        return;
    }
    memcpy(metadata_copy, metadata, metadata_size);
    
    sx_event_t evt = {
        .type = SX_EVT_RADIO_METADATA,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = metadata_copy
    };
    sx_dispatcher_post_event(&evt);
}

esp_err_t sx_radio_set_display_mode(sx_radio_display_mode_t mode) {
    if (mode >= 2) {  // Only 2 modes for now
        return ESP_ERR_INVALID_ARG;
    }
    s_display_mode = mode;
    ESP_LOGI(TAG, "Display mode set to %d", mode);
    return ESP_OK;
}

sx_radio_display_mode_t sx_radio_get_display_mode(void) {
    return s_display_mode;
}

esp_err_t sx_radio_get_buffer_status(size_t *buffered_bytes, size_t *buffer_capacity) {
    if (!buffered_bytes || !buffer_capacity) {
        return ESP_ERR_INVALID_ARG;
    }
    *buffered_bytes = s_buffered_bytes;
    *buffer_capacity = s_buffer_capacity;
    return ESP_OK;
}
