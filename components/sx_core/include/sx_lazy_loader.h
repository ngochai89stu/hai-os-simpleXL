#pragma once

#include <esp_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Lazy service types
 * 
 * Services that can be initialized on-demand instead of at boot time
 */
typedef enum {
    SX_LAZY_SERVICE_WIFI,              // WiFi service
    SX_LAZY_SERVICE_STT,                 // Speech-to-Text service
    SX_LAZY_SERVICE_WAKE_WORD,          // Wake word detection service
    SX_LAZY_SERVICE_AFE,                 // Audio Front-End service
    SX_LAZY_SERVICE_TTS,                // Text-to-Speech service
    SX_LAZY_SERVICE_BLE_NAV,            // BLE Navigation service
    SX_LAZY_SERVICE_CHATBOT,             // Chatbot service
    SX_LAZY_SERVICE_RADIO,              // Radio streaming service
    SX_LAZY_SERVICE_MUSIC_ONLINE,       // Online music service
    SX_LAZY_SERVICE_SD_CARD,            // SD card service
    SX_LAZY_SERVICE_IR,                 // IR control service
    SX_LAZY_SERVICE_BLUETOOTH,          // Bluetooth service
    SX_LAZY_SERVICE_WEATHER,            // Weather service
    SX_LAZY_SERVICE_NAVIGATION,         // Navigation service
    SX_LAZY_SERVICE_DIAGNOSTICS,        // Diagnostics service
    SX_LAZY_SERVICE_INTENT,              // Intent parsing service
    SX_LAZY_SERVICE_PROTOCOL_BRIDGE,    // Audio protocol bridge
    SX_LAZY_SERVICE_PLAYLIST,           // Playlist manager
    SX_LAZY_SERVICE_PROTOCOL_WS,        // WebSocket protocol
    SX_LAZY_SERVICE_PROTOCOL_MQTT,      // MQTT protocol
    SX_LAZY_SERVICE_MAX,                 // Maximum service count
} sx_lazy_service_t;

/**
 * @brief Initialize a lazy service
 * 
 * This function will initialize the service if it hasn't been initialized yet.
 * It's safe to call multiple times - subsequent calls will be no-ops.
 * 
 * @param service Service type to initialize
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sx_lazy_service_init(sx_lazy_service_t service);

/**
 * @brief Check if a lazy service is initialized
 * 
 * @param service Service type to check
 * @return true if initialized, false otherwise
 */
bool sx_lazy_service_is_initialized(sx_lazy_service_t service);

/**
 * @brief Deinitialize a lazy service (optional, for memory cleanup)
 * 
 * Note: This is optional and may not be implemented for all services.
 * Use with caution as it may break functionality if the service is still in use.
 * 
 * @param service Service type to deinitialize
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sx_lazy_service_deinit(sx_lazy_service_t service);

#ifdef __cplusplus
}
#endif


















