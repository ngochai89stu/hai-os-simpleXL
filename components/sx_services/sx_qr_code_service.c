#include "sx_qr_code_service.h"
// Include other headers after sx_qr_code_service.h to ensure struct definition is visible
#include "sx_dispatcher.h"
#include "sx_event.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
// P0.1: Removed LVGL include - UI task handles LVGL QR code widget creation

#ifdef __cplusplus
extern "C" {
#endif

static const char *TAG = "sx_qrcode";

// QR code service state
static bool s_initialized = false;

esp_err_t sx_qr_code_service_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "QR code service initialized");
    ESP_LOGW(TAG, "QR code generation requires external library (qrcodegen or similar)");
    return ESP_OK;
}

esp_err_t sx_qr_code_generate(const char *text, const sx_qr_code_config_t *config, sx_qr_code_result_t *result) {
    if (!s_initialized || text == NULL || result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Default configuration
    sx_qr_code_config_t default_config = {
        .version = 0,  // Auto
        .error_correction = 1, // M (Medium)
        .module_size = 3,
        .border = 4,
    };
    
    const sx_qr_code_config_t *cfg = (config != NULL) ? config : &default_config;
    
    ESP_LOGI(TAG, "Generating QR code for: %s", text);
    
    // P0.1: Generate QR code data without LVGL
    // Use simple placeholder pattern (in production, use proper QR code library)
    uint16_t size = 21 + cfg->border * 2; // Version 1 QR code size
    size_t data_size = (size * size + 7) / 8; // 1 bit per pixel
    
    uint8_t *data = (uint8_t *)malloc(data_size);
    if (data == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    memset(data, 0, data_size);
    
    // Simple pattern based on text hash
    uint32_t hash = 0;
    for (const char *p = text; *p; p++) {
        hash = hash * 31 + *p;
    }
    
    for (uint16_t y = 0; y < size; y++) {
        for (uint16_t x = 0; x < size; x++) {
            uint32_t pattern = hash + x * 17 + y * 23;
            if (pattern % 3 == 0) {
                uint16_t bit_index = y * size + x;
                data[bit_index / 8] |= (1 << (7 - (bit_index % 8)));
            }
        }
    }
    
    result->width = size * cfg->module_size;
    result->height = size * cfg->module_size;
    result->data = data;
    result->data_size = data_size;
    
    ESP_LOGI(TAG, "Generated QR code: %dx%d", result->width, result->height);
    return ESP_OK;
}

esp_err_t sx_qr_code_generate_ip(const char *ip_address, uint16_t port, sx_qr_code_result_t *result) {
    if (!s_initialized || ip_address == NULL || result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Format text for QR code
    char qr_text[128];
    if (port > 0) {
        snprintf(qr_text, sizeof(qr_text), "http://%s:%d", ip_address, port);
    } else {
        snprintf(qr_text, sizeof(qr_text), "http://%s", ip_address);
    }
    
    ESP_LOGI(TAG, "Generating QR code for IP: %s", qr_text);
    return sx_qr_code_generate(qr_text, NULL, result);
}

void sx_qr_code_free_result(sx_qr_code_result_t *result) {
    if (result != NULL && result->data != NULL) {
        free(result->data);
        result->data = NULL;
        result->data_size = 0;
        result->width = 0;
        result->height = 0;
    }
}

// P0.1: sx_qr_code_create_lvgl_widget() removed - moved to UI helper
// UI task should create LVGL QR code widgets directly using sx_lvgl.h
// Service only provides QR code data, UI handles LVGL operations

#ifdef __cplusplus
}
#endif



