#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// BLE Navigation Service
// Receives navigation data from Android app via BLE (Google Maps notification)

// BLE UUIDs (matching Android app from repo mẫu)
#define SX_NAV_BLE_SERVICE_UUID     "ec91d7ab-e87c-48d5-adfa-cc4b2951298a"
#define SX_NAV_BLE_CHA_SETTINGS     "9d37a346-63d3-4df6-8eee-f0242949f59f"  // Android -> ESP32 settings
#define SX_NAV_BLE_CHA_NAV          "0b11deef-1563-447f-aece-d3dfeb1c1f20"
#define SX_NAV_BLE_CHA_NAV_ICON      "d4d8fcca-16b2-4b8e-8ed5-90137c44a8ad"
#define SX_NAV_BLE_CHA_GPS_SPEED     "98b6073a-5cf3-4e73-b6d3-f8e05fa018a9"
#define SX_NAV_BLE_CHA_COMMAND       "a1b2c3d4-e5f6-7890-abcd-ef1234567890"  // ESP32 -> Android command

// Navigation data from BLE (key-value format)
typedef struct {
    char next_road[128];           // nextRd
    char next_road_desc[128];       // nextRdDesc
    char dist_to_next[64];          // distToNext
    char total_dist[64];             // totalDist
    char eta[64];                   // eta
    char ete[64];                   // ete
    char icon_hash[16];             // iconHash
    int speed_kmh;                  // speed (from GPS)
} sx_nav_ble_data_t;

// BLE connection state callback
typedef void (*sx_nav_ble_connection_cb_t)(bool connected, void *user_data);

// Navigation data callback
typedef void (*sx_nav_ble_data_cb_t)(const sx_nav_ble_data_t *data, void *user_data);

// Icon bitmap data (64x64 RGB565 = 8192 bytes)
#define SX_NAV_ICON_SIZE 8192
#define SX_NAV_ICON_WIDTH 64
#define SX_NAV_ICON_HEIGHT 64

// Icon bitmap structure
typedef struct {
    char hash[16];
    uint8_t bitmap[SX_NAV_ICON_SIZE];  // RGB565 format
    bool valid;
} sx_nav_icon_t;

// Get current icon (for UI display)
esp_err_t sx_navigation_ble_get_icon(sx_nav_icon_t *icon);

// Initialize BLE Navigation service
esp_err_t sx_navigation_ble_init(void);

// Deinitialize BLE Navigation service
void sx_navigation_ble_deinit(void);

// Start BLE advertising
esp_err_t sx_navigation_ble_start(void);

// Stop BLE advertising
esp_err_t sx_navigation_ble_stop(void);

// Check if device is connected
bool sx_navigation_ble_is_connected(void);

// Set connection callback
void sx_navigation_ble_set_connection_callback(sx_nav_ble_connection_cb_t callback, void *user_data);

// Set navigation data callback
void sx_navigation_ble_set_data_callback(sx_nav_ble_data_cb_t callback, void *user_data);

// Send command to Android app (e.g., open Google Maps)
// command_json: JSON string with command (e.g., {"action":"open_google_maps","origin":"nhà","destination":"bến xe miền tây"})
// Returns: ESP_OK on success
esp_err_t sx_navigation_ble_send_command(const char *command_json);

#ifdef __cplusplus
}
#endif

