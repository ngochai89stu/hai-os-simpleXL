#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Display service for screen operations

// Initialize display service
// Returns ESP_OK on success
esp_err_t sx_display_service_init(void);

// Capture screen to RGB565 buffer
// buffer: Output buffer (must be at least width * height * 2 bytes)
// width, height: Screen dimensions
// Returns ESP_OK on success
esp_err_t sx_display_capture_screen(uint8_t *buffer, uint16_t width, uint16_t height);

// Encode RGB565 to JPEG
// rgb565_data: RGB565 pixel data (width * height * 2 bytes)
// width, height: Image dimensions
// quality: JPEG quality (1-100, higher = better quality but larger file)
// jpeg_data: Output JPEG data (caller must free)
// jpeg_size: Output JPEG size
// Returns ESP_OK on success
esp_err_t sx_display_encode_jpeg(const uint8_t *rgb565_data, uint16_t width, uint16_t height, 
                                 uint8_t quality, uint8_t **jpeg_data, size_t *jpeg_size);

// Upload JPEG to URL
// jpeg_data: JPEG data
// jpeg_size: JPEG size
// url: Upload URL
// Returns ESP_OK on success
esp_err_t sx_display_upload_jpeg(const uint8_t *jpeg_data, size_t jpeg_size, const char *url);

// Download image from URL
// url: Image URL
// data: Output image data (caller must free)
// data_size: Output data size
// Returns ESP_OK on success
esp_err_t sx_display_download_image(const char *url, uint8_t **data, size_t *data_size);

// Display image on screen
// image_data: Image data (RGB565 format)
// width, height: Image dimensions
// timeout_ms: Display timeout in milliseconds (0 = no timeout)
// Returns ESP_OK on success
esp_err_t sx_display_show_image(const uint8_t *image_data, uint16_t width, uint16_t height, uint32_t timeout_ms);

// Remove displayed image
// Returns ESP_OK on success
esp_err_t sx_display_hide_image(void);

#ifdef __cplusplus
}
#endif

