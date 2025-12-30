#include "sx_mcp_server.h"
#include "sx_mcp_tools.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "cJSON.h"

static const char *TAG = "sx_mcp_server";

static bool s_initialized = false;
static sx_mcp_server_send_cb_t s_send_callback = NULL;

// Forward declarations for tool functions
extern cJSON* mcp_tool_get_device_status(cJSON *params, const char *id);
extern cJSON* mcp_tool_audio_set_volume(cJSON *params, const char *id);
extern cJSON* mcp_tool_screen_set_brightness(cJSON *params, const char *id);
extern cJSON* mcp_tool_screen_set_theme(cJSON *params, const char *id);
extern cJSON* mcp_tool_screen_set_rotation(cJSON *params, const char *id);
extern cJSON* mcp_tool_network_ip2qrcode(cJSON *params, const char *id);
extern cJSON* mcp_tool_radio_play_station(cJSON *params, const char *id);
extern cJSON* mcp_tool_radio_play_url(cJSON *params, const char *id);
extern cJSON* mcp_tool_radio_stop(cJSON *params, const char *id);
extern cJSON* mcp_tool_radio_get_stations(cJSON *params, const char *id);
extern cJSON* mcp_tool_radio_set_display_mode(cJSON *params, const char *id);

// Tool registry
typedef struct {
    const char *name;
    const char *description;
    cJSON* (*handler)(cJSON *params, const char *id);
    bool user_only;
    cJSON *input_schema;  // JSON schema for input parameters
} mcp_tool_entry_t;

// Tool registry - dynamic array
#define MAX_TOOLS 100
static mcp_tool_entry_t s_tools[MAX_TOOLS];
static size_t s_tools_count = 0;

// Register built-in tools
static void register_builtin_tools(void) {
    // Device control
    sx_mcp_server_register_tool("self.get_device_status", 
        "Provides real-time device information including audio, screen, network, and system status", 
        mcp_tool_get_device_status, false);
    
    sx_mcp_server_register_tool("self.audio_speaker.set_volume", 
        "Set the volume of the audio speaker (0-100)", 
        mcp_tool_audio_set_volume, false);
    
    sx_mcp_server_register_tool("self.screen.set_brightness", 
        "Set the brightness of the screen (0-100)", 
        mcp_tool_screen_set_brightness, false);
    
    sx_mcp_server_register_tool("self.screen.set_theme", 
        "Set the theme of the screen (light or dark)", 
        mcp_tool_screen_set_theme, false);
    
    sx_mcp_server_register_tool("self.screen.set_rotation", 
        "Set the rotation of the screen display (0, 90, 180, or 270 degrees)", 
        mcp_tool_screen_set_rotation, false);
    
    // Network
    sx_mcp_server_register_tool("self.network.ip2qrcode", 
        "Print the QR code of the IP address connected to WiFi network. Returns IP address, SSID, and connection status.", 
        mcp_tool_network_ip2qrcode, false);
    
    // Radio
    sx_mcp_server_register_tool("self.radio.play_station", 
        "Play a radio station by name. Use this tool when user requests to play radio or listen to a specific station.", 
        mcp_tool_radio_play_station, false);
    
    sx_mcp_server_register_tool("self.radio.play_url", 
        "Play a radio stream from a custom URL. Use this tool when user provides a specific radio stream URL.", 
        mcp_tool_radio_play_url, false);
    
    sx_mcp_server_register_tool("self.radio.stop", 
        "Stop the currently playing radio stream.", 
        mcp_tool_radio_stop, false);
    
    sx_mcp_server_register_tool("self.radio.get_stations", 
        "Get the list of available radio stations.", 
        mcp_tool_radio_get_stations, false);
    
    sx_mcp_server_register_tool("self.radio.set_display_mode", 
        "Set the display mode for radio playback. You can choose to display spectrum or station info.", 
        mcp_tool_radio_set_display_mode, false);
}

esp_err_t sx_mcp_server_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }

    s_tools_count = 0;
    register_builtin_tools();
    
    s_initialized = true;
    ESP_LOGI(TAG, "MCP server initialized with %zu built-in tools", s_tools_count);
    return ESP_OK;
}

esp_err_t sx_mcp_server_register_tool(const char *name, const char *description,
                                      cJSON* (*handler)(cJSON *params, const char *id),
                                      bool user_only) {
    if (!name || !description || !handler) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (s_tools_count >= MAX_TOOLS) {
        ESP_LOGE(TAG, "Tool registry full, cannot register: %s", name);
        return ESP_ERR_NO_MEM;
    }
    
    // Check for duplicate
    for (size_t i = 0; i < s_tools_count; i++) {
        if (strcmp(s_tools[i].name, name) == 0) {
            ESP_LOGW(TAG, "Tool already registered: %s", name);
            return ESP_OK;  // Already registered
        }
    }
    
    s_tools[s_tools_count].name = name;
    s_tools[s_tools_count].description = description;
    s_tools[s_tools_count].handler = handler;
    s_tools[s_tools_count].user_only = user_only;
    s_tools[s_tools_count].input_schema = NULL;
    
    s_tools_count++;
    ESP_LOGI(TAG, "Registered tool: %s%s", name, user_only ? " [user]" : "");
    
    return ESP_OK;
}

// Get tools list
static cJSON* handle_tools_list(const char *id, bool list_user_only) {
    cJSON *result = cJSON_CreateObject();
    cJSON *tools = cJSON_CreateArray();
    
    for (size_t i = 0; i < s_tools_count; i++) {
        if (s_tools[i].user_only && !list_user_only) {
            continue;  // Skip user-only tools unless requested
        }
        
        cJSON *tool = cJSON_CreateObject();
        cJSON_AddStringToObject(tool, "name", s_tools[i].name);
        cJSON_AddStringToObject(tool, "description", s_tools[i].description);
        
        // Add input schema if available
        if (s_tools[i].input_schema) {
            cJSON_AddItemToObject(tool, "inputSchema", cJSON_Duplicate(s_tools[i].input_schema, 1));
        } else {
            // Create basic schema
            cJSON *input_schema = cJSON_CreateObject();
            cJSON_AddStringToObject(input_schema, "type", "object");
            cJSON_AddObjectToObject(input_schema, "properties");
            cJSON_AddItemToObject(tool, "inputSchema", input_schema);
        }
        
        // Add audience annotation for user-only tools
        if (s_tools[i].user_only) {
            cJSON *annotations = cJSON_CreateObject();
            cJSON *audience = cJSON_CreateArray();
            cJSON_AddItemToArray(audience, cJSON_CreateString("user"));
            cJSON_AddItemToObject(annotations, "audience", audience);
            cJSON_AddItemToObject(tool, "annotations", annotations);
        }
        
        cJSON_AddItemToArray(tools, tool);
    }
    
    cJSON_AddItemToObject(result, "tools", tools);
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    if (id) {
        cJSON_AddStringToObject(response, "id", id);
    }
    cJSON_AddItemToObject(response, "result", result);
    
    return response;
}

// Call tool
static cJSON* handle_tools_call(const char *id, const char *tool_name, cJSON *arguments) {
    // Find tool
    mcp_tool_entry_t *tool = NULL;
    for (size_t i = 0; i < s_tools_count; i++) {
        if (strcmp(s_tools[i].name, tool_name) == 0) {
            tool = &s_tools[i];
            break;
        }
    }
    
    if (tool == NULL) {
        // Tool not found - return error
        cJSON *error = cJSON_CreateObject();
        cJSON_AddNumberToObject(error, "code", -32601);
        cJSON_AddStringToObject(error, "message", "Method not found");
        
        cJSON *response = cJSON_CreateObject();
        cJSON_AddStringToObject(response, "jsonrpc", "2.0");
        if (id) {
            cJSON_AddStringToObject(response, "id", id);
        }
        cJSON_AddItemToObject(response, "error", error);
        return response;
    }
    
    // Call tool handler
    cJSON *tool_result = tool->handler(arguments, id);
    
    // Tool handler should return a complete JSON-RPC response
    return tool_result;
}

char *sx_mcp_server_parse_message(const char *message) {
    if (!s_initialized || !message) {
        return NULL;
    }

    // Parse JSON-RPC 2.0 message
    cJSON *json = cJSON_Parse(message);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON message");
        return NULL;
    }

    // Extract method and params
    cJSON *method_json = cJSON_GetObjectItem(json, "method");
    cJSON *params_json = cJSON_GetObjectItem(json, "params");
    cJSON *id_json = cJSON_GetObjectItem(json, "id");

    if (!method_json || !cJSON_IsString(method_json)) {
        cJSON_Delete(json);
        ESP_LOGE(TAG, "Invalid method in message");
        return NULL;
    }

    const char *method = cJSON_GetStringValue(method_json);
    const char *id = (id_json && cJSON_IsString(id_json)) ? id_json->valuestring : NULL;
    
    ESP_LOGD(TAG, "MCP method: %s", method);

    cJSON *response = NULL;

    // Handle MCP methods
    if (strcmp(method, "tools/list") == 0) {
        // Check if list_user_only is requested
        bool list_user_only = false;
        if (params_json && cJSON_IsObject(params_json)) {
            cJSON *cursor = cJSON_GetObjectItem(params_json, "cursor");
            // If cursor indicates user-only, set flag
            // For now, always list all tools
        }
        response = handle_tools_list(id, list_user_only);
    } else if (strcmp(method, "tools/call") == 0) {
        // Extract tool name and arguments
        if (!params_json || !cJSON_IsObject(params_json)) {
            cJSON *error = cJSON_CreateObject();
            cJSON_AddNumberToObject(error, "code", -32602);
            cJSON_AddStringToObject(error, "message", "Invalid params");
            
            response = cJSON_CreateObject();
            cJSON_AddStringToObject(response, "jsonrpc", "2.0");
            if (id) {
                cJSON_AddStringToObject(response, "id", id);
            }
            cJSON_AddItemToObject(response, "error", error);
        } else {
            cJSON *name = cJSON_GetObjectItem(params_json, "name");
            cJSON *arguments = cJSON_GetObjectItem(params_json, "arguments");
            
            if (!name || !cJSON_IsString(name)) {
                cJSON *error = cJSON_CreateObject();
                cJSON_AddNumberToObject(error, "code", -32602);
                cJSON_AddStringToObject(error, "message", "Invalid params: name required");
                
                response = cJSON_CreateObject();
                cJSON_AddStringToObject(response, "jsonrpc", "2.0");
                if (id) {
                    cJSON_AddStringToObject(response, "id", id);
                }
                cJSON_AddItemToObject(response, "error", error);
            } else {
                response = handle_tools_call(id, name->valuestring, arguments);
            }
        }
    } else {
        // Unknown method
        cJSON *error = cJSON_CreateObject();
        cJSON_AddNumberToObject(error, "code", -32601);
        cJSON_AddStringToObject(error, "message", "Method not found");
        
        response = cJSON_CreateObject();
        cJSON_AddStringToObject(response, "jsonrpc", "2.0");
        if (id) {
            cJSON_AddStringToObject(response, "id", id);
        }
        cJSON_AddItemToObject(response, "error", error);
    }

    cJSON_Delete(json);

    if (response == NULL) {
        return NULL;
    }

    // Convert to string
    char *response_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);

    // Call send callback if set
    if (s_send_callback != NULL && response_str != NULL) {
        s_send_callback(response_str);
    }
    
    return response_str;
}

esp_err_t sx_mcp_server_set_send_callback(sx_mcp_server_send_cb_t send_cb) {
    s_send_callback = send_cb;
    ESP_LOGI(TAG, "MCP send callback %s", send_cb ? "registered" : "unregistered");
    return ESP_OK;
}

