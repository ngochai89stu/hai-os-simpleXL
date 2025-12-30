#include "sx_weather_service.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "sx_wifi_service.h"
#include "sx_settings_service.h"

static const char *TAG = "sx_weather";

// Default configuration
#define WEATHER_UPDATE_INTERVAL_MS_DEFAULT (30 * 60 * 1000)  // 30 minutes
#define OPEN_WEATHERMAP_API_KEY_DEFAULT "ae8d3c2fda691593ce3e84472ef25784"  // Demo key
#define WEATHER_API_ENDPOINT "https://api.openweathermap.org/data/2.5/weather"
#define IP_LOCATION_API_ENDPOINT "https://ipwho.is"
#define WEATHER_HTTP_TIMEOUT_MS 10000

static bool s_initialized = false;
static sx_weather_info_t s_weather_info = {0};
static sx_weather_config_t s_config = {0};
static uint32_t s_last_update_time = 0;

// Helper: URL encode
static char* url_encode(const char *value) {
    if (!value) return NULL;
    
    size_t len = strlen(value);
    char *encoded = (char *)malloc(len * 3 + 1);
    if (!encoded) return NULL;
    
    size_t pos = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)value[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded[pos++] = c;
        } else {
            encoded[pos++] = '%';
            encoded[pos++] = "0123456789ABCDEF"[c >> 4];
            encoded[pos++] = "0123456789ABCDEF"[c & 0x0F];
        }
    }
    encoded[pos] = '\0';
    return encoded;
}

// Helper: Capitalize words
static void capitalize_words(char *str) {
    bool new_word = true;
    for (size_t i = 0; str[i]; i++) {
        if (isspace((unsigned char)str[i])) {
            new_word = true;
        } else if (new_word) {
            str[i] = (char)toupper((unsigned char)str[i]);
            new_word = false;
        }
    }
}

// HTTP event handler
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Store response data (we'll parse it after the request completes)
                ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t sx_weather_detect_city_from_ip(char *city_out, size_t city_out_size) {
    if (!city_out || city_out_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!sx_wifi_is_connected()) {
        ESP_LOGE(TAG, "WiFi not connected, cannot detect city from IP");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_http_client_config_t config = {
        .url = IP_LOCATION_API_ENDPOINT,
        .event_handler = http_event_handler,
        .timeout_ms = WEATHER_HTTP_TIMEOUT_MS,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }
    
    esp_err_t ret = esp_http_client_perform(client);
    if (ret == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "IP-Who-Is Status: %d", status_code);
        
        if (status_code == 200) {
            int content_length = esp_http_client_get_content_length(client);
            if (content_length > 0 && content_length < 2048) {
                char *buffer = (char *)malloc(content_length + 1);
                if (buffer) {
                    int data_read = esp_http_client_read_response(client, buffer, content_length);
                    if (data_read > 0) {
                        buffer[data_read] = '\0';
                        ESP_LOGI(TAG, "IP-Who-Is Response: %s", buffer);
                        
                        cJSON *root = cJSON_Parse(buffer);
                        if (root) {
                            cJSON *success = cJSON_GetObjectItemCaseSensitive(root, "success");
                            if (cJSON_IsBool(success) && cJSON_IsTrue(success)) {
                                cJSON *city_json = cJSON_GetObjectItemCaseSensitive(root, "region");
                                if (cJSON_IsString(city_json)) {
                                    strncpy(city_out, city_json->valuestring, city_out_size - 1);
                                    city_out[city_out_size - 1] = '\0';
                                    ESP_LOGI(TAG, "Auto-detected Region: %s", city_out);
                                    free(buffer);
                                    cJSON_Delete(root);
                                    esp_http_client_cleanup(client);
                                    return ESP_OK;
                                }
                            }
                            cJSON_Delete(root);
                        }
                    }
                    free(buffer);
                }
            }
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(ret));
    }
    
    esp_http_client_cleanup(client);
    return ESP_FAIL;
}

esp_err_t sx_weather_service_init(const sx_weather_config_t *config) {
    if (s_initialized) {
        ESP_LOGW(TAG, "Weather service already initialized");
        return ESP_OK;
    }
    
    memset(&s_weather_info, 0, sizeof(s_weather_info));
    s_last_update_time = 0;
    
    if (config) {
        s_config = *config;
    } else {
        memset(&s_config, 0, sizeof(s_config));
    }
    
    // Set defaults
    if (!s_config.api_key) {
        // Try to get from settings
        char api_key[128];
        if (sx_settings_get_string_default("weather_api_key", api_key, sizeof(api_key), OPEN_WEATHERMAP_API_KEY_DEFAULT) == ESP_OK) {
            // Store in config (we'll need to keep a copy)
            static char s_api_key_storage[128];
            strncpy(s_api_key_storage, api_key, sizeof(s_api_key_storage) - 1);
            s_config.api_key = s_api_key_storage;
        } else {
            s_config.api_key = OPEN_WEATHERMAP_API_KEY_DEFAULT;
        }
    }
    
    if (!s_config.city) {
        // Try to get from settings
        char city[64];
        if (sx_settings_get_string_default("weather_city", city, sizeof(city), "") == ESP_OK && strlen(city) > 0) {
            static char s_city_storage[64];
            strncpy(s_city_storage, city, sizeof(s_city_storage) - 1);
            s_config.city = s_city_storage;
        }
    }
    
    if (s_config.update_interval_ms == 0) {
        s_config.update_interval_ms = WEATHER_UPDATE_INTERVAL_MS_DEFAULT;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "Weather service initialized (city=%s, interval=%u ms)", 
             s_config.city ? s_config.city : "auto", s_config.update_interval_ms);
    return ESP_OK;
}

bool sx_weather_needs_update(void) {
    if (!s_initialized) return false;
    
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    return (current_time - s_last_update_time) >= s_config.update_interval_ms;
}

esp_err_t sx_weather_fetch(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Wait for WiFi connection
    int wait_retries = 0;
    while (!sx_wifi_is_connected() && wait_retries < 20) {
        ESP_LOGI(TAG, "Waiting for WiFi before fetching weather...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        wait_retries++;
    }
    
    if (!sx_wifi_is_connected()) {
        ESP_LOGE(TAG, "WiFi not connected, skipping weather update");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Get city
    char city[64] = {0};
    if (s_config.city && strlen(s_config.city) > 0) {
        strncpy(city, s_config.city, sizeof(city) - 1);
    } else {
        // Try to get from settings
        if (sx_settings_get_string("weather_city", city, sizeof(city)) != ESP_OK || strlen(city) == 0) {
            // Auto-detect from IP
            if (sx_weather_detect_city_from_ip(city, sizeof(city)) != ESP_OK) {
                strncpy(city, "Hanoi", sizeof(city) - 1);  // Default fallback
            }
        }
    }
    
    // Get API key
    const char *api_key = s_config.api_key;
    if (!api_key || strlen(api_key) == 0) {
        char api_key_buf[128];
        if (sx_settings_get_string_default("weather_api_key", api_key_buf, sizeof(api_key_buf), OPEN_WEATHERMAP_API_KEY_DEFAULT) == ESP_OK) {
            static char s_api_key_storage[128];
            strncpy(s_api_key_storage, api_key_buf, sizeof(s_api_key_storage) - 1);
            api_key = s_api_key_storage;
        } else {
            api_key = OPEN_WEATHERMAP_API_KEY_DEFAULT;
        }
    }
    
    // Build URL
    char *city_encoded = url_encode(city);
    if (!city_encoded) {
        ESP_LOGE(TAG, "Failed to encode city name");
        return ESP_ERR_NO_MEM;
    }
    
    char url[512];
    snprintf(url, sizeof(url), "%s?q=%s&appid=%s&units=metric&lang=en", 
             WEATHER_API_ENDPOINT, city_encoded, api_key);
    free(city_encoded);
    
    ESP_LOGI(TAG, "Fetching weather for city: %s", city);
    
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .timeout_ms = WEATHER_HTTP_TIMEOUT_MS,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }
    
    esp_err_t ret = esp_http_client_perform(client);
    if (ret == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Weather API Status: %d", status_code);
        
        if (status_code == 200) {
            int content_length = esp_http_client_get_content_length(client);
            if (content_length > 0 && content_length < 4096) {
                char *buffer = (char *)malloc(content_length + 1);
                if (buffer) {
                    int data_read = esp_http_client_read_response(client, buffer, content_length);
                    if (data_read > 0) {
                        buffer[data_read] = '\0';
                        
                        // Parse JSON response
                        cJSON *root = cJSON_Parse(buffer);
                        if (root) {
                            // Parse weather data
                            cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
                            if (cJSON_IsString(name)) {
                                strncpy(s_weather_info.city, name->valuestring, sizeof(s_weather_info.city) - 1);
                            }
                            
                            cJSON *weather_array = cJSON_GetObjectItemCaseSensitive(root, "weather");
                            if (cJSON_IsArray(weather_array) && cJSON_GetArraySize(weather_array) > 0) {
                                cJSON *weather = cJSON_GetArrayItem(weather_array, 0);
                                cJSON *description = cJSON_GetObjectItemCaseSensitive(weather, "description");
                                cJSON *icon = cJSON_GetObjectItemCaseSensitive(weather, "icon");
                                
                                if (cJSON_IsString(description)) {
                                    strncpy(s_weather_info.description, description->valuestring, sizeof(s_weather_info.description) - 1);
                                    capitalize_words(s_weather_info.description);
                                }
                                if (cJSON_IsString(icon)) {
                                    strncpy(s_weather_info.icon_code, icon->valuestring, sizeof(s_weather_info.icon_code) - 1);
                                }
                            }
                            
                            cJSON *main = cJSON_GetObjectItemCaseSensitive(root, "main");
                            if (cJSON_IsObject(main)) {
                                cJSON *temp = cJSON_GetObjectItemCaseSensitive(main, "temp");
                                cJSON *feels_like = cJSON_GetObjectItemCaseSensitive(main, "feels_like");
                                cJSON *humidity = cJSON_GetObjectItemCaseSensitive(main, "humidity");
                                cJSON *pressure = cJSON_GetObjectItemCaseSensitive(main, "pressure");
                                
                                if (cJSON_IsNumber(temp)) {
                                    s_weather_info.temp = (float)cJSON_GetNumberValue(temp);
                                }
                                if (cJSON_IsNumber(feels_like)) {
                                    s_weather_info.feels_like = (float)cJSON_GetNumberValue(feels_like);
                                }
                                if (cJSON_IsNumber(humidity)) {
                                    s_weather_info.humidity = (int)cJSON_GetNumberValue(humidity);
                                }
                                if (cJSON_IsNumber(pressure)) {
                                    s_weather_info.pressure = (int)cJSON_GetNumberValue(pressure);
                                }
                            }
                            
                            cJSON *wind = cJSON_GetObjectItemCaseSensitive(root, "wind");
                            if (cJSON_IsObject(wind)) {
                                cJSON *speed = cJSON_GetObjectItemCaseSensitive(wind, "speed");
                                if (cJSON_IsNumber(speed)) {
                                    s_weather_info.wind_speed = (float)cJSON_GetNumberValue(speed);
                                }
                            }
                            
                            s_weather_info.valid = true;
                            s_last_update_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                            
                            ESP_LOGI(TAG, "Weather updated: %s, %.1f°C, %s", 
                                    s_weather_info.city, s_weather_info.temp, s_weather_info.description);
                            
                            cJSON_Delete(root);
                        } else {
                            ESP_LOGE(TAG, "Failed to parse weather JSON");
                        }
                    }
                    free(buffer);
                }
            }
        } else {
            ESP_LOGE(TAG, "Weather API returned error status: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(ret));
    }
    
    esp_http_client_cleanup(client);
    return (s_weather_info.valid) ? ESP_OK : ESP_FAIL;
}

const sx_weather_info_t* sx_weather_get_info(void) {
    return s_initialized ? &s_weather_info : NULL;
}

esp_err_t sx_weather_set_api_key(const char *api_key) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!api_key) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Store in settings
    esp_err_t ret = sx_settings_set_string("weather_api_key", api_key);
    if (ret == ESP_OK) {
        sx_settings_commit();
        // Update config
        static char s_api_key_storage[128];
        strncpy(s_api_key_storage, api_key, sizeof(s_api_key_storage) - 1);
        s_config.api_key = s_api_key_storage;
    }
    return ret;
}

esp_err_t sx_weather_set_city(const char *city) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!city) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Store in settings
    esp_err_t ret = sx_settings_set_string("weather_city", city);
    if (ret == ESP_OK) {
        sx_settings_commit();
        // Update config
        static char s_city_storage[64];
        strncpy(s_city_storage, city, sizeof(s_city_storage) - 1);
        s_config.city = s_city_storage;
    }
    return ret;
}












