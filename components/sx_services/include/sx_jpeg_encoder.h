#pragma once

#include <esp_err.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// JPEG encoder service
// Note: This is a minimal JPEG encoder - for production, use ESP32 hardware encoder or libjpeg-turbo

// Encode RGB565 to JPEG
// rgb565_data: RGB565 pixel data (width * height * 2 bytes)
// width, height: Image dimensions
// quality: JPEG quality (1-100, higher = better quality but larger file)
// jpeg_data: Output JPEG data (caller must free)
// jpeg_size: Output JPEG size
// Returns ESP_OK on success
esp_err_t sx_jpeg_encode_rgb565(const uint8_t *rgb565_data, uint16_t width, uint16_t height,
                                uint8_t quality, uint8_t **jpeg_data, size_t *jpeg_size);

#ifdef __cplusplus
}
#endif






