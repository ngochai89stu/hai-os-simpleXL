#include "sx_stt_service.h"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_settings_service.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "cJSON.h"

static const char *TAG = "sx_stt";

// STT service state
static bool s_initialized = false;
static bool s_active = false;
static sx_stt_config_t s_config = {0};
static sx_stt_result_cb_t s_result_callback = NULL;
static void *s_user_data = NULL;
static char s_last_error[256] = {0};

// Audio chunk queue
#define STT_CHUNK_QUEUE_SIZE 5
static QueueHandle_t s_chunk_queue = NULL;
static TaskHandle_t s_stt_task_handle = NULL;
static SemaphoreHandle_t s_stt_mutex = NULL;

// Audio chunk structure
typedef struct {
    int16_t *pcm;
    size_t sample_count;
} stt_audio_chunk_t;

static void sx_stt_task(void *arg) {
    (void)arg;
    
    ESP_LOGI(TAG, "STT task started");
    
    while (s_active) {
        stt_audio_chunk_t chunk;
        if (xQueueReceive(s_chunk_queue, &chunk, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Send audio chunk to STT endpoint
            esp_http_client_config_t http_config = {
                .url = s_config.endpoint_url,
                .timeout_ms = 10000,
            };
            
            esp_http_client_handle_t client = esp_http_client_init(&http_config);
            if (client == NULL) {
                ESP_LOGE(TAG, "Failed to create HTTP client");
                free(chunk.pcm);
                continue;
            }
            
            // Set headers
            esp_http_client_set_method(client, HTTP_METHOD_POST);
            esp_http_client_set_header(client, "Content-Type", "audio/pcm;rate=16000;channels=1");
            if (s_config.api_key != NULL) {
                char auth_header[128];
                snprintf(auth_header, sizeof(auth_header), "Bearer %s", s_config.api_key);
                esp_http_client_set_header(client, "Authorization", auth_header);
            }
            
            // Set POST data (PCM audio)
            size_t data_len = chunk.sample_count * sizeof(int16_t);
            esp_http_client_set_post_field(client, (const char *)chunk.pcm, data_len);
            
            // Perform request
            esp_err_t ret = esp_http_client_perform(client);
            if (ret == ESP_OK) {
                int status_code = esp_http_client_get_status_code(client);
                int content_length = esp_http_client_get_content_length(client);
                
                if (status_code == 200 && content_length > 0) {
                    // Read response
                    char *response = (char *)malloc(content_length + 1);
                    if (response != NULL) {
                        int read_len = esp_http_client_read_response(client, response, content_length);
                        if (read_len > 0) {
                            response[read_len] = '\0';
                            
                            // Parse JSON response
                            cJSON *json = cJSON_Parse(response);
                            if (json != NULL) {
                                cJSON *transcript = cJSON_GetObjectItem(json, "transcript");
                                cJSON *is_final = cJSON_GetObjectItem(json, "is_final");
                                
                                if (transcript != NULL && cJSON_IsString(transcript)) {
                                    bool final = (is_final != NULL && cJSON_IsTrue(is_final));
                                    
                                    // Call callback
                                    if (s_result_callback != NULL) {
                                        s_result_callback(transcript->valuestring, final, s_user_data);
                                    }
                                    
                                    // Dispatch event if final
                                    if (final) {
                                        sx_event_t evt = {
                                            .type = SX_EVT_UI_INPUT,
                                            .arg0 = 0,
                                            .arg1 = 0,
                                            .ptr = strdup(transcript->valuestring), // Allocate copy for event
                                        };
                                        sx_dispatcher_post_event(&evt);
                                    }
                                }
                                cJSON_Delete(json);
                            }
                        }
                        free(response);
                    }
                } else {
                    ESP_LOGW(TAG, "STT request failed: status=%d", status_code);
                    snprintf(s_last_error, sizeof(s_last_error), "HTTP %d", status_code);
                }
            } else {
                ESP_LOGE(TAG, "STT HTTP request failed: %s", esp_err_to_name(ret));
                snprintf(s_last_error, sizeof(s_last_error), "HTTP error: %s", esp_err_to_name(ret));
            }
            
            esp_http_client_cleanup(client);
            free(chunk.pcm);
        }
    }
    
    ESP_LOGI(TAG, "STT task ended");
    vTaskDelete(NULL);
}

// Static buffers for settings-loaded configuration
static char s_settings_endpoint_url[512] = {0};
static char s_settings_api_key[256] = {0};

esp_err_t sx_stt_service_init(const sx_stt_config_t *config) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    s_stt_mutex = xSemaphoreCreateMutex();
    if (s_stt_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    s_chunk_queue = xQueueCreate(STT_CHUNK_QUEUE_SIZE, sizeof(stt_audio_chunk_t));
    if (s_chunk_queue == NULL) {
        vSemaphoreDelete(s_stt_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Copy configuration - load from settings if not provided
    memset(&s_config, 0, sizeof(s_config));
    memset(s_settings_endpoint_url, 0, sizeof(s_settings_endpoint_url));
    memset(s_settings_api_key, 0, sizeof(s_settings_api_key));
    
    if (config != NULL && config->endpoint_url != NULL) {
        // Use provided configuration
        s_config.endpoint_url = config->endpoint_url;
        s_config.api_key = config->api_key;
    } else {
        // Load from settings
        if (sx_settings_get_string_default("stt_endpoint_url", s_settings_endpoint_url, 
                                            sizeof(s_settings_endpoint_url), NULL) == ESP_OK &&
            s_settings_endpoint_url[0] != '\0') {
            s_config.endpoint_url = s_settings_endpoint_url;
        } else {
            ESP_LOGW(TAG, "STT endpoint URL not configured");
            vSemaphoreDelete(s_stt_mutex);
            vQueueDelete(s_chunk_queue);
            return ESP_ERR_INVALID_ARG;
        }
        
        if (sx_settings_get_string_default("stt_api_key", s_settings_api_key,
                                            sizeof(s_settings_api_key), NULL) == ESP_OK &&
            s_settings_api_key[0] != '\0') {
            s_config.api_key = s_settings_api_key;
        }
    }
    
    s_config.chunk_duration_ms = (config != NULL && config->chunk_duration_ms > 0) ? config->chunk_duration_ms : 1000;
    s_config.sample_rate_hz = (config != NULL && config->sample_rate_hz > 0) ? config->sample_rate_hz : 16000;
    s_config.auto_send = (config != NULL) ? config->auto_send : true;
    
    s_initialized = true;
    s_active = false;
    memset(s_last_error, 0, sizeof(s_last_error));
    
    ESP_LOGI(TAG, "STT service initialized (endpoint: %s)", s_config.endpoint_url);
    return ESP_OK;
}

esp_err_t sx_stt_start_session(sx_stt_result_cb_t callback, void *user_data) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_stt_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (s_active) {
        xSemaphoreGive(s_stt_mutex);
        return ESP_ERR_INVALID_STATE; // Already active
    }
    
    s_result_callback = callback;
    s_user_data = user_data;
    s_active = true;
    
    // Create STT task
    BaseType_t ret = xTaskCreate(sx_stt_task, "sx_stt", 4096, NULL, 5, &s_stt_task_handle);
    if (ret != pdPASS) {
        s_active = false;
        s_result_callback = NULL;
        s_user_data = NULL;
        xSemaphoreGive(s_stt_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    xSemaphoreGive(s_stt_mutex);
    ESP_LOGI(TAG, "STT session started");
    return ESP_OK;
}

esp_err_t sx_stt_stop_session(void) {
    if (!s_initialized || !s_active) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_stt_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    s_active = false;
    
    // Wait for task to finish
    if (s_stt_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(200));
        if (s_stt_task_handle != NULL) {
            vTaskDelete(s_stt_task_handle);
            s_stt_task_handle = NULL;
        }
    }
    
    // Clear queue
    stt_audio_chunk_t chunk;
    while (xQueueReceive(s_chunk_queue, &chunk, 0) == pdTRUE) {
        if (chunk.pcm != NULL) {
            free(chunk.pcm);
        }
    }
    
    s_result_callback = NULL;
    s_user_data = NULL;
    
    xSemaphoreGive(s_stt_mutex);
    ESP_LOGI(TAG, "STT session stopped");
    return ESP_OK;
}

esp_err_t sx_stt_send_audio_chunk(const int16_t *pcm, size_t sample_count) {
    if (!s_initialized || !s_active || pcm == NULL || sample_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Allocate chunk
    stt_audio_chunk_t chunk;
    chunk.pcm = (int16_t *)malloc(sample_count * sizeof(int16_t));
    if (chunk.pcm == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    memcpy(chunk.pcm, pcm, sample_count * sizeof(int16_t));
    chunk.sample_count = sample_count;
    
    // Send to queue
    if (xQueueSend(s_chunk_queue, &chunk, pdMS_TO_TICKS(100)) != pdTRUE) {
        free(chunk.pcm);
        ESP_LOGW(TAG, "STT chunk queue full, dropping chunk");
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

bool sx_stt_is_active(void) {
    return s_active;
}

const char* sx_stt_get_last_error(void) {
    return s_last_error;
}


