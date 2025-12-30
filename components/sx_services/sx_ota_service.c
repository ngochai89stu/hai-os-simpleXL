#include "sx_ota_service.h"

#include <esp_log.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "sx_dispatcher.h"
#include "sx_event.h"

static const char *TAG = "sx_ota";

static bool s_initialized = false;
static bool s_updating = false;
static char s_last_error[256] = {0};
static void (*s_progress_callback)(int percent) = NULL;

// Forward declarations
static void sx_ota_task(void *arg);

esp_err_t sx_ota_service_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "OTA service initialized");
    return ESP_OK;
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                int64_t content_len = esp_http_client_get_content_length(evt->client);
                if (content_len > 0 && s_progress_callback != NULL) {
                    int64_t data_len = evt->data_len;
                    // Calculate progress (simplified)
                    if (content_len > 0) {
                        int percent = (int)((data_len * 100) / content_len);
                        if (percent > 100) percent = 100;
                        s_progress_callback(percent);
                    }
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void sx_ota_task(void *arg) {
    sx_ota_config_t *config = (sx_ota_config_t *)arg;
    
    ESP_LOGI(TAG, "Starting OTA update from: %s", config->url);
    
    esp_http_client_config_t http_config = {
        .url = config->url,
        .cert_pem = config->cert_pem,
        .timeout_ms = config->timeout_ms,
        .event_handler = http_event_handler,
    };
    
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };
    
    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t ret = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed: %s", esp_err_to_name(ret));
        snprintf(s_last_error, sizeof(s_last_error), "OTA begin failed: %s", esp_err_to_name(ret));
        s_updating = false;
        free(config);
        vTaskDelete(NULL);
        return;
    }
    
    while (1) {
        ret = esp_https_ota_perform(https_ota_handle);
        if (ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        
        // Report progress
        int image_header_was_was_valid = esp_https_ota_get_image_len_read(https_ota_handle);
        int image_header_total_len = esp_https_ota_get_image_size(https_ota_handle);
        if (image_header_total_len > 0 && s_progress_callback != NULL) {
            int percent = (int)((image_header_was_was_valid * 100) / image_header_total_len);
            if (percent > 100) percent = 100;
            s_progress_callback(percent);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA update successful");
        esp_err_t ret2 = esp_https_ota_finish(https_ota_handle);
        if (ret2 != ESP_OK) {
            ESP_LOGE(TAG, "esp_https_ota_finish failed: %s", esp_err_to_name(ret2));
            snprintf(s_last_error, sizeof(s_last_error), "OTA finish failed: %s", esp_err_to_name(ret2));
        } else {
            ESP_LOGI(TAG, "OTA update completed, rebooting...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
        }
    } else {
        ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ret));
        snprintf(s_last_error, sizeof(s_last_error), "OTA update failed: %s", esp_err_to_name(ret));
        esp_https_ota_abort(https_ota_handle);
    }
    
    s_updating = false;
    free(config);
    vTaskDelete(NULL);
}

esp_err_t sx_ota_start_update(const sx_ota_config_t *config, void (*progress_callback)(int percent)) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (config == NULL || config->url == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (s_updating) {
        ESP_LOGW(TAG, "OTA update already in progress");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Allocate config copy for task
    sx_ota_config_t *config_copy = malloc(sizeof(sx_ota_config_t));
    if (config_copy == NULL) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(config_copy, config, sizeof(sx_ota_config_t));
    
    // Copy URL
    size_t url_len = strlen(config->url) + 1;
    config_copy->url = malloc(url_len);
    if (config_copy->url == NULL) {
        free(config_copy);
        return ESP_ERR_NO_MEM;
    }
    strcpy((char *)config_copy->url, config->url);
    
    // Copy cert if provided
    if (config->cert_pem != NULL) {
        size_t cert_len = strlen(config->cert_pem) + 1;
        config_copy->cert_pem = malloc(cert_len);
        if (config_copy->cert_pem == NULL) {
            free((void *)config_copy->url);
            free(config_copy);
            return ESP_ERR_NO_MEM;
        }
        strcpy((char *)config_copy->cert_pem, config->cert_pem);
    } else {
        config_copy->cert_pem = NULL;
    }
    
    s_progress_callback = progress_callback;
    s_updating = true;
    memset(s_last_error, 0, sizeof(s_last_error));
    
    // Create OTA task
    BaseType_t ret = xTaskCreatePinnedToCore(
        sx_ota_task,
        "sx_ota",
        8192,
        config_copy,
        5,
        NULL,
        tskNO_AFFINITY
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA task");
        free((void *)config_copy->url);
        if (config_copy->cert_pem != NULL) {
            free((void *)config_copy->cert_pem);
        }
        free(config_copy);
        s_updating = false;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "OTA update task created");
    return ESP_OK;
}

bool sx_ota_is_updating(void) {
    return s_updating;
}

const char *sx_ota_get_last_error(void) {
    return s_last_error;
}

