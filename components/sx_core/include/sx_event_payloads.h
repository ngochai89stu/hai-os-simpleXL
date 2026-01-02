#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
// sx_qr_code_result_t is defined in sx_qr_code_service.h, no forward declaration needed

// Display service event payloads (P0.1)
typedef struct {
    uint8_t *buffer;        // RGB565 buffer (owned by UI, service must copy if needed)
    uint16_t width;
    uint16_t height;
    uint32_t buffer_size;   // width * height * 2 (RGB565)
} sx_display_capture_payload_t;

typedef struct {
    uint8_t *image_data;    // RGB565 image data (owned by service, UI must copy)
    uint16_t width;
    uint16_t height;
    uint32_t timeout_ms;    // Display timeout (0 = no timeout)
} sx_display_preview_payload_t;

// Theme service event payload (P0.1)
typedef struct {
    uint8_t theme_type;     // sx_theme_type_t
    uint32_t bg_primary;
    uint32_t bg_secondary;
    uint32_t bg_tertiary;
    uint32_t text_primary;
    uint32_t text_secondary;
    uint32_t accent;
    uint32_t error;
    uint32_t success;
} sx_theme_data_t;

// Image service event payload (P0.1)
typedef struct {
    uint8_t *data;          // Image data (owned by service, UI must copy if needed)
    size_t data_size;
    uint16_t width;
    uint16_t height;
    uint8_t image_type;     // sx_image_type_t
    bool has_alpha;
} sx_image_load_payload_t;

// QR code service uses existing sx_qr_code_result_t from sx_qr_code_service.h

#ifdef __cplusplus
}
#endif

