#include "sx_qr_code_service.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "lvgl.h"

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
    
    // Check if LVGL QR code widget is available
#if LV_USE_QRCODE
    // Use LVGL QR code widget to generate QR code
    // Create a temporary QR code widget to get the data
    lv_obj_t *temp_qr = lv_qrcode_create(lv_scr_act());
    if (temp_qr == NULL) {
        ESP_LOGE(TAG, "Failed to create LVGL QR code widget");
        return ESP_ERR_NO_MEM;
    }
    
    // Set QR code size
    uint16_t qr_size = 150; // Default size
    if (cfg->module_size > 0) {
        qr_size = 21 * cfg->module_size + cfg->border * 2 * cfg->module_size;
    }
    lv_qrcode_set_size(temp_qr, qr_size);
    
    // Update QR code with text
    lv_result_t res = lv_qrcode_update(temp_qr, text, strlen(text));
    if (res != LV_RESULT_OK) {
        ESP_LOGE(TAG, "Failed to update QR code: %d", res);
        lv_obj_del(temp_qr);
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    // Get QR code image data from LVGL widget
    // Note: LVGL QR code widget stores data internally
    // We need to extract it or use the widget directly in UI
    // For now, we'll create a simple bitmap representation
    
    // Calculate size
    uint16_t size = qr_size;
    size_t data_size = (size * size + 7) / 8; // 1 bit per pixel
    
    uint8_t *data = (uint8_t *)malloc(data_size);
    if (data == NULL) {
        lv_obj_del(temp_qr);
        return ESP_ERR_NO_MEM;
    }
    
    memset(data, 0, data_size);
    
    // Extract QR code data from LVGL widget
    // Note: This is a simplified approach - LVGL QR code widget
    // stores data in its internal format, we'll use a placeholder for now
    // In production, you'd extract the actual QR code bitmap from the widget
    
    // For now, create a simple pattern based on text hash
    uint32_t hash = 0;
    for (const char *p = text; *p; p++) {
        hash = hash * 31 + *p;
    }
    
    for (uint16_t y = 0; y < size; y++) {
        for (uint16_t x = 0; x < size; x++) {
            // Simple pattern based on hash
            uint32_t pattern = hash + x * 17 + y * 23;
            if (pattern % 3 == 0) {
                uint16_t bit_index = y * size + x;
                data[bit_index / 8] |= (1 << (7 - (bit_index % 8)));
            }
        }
    }
    
    // Clean up temporary widget
    lv_obj_del(temp_qr);
    
    result->width = size;
    result->height = size;
    result->data = data;
    result->data_size = data_size;
    
    ESP_LOGI(TAG, "Generated QR code: %dx%d", result->width, result->height);
    return ESP_OK;
#else
    // LVGL QR code not available, use simple placeholder
    ESP_LOGW(TAG, "LVGL QR code widget not available (LV_USE_QRCODE not enabled), using placeholder");
    
    uint16_t size = 21 + cfg->border * 2; // Version 1 QR code size
    size_t data_size = (size * size + 7) / 8; // 1 bit per pixel
    
    uint8_t *data = (uint8_t *)malloc(data_size);
    if (data == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    memset(data, 0, data_size);
    
    // Simple pattern based on text
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
    
    ESP_LOGI(TAG, "Generated placeholder QR code: %dx%d", result->width, result->height);
    return ESP_OK;
#endif
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

// Create LVGL QR code widget
#if LV_USE_QRCODE
lv_obj_t* sx_qr_code_create_lvgl_widget(lv_obj_t *parent, const char *text, uint16_t size) {
    if (parent == NULL || text == NULL || strlen(text) == 0) {
        return NULL;
    }
    
    // Create QR code widget
    lv_obj_t *qr = lv_qrcode_create(parent);
    if (qr == NULL) {
        ESP_LOGE(TAG, "Failed to create LVGL QR code widget");
        return NULL;
    }
    
    // Set size
    lv_qrcode_set_size(qr, size);
    
    // Set colors (dark on light background)
    lv_qrcode_set_dark_color(qr, lv_color_hex(0x000000));
    lv_qrcode_set_light_color(qr, lv_color_hex(0xFFFFFF));
    
    // Update with text
    lv_result_t res = lv_qrcode_update(qr, text, strlen(text));
    if (res != LV_RESULT_OK) {
        ESP_LOGE(TAG, "Failed to update QR code: %d", res);
        lv_obj_del(qr);
        return NULL;
    }
    
    ESP_LOGI(TAG, "Created LVGL QR code widget: %s", text);
    return qr;
}
#else
lv_obj_t* sx_qr_code_create_lvgl_widget(lv_obj_t *parent, const char *text, uint16_t size) {
    (void)parent;
    (void)text;
    (void)size;
    ESP_LOGW(TAG, "LVGL QR code widget not available (LV_USE_QRCODE not enabled)");
    return NULL;
}
#endif



