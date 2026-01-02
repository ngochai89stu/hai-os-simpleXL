#include "sx_protocol_common.h"
#include "sx_protocol_base.h"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_event_string_pool.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include <cJSON.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Phase 1: Extract common JSON building code to reduce duplication

char* sx_protocol_common_build_user_message_json(const char *text) {
    if (!text) return NULL;
    
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddStringToObject(json, "type", "user_message");
    cJSON_AddStringToObject(json, "text", text);
    
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    return json_str;
}

char* sx_protocol_common_build_wake_word_json(const char *session_id, const char *wake_word) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;
    
    if (session_id && session_id[0] != '\0') {
        cJSON_AddStringToObject(json, "session_id", session_id);
    }
    cJSON_AddStringToObject(json, "type", "wake_word_detected");
    if (wake_word) {
        cJSON_AddStringToObject(json, "wake_word", wake_word);
    }
    
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    return json_str;
}

char* sx_protocol_common_build_start_listening_json(const char *session_id, sx_listening_mode_t mode) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;
    
    if (session_id && session_id[0] != '\0') {
        cJSON_AddStringToObject(json, "session_id", session_id);
    }
    cJSON_AddStringToObject(json, "type", "start_listening");
    
    const char *mode_str = "auto_stop";
    switch (mode) {
        case SX_LISTENING_MODE_MANUAL_STOP:
            mode_str = "manual_stop";
            break;
        case SX_LISTENING_MODE_REALTIME:
            mode_str = "realtime";
            break;
        default:
            mode_str = "auto_stop";
            break;
    }
    cJSON_AddStringToObject(json, "mode", mode_str);
    
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    return json_str;
}

char* sx_protocol_common_build_stop_listening_json(const char *session_id) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;
    
    if (session_id && session_id[0] != '\0') {
        cJSON_AddStringToObject(json, "session_id", session_id);
    }
    cJSON_AddStringToObject(json, "type", "stop_listening");
    
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    return json_str;
}

char* sx_protocol_common_build_abort_speaking_json(const char *session_id, sx_abort_reason_t reason) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;
    
    if (session_id && session_id[0] != '\0') {
        cJSON_AddStringToObject(json, "session_id", session_id);
    }
    cJSON_AddStringToObject(json, "type", "abort");
    
    if (reason == SX_ABORT_REASON_WAKE_WORD_DETECTED) {
        cJSON_AddStringToObject(json, "reason", "wake_word_detected");
    }
    
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    return json_str;
}

char* sx_protocol_common_build_mcp_message_json(const char *session_id, const char *payload) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;
    
    if (session_id && session_id[0] != '\0') {
        cJSON_AddStringToObject(json, "session_id", session_id);
    }
    cJSON_AddStringToObject(json, "type", "mcp");
    
    // Parse payload as JSON object if it's a string
    cJSON *payload_obj = cJSON_Parse(payload);
    if (payload_obj) {
        cJSON_AddItemToObject(json, "payload", payload_obj);
    } else {
        // If parsing fails, treat as raw string
        cJSON_AddStringToObject(json, "payload", payload);
    }
    
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    return json_str;
}

esp_err_t sx_protocol_common_handle_error(const char *error_msg) {
    if (!error_msg) return ESP_ERR_INVALID_ARG;
    
    char *error_str = sx_event_alloc_string(error_msg);
    if (error_str) {
        sx_event_t evt = {
            .type = SX_EVT_PROTOCOL_ERROR,
            .arg0 = 0,
            .ptr = error_str,
        };
        sx_dispatcher_post_event(&evt);
        return ESP_OK;
    }
    
    return ESP_ERR_NO_MEM;
}

bool sx_protocol_common_is_valid_state(sx_protocol_base_t *protocol) {
    return (protocol != NULL && 
            protocol->ops != NULL && 
            protocol->ops->is_connected != NULL &&
            protocol->ops->is_connected(protocol));
}

// Phase 2: Common timeout detection pattern (used by both WS and MQTT)
bool sx_protocol_common_check_timeout(uint32_t last_incoming_time, uint32_t timeout_ms) {
    if (last_incoming_time == 0) return false;
    
    uint32_t now = xTaskGetTickCount();
    uint32_t elapsed = (now - last_incoming_time) * portTICK_PERIOD_MS;
    
    return elapsed > timeout_ms;
}

// Phase 2: Update last incoming time (returns current tick count)
uint32_t sx_protocol_common_update_last_incoming_time(uint32_t *last_incoming_time) {
    if (!last_incoming_time) return 0;
    
    uint32_t now = xTaskGetTickCount();
    *last_incoming_time = now;
    return now;
}

