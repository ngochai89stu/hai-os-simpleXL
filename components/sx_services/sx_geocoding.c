// Geocoding Service
// Converts addresses/place names to GPS coordinates
// Currently supports hardcoded common locations in Ho Chi Minh City
// Future: Can integrate with Google Maps Geocoding API

#include "sx_geocoding.h"
#include <esp_log.h>
#include <string.h>
#include <ctype.h>
#include "esp_http_client.h"
#include "esp_timer.h"
#include "cJSON.h"

static const char *TAG = "sx_geocoding";

// Google Maps Geocoding API configuration
// Set these via settings service or Kconfig
static const char *s_google_maps_api_key = NULL;  // Set via sx_geocoding_set_api_key()
static bool s_use_api = false;  // Enable API geocoding

// Offline cache for geocoding
#define MAX_CACHED_GEOCODING 50
#define CACHE_EXPIRY_SECONDS 86400  // 24 hours
typedef struct {
    char address[128];
    sx_nav_coordinate_t coord;
    uint32_t timestamp;
    bool valid;
} geocoding_cache_t;

static geocoding_cache_t s_geocoding_cache[MAX_CACHED_GEOCODING] = {0};
static size_t s_geocoding_cache_count = 0;

// Common locations in Ho Chi Minh City (hardcoded)
// Future: Can be loaded from config file or fetched via API
typedef struct {
    const char *name;
    double latitude;
    double longitude;
} geocoding_location_t;

static const geocoding_location_t s_locations[] = {
    // Common place names
    {"nhà", 10.762622, 106.660172},  // Default home location
    {"nha", 10.762622, 106.660172},
    {"home", 10.762622, 106.660172},
    
    // Bus stations
    {"bến xe miền tây", 10.7769, 106.7009},
    {"ben xe mien tay", 10.7769, 106.7009},
    {"bến xe miền đông", 10.8500, 106.8333},
    {"ben xe mien dong", 10.8500, 106.8333},
    
    // Airports
    {"sân bay tân sơn nhất", 10.8188, 106.6520},
    {"san bay tan son nhat", 10.8188, 106.6520},
    {"sân bay", 10.8188, 106.6520},
    {"san bay", 10.8188, 106.6520},
    {"airport", 10.8188, 106.6520},
    
    // Popular destinations
    {"chợ bến thành", 10.7720, 106.6983},
    {"cho ben thanh", 10.7720, 106.6983},
    {"bến thành", 10.7720, 106.6983},
    {"ben thanh", 10.7720, 106.6983},
    
    {"landmark 81", 10.7947, 106.7219},
    {"landmark", 10.7947, 106.7219},
    
    {"bưu điện thành phố", 10.7794, 106.6992},
    {"buu dien thanh pho", 10.7794, 106.6992},
    
    {"nhà thờ đức bà", 10.7796, 106.6990},
    {"nha tho duc ba", 10.7796, 106.6990},
    
    {"bảo tàng lịch sử", 10.7892, 106.7054},
    {"bao tang lich su", 10.7892, 106.7054},
    
    {NULL, 0, 0}  // End marker
};

// Normalize string: lowercase, remove accents (simple version)
static void normalize_string(char *str) {
    if (!str) return;
    
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
    
    // Remove common Vietnamese accent marks (simple approach)
    // This is a simplified version - full implementation would use proper Unicode normalization
}

// Set Google Maps API key (optional, enables API geocoding)
void sx_geocoding_set_api_key(const char *api_key) {
    s_google_maps_api_key = api_key;
    s_use_api = (api_key != NULL && strlen(api_key) > 0);
    ESP_LOGI(TAG, "Google Maps API key %s", s_use_api ? "set" : "cleared");
}

// Geocode address using Google Maps API (if enabled)
static esp_err_t geocode_via_api(const char *address, sx_nav_coordinate_t *coord) {
    if (!s_use_api || !s_google_maps_api_key) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    // Build API URL
    char url[512];
    snprintf(url, sizeof(url), 
             "https://maps.googleapis.com/maps/api/geocode/json?address=%s&key=%s",
             address, s_google_maps_api_key);
    
    // Initialize HTTP client
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    // Perform request
    esp_err_t ret = esp_http_client_perform(client);
    if (ret != ESP_OK) {
        esp_http_client_cleanup(client);
        return ret;
    }
    
    // Check status code
    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        ESP_LOGE(TAG, "Geocoding API failed: HTTP %d", status_code);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    
    // Read response
    int content_length = esp_http_client_get_content_length(client);
    if (content_length <= 0 || content_length > 4096) {
        esp_http_client_cleanup(client);
        return ESP_ERR_INVALID_SIZE;
    }
    
    char *response = (char *)malloc(content_length + 1);
    if (response == NULL) {
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }
    
    int read_len = esp_http_client_read_response(client, response, content_length);
    response[read_len] = '\0';
    
    // Parse JSON response
    cJSON *json = cJSON_Parse(response);
    if (json == NULL) {
        free(response);
        esp_http_client_cleanup(client);
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    // Extract coordinates from response
    cJSON *results = cJSON_GetObjectItem(json, "results");
    if (results != NULL && cJSON_IsArray(results) && cJSON_GetArraySize(results) > 0) {
        cJSON *first_result = cJSON_GetArrayItem(results, 0);
        cJSON *geometry = cJSON_GetObjectItem(first_result, "geometry");
        if (geometry != NULL) {
            cJSON *location = cJSON_GetObjectItem(geometry, "location");
            if (location != NULL) {
                cJSON *lat = cJSON_GetObjectItem(location, "lat");
                cJSON *lng = cJSON_GetObjectItem(location, "lng");
                if (lat != NULL && lng != NULL && cJSON_IsNumber(lat) && cJSON_IsNumber(lng)) {
                    coord->latitude = lat->valuedouble;
                    coord->longitude = lng->valuedouble;
                    ESP_LOGI(TAG, "Geocoded via API '%s' -> (%.6f, %.6f)", 
                             address, coord->latitude, coord->longitude);
                    cJSON_Delete(json);
                    free(response);
                    esp_http_client_cleanup(client);
                    return ESP_OK;
                }
            }
        }
    }
    
    cJSON_Delete(json);
    free(response);
    esp_http_client_cleanup(client);
    return ESP_ERR_NOT_FOUND;
}

// Check geocoding cache
static bool check_geocoding_cache(const char *address, sx_nav_coordinate_t *coord) {
    uint32_t current_time = (uint32_t)(esp_timer_get_time() / 1000000);
    
    for (size_t i = 0; i < s_geocoding_cache_count; i++) {
        if (strcmp(s_geocoding_cache[i].address, address) == 0 && s_geocoding_cache[i].valid) {
            if (current_time - s_geocoding_cache[i].timestamp < CACHE_EXPIRY_SECONDS) {
                *coord = s_geocoding_cache[i].coord;
                ESP_LOGI(TAG, "Geocoding found in cache: %s", address);
                return true;
            }
        }
    }
    return false;
}

// Save geocoding to cache
static void save_geocoding_to_cache(const char *address, const sx_nav_coordinate_t *coord) {
    uint32_t current_time = (uint32_t)(esp_timer_get_time() / 1000000);
    
    // Find existing entry or create new one
    size_t index = s_geocoding_cache_count;
    for (size_t i = 0; i < s_geocoding_cache_count; i++) {
        if (strcmp(s_geocoding_cache[i].address, address) == 0) {
            index = i;
            break;
        }
    }
    
    if (index >= MAX_CACHED_GEOCODING) {
        // Cache full, replace oldest entry
        index = 0;
        uint32_t oldest_time = s_geocoding_cache[0].timestamp;
        for (size_t i = 1; i < s_geocoding_cache_count; i++) {
            if (s_geocoding_cache[i].timestamp < oldest_time) {
                oldest_time = s_geocoding_cache[i].timestamp;
                index = i;
            }
        }
    } else if (index == s_geocoding_cache_count) {
        s_geocoding_cache_count++;
    }
    
    strncpy(s_geocoding_cache[index].address, address, sizeof(s_geocoding_cache[index].address) - 1);
    s_geocoding_cache[index].coord = *coord;
    s_geocoding_cache[index].timestamp = current_time;
    s_geocoding_cache[index].valid = true;
    ESP_LOGI(TAG, "Geocoding saved to cache: %s", address);
}

esp_err_t sx_geocoding_geocode(const char *address, sx_nav_coordinate_t *coord) {
    if (!address || !coord) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check cache first
    if (check_geocoding_cache(address, coord)) {
        return ESP_OK;
    }
    
    // Try API geocoding first if enabled
    if (s_use_api) {
        esp_err_t ret = geocode_via_api(address, coord);
        if (ret == ESP_OK) {
            // Save to cache
            save_geocoding_to_cache(address, coord);
            return ESP_OK;
        }
        // Fall back to hardcoded locations if API fails
        ESP_LOGW(TAG, "API geocoding failed, trying hardcoded locations");
    }
    
    // Normalize input
    char normalized[128];
    strncpy(normalized, address, sizeof(normalized) - 1);
    normalized[sizeof(normalized) - 1] = '\0';
    normalize_string(normalized);
    
    // Search in location database
    for (int i = 0; s_locations[i].name != NULL; i++) {
        if (strstr(normalized, s_locations[i].name) != NULL) {
            coord->latitude = s_locations[i].latitude;
            coord->longitude = s_locations[i].longitude;
            ESP_LOGI(TAG, "Geocoded '%s' -> (%.6f, %.6f)", address, 
                     coord->latitude, coord->longitude);
            // Save to cache
            save_geocoding_to_cache(address, coord);
            return ESP_OK;
        }
    }
    
    ESP_LOGW(TAG, "Address not found: '%s'", address);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t sx_geocoding_reverse_geocode(const sx_nav_coordinate_t *coord, 
                                       char *address, size_t address_len) {
    if (!coord || !address || address_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Find closest location
    double min_dist = 1e10;
    const char *closest_name = NULL;
    
    for (int i = 0; s_locations[i].name != NULL; i++) {
        double lat_diff = coord->latitude - s_locations[i].latitude;
        double lon_diff = coord->longitude - s_locations[i].longitude;
        double dist = lat_diff * lat_diff + lon_diff * lon_diff;
        
        if (dist < min_dist) {
            min_dist = dist;
            closest_name = s_locations[i].name;
        }
    }
    
    if (closest_name && min_dist < 0.01) {  // Within ~1km
        strncpy(address, closest_name, address_len - 1);
        address[address_len - 1] = '\0';
        return ESP_OK;
    }
    
    // Format as coordinates if no match
    snprintf(address, address_len, "%.6f,%.6f", coord->latitude, coord->longitude);
    return ESP_OK;
}

bool sx_geocoding_is_recognized(const char *address) {
    if (!address) {
        return false;
    }
    
    char normalized[128];
    strncpy(normalized, address, sizeof(normalized) - 1);
    normalized[sizeof(normalized) - 1] = '\0';
    normalize_string(normalized);
    
    for (int i = 0; s_locations[i].name != NULL; i++) {
        if (strstr(normalized, s_locations[i].name) != NULL) {
            return true;
        }
    }
    
    return false;
}

