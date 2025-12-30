// MCP Tools - Radio Control
// Based on xiaozhi-esp32_vietnam_ref implementation

#include "sx_mcp_tools.h"
#include "sx_radio_service.h"

#include <string.h>
#include "cJSON.h"

// Use shared helpers from sx_mcp_tools.c
extern cJSON* mcp_tool_create_response(const char *id, cJSON *result, cJSON *error);
extern cJSON* mcp_tool_create_error(const char *id, int code, const char *message);
extern cJSON* mcp_tool_create_success(const char *id, cJSON *result);

// self.radio.play_station
cJSON* mcp_tool_radio_play_station(cJSON *params, const char *id) {
    cJSON *station_name = cJSON_GetObjectItem(params, "station_name");
    if (!station_name || !cJSON_IsString(station_name)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: station_name required");
    }
    
    const char *station = station_name->valuestring;
    esp_err_t ret = sx_radio_play_station(station);
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    if (ret == ESP_OK) {
        cJSON_AddStringToObject(result, "message", "Radio station started playing");
        cJSON_AddStringToObject(result, "station_name", station);
    } else {
        cJSON_AddStringToObject(result, "message", "Failed to find or play radio station");
        cJSON_AddStringToObject(result, "error", esp_err_to_name(ret));
    }
    
    return mcp_tool_create_success(id, result);
}

// self.radio.play_url
cJSON* mcp_tool_radio_play_url(cJSON *params, const char *id) {
    cJSON *url = cJSON_GetObjectItem(params, "url");
    if (!url || !cJSON_IsString(url)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: url required");
    }
    
    cJSON *name = cJSON_GetObjectItem(params, "name");
    const char *station_name = (name && cJSON_IsString(name)) ? name->valuestring : NULL;
    
    esp_err_t ret = sx_radio_play_url(url->valuestring);
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    if (ret == ESP_OK) {
        cJSON_AddStringToObject(result, "message", "Radio stream started playing");
        cJSON_AddStringToObject(result, "url", url->valuestring);
        if (station_name) {
            cJSON_AddStringToObject(result, "name", station_name);
        }
    } else {
        cJSON_AddStringToObject(result, "message", "Failed to play radio stream");
        cJSON_AddStringToObject(result, "error", esp_err_to_name(ret));
    }
    
    return mcp_tool_create_success(id, result);
}

// self.radio.stop
cJSON* mcp_tool_radio_stop(cJSON *params, const char *id) {
    (void)params;
    
    esp_err_t ret = sx_radio_stop_playback();
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    if (ret == ESP_OK) {
        cJSON_AddStringToObject(result, "message", "Radio stopped");
    } else {
        cJSON_AddStringToObject(result, "message", "Failed to stop radio");
        cJSON_AddStringToObject(result, "error", esp_err_to_name(ret));
    }
    
    return mcp_tool_create_success(id, result);
}

// self.radio.get_stations
cJSON* mcp_tool_radio_get_stations(cJSON *params, const char *id) {
    (void)params;
    
    // Get station list from station table
    // Note: Station table is static, so we list all available stations
    cJSON *result = cJSON_CreateObject();
    cJSON *stations = cJSON_CreateArray();
    
    // Vietnamese VOV stations (from sx_radio_station_table.h)
    const char *vn_stations[] = {
        "VOV1", "VOV2", "VOV3", "VOV5",
        "VOV_GT_HN", "VOV_GT_HCM", "VOV_MEKONG",
        "VOV4_MT", "VOV4_TB", "VOV4_DB", "VOV4_TN", "VOV4_DBSCL", "VOV4_HCM",
        "VOV5_EN"
    };
    
    for (size_t i = 0; i < sizeof(vn_stations) / sizeof(vn_stations[0]); i++) {
        cJSON_AddItemToArray(stations, cJSON_CreateString(vn_stations[i]));
    }
    
    cJSON_AddBoolToObject(result, "success", true);
    cJSON_AddItemToObject(result, "stations", stations);
    
    return mcp_tool_create_success(id, result);
}

// self.radio.set_display_mode
cJSON* mcp_tool_radio_set_display_mode(cJSON *params, const char *id) {
    cJSON *mode = cJSON_GetObjectItem(params, "mode");
    if (!mode || !cJSON_IsString(mode)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: mode required (spectrum/info)");
    }
    
    const char *mode_str = mode->valuestring;
    sx_radio_display_mode_t display_mode;
    
    if (strcmp(mode_str, "spectrum") == 0) {
        // Note: Spectrum mode may need to be added to sx_radio_service
        display_mode = SX_RADIO_DISPLAY_METADATA;  // Use metadata as placeholder
    } else if (strcmp(mode_str, "info") == 0) {
        display_mode = SX_RADIO_DISPLAY_INFO;
    } else {
        return mcp_tool_create_error(id, -32602, "Mode must be 'spectrum' or 'info'");
    }
    
    esp_err_t ret = sx_radio_set_display_mode(display_mode);
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    cJSON_AddStringToObject(result, "mode", mode_str);
    if (ret != ESP_OK) {
        cJSON_AddStringToObject(result, "error", esp_err_to_name(ret));
    }
    
    return mcp_tool_create_success(id, result);
}

