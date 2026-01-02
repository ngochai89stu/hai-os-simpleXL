#include "ui_qr_code_helpers.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ui_qr_code_helpers";

lv_obj_t* sx_ui_qr_code_create_widget(lv_obj_t *parent, const char *text, uint16_t size) {
    if (parent == NULL || text == NULL || size == 0) {
        ESP_LOGE(TAG, "Invalid parameters: parent=%p, text=%p, size=%d", parent, text, size);
        return NULL;
    }

    // Generate QR code data using service
    sx_qr_code_result_t qr_result = {0};
    esp_err_t ret = sx_qr_code_generate(text, NULL, &qr_result);
    if (ret != ESP_OK || qr_result.data == NULL) {
        ESP_LOGE(TAG, "Failed to generate QR code: %s", esp_err_to_name(ret));
        return NULL;
    }

    // Check if LVGL QR code is enabled
#if LV_USE_QRCODE
    // Create QR code widget
    lv_obj_t *qr_widget = lv_qrcode_create(parent);
    if (qr_widget == NULL) {
        ESP_LOGE(TAG, "Failed to create LVGL QR code widget");
        sx_qr_code_free_result(&qr_result);
        return NULL;
    }

    // Set size
    lv_obj_set_size(qr_widget, size, size);

    // Update QR code with generated data
    // Note: lv_qrcode_update() expects the QR code data in a specific format
    // We need to convert our bitmap data to the format LVGL expects
    // LVGL QR code uses a different format, so we'll use the text directly
    lv_qrcode_update(qr_widget, text, strlen(text));

    // Free QR code result (LVGL has its own copy)
    sx_qr_code_free_result(&qr_result);

    ESP_LOGD(TAG, "Created QR code widget: %dx%d for text: %s", size, size, text);
    return qr_widget;
#else
    ESP_LOGW(TAG, "LV_USE_QRCODE not enabled, cannot create QR code widget");
    sx_qr_code_free_result(&qr_result);
    return NULL;
#endif
}






