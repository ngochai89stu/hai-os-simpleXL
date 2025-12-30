#pragma once

#include <esp_err.h>
#include <stddef.h>
#include <stdbool.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize MCP (Model Context Protocol) server
 * 
 * @return ESP_OK on success
 */
esp_err_t sx_mcp_server_init(void);

/**
 * @brief Parse and process MCP message
 * 
 * @param message Input message string
 * @return Response string (caller must free with free())
 */
char *sx_mcp_server_parse_message(const char *message);

/**
 * @brief Register a tool with MCP server
 * 
 * @param name Tool name (e.g., "self.music.play_song")
 * @param description Tool description
 * @param handler Tool handler function
 * @param user_only If true, tool is only visible to user (not AI)
 * @return ESP_OK on success
 */
esp_err_t sx_mcp_server_register_tool(const char *name, const char *description,
                                      cJSON* (*handler)(cJSON *params, const char *id),
                                      bool user_only);

/**
 * @brief Set callback function for sending MCP responses
 * 
 * @param send_cb Callback function to send response (payload is JSON string, caller must free)
 * @return ESP_OK on success
 */
typedef void (*sx_mcp_server_send_cb_t)(const char *payload);
esp_err_t sx_mcp_server_set_send_callback(sx_mcp_server_send_cb_t send_cb);

#ifdef __cplusplus
}
#endif

