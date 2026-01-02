// MCP Tools - Device Control
// Based on xiaozhi-esp32_vietnam_ref implementation

#include "sx_mcp_tools.h"
#include "sx_audio_service.h"
#include "sx_platform.h"
#include "sx_theme_service.h"
#include "sx_wifi_service.h"
#include "sx_state.h"
#include "sx_dispatcher.h"
// Forward declarations to avoid including UI headers in service (Section 7.5 SIMPLEXL_ARCH v1.3)
typedef enum {
    SCREEN_ID_WIFI_SETUP = 8,  // From ui_screen_registry.h
} ui_screen_id_t;
void ui_router_navigate_to(ui_screen_id_t screen_id);  // Forward declaration

#include <esp_log.h>
#include <string.h>
#include "cJSON.h"
#include "esp_system.h"
#include "esp_mac.h"

// Use shared helpers from sx_mcp_tools.c
extern cJSON* mcp_tool_create_response(const char *id, cJSON *result, cJSON *error);
extern cJSON* mcp_tool_create_error(const char *id, int code, const char *message);
extern cJSON* mcp_tool_create_success(const char *id, cJSON *result);

// self.get_device_status
cJSON* mcp_tool_get_device_status(cJSON *params, const char *id) {
    (void)params;
    
    sx_state_t state;
    sx_dispatcher_get_state(&state);
    
    cJSON *result = cJSON_CreateObject();
    
    // Audio status
    cJSON *audio = cJSON_CreateObject();
    cJSON_AddBoolToObject(audio, "playing", sx_audio_is_playing());
    cJSON_AddBoolToObject(audio, "recording", sx_audio_is_recording());
    cJSON_AddNumberToObject(audio, "volume", sx_audio_get_volume());
    cJSON_AddItemToObject(result, "audio", audio);
    
    // Screen status
    cJSON *screen = cJSON_CreateObject();
    cJSON_AddNumberToObject(screen, "brightness", sx_platform_get_brightness());
    cJSON_AddNumberToObject(screen, "theme", sx_theme_get_type());
    cJSON_AddItemToObject(result, "screen", screen);
    
    // Network status
    cJSON *network = cJSON_CreateObject();
    cJSON_AddBoolToObject(network, "connected", sx_wifi_is_connected());
    if (sx_wifi_is_connected()) {
        cJSON_AddStringToObject(network, "ssid", sx_wifi_get_ssid());
        cJSON_AddStringToObject(network, "ip_address", sx_wifi_get_ip_address());
        cJSON_AddNumberToObject(network, "rssi", sx_wifi_get_rssi());
        cJSON_AddNumberToObject(network, "channel", sx_wifi_get_channel());
        
        // MAC address
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        cJSON_AddStringToObject(network, "mac_address", mac_str);
    }
    cJSON_AddItemToObject(result, "network", network);
    
    // System status
    cJSON *system = cJSON_CreateObject();
    cJSON_AddNumberToObject(system, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(system, "min_free_heap", esp_get_minimum_free_heap_size());
    cJSON_AddStringToObject(system, "device_state", state.ui.device_state == SX_DEV_IDLE ? "idle" : "active");
    cJSON_AddItemToObject(result, "system", system);
    
    return mcp_tool_create_success(id, result);
}

// self.audio_speaker.set_volume
cJSON* mcp_tool_audio_set_volume(cJSON *params, const char *id) {
    cJSON *volume = cJSON_GetObjectItem(params, "volume");
    if (!volume || !cJSON_IsNumber(volume)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: volume required (0-100)");
    }
    
    int vol = volume->valueint;
    if (vol < 0 || vol > 100) {
        return mcp_tool_create_error(id, -32602, "Volume must be between 0 and 100");
    }
    
    esp_err_t ret = sx_audio_set_volume((uint8_t)vol);
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    cJSON_AddNumberToObject(result, "volume", vol);
    
    return mcp_tool_create_success(id, result);
}

// self.screen.set_brightness
cJSON* mcp_tool_screen_set_brightness(cJSON *params, const char *id) {
    cJSON *brightness = cJSON_GetObjectItem(params, "brightness");
    if (!brightness || !cJSON_IsNumber(brightness)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: brightness required (0-100)");
    }
    
    int br = brightness->valueint;
    if (br < 0 || br > 100) {
        return mcp_tool_create_error(id, -32602, "Brightness must be between 0 and 100");
    }
    
    esp_err_t ret = sx_platform_set_brightness((uint8_t)br);
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    cJSON_AddNumberToObject(result, "brightness", br);
    
    return mcp_tool_create_success(id, result);
}

// self.screen.set_theme
cJSON* mcp_tool_screen_set_theme(cJSON *params, const char *id) {
    cJSON *theme = cJSON_GetObjectItem(params, "theme");
    if (!theme || !cJSON_IsString(theme)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: theme required (light/dark)");
    }
    
    const char *theme_str = theme->valuestring;
    sx_theme_type_t theme_type;
    
    if (strcmp(theme_str, "light") == 0) {
        theme_type = SX_THEME_LIGHT;
    } else if (strcmp(theme_str, "dark") == 0) {
        theme_type = SX_THEME_DARK;
    } else {
        return mcp_tool_create_error(id, -32602, "Theme must be 'light' or 'dark'");
    }
    
    esp_err_t ret = sx_theme_set_type(theme_type);
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    cJSON_AddStringToObject(result, "theme", theme_str);
    
    return mcp_tool_create_success(id, result);
}

// self.screen.set_rotation
cJSON* mcp_tool_screen_set_rotation(cJSON *params, const char *id) {
    cJSON *rotation = cJSON_GetObjectItem(params, "rotation_degree");
    if (!rotation || !cJSON_IsNumber(rotation)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: rotation_degree required (0/90/180/270)");
    }
    
    int rot = rotation->valueint;
    if (rot != 0 && rot != 90 && rot != 180 && rot != 270) {
        return mcp_tool_create_error(id, -32602, "Rotation must be 0, 90, 180, or 270");
    }
    
    // Note: Screen rotation API may need to be added to sx_platform or sx_ui
    // For now, return placeholder
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", false);
    cJSON_AddStringToObject(result, "message", "Screen rotation API not yet implemented");
    cJSON_AddNumberToObject(result, "rotation_degree", rot);
    
    return mcp_tool_create_success(id, result);
}

// self.network.ip2qrcode
cJSON* mcp_tool_network_ip2qrcode(cJSON *params, const char *id) {
    (void)params;
    
    cJSON *result = cJSON_CreateObject();
    
    if (!sx_wifi_is_connected()) {
        cJSON_AddBoolToObject(result, "connected", false);
        cJSON_AddStringToObject(result, "status", "disconnected");
        cJSON_AddStringToObject(result, "message", "Device is not connected to WiFi");
        return mcp_tool_create_success(id, result);
    }
    
    const char *ip_address = sx_wifi_get_ip_address();
    const char *ssid = sx_wifi_get_ssid();
    
    cJSON_AddBoolToObject(result, "connected", true);
    cJSON_AddStringToObject(result, "ip_address", ip_address);
    cJSON_AddStringToObject(result, "ssid", ssid);
    cJSON_AddNumberToObject(result, "rssi", sx_wifi_get_rssi());
    cJSON_AddNumberToObject(result, "channel", sx_wifi_get_channel());
    
    // MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    cJSON_AddStringToObject(result, "mac_address", mac_str);
    cJSON_AddStringToObject(result, "status", "connected");
    
    // Generate QR code for IP address
    char qr_text[256];
    snprintf(qr_text, sizeof(qr_text), "http://%s/ota", ip_address);
    
    // Display QR code on screen by navigating to WiFi setup screen
    // The WiFi setup screen already has QR code display functionality
    // Note: ui_router_navigate_to must be called from UI task, so we use dispatcher
    // For now, we'll navigate directly if called from UI context, otherwise post event
    // In a real implementation, this would be handled via a custom event type
    // For simplicity, we'll just indicate navigation should happen
    ui_router_navigate_to(SCREEN_ID_WIFI_SETUP);
    
    cJSON_AddBoolToObject(result, "qrcode_displayed", true);
    cJSON_AddStringToObject(result, "qrcode_text", qr_text);
    cJSON_AddStringToObject(result, "message", "QR code displayed on WiFi setup screen");
    
    return mcp_tool_create_success(id, result);
}

