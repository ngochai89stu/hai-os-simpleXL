#pragma once

#include <esp_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: OTA Service - Over-the-air firmware updates

typedef struct {
    const char *url;           // OTA update URL
    const char *cert_pem;      // Certificate for HTTPS (NULL for HTTP)
    uint32_t timeout_ms;       // Connection timeout
} sx_ota_config_t;

// Initialize OTA service
esp_err_t sx_ota_service_init(void);

// Start OTA update
// Returns ESP_OK if update started successfully
// Progress callback: called with progress percentage (0-100)
esp_err_t sx_ota_start_update(const sx_ota_config_t *config, 
                               void (*progress_callback)(int percent));

// Check if OTA update is in progress
bool sx_ota_is_updating(void);

// Get last OTA error message
const char *sx_ota_get_last_error(void);

#ifdef __cplusplus
}
#endif







