#pragma once

#include <esp_err.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

// Register all MCP tools with the MCP server
esp_err_t sx_mcp_tools_register_all(void);

// Device control tools
cJSON* mcp_tool_get_device_status(cJSON *params, const char *id);
cJSON* mcp_tool_audio_set_volume(cJSON *params, const char *id);
cJSON* mcp_tool_screen_set_brightness(cJSON *params, const char *id);
cJSON* mcp_tool_screen_set_theme(cJSON *params, const char *id);
cJSON* mcp_tool_screen_set_rotation(cJSON *params, const char *id);
cJSON* mcp_tool_network_ip2qrcode(cJSON *params, const char *id);

// Radio tools
cJSON* mcp_tool_radio_play_station(cJSON *params, const char *id);
cJSON* mcp_tool_radio_play_url(cJSON *params, const char *id);
cJSON* mcp_tool_radio_stop(cJSON *params, const char *id);
cJSON* mcp_tool_radio_get_stations(cJSON *params, const char *id);
cJSON* mcp_tool_radio_set_display_mode(cJSON *params, const char *id);

// Navigation tools
cJSON* mcp_tool_navigation_start(cJSON *params, const char *id);
cJSON* mcp_tool_navigation_stop(cJSON *params, const char *id);
cJSON* mcp_tool_navigation_get_status(cJSON *params, const char *id);
cJSON* mcp_tool_navigation_open_google_maps(cJSON *params, const char *id);

#ifdef __cplusplus
}
#endif



