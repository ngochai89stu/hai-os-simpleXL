#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Bluetooth Service
// Bluetooth audio (A2DP) and pairing support

// Bluetooth connection state
typedef enum {
    SX_BT_STATE_IDLE = 0,
    SX_BT_STATE_DISCOVERING,
    SX_BT_STATE_CONNECTING,
    SX_BT_STATE_CONNECTED,
    SX_BT_STATE_DISCONNECTING,
    SX_BT_STATE_ERROR,
} sx_bluetooth_state_t;

// Bluetooth device info
typedef struct {
    char name[64];
    char address[18];  // MAC address as string "XX:XX:XX:XX:XX:XX"
    bool paired;
    bool connected;
} sx_bluetooth_device_t;

// Bluetooth configuration
typedef struct {
    const char *device_name;     // Bluetooth device name
    bool auto_connect;            // Auto-connect to last paired device
    bool discoverable;            // Make device discoverable
    uint32_t scan_timeout_ms;     // Scan timeout in milliseconds
} sx_bluetooth_config_t;

// Connection callback
typedef void (*sx_bluetooth_connection_cb_t)(sx_bluetooth_state_t state, const char *device_address);

// Audio data callback (for A2DP sink)
typedef void (*sx_bluetooth_audio_cb_t)(const int16_t *pcm, size_t samples, uint32_t sample_rate);

// Initialize Bluetooth service
// config: Bluetooth configuration (can be NULL for defaults)
// Returns: ESP_OK on success
esp_err_t sx_bluetooth_service_init(const sx_bluetooth_config_t *config);

// Deinitialize Bluetooth service
void sx_bluetooth_service_deinit(void);

// Start Bluetooth
// Returns: ESP_OK on success
esp_err_t sx_bluetooth_start(void);

// Stop Bluetooth
// Returns: ESP_OK on success
esp_err_t sx_bluetooth_stop(void);

// Start device discovery
// Returns: ESP_OK on success
esp_err_t sx_bluetooth_start_discovery(void);

// Stop device discovery
// Returns: ESP_OK on success
esp_err_t sx_bluetooth_stop_discovery(void);

// Get discovered devices
// devices: Output array of devices
// max_devices: Maximum number of devices
// device_count: Output actual number of devices
// Returns: ESP_OK on success
esp_err_t sx_bluetooth_get_discovered_devices(sx_bluetooth_device_t *devices,
                                              size_t max_devices, size_t *device_count);

// Connect to device
// address: Device MAC address
// Returns: ESP_OK on success
esp_err_t sx_bluetooth_connect(const char *address);

// Disconnect from current device
// Returns: ESP_OK on success
esp_err_t sx_bluetooth_disconnect(void);

// Get current connection state
sx_bluetooth_state_t sx_bluetooth_get_state(void);

// Get connected device info
// device: Output device info
// Returns: ESP_OK on success
esp_err_t sx_bluetooth_get_connected_device(sx_bluetooth_device_t *device);

// Set connection callback
void sx_bluetooth_set_connection_callback(sx_bluetooth_connection_cb_t callback);

// Set audio data callback (for A2DP sink mode)
void sx_bluetooth_set_audio_callback(sx_bluetooth_audio_cb_t callback);

// Feed audio data to Bluetooth (for A2DP source mode)
// pcm: PCM audio data
// samples: Number of samples
// sample_rate: Sample rate in Hz
// Returns: ESP_OK on success
esp_err_t sx_bluetooth_service_feed_audio(const int16_t *pcm, size_t samples, uint32_t sample_rate);

// Enable/disable Bluetooth
void sx_bluetooth_enable(bool enable);

// Check if Bluetooth is enabled
bool sx_bluetooth_is_enabled(void);

#ifdef __cplusplus
}
#endif

