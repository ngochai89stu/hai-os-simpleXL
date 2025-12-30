#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include "sx_navigation_service.h"

#ifdef __cplusplus
extern "C" {
#endif

// Geocoding Service
// Converts addresses/place names to GPS coordinates
// Currently supports hardcoded common locations
// Future: Can integrate with Google Maps Geocoding API via BLE

// Geocode an address to coordinates
// address: Address string (e.g., "nhà", "bến xe miền tây", "sân bay Tân Sơn Nhất")
// coord: Output coordinate
// Returns: ESP_OK on success, ESP_ERR_NOT_FOUND if address not recognized
esp_err_t sx_geocoding_geocode(const char *address, sx_nav_coordinate_t *coord);

// Reverse geocode: Convert coordinates to address
// coord: Input coordinate
// address: Output address buffer
// address_len: Size of address buffer
// Returns: ESP_OK on success
esp_err_t sx_geocoding_reverse_geocode(const sx_nav_coordinate_t *coord, 
                                       char *address, size_t address_len);

// Check if an address is recognized
bool sx_geocoding_is_recognized(const char *address);

// Set Google Maps API key (optional, enables API geocoding)
// If set, geocoding will try API first, then fall back to hardcoded locations
void sx_geocoding_set_api_key(const char *api_key);

#ifdef __cplusplus
}
#endif

