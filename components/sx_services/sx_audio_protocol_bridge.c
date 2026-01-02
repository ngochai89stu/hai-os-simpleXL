#include "sx_audio_protocol_bridge.h"
#include "sx_audio_service.h"
#include "sx_codec_opus.h"
#include "sx_protocol_ws.h"
#include "sx_protocol_mqtt.h"
#include "sx_protocol_audio.h"
#include "sx_protocol_base.h"
#include "sx_protocol_factory.h"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_state.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "sx_audio_bridge";

// Bridge state
static bool s_initialized = false;
static bool s_started = false;
static bool s_send_enabled = false;
static bool s_receive_enabled = false;

// Opus encoder state
static bool s_opus_encoder_initialized = false;
static uint32_t s_opus_sample_rate = 16000;
static uint32_t s_opus_frame_duration_ms = 20;  // 20ms frames
static uint32_t s_opus_frame_samples = 0;  // Calculated from sample rate
static uint32_t s_timestamp = 0;  // Audio timestamp counter

// Audio buffers
#define OPUS_ENCODE_BUFFER_SIZE 4000  // Max Opus frame size
__attribute__((unused)) static uint8_t s_opus_encode_buffer[OPUS_ENCODE_BUFFER_SIZE];
__attribute__((unused)) static int16_t s_pcm_buffer[960];  // Max 20ms @ 48kHz = 960 samples

// PCM accumulation buffer for recording callback
static int16_t s_accumulated_pcm[960] = {0};  // Max 20ms @ 48kHz
static size_t s_accumulated_count = 0;
static SemaphoreHandle_t s_pcm_mutex = NULL;

// Task handles
static TaskHandle_t s_send_task_handle = NULL;
static TaskHandle_t s_receive_task_handle = NULL;

// Queue for audio packets to send
#define AUDIO_SEND_QUEUE_SIZE 20      // 400ms buffer @ 20ms frames (optimized from 10)
static QueueHandle_t s_audio_send_queue = NULL;

// Queue for audio packets to play
#define AUDIO_RECEIVE_QUEUE_SIZE 30   // 600ms buffer @ 20ms frames for jitter tolerance (optimized from 10)
static QueueHandle_t s_audio_receive_queue = NULL;

// Error statistics (optimization: error monitoring)
static uint32_t s_send_error_count = 0;
static uint32_t s_receive_drop_count = 0;
static uint32_t s_decode_error_count = 0;

// Recording callback function (called from audio service)
static void recording_callback(const int16_t *pcm, size_t sample_count, uint32_t sample_rate) {
    if (pcm == NULL || sample_count == 0) {
        return;
    }
    
    if (xSemaphoreTake(s_pcm_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {  // Increased timeout from 10ms to 50ms
        // Accumulate samples
        size_t max_samples = sizeof(s_accumulated_pcm) / sizeof(int16_t);
        size_t copy_count = (s_accumulated_count + sample_count <= max_samples) ? 
                           sample_count : (max_samples - s_accumulated_count);
        
        if (copy_count > 0) {
            memcpy(&s_accumulated_pcm[s_accumulated_count], pcm, copy_count * sizeof(int16_t));
            s_accumulated_count += copy_count;
        }
        
        xSemaphoreGive(s_pcm_mutex);
    }
}

// Forward declarations
static void audio_send_task(void *arg);
static void audio_receive_task(void *arg);
static void on_audio_packet_received(const sx_audio_stream_packet_t *packet);
static void recording_callback(const int16_t *pcm, size_t sample_count, uint32_t sample_rate);

// Initialize Opus encoder
static esp_err_t init_opus_encoder(uint32_t sample_rate) {
    if (s_opus_encoder_initialized && s_opus_sample_rate == sample_rate) {
        return ESP_OK;  // Already initialized with same sample rate
    }
    
    // Deinit if already initialized with different sample rate
    if (s_opus_encoder_initialized) {
        sx_codec_opus_encoder_deinit();
        s_opus_encoder_initialized = false;
    }
    
    sx_opus_encoder_config_t opus_cfg = {
        .sample_rate_hz = sample_rate,
        .channels = 1,  // Mono
        .application = 0,  // VOIP
        .bitrate_bps = 16000,  // 16 kbps
    };
    
    esp_err_t ret = sx_codec_opus_encoder_init(&opus_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Opus encoder: %s", esp_err_to_name(ret));
        return ret;
    }
    
    s_opus_encoder_initialized = true;
    s_opus_sample_rate = sample_rate;
    s_opus_frame_samples = (sample_rate * s_opus_frame_duration_ms) / 1000;
    
    ESP_LOGI(TAG, "Opus encoder initialized: %lu Hz, %lu samples/frame",
             s_opus_sample_rate, s_opus_frame_samples);
    
    return ESP_OK;
}

// Audio send task - records audio and sends via protocol
static void audio_send_task(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "Audio send task started");
    
    // Initialize Opus encoder
    if (init_opus_encoder(16000) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Opus encoder, task exiting");
        vTaskDelete(NULL);
        return;
    }
    
    // Set up recording callback to receive PCM data
    extern esp_err_t sx_audio_set_recording_callback(sx_audio_recording_callback_t callback);
    
    // Reset accumulated buffer
    if (xSemaphoreTake(s_pcm_mutex, portMAX_DELAY) == pdTRUE) {
        s_accumulated_count = 0;
        xSemaphoreGive(s_pcm_mutex);
    }
    
    // Set callback to accumulate PCM samples
    sx_audio_set_recording_callback(recording_callback);
    
    // Start recording through audio service
    esp_err_t ret = sx_audio_start_recording();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start recording: %s", esp_err_to_name(ret));
        sx_audio_set_recording_callback(NULL);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Recording started, collecting PCM data...");
    
    // Note: pcm_frame removed - using stack-allocated frame_samples[960] instead (line 184)
    
    uint8_t *opus_packet = (uint8_t *)malloc(OPUS_ENCODE_BUFFER_SIZE);
    if (opus_packet == NULL) {
        ESP_LOGE(TAG, "Failed to allocate Opus packet buffer");
        sx_audio_stop_recording();
        sx_audio_set_recording_callback(NULL);
        vTaskDelete(NULL);
        return;
    }
    
    while (s_send_enabled) {
        // Wait for frame duration
        vTaskDelay(pdMS_TO_TICKS(s_opus_frame_duration_ms));
        
        // Check if we have enough accumulated samples
        bool has_enough_samples = false;
        if (xSemaphoreTake(s_pcm_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {  // Increased timeout from 10ms to 50ms
            has_enough_samples = (s_accumulated_count >= s_opus_frame_samples);
            xSemaphoreGive(s_pcm_mutex);
        }
        
        if (has_enough_samples) {
            // Copy samples for encoding (with mutex protection)
            int16_t frame_samples[960];
            size_t samples_to_encode = 0;
            
            if (xSemaphoreTake(s_pcm_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {  // Increased timeout from 10ms to 50ms
                samples_to_encode = (s_accumulated_count >= s_opus_frame_samples) ? 
                                   s_opus_frame_samples : s_accumulated_count;
                memcpy(frame_samples, s_accumulated_pcm, samples_to_encode * sizeof(int16_t));
                
                // Remove used samples
                if (s_accumulated_count > samples_to_encode) {
                    size_t remaining = s_accumulated_count - samples_to_encode;
                    memmove(s_accumulated_pcm, &s_accumulated_pcm[samples_to_encode], 
                           remaining * sizeof(int16_t));
                    s_accumulated_count = remaining;
                } else {
                    s_accumulated_count = 0;
                }
                xSemaphoreGive(s_pcm_mutex);
            }
            
            if (samples_to_encode >= s_opus_frame_samples) {
                // Encode to Opus
                size_t opus_size = 0;
                ret = sx_codec_opus_encode(frame_samples, s_opus_frame_samples,
                                          opus_packet, OPUS_ENCODE_BUFFER_SIZE, &opus_size);
                
                if (ret == ESP_OK && opus_size > 0) {
                    // Create audio packet (copy payload)
                    uint8_t *packet_payload = (uint8_t *)malloc(opus_size);
                    if (packet_payload != NULL) {
                        memcpy(packet_payload, opus_packet, opus_size);
                        
                        sx_audio_stream_packet_t packet = {
                            .sample_rate = s_opus_sample_rate,
                            .frame_duration = s_opus_frame_duration_ms,
                            .timestamp = s_timestamp,
                            .payload = packet_payload,
                            .payload_size = opus_size,
                        };
                        
                        // Phase 1: Migrate to use protocol base interface (eliminate duplicate code)
                        sx_protocol_base_t *protocol = sx_protocol_factory_get_current();
                        esp_err_t send_ret = ESP_FAIL;
                        if (protocol && protocol->ops && protocol->ops->is_connected && 
                            protocol->ops->is_connected(protocol) &&
                            protocol->ops->is_audio_channel_opened &&
                            protocol->ops->is_audio_channel_opened(protocol) &&
                            protocol->ops->send_audio) {
                            send_ret = protocol->ops->send_audio(protocol, &packet);
                        }
                        // Break loop on error to avoid spamming
                        if (send_ret != ESP_OK) {
                            free(packet_payload);  // Fix: Free before break to prevent memory leak
                            s_send_error_count++;  // Track error count
                            ESP_LOGW(TAG, "Audio send failed (errors: %lu), breaking loop", s_send_error_count);
                            break;
                        }
                        
                        free(packet_payload);
                        s_timestamp += s_opus_frame_duration_ms;
                    }
                }
            }
        }
    }
    
    // Cleanup
    free(opus_packet);
    sx_audio_set_recording_callback(NULL);
    sx_audio_stop_recording();
    ESP_LOGI(TAG, "Audio send task stopped");
    vTaskDelete(NULL);
}

// Audio receive task - receives audio from protocol and plays
static void audio_receive_task(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "Audio receive task started");
    
    // Initialize Opus decoder
    sx_opus_decoder_config_t decoder_cfg = {
        .sample_rate_hz = 16000,  // Will be updated from packet
        .channels = 1,
    };
    esp_err_t decoder_ret = sx_codec_opus_decoder_init(&decoder_cfg);
    if (decoder_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Opus decoder: %s", esp_err_to_name(decoder_ret));
        vTaskDelete(NULL);
        return;
    }
    
    uint32_t current_sample_rate = 16000;
    uint32_t current_frame_duration = 60;  // Default 60ms như repo mẫu
    
    int16_t *pcm_buffer = (int16_t *)malloc(960 * sizeof(int16_t));  // Max frame size (20ms @ 48kHz)
    if (pcm_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate PCM buffer for receive task");
        sx_codec_opus_decoder_deinit();
        vTaskDelete(NULL);
        return;
    }
    
    sx_audio_stream_packet_t packet;
    
    while (s_receive_enabled) {
        // Wait for audio packet from queue
        if (xQueueReceive(s_audio_receive_queue, &packet, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (packet.payload != NULL && packet.payload_size > 0) {
                // Update decoder if sample rate or frame duration changed
                if (packet.sample_rate != current_sample_rate || 
                    packet.frame_duration != current_frame_duration) {
                    decoder_cfg.sample_rate_hz = packet.sample_rate;
                    esp_err_t ret = sx_codec_opus_decoder_init(&decoder_cfg);
                    if (ret == ESP_OK) {
                        current_sample_rate = packet.sample_rate;
                        current_frame_duration = packet.frame_duration;
                        ESP_LOGI(TAG, "Decoder updated: %lu Hz, %lu ms frame",
                                current_sample_rate, current_frame_duration);
                    } else {
                        ESP_LOGW(TAG, "Failed to update decoder: %s", esp_err_to_name(ret));
                    }
                }
                
                // Decode Opus to PCM
                size_t pcm_samples = 0;
                esp_err_t ret = sx_codec_opus_decode(packet.payload, packet.payload_size,
                                                    pcm_buffer, 960, &pcm_samples);
                
                if (ret == ESP_OK && pcm_samples > 0) {
                    // Feed PCM to audio service
                    sx_audio_service_feed_pcm(pcm_buffer, pcm_samples, packet.sample_rate);
                    ESP_LOGD(TAG, "Decoded and fed %zu PCM samples @ %lu Hz", 
                            pcm_samples, packet.sample_rate);
                } else {
                    s_decode_error_count++;  // Track decode errors
                    ESP_LOGW(TAG, "Failed to decode Opus packet (errors: %lu): %s", 
                            s_decode_error_count, esp_err_to_name(ret));
                }
            }
            
            // Free packet payload if allocated
            if (packet.payload != NULL) {
                free(packet.payload);
            }
        }
    }
    
    free(pcm_buffer);
    sx_codec_opus_decoder_deinit();
    ESP_LOGI(TAG, "Audio receive task stopped");
    vTaskDelete(NULL);
}

// Callback for incoming audio packets from protocol
static void on_audio_packet_received(const sx_audio_stream_packet_t *packet) {
    if (!s_receive_enabled || packet == NULL) {
        return;
    }

    // State-based audio routing: only accept audio when device is BUSY (speaking)
    sx_state_t st;
    sx_dispatcher_get_state(&st);
    if (st.ui.device_state != SX_DEV_BUSY) {
        // Drop packet if not in correct state
        return;
    }
    
    // Copy packet to queue
    sx_audio_stream_packet_t packet_copy = *packet;
    packet_copy.payload = (uint8_t *)malloc(packet->payload_size);
    if (packet_copy.payload == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for audio packet");
        return;
    }
    
    memcpy(packet_copy.payload, packet->payload, packet->payload_size);
    
    if (xQueueSend(s_audio_receive_queue, &packet_copy, 0) != pdTRUE) {
        s_receive_drop_count++;  // Track dropped packets
        ESP_LOGW(TAG, "Audio receive queue full, dropping packet (drops: %lu)", s_receive_drop_count);
        free(packet_copy.payload);
    }
}

esp_err_t sx_audio_protocol_bridge_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    // Create queues
    s_audio_send_queue = xQueueCreate(AUDIO_SEND_QUEUE_SIZE, sizeof(sx_audio_stream_packet_t));
    s_audio_receive_queue = xQueueCreate(AUDIO_RECEIVE_QUEUE_SIZE, sizeof(sx_audio_stream_packet_t));
    
    if (s_audio_send_queue == NULL || s_audio_receive_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create audio queues");
        return ESP_ERR_NO_MEM;
    }
    
    // Create mutex for PCM buffer
    s_pcm_mutex = xSemaphoreCreateMutex();
    if (s_pcm_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create PCM mutex");
        return ESP_ERR_NO_MEM;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "Audio protocol bridge initialized");
    return ESP_OK;
}

esp_err_t sx_audio_protocol_bridge_start(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_started) {
        return ESP_OK;
    }
    
    s_started = true;
    ESP_LOGI(TAG, "Audio protocol bridge started");
    return ESP_OK;
}

esp_err_t sx_audio_protocol_bridge_stop(void) {
    if (!s_started) {
        return ESP_OK;
    }
    
    // Disable send/receive
    sx_audio_protocol_bridge_enable_send(false);
    sx_audio_protocol_bridge_enable_receive(false);
    
    s_started = false;
    ESP_LOGI(TAG, "Audio protocol bridge stopped");
    return ESP_OK;
}

esp_err_t sx_audio_protocol_bridge_enable_send(bool enable) {
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_send_enabled == enable) {
        return ESP_OK;  // Already in desired state
    }
    
    s_send_enabled = enable;
    
    if (enable) {
        // Start send task
        xTaskCreatePinnedToCore(audio_send_task, "audio_send", 4096, NULL, 5, 
                               &s_send_task_handle, tskNO_AFFINITY);
        ESP_LOGI(TAG, "Audio sending enabled");
    } else {
        // Stop send task
        if (s_send_task_handle != NULL) {
            vTaskDelete(s_send_task_handle);
            s_send_task_handle = NULL;
        }
        ESP_LOGI(TAG, "Audio sending disabled");
    }
    
    return ESP_OK;
}

esp_err_t sx_audio_protocol_bridge_enable_receive(bool enable) {
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_receive_enabled == enable) {
        return ESP_OK;  // Already in desired state
    }
    
    s_receive_enabled = enable;
    
    if (enable) {
        // Phase 1: Register audio callback with all available protocols (via base interface)
        sx_protocol_base_t *ws_protocol = sx_protocol_ws_get_base();
        if (ws_protocol && ws_protocol->ops && ws_protocol->ops->set_audio_callback) {
            ws_protocol->ops->set_audio_callback(ws_protocol, on_audio_packet_received);
        }
        
        sx_protocol_base_t *mqtt_protocol = sx_protocol_mqtt_get_base();
        if (mqtt_protocol && mqtt_protocol->ops && mqtt_protocol->ops->set_audio_callback) {
            mqtt_protocol->ops->set_audio_callback(mqtt_protocol, on_audio_packet_received);
        }
        
        // Start receive task
        xTaskCreatePinnedToCore(audio_receive_task, "audio_recv", 4096, NULL, 5,
                               &s_receive_task_handle, tskNO_AFFINITY);
        ESP_LOGI(TAG, "Audio receiving enabled");
    } else {
        // Unregister callbacks
        sx_protocol_base_t *ws_protocol = sx_protocol_ws_get_base();
        if (ws_protocol && ws_protocol->ops && ws_protocol->ops->set_audio_callback) {
            ws_protocol->ops->set_audio_callback(ws_protocol, NULL);
        }
        
        sx_protocol_base_t *mqtt_protocol = sx_protocol_mqtt_get_base();
        if (mqtt_protocol && mqtt_protocol->ops && mqtt_protocol->ops->set_audio_callback) {
            mqtt_protocol->ops->set_audio_callback(mqtt_protocol, NULL);
        }
        
        // Stop receive task
        if (s_receive_task_handle != NULL) {
            vTaskDelete(s_receive_task_handle);
            s_receive_task_handle = NULL;
        }
        ESP_LOGI(TAG, "Audio receiving disabled");
    }
    
    return ESP_OK;
}

bool sx_audio_protocol_bridge_is_sending_enabled(void) {
    return s_send_enabled;
}

bool sx_audio_protocol_bridge_is_receiving_enabled(void) {
    return s_receive_enabled;
}

// Update frame duration from server hello message (optimization: dynamic frame duration)
esp_err_t sx_audio_protocol_bridge_update_frame_duration(uint32_t frame_duration_ms) {
    if (frame_duration_ms == 0 || frame_duration_ms > 120) {
        return ESP_ERR_INVALID_ARG;  // Sanity check: 1-120ms
    }
    
    if (s_opus_frame_duration_ms != frame_duration_ms) {
        s_opus_frame_duration_ms = frame_duration_ms;
        s_opus_frame_samples = (s_opus_sample_rate * s_opus_frame_duration_ms) / 1000;
        ESP_LOGI(TAG, "Frame duration updated: %lu ms (%lu samples/frame @ %lu Hz)",
                s_opus_frame_duration_ms, s_opus_frame_samples, s_opus_sample_rate);
    }
    
    return ESP_OK;
}

// Get error statistics
sx_audio_bridge_stats_t sx_audio_protocol_bridge_get_stats(void) {
    sx_audio_bridge_stats_t stats = {
        .send_errors = s_send_error_count,
        .receive_drops = s_receive_drop_count,
        .decode_errors = s_decode_error_count,
    };
    return stats;
}

// Reset error statistics
void sx_audio_protocol_bridge_reset_stats(void) {
    s_send_error_count = 0;
    s_receive_drop_count = 0;
    s_decode_error_count = 0;
    ESP_LOGI(TAG, "Error statistics reset");
}

