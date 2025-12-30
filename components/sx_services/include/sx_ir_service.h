#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// IR service for controlling devices via infrared

typedef struct {
    int tx_gpio;
    int rx_gpio;
    uint32_t carrier_hz;
} sx_ir_config_t;

// IR protocol types
typedef enum {
    IR_PROTOCOL_NEC,
    IR_PROTOCOL_TOSHIBA_AC,
    IR_PROTOCOL_SHARP_AC,
    IR_PROTOCOL_RAW,
} ir_protocol_t;

// Air conditioner commands
typedef enum {
    IR_AC_POWER_ON,
    IR_AC_POWER_OFF,
    IR_AC_TEMP_UP,
    IR_AC_TEMP_DOWN,
    IR_AC_MODE_COOL,
    IR_AC_MODE_HEAT,
    IR_AC_MODE_AUTO,
    IR_AC_MODE_FAN,
    IR_AC_FAN_SPEED_LOW,
    IR_AC_FAN_SPEED_MEDIUM,
    IR_AC_FAN_SPEED_HIGH,
    IR_AC_FAN_SPEED_AUTO,
} ir_ac_command_t;

// Sharp AC remote models
typedef enum {
    SHARP_AC_MODEL_A907,  // Default, newest
    SHARP_AC_MODEL_A903,
    SHARP_AC_MODEL_A705,
} sharp_ac_model_t;

// Sharp AC mode
typedef enum {
    SHARP_AC_MODE_AUTO = 0,
    SHARP_AC_MODE_COOL = 2,
    SHARP_AC_MODE_HEAT = 1,
    SHARP_AC_MODE_DRY = 3,
    SHARP_AC_MODE_FAN = 0,  // A705 only
} sharp_ac_mode_t;

// Sharp AC fan speed
typedef enum {
    SHARP_AC_FAN_AUTO = 2,
    SHARP_AC_FAN_MIN = 4,
    SHARP_AC_FAN_MED = 3,
    SHARP_AC_FAN_HIGH = 5,
    SHARP_AC_FAN_MAX = 7,
} sharp_ac_fan_t;

// Sharp AC power state
typedef enum {
    SHARP_AC_POWER_UNKNOWN = 0,
    SHARP_AC_POWER_ON_FROM_OFF = 1,
    SHARP_AC_POWER_OFF = 2,
    SHARP_AC_POWER_ON = 3,
} sharp_ac_power_t;

// Sharp AC state structure (13 bytes)
#define SHARP_AC_STATE_LENGTH 13
typedef struct {
    uint8_t raw[SHARP_AC_STATE_LENGTH];
    struct {
        uint8_t temp : 4;        // Temperature (15-30°C, offset from 15)
        uint8_t model_bit : 1;    // Model bit
        uint8_t : 3;
        uint8_t power_special : 4; // Power state
        uint8_t mode : 2;        // Mode (Auto/Cool/Heat/Dry)
        uint8_t : 1;
        uint8_t clean : 1;
        uint8_t fan : 3;         // Fan speed
        uint8_t : 1;
        uint8_t timer_hours : 4;
        uint8_t : 2;
        uint8_t timer_type : 1;
        uint8_t timer_enabled : 1;
        uint8_t swing : 3;
        uint8_t : 5;
        uint8_t special : 8;     // Special functions
        uint8_t : 2;
        uint8_t ion : 1;
        uint8_t : 1;
        uint8_t model2 : 1;
        uint8_t : 3;
        uint8_t checksum : 4;    // Checksum (nibble)
    } fields;
} sharp_ac_state_t;

// Toshiba AC remote models
typedef enum {
    TOSHIBA_AC_REMOTE_A,
    TOSHIBA_AC_REMOTE_B,
} toshiba_ac_model_t;

// Toshiba AC mode
typedef enum {
    TOSHIBA_AC_MODE_AUTO = 0,
    TOSHIBA_AC_MODE_COOL = 1,
    TOSHIBA_AC_MODE_DRY = 2,
    TOSHIBA_AC_MODE_HEAT = 3,
    TOSHIBA_AC_MODE_FAN = 4,
    TOSHIBA_AC_MODE_OFF = 7,
} toshiba_ac_mode_t;

// Toshiba AC fan speed
typedef enum {
    TOSHIBA_AC_FAN_AUTO = 0,
    TOSHIBA_AC_FAN_MIN = 1,
    TOSHIBA_AC_FAN_MED = 3,
    TOSHIBA_AC_FAN_MAX = 5,
} toshiba_ac_fan_t;

// Toshiba AC swing
typedef enum {
    TOSHIBA_AC_SWING_STEP = 0,
    TOSHIBA_AC_SWING_ON = 1,
    TOSHIBA_AC_SWING_OFF = 2,
    TOSHIBA_AC_SWING_TOGGLE = 4,
} toshiba_ac_swing_t;

// Toshiba AC state structure (variable length: 5/6/9 bytes)
#define TOSHIBA_AC_STATE_LENGTH_SHORT 5
#define TOSHIBA_AC_STATE_LENGTH 6
#define TOSHIBA_AC_STATE_LENGTH_LONG 9
typedef struct {
    uint8_t raw[TOSHIBA_AC_STATE_LENGTH_LONG];
    uint16_t length;  // Actual length (5, 6, or 9)
    struct {
        uint8_t length_nibble : 4;  // Payload length
        uint8_t model : 4;           // Remote type
        uint8_t temp : 4;            // Temperature (17-30°C, offset from 17)
        uint8_t swing : 3;           // Swing
        uint8_t mode : 3;            // Mode
        uint8_t fan : 3;             // Fan speed
        uint8_t filter : 1;           // Filter
        uint8_t eco_turbo : 8;        // Eco/Turbo (long message only)
    } fields;
} toshiba_ac_state_t;

// IR code structure for a specific model (legacy, for simple commands)
typedef struct {
    const char *model_name;      // Model name (e.g., "Toshiba RAS-B10GK")
    ir_protocol_t protocol;      // Protocol type
    uint8_t address;             // Device address
    uint8_t codes[12];           // Command codes: [POWER_ON, POWER_OFF, TEMP_UP, TEMP_DOWN, 
                                  //                MODE_COOL, MODE_HEAT, MODE_AUTO, 
                                  //                FAN_LOW, FAN_MEDIUM, FAN_HIGH, ...]
} ir_ac_model_code_t;

esp_err_t sx_ir_service_init(const sx_ir_config_t *cfg);
esp_err_t sx_ir_service_start(void);
esp_err_t sx_ir_service_stop(void);

// Deinitialize IR service (cleanup resources)
esp_err_t sx_ir_service_deinit(void);

// Send raw IR pattern
esp_err_t sx_ir_send_raw(const uint16_t *pulses_us, size_t count);

// Send NEC protocol command
esp_err_t sx_ir_send_nec(uint8_t address, uint8_t command);

// Send air conditioner command (Toshiba/Sharp)
esp_err_t sx_ir_send_ac_command(ir_protocol_t protocol, ir_ac_command_t command, int temperature);

// Register custom IR code for a specific model
esp_err_t sx_ir_register_model_code(const ir_ac_model_code_t *model_code);

// Send command using registered model
esp_err_t sx_ir_send_ac_command_by_model(const char *brand, const char *model, ir_ac_command_t command, int temperature);

// Get IR code for a specific model and command
esp_err_t sx_ir_get_code(const char *brand, const char *model, ir_ac_command_t command, uint8_t *address, uint8_t *code);

// Get protocol for a model
ir_protocol_t sx_ir_get_protocol(const char *brand, const char *model);

// List all available models
void sx_ir_list_models(char *buffer, size_t buffer_size);

// ===== Sharp AC Full Protocol API =====

// Initialize Sharp AC state with defaults
esp_err_t sx_ir_sharp_ac_init_state(sharp_ac_state_t *state, sharp_ac_model_t model);

// Set Sharp AC power
esp_err_t sx_ir_sharp_ac_set_power(sharp_ac_state_t *state, bool on, bool prev_on);

// Set Sharp AC temperature (15-30°C)
esp_err_t sx_ir_sharp_ac_set_temp(sharp_ac_state_t *state, uint8_t temp);

// Set Sharp AC mode
esp_err_t sx_ir_sharp_ac_set_mode(sharp_ac_state_t *state, sharp_ac_mode_t mode);

// Set Sharp AC fan speed
esp_err_t sx_ir_sharp_ac_set_fan(sharp_ac_state_t *state, sharp_ac_fan_t fan);

// Set Sharp AC swing vertical
esp_err_t sx_ir_sharp_ac_set_swing(sharp_ac_state_t *state, uint8_t swing);

// Set Sharp AC turbo mode
esp_err_t sx_ir_sharp_ac_set_turbo(sharp_ac_state_t *state, bool on);

// Calculate and set Sharp AC checksum
esp_err_t sx_ir_sharp_ac_calc_checksum(sharp_ac_state_t *state);

// Validate Sharp AC checksum
bool sx_ir_sharp_ac_validate_checksum(const sharp_ac_state_t *state);

// Send Sharp AC state
esp_err_t sx_ir_sharp_ac_send(const sharp_ac_state_t *state, uint16_t repeat);

// ===== Toshiba AC Full Protocol API =====

// Initialize Toshiba AC state with defaults
esp_err_t sx_ir_toshiba_ac_init_state(toshiba_ac_state_t *state, toshiba_ac_model_t model);

// Set Toshiba AC power
esp_err_t sx_ir_toshiba_ac_set_power(toshiba_ac_state_t *state, bool on);

// Set Toshiba AC temperature (17-30°C)
esp_err_t sx_ir_toshiba_ac_set_temp(toshiba_ac_state_t *state, uint8_t temp);

// Set Toshiba AC mode
esp_err_t sx_ir_toshiba_ac_set_mode(toshiba_ac_state_t *state, toshiba_ac_mode_t mode);

// Set Toshiba AC fan speed
esp_err_t sx_ir_toshiba_ac_set_fan(toshiba_ac_state_t *state, toshiba_ac_fan_t fan);

// Set Toshiba AC swing
esp_err_t sx_ir_toshiba_ac_set_swing(toshiba_ac_state_t *state, toshiba_ac_swing_t swing);

// Set Toshiba AC turbo mode
esp_err_t sx_ir_toshiba_ac_set_turbo(toshiba_ac_state_t *state, bool on);

// Set Toshiba AC economy mode
esp_err_t sx_ir_toshiba_ac_set_econo(toshiba_ac_state_t *state, bool on);

// Calculate and set Toshiba AC checksum
esp_err_t sx_ir_toshiba_ac_calc_checksum(toshiba_ac_state_t *state);

// Validate Toshiba AC checksum
bool sx_ir_toshiba_ac_validate_checksum(const toshiba_ac_state_t *state);

// Send Toshiba AC state
esp_err_t sx_ir_toshiba_ac_send(const toshiba_ac_state_t *state, uint16_t repeat);

// ===== IR Receive Support =====

// IR receive callback type
typedef void (*sx_ir_receive_callback_t)(const uint16_t *pulses, size_t count, void *user_data);

// IR receive configuration
typedef struct {
    int rx_gpio;                              // GPIO for IR receiver
    uint32_t timeout_ms;                      // Receive timeout in ms
    sx_ir_receive_callback_t callback;        // Callback when signal received
    void *user_data;                          // User data for callback
} sx_ir_receive_config_t;

// Start IR receive
esp_err_t sx_ir_receive_start(const sx_ir_receive_config_t *config);

// Stop IR receive
esp_err_t sx_ir_receive_stop(void);

// Get last captured IR signal
esp_err_t sx_ir_receive_get_last(uint16_t **pulses, size_t *count);

// Free captured IR signal buffer
void sx_ir_receive_free_buffer(uint16_t *pulses);

// Decode NEC protocol from captured pulses
esp_err_t sx_ir_decode_nec(const uint16_t *pulses, size_t count, uint8_t *address, uint8_t *command);

// Decode Sharp AC from captured pulses
esp_err_t sx_ir_decode_sharp_ac(const uint16_t *pulses, size_t count, sharp_ac_state_t *state);

// Decode Toshiba AC from captured pulses
esp_err_t sx_ir_decode_toshiba_ac(const uint16_t *pulses, size_t count, toshiba_ac_state_t *state);

// Detect protocol from captured pulses
ir_protocol_t sx_ir_detect_protocol(const uint16_t *pulses, size_t count);

#ifdef __cplusplus
}
#endif






