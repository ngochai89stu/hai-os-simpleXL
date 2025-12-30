// MCP Tools - IR Control (Air Conditioner)
// Hỗ trợ điều khiển điều hòa Toshiba và Sharp qua IR

#include "sx_mcp_tools.h"
#include "sx_mcp_server.h"
#include "sx_ir_service.h"

#include <esp_log.h>
#include <string.h>
#include "cJSON.h"

static const char *TAG = "sx_mcp_tools_ir";

// Use shared helpers from sx_mcp_tools.c
extern cJSON* mcp_tool_create_error(const char *id, int code, const char *message);
extern cJSON* mcp_tool_create_success(const char *id, cJSON *result);

// self.ir_control.ac_command
// Điều khiển điều hòa qua IR (Toshiba, Sharp)
cJSON* mcp_tool_ir_ac_command(cJSON *params, const char *id) {
    cJSON *brand = cJSON_GetObjectItem(params, "brand");
    cJSON *command = cJSON_GetObjectItem(params, "command");
    cJSON *temperature = cJSON_GetObjectItem(params, "temperature");
    
    if (!brand || !cJSON_IsString(brand)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: brand required (toshiba/sharp)");
    }
    if (!command || !cJSON_IsString(command)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: command required");
    }
    
    const char *brand_str = brand->valuestring;
    const char *cmd_str = command->valuestring;
    int temp = temperature && cJSON_IsNumber(temperature) ? temperature->valueint : 0;
    
    // Determine protocol
    ir_protocol_t protocol;
    if (strcasecmp(brand_str, "toshiba") == 0) {
        protocol = IR_PROTOCOL_TOSHIBA_AC;
    } else if (strcasecmp(brand_str, "sharp") == 0) {
        protocol = IR_PROTOCOL_SHARP_AC;
    } else {
        return mcp_tool_create_error(id, -32602, "Unsupported brand. Supported: toshiba, sharp");
    }
    
    // Parse command
    ir_ac_command_t ac_cmd;
    if (strcasecmp(cmd_str, "power_on") == 0 || strcasecmp(cmd_str, "bật") == 0 || strcasecmp(cmd_str, "on") == 0) {
        ac_cmd = IR_AC_POWER_ON;
    } else if (strcasecmp(cmd_str, "power_off") == 0 || strcasecmp(cmd_str, "tắt") == 0 || strcasecmp(cmd_str, "off") == 0) {
        ac_cmd = IR_AC_POWER_OFF;
    } else if (strcasecmp(cmd_str, "temp_up") == 0 || strcasecmp(cmd_str, "tăng_nhiệt") == 0 || strcasecmp(cmd_str, "tăng nhiệt") == 0) {
        ac_cmd = IR_AC_TEMP_UP;
    } else if (strcasecmp(cmd_str, "temp_down") == 0 || strcasecmp(cmd_str, "giảm_nhiệt") == 0 || strcasecmp(cmd_str, "giảm nhiệt") == 0) {
        ac_cmd = IR_AC_TEMP_DOWN;
    } else if (strcasecmp(cmd_str, "mode_cool") == 0 || strcasecmp(cmd_str, "làm_mát") == 0 || strcasecmp(cmd_str, "làm mát") == 0) {
        ac_cmd = IR_AC_MODE_COOL;
    } else if (strcasecmp(cmd_str, "mode_heat") == 0 || strcasecmp(cmd_str, "sưởi") == 0) {
        ac_cmd = IR_AC_MODE_HEAT;
    } else if (strcasecmp(cmd_str, "mode_auto") == 0 || strcasecmp(cmd_str, "tự_động") == 0 || strcasecmp(cmd_str, "tự động") == 0) {
        ac_cmd = IR_AC_MODE_AUTO;
    } else if (strcasecmp(cmd_str, "fan_low") == 0 || strcasecmp(cmd_str, "quạt_thấp") == 0 || strcasecmp(cmd_str, "quạt thấp") == 0) {
        ac_cmd = IR_AC_FAN_SPEED_LOW;
    } else if (strcasecmp(cmd_str, "fan_medium") == 0 || strcasecmp(cmd_str, "quạt_trung") == 0 || strcasecmp(cmd_str, "quạt trung") == 0) {
        ac_cmd = IR_AC_FAN_SPEED_MEDIUM;
    } else if (strcasecmp(cmd_str, "fan_high") == 0 || strcasecmp(cmd_str, "quạt_cao") == 0 || strcasecmp(cmd_str, "quạt cao") == 0) {
        ac_cmd = IR_AC_FAN_SPEED_HIGH;
    } else {
        return mcp_tool_create_error(id, -32602, 
            "Invalid command. Supported: power_on, power_off, temp_up, temp_down, mode_cool, mode_heat, mode_auto, fan_low, fan_medium, fan_high");
    }
    
    // Check if model is specified
    cJSON *model = cJSON_GetObjectItem(params, "model");
    const char *model_str = model && cJSON_IsString(model) ? model->valuestring : NULL;
    
    // Send IR command using full AC state API
    esp_err_t ret = ESP_OK;
    
    if (protocol == IR_PROTOCOL_SHARP_AC) {
        // Use Sharp AC full protocol
        sharp_ac_state_t state;
        ret = sx_ir_sharp_ac_init_state(&state, SHARP_AC_MODEL_A907); // Default to A907
        if (ret == ESP_OK) {
            // Set power
            if (ac_cmd == IR_AC_POWER_ON) {
                ret = sx_ir_sharp_ac_set_power(&state, true, false);
            } else if (ac_cmd == IR_AC_POWER_OFF) {
                ret = sx_ir_sharp_ac_set_power(&state, false, false);
            }
            
            // Set temperature if provided
            if (temp > 0 && ret == ESP_OK) {
                ret = sx_ir_sharp_ac_set_temp(&state, temp);
            }
            
            // Set mode
            if (ret == ESP_OK) {
                if (ac_cmd == IR_AC_MODE_COOL) {
                    ret = sx_ir_sharp_ac_set_mode(&state, SHARP_AC_MODE_COOL);
                } else if (ac_cmd == IR_AC_MODE_HEAT) {
                    ret = sx_ir_sharp_ac_set_mode(&state, SHARP_AC_MODE_HEAT);
                } else if (ac_cmd == IR_AC_MODE_AUTO) {
                    ret = sx_ir_sharp_ac_set_mode(&state, SHARP_AC_MODE_AUTO);
                }
            }
            
            // Set fan
            if (ret == ESP_OK) {
                if (ac_cmd == IR_AC_FAN_SPEED_LOW) {
                    ret = sx_ir_sharp_ac_set_fan(&state, SHARP_AC_FAN_MIN);
                } else if (ac_cmd == IR_AC_FAN_SPEED_MEDIUM) {
                    ret = sx_ir_sharp_ac_set_fan(&state, SHARP_AC_FAN_MED);
                } else if (ac_cmd == IR_AC_FAN_SPEED_HIGH) {
                    ret = sx_ir_sharp_ac_set_fan(&state, SHARP_AC_FAN_HIGH);
                } else if (ac_cmd == IR_AC_FAN_SPEED_AUTO) {
                    ret = sx_ir_sharp_ac_set_fan(&state, SHARP_AC_FAN_AUTO);
                }
            }
            
            // Send state
            if (ret == ESP_OK) {
                ret = sx_ir_sharp_ac_send(&state, 0);
            }
        }
    } else if (protocol == IR_PROTOCOL_TOSHIBA_AC) {
        // Use Toshiba AC full protocol
        toshiba_ac_state_t state;
        ret = sx_ir_toshiba_ac_init_state(&state, TOSHIBA_AC_REMOTE_A); // Default to Remote A
        if (ret == ESP_OK) {
            // Set power
            if (ac_cmd == IR_AC_POWER_ON) {
                ret = sx_ir_toshiba_ac_set_power(&state, true);
            } else if (ac_cmd == IR_AC_POWER_OFF) {
                ret = sx_ir_toshiba_ac_set_power(&state, false);
            }
            
            // Set temperature if provided
            if (temp > 0 && ret == ESP_OK) {
                ret = sx_ir_toshiba_ac_set_temp(&state, temp);
            }
            
            // Set mode
            if (ret == ESP_OK) {
                if (ac_cmd == IR_AC_MODE_COOL) {
                    ret = sx_ir_toshiba_ac_set_mode(&state, TOSHIBA_AC_MODE_COOL);
                } else if (ac_cmd == IR_AC_MODE_HEAT) {
                    ret = sx_ir_toshiba_ac_set_mode(&state, TOSHIBA_AC_MODE_HEAT);
                } else if (ac_cmd == IR_AC_MODE_AUTO) {
                    ret = sx_ir_toshiba_ac_set_mode(&state, TOSHIBA_AC_MODE_AUTO);
                }
            }
            
            // Set fan
            if (ret == ESP_OK) {
                if (ac_cmd == IR_AC_FAN_SPEED_LOW) {
                    ret = sx_ir_toshiba_ac_set_fan(&state, TOSHIBA_AC_FAN_MIN);
                } else if (ac_cmd == IR_AC_FAN_SPEED_MEDIUM) {
                    ret = sx_ir_toshiba_ac_set_fan(&state, TOSHIBA_AC_FAN_MED);
                } else if (ac_cmd == IR_AC_FAN_SPEED_HIGH) {
                    ret = sx_ir_toshiba_ac_set_fan(&state, TOSHIBA_AC_FAN_MAX);
                } else if (ac_cmd == IR_AC_FAN_SPEED_AUTO) {
                    ret = sx_ir_toshiba_ac_set_fan(&state, TOSHIBA_AC_FAN_AUTO);
                }
            }
            
            // Send state
            if (ret == ESP_OK) {
                ret = sx_ir_toshiba_ac_send(&state, 0);
            }
        }
    } else {
        // Fallback to old method for other protocols
        if (model_str != NULL && strlen(model_str) > 0) {
            ret = sx_ir_send_ac_command_by_model(brand_str, model_str, ac_cmd, temp);
        } else {
            ret = sx_ir_send_ac_command(protocol, ac_cmd, temp);
        }
    }
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", ret == ESP_OK);
    cJSON_AddStringToObject(result, "brand", brand_str);
    if (model_str != NULL) {
        cJSON_AddStringToObject(result, "model", model_str);
    }
    cJSON_AddStringToObject(result, "command", cmd_str);
    if (temp > 0) {
        cJSON_AddNumberToObject(result, "temperature", temp);
    }
    if (ret != ESP_OK) {
        cJSON_AddStringToObject(result, "error", esp_err_to_name(ret));
    }
    
    return mcp_tool_create_success(id, result);
}

// self.ir_control.capture
// Capture và decode IR signal từ remote
cJSON* mcp_tool_ir_capture(cJSON *params, const char *id) {
    cJSON *timeout = cJSON_GetObjectItem(params, "timeout");
    int timeout_ms = timeout && cJSON_IsNumber(timeout) ? timeout->valueint : 5000; // Default 5 seconds
    
    // Start IR receive
    sx_ir_receive_config_t rx_cfg = {
        .rx_gpio = -1,  // Will use configured GPIO
        .timeout_ms = timeout_ms,
        .callback = NULL,
        .user_data = NULL,
    };
    
    esp_err_t ret = sx_ir_receive_start(&rx_cfg);
    if (ret != ESP_OK) {
        return mcp_tool_create_error(id, -32603, "Failed to start IR receive");
    }
    
    // Wait for signal
    uint16_t *pulses = NULL;
    size_t count = 0;
    ret = sx_ir_receive_get_last(&pulses, &count);
    
    // Stop receive
    sx_ir_receive_stop();
    
    if (ret != ESP_OK) {
        return mcp_tool_create_error(id, -32603, "No IR signal captured (timeout or error)");
    }
    
    // Detect protocol
    ir_protocol_t protocol = sx_ir_detect_protocol(pulses, count);
    
    // Create result
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", true);
    cJSON_AddNumberToObject(result, "pulse_count", count);
    
    // Protocol info
    const char *protocol_str = "raw";
    switch (protocol) {
        case IR_PROTOCOL_NEC: protocol_str = "nec"; break;
        case IR_PROTOCOL_SHARP_AC: protocol_str = "sharp_ac"; break;
        case IR_PROTOCOL_TOSHIBA_AC: protocol_str = "toshiba_ac"; break;
        default: protocol_str = "raw"; break;
    }
    cJSON_AddStringToObject(result, "protocol", protocol_str);
    
    // Try to decode
    if (protocol == IR_PROTOCOL_NEC) {
        uint8_t address = 0, command = 0;
        if (sx_ir_decode_nec(pulses, count, &address, &command) == ESP_OK) {
            cJSON *decoded = cJSON_CreateObject();
            cJSON_AddNumberToObject(decoded, "address", address);
            cJSON_AddNumberToObject(decoded, "command", command);
            cJSON_AddItemToObject(result, "decoded", decoded);
        }
    } else if (protocol == IR_PROTOCOL_SHARP_AC) {
        sharp_ac_state_t state;
        if (sx_ir_decode_sharp_ac(pulses, count, &state) == ESP_OK) {
            cJSON *decoded = cJSON_CreateObject();
            cJSON_AddNumberToObject(decoded, "temp", (state.raw[4] & 0x0F) + 15);
            cJSON_AddNumberToObject(decoded, "mode", (state.raw[6] >> 0) & 0x03);
            cJSON_AddNumberToObject(decoded, "fan", (state.raw[6] >> 4) & 0x07);
            cJSON_AddItemToObject(result, "decoded", decoded);
        }
    } else if (protocol == IR_PROTOCOL_TOSHIBA_AC) {
        toshiba_ac_state_t state;
        if (sx_ir_decode_toshiba_ac(pulses, count, &state) == ESP_OK) {
            cJSON *decoded = cJSON_CreateObject();
            cJSON_AddNumberToObject(decoded, "temp", (state.raw[5] & 0x0F) + 17);
            cJSON_AddNumberToObject(decoded, "mode", (state.raw[6] >> 0) & 0x07);
            cJSON_AddNumberToObject(decoded, "fan", (state.raw[6] >> 3) & 0x07);
            cJSON_AddItemToObject(result, "decoded", decoded);
        }
    }
    
    // Free buffer
    sx_ir_receive_free_buffer(pulses);
    
    return mcp_tool_create_success(id, result);
}

// Register IR MCP tools
esp_err_t sx_mcp_tools_ir_register(void) {
    // Register IR AC control tool
    esp_err_t ret = sx_mcp_server_register_tool("self.ir_control.ac_command",
        "Điều khiển điều hòa qua IR. brand = toshiba | sharp. command = power_on | power_off | temp_up | temp_down | mode_cool | mode_heat | mode_auto | fan_low | fan_medium | fan_high. temperature (optional) = nhiệt độ mong muốn.",
        mcp_tool_ir_ac_command, false);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IR AC command tool: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register IR capture tool
    ret = sx_mcp_server_register_tool("self.ir_control.capture",
        "Capture và decode IR signal từ remote. timeout (optional) = thời gian chờ (ms, mặc định 5000).",
        mcp_tool_ir_capture, false);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "IR MCP tools registered");
    } else {
        ESP_LOGE(TAG, "Failed to register IR capture tool: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

