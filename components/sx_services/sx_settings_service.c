#include "sx_settings_service.h"

#include <esp_log.h>
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "sx_settings";
static const char *NVS_NAMESPACE = "sx_settings";

static nvs_handle_t s_nvs_handle = 0;
static bool s_initialized = false;

esp_err_t sx_settings_service_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    // Open NVS namespace
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "Settings service initialized");
    return ESP_OK;
}

esp_err_t sx_settings_set_string(const char *key, const char *value) {
    if (!s_initialized || key == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = nvs_set_str(s_nvs_handle, key, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set string '%s': %s", key, esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t sx_settings_get_string(const char *key, char *value, size_t max_len) {
    if (!s_initialized || key == NULL || value == NULL || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    size_t required_size = max_len;
    esp_err_t ret = nvs_get_str(s_nvs_handle, key, value, &required_size);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        value[0] = '\0';
        return ESP_ERR_NOT_FOUND;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get string '%s': %s", key, esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t sx_settings_get_string_default(const char *key, char *value, size_t max_len, const char *default_value) {
    esp_err_t ret = sx_settings_get_string(key, value, max_len);
    if (ret == ESP_ERR_NOT_FOUND) {
        if (default_value != NULL) {
            strncpy(value, default_value, max_len - 1);
            value[max_len - 1] = '\0';
        } else {
            value[0] = '\0';
        }
        return ESP_OK;
    }
    return ret;
}

esp_err_t sx_settings_set_int(const char *key, int32_t value) {
    if (!s_initialized || key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = nvs_set_i32(s_nvs_handle, key, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set int '%s': %s", key, esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t sx_settings_get_int(const char *key, int32_t *value) {
    if (!s_initialized || key == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = nvs_get_i32(s_nvs_handle, key, value);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_ERR_NOT_FOUND;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get int '%s': %s", key, esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t sx_settings_get_int_default(const char *key, int32_t *value, int32_t default_value) {
    esp_err_t ret = sx_settings_get_int(key, value);
    if (ret == ESP_ERR_NOT_FOUND) {
        *value = default_value;
        return ESP_OK;
    }
    return ret;
}

esp_err_t sx_settings_set_bool(const char *key, bool value) {
    return sx_settings_set_int(key, value ? 1 : 0);
}

esp_err_t sx_settings_get_bool(const char *key, bool *value) {
    if (value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int32_t int_value;
    esp_err_t ret = sx_settings_get_int(key, &int_value);
    if (ret != ESP_OK) {
        return ret;
    }
    
    *value = (int_value != 0);
    return ESP_OK;
}

esp_err_t sx_settings_get_bool_default(const char *key, bool *value, bool default_value) {
    esp_err_t ret = sx_settings_get_bool(key, value);
    if (ret == ESP_ERR_NOT_FOUND) {
        *value = default_value;
        return ESP_OK;
    }
    return ret;
}

esp_err_t sx_settings_set_blob(const char *key, const void *value, size_t len) {
    if (!s_initialized || key == NULL || value == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = nvs_set_blob(s_nvs_handle, key, value, len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set blob '%s': %s", key, esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t sx_settings_get_blob(const char *key, void *value, size_t *len) {
    if (!s_initialized || key == NULL || value == NULL || len == NULL || *len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    size_t required_size = *len;
    esp_err_t ret = nvs_get_blob(s_nvs_handle, key, value, &required_size);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        *len = 0;
        return ESP_ERR_NOT_FOUND;
    }
    if (ret == ESP_ERR_NVS_INVALID_LENGTH) {
        *len = required_size;
        return ESP_ERR_INVALID_SIZE;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get blob '%s': %s", key, esp_err_to_name(ret));
        return ret;
    }
    
    *len = required_size;
    return ESP_OK;
}

esp_err_t sx_settings_get_blob_size(const char *key, size_t *len) {
    if (!s_initialized || key == NULL || len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    size_t required_size = 0;
    esp_err_t ret = nvs_get_blob(s_nvs_handle, key, NULL, &required_size);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        *len = 0;
        return ESP_ERR_NOT_FOUND;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get blob size '%s': %s", key, esp_err_to_name(ret));
        return ret;
    }
    
    *len = required_size;
    return ESP_OK;
}

esp_err_t sx_settings_delete(const char *key) {
    if (!s_initialized || key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = nvs_erase_key(s_nvs_handle, key);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK; // Already deleted
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete '%s': %s", key, esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t sx_settings_commit(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = nvs_commit(s_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit settings: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t sx_settings_erase_all(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = nvs_erase_all(s_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase all settings: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_commit(s_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit after erase: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "All settings erased");
    return ESP_OK;
}














