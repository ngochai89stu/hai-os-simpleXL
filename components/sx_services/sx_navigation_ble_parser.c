// Key-Value Parser Utility
// Parses key-value string format from Android app: "key1=value1\nkey2=value2"

#include "sx_navigation_ble_parser.h"
#include <string.h>
#include <stdlib.h>

esp_err_t sx_nav_ble_parse_kv(const char *input, sx_nav_ble_kv_t *kv, size_t max_pairs) {
    if (!input || !kv || max_pairs == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    kv->count = 0;
    const char *line_start = input;
    size_t input_len = strlen(input);
    
    for (size_t i = 0; i < input_len && kv->count < max_pairs; i++) {
        if (input[i] == '\n' || input[i] == '\0') {
            // Process line
            size_t line_len = &input[i] - line_start;
            if (line_len > 0) {
                // Find '=' separator
                const char *eq_pos = memchr(line_start, '=', line_len);
                if (eq_pos && eq_pos > line_start) {
                    size_t key_len = eq_pos - line_start;
                    size_t value_len = &input[i] - (eq_pos + 1);
                    
                    if (key_len < sizeof(kv->pairs[kv->count].key) && 
                        value_len < sizeof(kv->pairs[kv->count].value)) {
                        memcpy(kv->pairs[kv->count].key, line_start, key_len);
                        kv->pairs[kv->count].key[key_len] = '\0';
                        
                        memcpy(kv->pairs[kv->count].value, eq_pos + 1, value_len);
                        kv->pairs[kv->count].value[value_len] = '\0';
                        
                        kv->count++;
                    }
                }
            }
            line_start = &input[i + 1];
        }
    }
    
    // Process last line if no trailing newline
    if (kv->count < max_pairs && line_start < &input[input_len]) {
        const char *eq_pos = strchr(line_start, '=');
        if (eq_pos && eq_pos > line_start) {
            size_t key_len = eq_pos - line_start;
            size_t value_len = &input[input_len] - (eq_pos + 1);
            
            if (key_len < sizeof(kv->pairs[kv->count].key) && 
                value_len < sizeof(kv->pairs[kv->count].value)) {
                memcpy(kv->pairs[kv->count].key, line_start, key_len);
                kv->pairs[kv->count].key[key_len] = '\0';
                
                memcpy(kv->pairs[kv->count].value, eq_pos + 1, value_len);
                kv->pairs[kv->count].value[value_len] = '\0';
                
                kv->count++;
            }
        }
    }
    
    return ESP_OK;
}

const char* sx_nav_ble_kv_get(const sx_nav_ble_kv_t *kv, const char *key) {
    if (!kv || !key) {
        return NULL;
    }
    
    for (size_t i = 0; i < kv->count; i++) {
        if (strcmp(kv->pairs[i].key, key) == 0) {
            return kv->pairs[i].value;
        }
    }
    
    return NULL;
}

const char* sx_nav_ble_kv_get_default(const sx_nav_ble_kv_t *kv, const char *key, const char *default_value) {
    const char *value = sx_nav_ble_kv_get(kv, key);
    return value ? value : default_value;
}

bool sx_nav_ble_kv_contains(const sx_nav_ble_kv_t *kv, const char *key) {
    return sx_nav_ble_kv_get(kv, key) != NULL;
}

