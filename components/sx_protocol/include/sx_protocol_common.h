#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include "sx_protocol_audio.h"
#include "sx_protocol_base.h"

#ifdef __cplusplus
extern "C" {
#endif

// Common JSON message building utilities
// These functions help reduce code duplication between WS and MQTT implementations

// Build user message JSON
// Returns allocated string (caller must free) or NULL on error
char* sx_protocol_common_build_user_message_json(const char *text);

// Build wake word detected JSON
char* sx_protocol_common_build_wake_word_json(const char *session_id, const char *wake_word);

// Build start listening JSON
char* sx_protocol_common_build_start_listening_json(const char *session_id, sx_listening_mode_t mode);

// Build stop listening JSON
char* sx_protocol_common_build_stop_listening_json(const char *session_id);

// Build abort speaking JSON
char* sx_protocol_common_build_abort_speaking_json(const char *session_id, sx_abort_reason_t reason);

// Build MCP message JSON
char* sx_protocol_common_build_mcp_message_json(const char *session_id, const char *payload);

// Common error handling
esp_err_t sx_protocol_common_handle_error(const char *error_msg);

// Common connection state helpers
bool sx_protocol_common_is_valid_state(sx_protocol_base_t *protocol);

// Common timeout detection (30 seconds pattern)
// Returns true if elapsed time since last_incoming_time exceeds timeout_ms
bool sx_protocol_common_check_timeout(uint32_t last_incoming_time, uint32_t timeout_ms);

// Update last incoming time (returns current tick count)
uint32_t sx_protocol_common_update_last_incoming_time(uint32_t *last_incoming_time);

#ifdef __cplusplus
}
#endif

