#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Weather information structure
typedef struct {
    char city[64];
    char description[128];
    char icon_code[16];
    float temp;
    int humidity;
    float feels_like;
    int pressure;
    float wind_speed;
    bool valid;
} sx_weather_info_t;

// Weather service configuration
typedef struct {
    const char *api_key;        // OpenWeatherMap API key (NULL to use default)
    const char *city;            // City name (NULL for auto-detect from IP)
    uint32_t update_interval_ms; // Update interval in milliseconds (default: 30 minutes)
} sx_weather_config_t;

// Initialize weather service
esp_err_t sx_weather_service_init(const sx_weather_config_t *config);

// Fetch weather data from API
esp_err_t sx_weather_fetch(void);

// Get current weather info
const sx_weather_info_t* sx_weather_get_info(void);

// Check if weather data needs update
bool sx_weather_needs_update(void);

// Set API key
esp_err_t sx_weather_set_api_key(const char *api_key);

// Set city manually
esp_err_t sx_weather_set_city(const char *city);

// Get city from IP address (auto-detect)
esp_err_t sx_weather_detect_city_from_ip(char *city_out, size_t city_out_size);

#ifdef __cplusplus
}
#endif





