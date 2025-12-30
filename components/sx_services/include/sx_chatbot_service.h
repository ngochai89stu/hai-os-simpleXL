#pragma once

#include <esp_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 4: Chatbot service with MCP support (AI assistant / MCP)

typedef struct {
    const char *endpoint;      // optional MCP endpoint URL
    const char *publish_topic; // optional MQTT publish topic
} sx_chatbot_config_t;

// Forward declare cJSON để tránh phụ thuộc mạnh vào cJSON.h trong header public
typedef struct cJSON cJSON;

// Handle incoming MCP message from protocol layer
// This should be called when receiving MCP messages from WebSocket/MQTT
esp_err_t sx_chatbot_handle_mcp_message(const char *message);

// Xử lý JSON chatbot chung (stt/tts/llm/mcp/không có type) cho cả WS/MQTT
// Trả về true nếu đã xử lý (đã post event hoặc chuyển tiếp MCP) để caller không xử lý lại.
bool sx_chatbot_handle_json_message(cJSON *root, const char *raw_fallback);

esp_err_t sx_chatbot_service_init(const sx_chatbot_config_t *cfg);
esp_err_t sx_chatbot_service_start(void);
esp_err_t sx_chatbot_service_stop(void);

// Send a user message (stub)
esp_err_t sx_chatbot_send_message(const char *text);

bool sx_chatbot_is_ready(void);

// Phase 5: Intent parsing integration
esp_err_t sx_chatbot_enable_intent_parsing(bool enable);

// Protocol integration
esp_err_t sx_chatbot_set_protocol_ws_available(bool available);
esp_err_t sx_chatbot_set_protocol_mqtt_available(bool available);

#ifdef __cplusplus
}
#endif


