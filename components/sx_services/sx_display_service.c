#include "sx_display_service.h"
#include "sx_image_service.h"
#include "sx_jpeg_encoder.h"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_event_payloads.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static const char *TAG = "sx_display";

// Display service state
static bool s_initialized = false;
// Note: Preview image object is now managed by UI task via events (P0.1)

// JPEG encoding helper - use sx_jpeg_encoder
static esp_err_t encode_rgb565_to_jpeg_simple(const uint8_t *rgb565_data, uint16_t width, uint16_t height,
                                               uint8_t quality, uint8_t **jpeg_data, size_t *jpeg_size) {
    // Use minimal JPEG encoder
    return sx_jpeg_encode_rgb565(rgb565_data, width, height, quality, jpeg_data, jpeg_size);
}

// Initialize display service
esp_err_t sx_display_service_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "Display service initialized");
    return ESP_OK;
}

// Capture screen to RGB565 buffer
// Note: This function now requests UI task to perform capture via event (P0.1)
// The actual capture is done by UI task, which sends SX_EVT_DISPLAY_CAPTURE_DONE event
esp_err_t sx_display_capture_screen(uint8_t *buffer, uint16_t width, uint16_t height) {
    if (!s_initialized || !buffer) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Request UI task to capture screen via event (P0.1 - remove LVGL from service)
    sx_event_t evt = {
        .type = SX_EVT_DISPLAY_CAPTURE_REQUEST,
        .priority = SX_EVT_PRIORITY_NORMAL,
        .arg0 = width,
        .arg1 = height,
        .ptr = buffer  // UI will fill this buffer
    };
    
    if (!sx_dispatcher_post_event(&evt)) {
        ESP_LOGE(TAG, "Failed to post capture request event");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Screen capture requested: %dx%d (UI task will handle)", width, height);
    // Note: Caller should wait for SX_EVT_DISPLAY_CAPTURE_DONE event
    // For synchronous API, this would need to be refactored to async pattern
    return ESP_OK;
}

// Encode RGB565 to JPEG
esp_err_t sx_display_encode_jpeg(const uint8_t *rgb565_data, uint16_t width, uint16_t height, 
                                 uint8_t quality, uint8_t **jpeg_data, size_t *jpeg_size) {
    if (!rgb565_data || !jpeg_data || !jpeg_size) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Use placeholder encoder (requires proper JPEG encoder)
    return encode_rgb565_to_jpeg_simple(rgb565_data, width, height, quality, jpeg_data, jpeg_size);
}

// Upload JPEG to URL
esp_err_t sx_display_upload_jpeg(const uint8_t *jpeg_data, size_t jpeg_size, const char *url) {
    if (!jpeg_data || jpeg_size == 0 || !url) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_http_client_config_t config = {0};
    config.url = url;
    config.timeout_ms = 30000;
    config.method = HTTP_METHOD_POST;
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }
    
    // Set content type
    esp_http_client_set_header(client, "Content-Type", "image/jpeg");
    esp_http_client_set_post_field(client, (const char *)jpeg_data, jpeg_size);
    
    esp_err_t ret = esp_http_client_perform(client);
    if (ret == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Upload status: %d", status_code);
        if (status_code >= 200 && status_code < 300) {
            ret = ESP_OK;
        } else {
            ret = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP upload failed: %s", esp_err_to_name(ret));
    }
    
    esp_http_client_cleanup(client);
    return ret;
}

// Download image from URL
esp_err_t sx_display_download_image(const char *url, uint8_t **data, size_t *data_size) {
    if (!url || !data || !data_size) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_http_client_config_t config = {0};
    config.url = url;
    config.timeout_ms = 30000;
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }
    
    esp_err_t ret = esp_http_client_perform(client);
    if (ret == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200) {
            int content_length = esp_http_client_get_content_length(client);
            if (content_length > 0 && content_length < 1024 * 1024) { // Max 1MB
                uint8_t *buffer = (uint8_t *)malloc(content_length);
                if (buffer) {
                    int data_read = esp_http_client_read_response(client, (char *)buffer, content_length);
                    if (data_read > 0) {
                        *data = buffer;
                        *data_size = data_read;
                        ESP_LOGI(TAG, "Downloaded image: %d bytes", data_read);
                        esp_http_client_cleanup(client);
                        return ESP_OK;
                    }
                    free(buffer);
                }
            }
        }
    }
    
    esp_http_client_cleanup(client);
    return ESP_FAIL;
}

// Display image on screen
// Note: image_data will be copied internally, caller can free after this call
// P0.1: Refactored to use events - UI task handles LVGL operations
esp_err_t sx_display_show_image(const uint8_t *image_data, uint16_t width, uint16_t height, uint32_t timeout_ms) {
    if (!s_initialized || !image_data) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Copy image data for event payload (service owns this copy, UI will copy again if needed)
    size_t data_size = width * height * 2; // RGB565 = 2 bytes per pixel
    sx_display_preview_payload_t *payload = (sx_display_preview_payload_t *)malloc(sizeof(sx_display_preview_payload_t));
    if (!payload) {
        ESP_LOGE(TAG, "Failed to allocate preview payload");
        return ESP_ERR_NO_MEM;
    }
    
    uint8_t *image_copy = (uint8_t *)malloc(data_size);
    if (!image_copy) {
        free(payload);
        ESP_LOGE(TAG, "Failed to allocate image copy");
        return ESP_ERR_NO_MEM;
    }
    memcpy(image_copy, image_data, data_size);
    
    payload->image_data = image_copy;
    payload->width = width;
    payload->height = height;
    payload->timeout_ms = timeout_ms;
    
    // Request UI task to display image via event (P0.1)
    sx_event_t evt = {
        .type = SX_EVT_DISPLAY_PREVIEW_REQUEST,
        .priority = SX_EVT_PRIORITY_NORMAL,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = payload  // UI will handle and free this payload
    };
    
    if (!sx_dispatcher_post_event(&evt)) {
        free(image_copy);
        free(payload);
        ESP_LOGE(TAG, "Failed to post preview request event");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Image preview requested: %dx%d (timeout: %lu ms)", width, height, timeout_ms);
    return ESP_OK;
}

// Remove displayed image
// P0.1: Refactored to use events - UI task handles LVGL operations
esp_err_t sx_display_hide_image(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Request UI task to hide preview via event (P0.1)
    sx_event_t evt = {
        .type = SX_EVT_DISPLAY_HIDE_REQUEST,
        .priority = SX_EVT_PRIORITY_NORMAL,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = NULL
    };
    
    if (!sx_dispatcher_post_event(&evt)) {
        ESP_LOGE(TAG, "Failed to post hide request event");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Hide preview requested");
    return ESP_OK;
}

