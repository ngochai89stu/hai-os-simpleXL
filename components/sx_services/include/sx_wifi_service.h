#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: WiFi service - Station mode connection management

typedef struct {
    char ssid[33];      // Max SSID length is 32
    int8_t rssi;        // Signal strength in dBm
    uint8_t channel;    // WiFi channel
    bool is_encrypted;  // Whether network requires password
    uint8_t auth_mode;  // Authentication mode (WPA2, WPA3, etc.)
} sx_wifi_network_info_t;

typedef struct {
    bool auto_reconnect;     // Enable auto-reconnect
    uint32_t reconnect_interval_ms; // Reconnect interval
} sx_wifi_config_t;

// Initialize WiFi service
esp_err_t sx_wifi_service_init(const sx_wifi_config_t *cfg);

// Start WiFi service
esp_err_t sx_wifi_service_start(void);

// Stop WiFi service
esp_err_t sx_wifi_service_stop(void);

// Scan for available WiFi networks
// Returns number of networks found, or negative on error
// networks array should be pre-allocated (max_networks entries)
int sx_wifi_scan(sx_wifi_network_info_t *networks, uint8_t max_networks);

// Connect to WiFi network
esp_err_t sx_wifi_connect(const char *ssid, const char *password);

// Disconnect from current WiFi network
esp_err_t sx_wifi_disconnect(void);

// Get connection status
bool sx_wifi_is_connected(void);

// Get current SSID
const char *sx_wifi_get_ssid(void);

// Get current RSSI
int8_t sx_wifi_get_rssi(void);

// Get current channel
uint8_t sx_wifi_get_channel(void);

// Get IP address (as string)
const char *sx_wifi_get_ip_address(void);

#ifdef __cplusplus
}
#endif














