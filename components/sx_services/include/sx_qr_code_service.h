#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: QR Code Service - Generate QR codes for display

// QR code configuration
typedef struct {
    uint8_t version;        // QR code version (1-40, 0 = auto)
    uint8_t error_correction; // Error correction level (0=L, 1=M, 2=Q, 3=H)
    uint8_t module_size;    // Module size in pixels (default: 3)
    uint8_t border;         // Border size in modules (default: 4)
} sx_qr_code_config_t;

// QR code result
typedef struct {
    uint16_t width;         // QR code width in pixels
    uint16_t height;        // QR code height in pixels
    uint8_t *data;          // QR code bitmap data (1 bit per pixel, row-major)
    size_t data_size;       // Data size in bytes
} sx_qr_code_result_t;

// Initialize QR code service
esp_err_t sx_qr_code_service_init(void);

// Generate QR code from text
// text: Text to encode
// config: QR code configuration (NULL for defaults)
// result: Output QR code result (caller must free result->data)
esp_err_t sx_qr_code_generate(const char *text, const sx_qr_code_config_t *config, sx_qr_code_result_t *result);

// Generate QR code for IP address
// ip_address: IP address string (e.g., "192.168.1.100")
// port: Port number (0 to omit)
// result: Output QR code result
esp_err_t sx_qr_code_generate_ip(const char *ip_address, uint16_t port, sx_qr_code_result_t *result);

// Free QR code result
void sx_qr_code_free_result(sx_qr_code_result_t *result);

// Create LVGL QR code widget from text
// parent: Parent LVGL object
// text: Text to encode in QR code
// size: QR code size in pixels
// Returns: LVGL QR code widget object, or NULL on error
// Note: Caller must delete the widget when done
// Note: Requires LVGL to be included before this header
#ifdef __cplusplus
}
#endif

// Forward declaration for LVGL object
typedef struct _lv_obj_t lv_obj_t;

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t* sx_qr_code_create_lvgl_widget(lv_obj_t *parent, const char *text, uint16_t size);

#ifdef __cplusplus
}
#endif



