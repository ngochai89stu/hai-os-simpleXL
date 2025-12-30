#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: Settings service - Persistent configuration storage using NVS

// Initialize settings service
esp_err_t sx_settings_service_init(void);

// String settings
esp_err_t sx_settings_set_string(const char *key, const char *value);
esp_err_t sx_settings_get_string(const char *key, char *value, size_t max_len);
esp_err_t sx_settings_get_string_default(const char *key, char *value, size_t max_len, const char *default_value);

// Integer settings
esp_err_t sx_settings_set_int(const char *key, int32_t value);
esp_err_t sx_settings_get_int(const char *key, int32_t *value);
esp_err_t sx_settings_get_int_default(const char *key, int32_t *value, int32_t default_value);

// Boolean settings
esp_err_t sx_settings_set_bool(const char *key, bool value);
esp_err_t sx_settings_get_bool(const char *key, bool *value);
esp_err_t sx_settings_get_bool_default(const char *key, bool *value, bool default_value);

// Blob settings
esp_err_t sx_settings_set_blob(const char *key, const void *value, size_t len);
esp_err_t sx_settings_get_blob(const char *key, void *value, size_t *len);
esp_err_t sx_settings_get_blob_size(const char *key, size_t *len);

// Delete setting
esp_err_t sx_settings_delete(const char *key);

// Commit changes (flush to NVS)
esp_err_t sx_settings_commit(void);

// Erase all settings
esp_err_t sx_settings_erase_all(void);

#ifdef __cplusplus
}
#endif







