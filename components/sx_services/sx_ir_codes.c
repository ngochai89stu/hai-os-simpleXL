// IR Code Database - Mã lệnh IR cho các model điều hòa
// Cách thêm mã mới: Thêm entry vào mảng s_ir_ac_codes[]

#include "sx_ir_service.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <esp_log.h>

static const char *TAG = "sx_ir_codes";

// Case-insensitive string comparison (strcasecmp alternative)
static int str_casecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int diff = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
        if (diff != 0) {
            return diff;
        }
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

// Database mã lệnh IR cho các model điều hòa
// Cấu trúc: model_name, protocol, address, [POWER_ON, POWER_OFF, TEMP_UP, TEMP_DOWN, 
//           MODE_COOL, MODE_HEAT, MODE_AUTO, FAN_LOW, FAN_MEDIUM, FAN_HIGH, ...]

static const ir_ac_model_code_t s_ir_ac_codes[] = {
    // Toshiba models
    {
        .model_name = "Toshiba_RAS-B10GK",
        .protocol = IR_PROTOCOL_TOSHIBA_AC,
        .address = 0xBF,
        .codes = {
            [IR_AC_POWER_ON] = 0x02,
            [IR_AC_POWER_OFF] = 0x03,
            [IR_AC_TEMP_UP] = 0x10,
            [IR_AC_TEMP_DOWN] = 0x11,
            [IR_AC_MODE_COOL] = 0x20,
            [IR_AC_MODE_HEAT] = 0x21,
            [IR_AC_MODE_AUTO] = 0x22,
            [IR_AC_FAN_SPEED_LOW] = 0x30,
            [IR_AC_FAN_SPEED_MEDIUM] = 0x31,
            [IR_AC_FAN_SPEED_HIGH] = 0x32,
        }
    },
    {
        .model_name = "Toshiba_RAS-B13GK",
        .protocol = IR_PROTOCOL_TOSHIBA_AC,
        .address = 0xBF,
        .codes = {
            [IR_AC_POWER_ON] = 0x02,
            [IR_AC_POWER_OFF] = 0x03,
            [IR_AC_TEMP_UP] = 0x10,
            [IR_AC_TEMP_DOWN] = 0x11,
            [IR_AC_MODE_COOL] = 0x20,
            [IR_AC_MODE_HEAT] = 0x21,
            [IR_AC_MODE_AUTO] = 0x22,
            [IR_AC_FAN_SPEED_LOW] = 0x30,
            [IR_AC_FAN_SPEED_MEDIUM] = 0x31,
            [IR_AC_FAN_SPEED_HIGH] = 0x32,
        }
    },
    // Toshiba RAS-H10C4KCVG-V - Model mới, mã lệnh cần test và điều chỉnh
    // Lưu ý: Mã lệnh này dựa trên protocol Toshiba chuẩn, có thể cần điều chỉnh theo model thực tế
    {
        .model_name = "Toshiba_RAS-H10C4KCVG-V",
        .protocol = IR_PROTOCOL_TOSHIBA_AC,
        .address = 0xBF,  // Address mặc định Toshiba, có thể cần thay đổi
        .codes = {
            [IR_AC_POWER_ON] = 0x02,      // Cần test và điều chỉnh
            [IR_AC_POWER_OFF] = 0x03,     // Cần test và điều chỉnh
            [IR_AC_TEMP_UP] = 0x10,        // Cần test và điều chỉnh
            [IR_AC_TEMP_DOWN] = 0x11,     // Cần test và điều chỉnh
            [IR_AC_MODE_COOL] = 0x20,     // Cần test và điều chỉnh
            [IR_AC_MODE_HEAT] = 0x21,     // Cần test và điều chỉnh
            [IR_AC_MODE_AUTO] = 0x22,     // Cần test và điều chỉnh
            [IR_AC_FAN_SPEED_LOW] = 0x30,  // Cần test và điều chỉnh
            [IR_AC_FAN_SPEED_MEDIUM] = 0x31, // Cần test và điều chỉnh
            [IR_AC_FAN_SPEED_HIGH] = 0x32,  // Cần test và điều chỉnh
        }
    },
    // Sharp models
    {
        .model_name = "Sharp_AY-XP10FR",
        .protocol = IR_PROTOCOL_SHARP_AC,
        .address = 0x5F,
        .codes = {
            [IR_AC_POWER_ON] = 0x01,
            [IR_AC_POWER_OFF] = 0x02,
            [IR_AC_TEMP_UP] = 0x10,
            [IR_AC_TEMP_DOWN] = 0x11,
            [IR_AC_MODE_COOL] = 0x20,
            [IR_AC_MODE_HEAT] = 0x21,
            [IR_AC_MODE_AUTO] = 0x22,
            [IR_AC_FAN_SPEED_LOW] = 0x40,
            [IR_AC_FAN_SPEED_MEDIUM] = 0x41,
            [IR_AC_FAN_SPEED_HIGH] = 0x42,
        }
    },
    {
        .model_name = "Sharp_AY-XP13FR",
        .protocol = IR_PROTOCOL_SHARP_AC,
        .address = 0x5F,
        .codes = {
            [IR_AC_POWER_ON] = 0x01,
            [IR_AC_POWER_OFF] = 0x02,
            [IR_AC_TEMP_UP] = 0x10,
            [IR_AC_TEMP_DOWN] = 0x11,
            [IR_AC_MODE_COOL] = 0x20,
            [IR_AC_MODE_HEAT] = 0x21,
            [IR_AC_MODE_AUTO] = 0x22,
            [IR_AC_FAN_SPEED_LOW] = 0x40,
            [IR_AC_FAN_SPEED_MEDIUM] = 0x41,
            [IR_AC_FAN_SPEED_HIGH] = 0x42,
        }
    },
    // Sharp AH-X10ZEW 1 HP - Model mới, mã lệnh cần test và điều chỉnh
    // Lưu ý: Mã lệnh này dựa trên protocol Sharp chuẩn, có thể cần điều chỉnh theo model thực tế
    {
        .model_name = "Sharp_AH-X10ZEW",
        .protocol = IR_PROTOCOL_SHARP_AC,
        .address = 0x5F,  // Address mặc định Sharp, có thể cần thay đổi
        .codes = {
            [IR_AC_POWER_ON] = 0x01,      // Cần test và điều chỉnh
            [IR_AC_POWER_OFF] = 0x02,     // Cần test và điều chỉnh
            [IR_AC_TEMP_UP] = 0x10,        // Cần test và điều chỉnh
            [IR_AC_TEMP_DOWN] = 0x11,     // Cần test và điều chỉnh
            [IR_AC_MODE_COOL] = 0x20,     // Cần test và điều chỉnh
            [IR_AC_MODE_HEAT] = 0x21,     // Cần test và điều chỉnh
            [IR_AC_MODE_AUTO] = 0x22,     // Cần test và điều chỉnh
            [IR_AC_FAN_SPEED_LOW] = 0x40,  // Cần test và điều chỉnh
            [IR_AC_FAN_SPEED_MEDIUM] = 0x41, // Cần test và điều chỉnh
            [IR_AC_FAN_SPEED_HIGH] = 0x42,  // Cần test và điều chỉnh
        }
    },
    // Thêm các model khác ở đây...
};

#define IR_AC_CODES_COUNT (sizeof(s_ir_ac_codes) / sizeof(s_ir_ac_codes[0]))

// Get IR code for a specific model and command
esp_err_t sx_ir_get_code(const char *brand, const char *model, ir_ac_command_t command, uint8_t *address, uint8_t *code) {
    if (brand == NULL || model == NULL || address == NULL || code == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Build model name: "Brand_Model"
    char model_name[64];
    snprintf(model_name, sizeof(model_name), "%s_%s", brand, model);
    
    // Search in database
    for (size_t i = 0; i < IR_AC_CODES_COUNT; i++) {
        if (str_casecmp(s_ir_ac_codes[i].model_name, model_name) == 0) {
            *address = s_ir_ac_codes[i].address;
            *code = s_ir_ac_codes[i].codes[command];
            ESP_LOGI(TAG, "Found code for %s: address=0x%02X, command=0x%02X", 
                     model_name, *address, *code);
            return ESP_OK;
        }
    }
    
    ESP_LOGW(TAG, "Model not found: %s", model_name);
    return ESP_ERR_NOT_FOUND;
}

// Get protocol for a model
ir_protocol_t sx_ir_get_protocol(const char *brand, const char *model) {
    if (brand == NULL || model == NULL) {
        return IR_PROTOCOL_NEC;
    }
    
    char model_name[64];
    snprintf(model_name, sizeof(model_name), "%s_%s", brand, model);
    
    for (size_t i = 0; i < IR_AC_CODES_COUNT; i++) {
        if (str_casecmp(s_ir_ac_codes[i].model_name, model_name) == 0) {
            return s_ir_ac_codes[i].protocol;
        }
    }
    
    // Default fallback
    if (str_casecmp(brand, "toshiba") == 0) {
        return IR_PROTOCOL_TOSHIBA_AC;
    } else if (str_casecmp(brand, "sharp") == 0) {
        return IR_PROTOCOL_SHARP_AC;
    }
    
    return IR_PROTOCOL_NEC;
}

// List all available models
void sx_ir_list_models(char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        return;
    }
    
    buffer[0] = '\0';
    for (size_t i = 0; i < IR_AC_CODES_COUNT; i++) {
        if (i > 0) {
            strncat(buffer, ", ", buffer_size - strlen(buffer) - 1);
        }
        strncat(buffer, s_ir_ac_codes[i].model_name, buffer_size - strlen(buffer) - 1);
    }
}

