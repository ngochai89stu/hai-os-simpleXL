#include "ui_image_helpers.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ui_image_helpers";

sx_lvgl_image_t* sx_ui_image_create_lvgl_rgb565(const uint8_t *rgb565_data, uint16_t width, uint16_t height) {
    if (rgb565_data == NULL || width == 0 || height == 0) {
        ESP_LOGE(TAG, "Invalid parameters: data=%p, width=%d, height=%d", rgb565_data, width, height);
        return NULL;
    }

    size_t data_size = (size_t)width * height * 2;  // RGB565 = 2 bytes per pixel

    // Allocate structure
    sx_lvgl_image_t *img = (sx_lvgl_image_t*)malloc(sizeof(sx_lvgl_image_t));
    if (img == NULL) {
        ESP_LOGE(TAG, "Failed to allocate sx_lvgl_image_t");
        return NULL;
    }
    memset(img, 0, sizeof(sx_lvgl_image_t));

    // Allocate RGB565 data buffer
    img->data = (uint8_t*)malloc(data_size);
    if (img->data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate RGB565 data buffer");
        free(img);
        return NULL;
    }
    memcpy(img->data, rgb565_data, data_size);

    // Allocate LVGL image descriptor
    img->img_dsc = (lv_img_dsc_t*)malloc(sizeof(lv_img_dsc_t));
    if (img->img_dsc == NULL) {
        ESP_LOGE(TAG, "Failed to allocate lv_img_dsc_t");
        free(img->data);
        free(img);
        return NULL;
    }

    // Initialize LVGL image descriptor
    img->img_dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
    img->img_dsc->header.cf = LV_COLOR_FORMAT_RGB565;
    img->img_dsc->header.flags = 0;
    img->img_dsc->header.w = width;
    img->img_dsc->header.h = height;
    img->img_dsc->header.stride = width * 2;  // RGB565 = 2 bytes per pixel
    img->img_dsc->header.reserved_2 = 0;
    img->img_dsc->data_size = data_size;
    img->img_dsc->data = img->data;

    img->width = width;
    img->height = height;

    ESP_LOGD(TAG, "Created LVGL image descriptor: %dx%d, size=%zu", width, height, data_size);
    return img;
}

void sx_ui_image_free_lvgl(sx_lvgl_image_t *img) {
    if (img == NULL) {
        return;
    }

    // Free LVGL image descriptor
    if (img->img_dsc != NULL) {
        free(img->img_dsc);
        img->img_dsc = NULL;
    }

    // Free RGB565 data
    if (img->data != NULL) {
        free(img->data);
        img->data = NULL;
    }

    // Free structure
    free(img);
}

