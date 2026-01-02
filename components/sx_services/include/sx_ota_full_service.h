#pragma once

#include <esp_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize OTA full service (singleton)
esp_err_t sx_ota_full_service_init(void);

// Trigger version check (async)
esp_err_t sx_ota_full_check_version(void);

// Start upgrade from custom URL
esp_err_t sx_ota_full_start_upgrade(const char *url);

// Activation helpers
bool sx_ota_full_has_activation(void);
const char *sx_ota_full_get_activation_code(void);
esp_err_t sx_ota_full_activate(void);

#ifdef __cplusplus
}
#endif







