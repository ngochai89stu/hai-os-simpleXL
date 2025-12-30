// MCP Tools - Navigation
// Allows chatbot to start navigation via voice commands

#include "sx_mcp_tools.h"
#include "sx_mcp_server.h"
#include "sx_navigation_service.h"
#include "sx_navigation_ble.h"
#include "sx_geocoding.h"
#include "sx_tts_service.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "cJSON.h"

static const char *TAG = "sx_mcp_tools_navigation";

// Use shared helpers
extern cJSON* mcp_tool_create_error(const char *id, int code, const char *message);
extern cJSON* mcp_tool_create_success(const char *id, cJSON *result);

// self.navigation.start
// Start navigation with origin and destination
// Parameters: origin (string, optional), destination (string, required)
// If origin not provided, uses current location
cJSON* mcp_tool_navigation_start(cJSON *params, const char *id) {
    cJSON *destination = cJSON_GetObjectItem(params, "destination");
    if (!destination || !cJSON_IsString(destination)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: destination required (string)");
    }
    
    cJSON *origin = cJSON_GetObjectItem(params, "origin");
    
    const char *dest_str = destination->valuestring;
    const char *origin_str = (origin && cJSON_IsString(origin)) ? origin->valuestring : NULL;
    
    ESP_LOGI(TAG, "Navigation start requested: %s -> %s", 
             origin_str ? origin_str : "current location", dest_str);
    
    // For now, create a simple route
    // In the future, this could:
    // 1. Call Google Maps API via BLE (if Android app connected)
    // 2. Use geocoding to convert addresses to coordinates
    // 3. Calculate route
    
    sx_nav_coordinate_t start = {0};
    sx_nav_coordinate_t end = {0};
    
    // Geocode addresses to coordinates
    if (origin_str && strcmp(origin_str, "current") != 0 && strcmp(origin_str, "here") != 0) {
        if (sx_geocoding_geocode(origin_str, &start) != ESP_OK) {
            // If geocoding fails, use default location
            start.latitude = 10.762622;
            start.longitude = 106.660172;
            ESP_LOGW(TAG, "Could not geocode origin '%s', using default", origin_str);
        }
    }
    
    if (sx_geocoding_geocode(dest_str, &end) != ESP_OK) {
        // If geocoding fails, use default destination
        end.latitude = 10.7769;
        end.longitude = 106.7009;
        ESP_LOGW(TAG, "Could not geocode destination '%s', using default", dest_str);
    }
    
    sx_nav_route_t route = {0};
    esp_err_t ret = sx_navigation_calculate_route(&start, &end, &route);
    
    cJSON *result = cJSON_CreateObject();
    
    if (ret == ESP_OK) {
        // Start navigation
        ret = sx_navigation_start(&route);
        if (ret == ESP_OK) {
            cJSON_AddBoolToObject(result, "success", true);
            cJSON_AddStringToObject(result, "message", "Navigation started");
            cJSON_AddStringToObject(result, "origin", origin_str ? origin_str : "current location");
            cJSON_AddStringToObject(result, "destination", dest_str);
            cJSON_AddNumberToObject(result, "distance_m", route.total_distance_m);
            cJSON_AddNumberToObject(result, "estimated_time_s", route.estimated_time_s);
            
            // Speak confirmation
            char tts_msg[256];
            snprintf(tts_msg, sizeof(tts_msg), "Bắt đầu điều hướng đến %s", dest_str);
            sx_tts_speak_simple(tts_msg);
        } else {
            cJSON_AddBoolToObject(result, "success", false);
            cJSON_AddStringToObject(result, "error", "Failed to start navigation");
        }
        
        sx_navigation_free_route(&route);
    } else {
        cJSON_AddBoolToObject(result, "success", false);
        cJSON_AddStringToObject(result, "error", "Failed to calculate route");
    }
    
    return mcp_tool_create_success(id, result);
}

// self.navigation.stop
// Stop current navigation
cJSON* mcp_tool_navigation_stop(cJSON *params, const char *id) {
    (void)params;
    
    esp_err_t ret = sx_navigation_stop();
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    
    if (ret == ESP_OK) {
        cJSON_AddStringToObject(result, "message", "Navigation stopped");
        sx_tts_speak_simple("Đã dừng điều hướng");
    } else {
        cJSON_AddStringToObject(result, "error", "Failed to stop navigation");
    }
    
    return mcp_tool_create_success(id, result);
}

// self.navigation.get_status
// Get current navigation status
cJSON* mcp_tool_navigation_get_status(cJSON *params, const char *id) {
    (void)params;
    
    sx_navigation_state_t state = sx_navigation_get_state();
    sx_nav_instruction_t instruction = {0};
    bool has_instruction = (sx_navigation_get_next_instruction(&instruction) == ESP_OK);
    
    cJSON *result = cJSON_CreateObject();
    
    const char *state_str = "idle";
    switch (state) {
        case SX_NAV_STATE_IDLE: state_str = "idle"; break;
        case SX_NAV_STATE_ROUTING: state_str = "routing"; break;
        case SX_NAV_STATE_NAVIGATING: state_str = "navigating"; break;
        case SX_NAV_STATE_ARRIVED: state_str = "arrived"; break;
        case SX_NAV_STATE_ERROR: state_str = "error"; break;
    }
    
    cJSON_AddStringToObject(result, "state", state_str);
    cJSON_AddBoolToObject(result, "active", state == SX_NAV_STATE_NAVIGATING);
    cJSON_AddBoolToObject(result, "ble_connected", sx_navigation_ble_is_connected());
    
    if (has_instruction) {
        cJSON *inst = cJSON_CreateObject();
        cJSON_AddStringToObject(inst, "text", instruction.text);
        cJSON_AddNumberToObject(inst, "distance_m", instruction.distance_m);
        cJSON_AddNumberToObject(inst, "time_s", instruction.time_s);
        cJSON_AddItemToObject(result, "current_instruction", inst);
    }
    
    return mcp_tool_create_success(id, result);
}

// self.navigation.open_google_maps
// Request to open Google Maps on connected phone via BLE
// This will trigger Android app to open Google Maps with navigation
cJSON* mcp_tool_navigation_open_google_maps(cJSON *params, const char *id) {
    cJSON *destination = cJSON_GetObjectItem(params, "destination");
    if (!destination || !cJSON_IsString(destination)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: destination required (string)");
    }
    
    cJSON *origin = cJSON_GetObjectItem(params, "origin");
    const char *dest_str = destination->valuestring;
    const char *origin_str = (origin && cJSON_IsString(origin)) ? origin->valuestring : NULL;
    
    ESP_LOGI(TAG, "Open Google Maps requested: %s -> %s", 
             origin_str ? origin_str : "current location", dest_str);
    
    cJSON *result = cJSON_CreateObject();
    
    // Check BLE connection
    if (!sx_navigation_ble_is_connected()) {
        cJSON_AddBoolToObject(result, "success", false);
        cJSON_AddStringToObject(result, "error", "BLE not connected to phone");
        cJSON_AddStringToObject(result, "message", "Please connect to phone via BLE first");
        return mcp_tool_create_success(id, result);
    }
    
    // Send command to Android app via BLE to open Google Maps
    cJSON *command = cJSON_CreateObject();
    cJSON_AddStringToObject(command, "action", "open_google_maps");
    if (origin_str) {
        cJSON_AddStringToObject(command, "origin", origin_str);
    }
    cJSON_AddStringToObject(command, "destination", dest_str);
    
    char *command_json = cJSON_Print(command);
    if (command_json) {
        esp_err_t ret = sx_navigation_ble_send_command(command_json);
        free(command_json);
        
        if (ret == ESP_OK) {
            cJSON_AddBoolToObject(result, "success", true);
            cJSON_AddStringToObject(result, "message", "Command sent to phone to open Google Maps");
        } else {
            cJSON_AddBoolToObject(result, "success", false);
            cJSON_AddStringToObject(result, "error", "Failed to send command via BLE");
            cJSON_AddStringToObject(result, "message", "Please ensure BLE is connected");
        }
    } else {
        cJSON_AddBoolToObject(result, "success", false);
        cJSON_AddStringToObject(result, "error", "Failed to create command JSON");
    }
    
    cJSON_Delete(command);
    cJSON_AddStringToObject(result, "origin", origin_str ? origin_str : "current location");
    cJSON_AddStringToObject(result, "destination", dest_str);
    
    // Speak confirmation
    char tts_msg[256];
    snprintf(tts_msg, sizeof(tts_msg), "Đã yêu cầu mở Google Maps đi đến %s trên điện thoại", dest_str);
    sx_tts_speak_simple(tts_msg);
    
    return mcp_tool_create_success(id, result);
}

// Register navigation tools
esp_err_t sx_mcp_tools_navigation_register(void) {
    esp_err_t ret = ESP_OK;
    
    ret = sx_mcp_server_register_tool("self.navigation.start",
        "Start navigation from origin to destination. "
        "Parameters: destination (required, string), origin (optional, string). "
        "If origin not provided, uses current location. "
        "Example: 'chỉ đường đi từ nhà đến bến xe miền tây'",
        mcp_tool_navigation_start, false);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register navigation.start tool");
        return ret;
    }
    
    ret = sx_mcp_server_register_tool("self.navigation.stop",
        "Stop current navigation",
        mcp_tool_navigation_stop, false);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register navigation.stop tool");
        return ret;
    }
    
    ret = sx_mcp_server_register_tool("self.navigation.get_status",
        "Get current navigation status and instruction",
        mcp_tool_navigation_get_status, false);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register navigation.get_status tool");
        return ret;
    }
    
    ret = sx_mcp_server_register_tool("self.navigation.open_google_maps",
        "Request to open Google Maps on connected phone via BLE. "
        "Android app will read Google Maps notification and send navigation data. "
        "Parameters: destination (required, string), origin (optional, string). "
        "Example: 'mở Google Maps đi đến sân bay'",
        mcp_tool_navigation_open_google_maps, false);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register navigation.open_google_maps tool");
        return ret;
    }
    
    ESP_LOGI(TAG, "Navigation MCP tools registered");
    return ESP_OK;
}

