#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Key-Value pair structure
typedef struct {
    char key[64];
    char value[256];
} sx_nav_ble_kv_pair_t;

// Key-Value parse result
typedef struct {
    sx_nav_ble_kv_pair_t pairs[16];  // Max 16 pairs
    size_t count;
} sx_nav_ble_kv_t;

// Parse key-value string format: "key1=value1\nkey2=value2"
esp_err_t sx_nav_ble_parse_kv(const char *input, sx_nav_ble_kv_t *kv, size_t max_pairs);

// Get value by key
const char* sx_nav_ble_kv_get(const sx_nav_ble_kv_t *kv, const char *key);

// Get value by key with default
const char* sx_nav_ble_kv_get_default(const sx_nav_ble_kv_t *kv, const char *key, const char *default_value);

// Check if key exists
bool sx_nav_ble_kv_contains(const sx_nav_ble_kv_t *kv, const char *key);

#ifdef __cplusplus
}
#endif



