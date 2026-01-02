#include "sx_ir_service.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_encoder.h"
#include "driver/gpio.h"

// External functions from sx_ir_codes.c
extern esp_err_t sx_ir_get_code(const char *brand, const char *model, ir_ac_command_t command, uint8_t *address, uint8_t *code);
extern ir_protocol_t sx_ir_get_protocol(const char *brand, const char *model);

static const char *TAG = "sx_ir";

static bool s_initialized = false;
static sx_ir_config_t s_cfg = {0};
static rmt_channel_handle_t s_rmt_tx_chan = NULL;
static rmt_encoder_handle_t s_rmt_bytes_encoder = NULL;

// IR Receive state
static rmt_channel_handle_t s_rmt_rx_chan = NULL;
static bool s_rx_active = false;
static sx_ir_receive_config_t s_rx_config = {0};
static uint16_t *s_captured_pulses = NULL;
static size_t s_captured_count = 0;
static QueueHandle_t s_rx_queue = NULL;

// Thread safety
static SemaphoreHandle_t s_ir_mutex = NULL;

// Memory management - static buffers để tránh fragmentation
#define MAX_IR_PULSES 300  // Đủ cho message dài nhất (13 bytes * 16 + header)
static rmt_symbol_word_t s_tx_symbol_buffer[MAX_IR_PULSES];
static uint16_t s_pulse_buffer[MAX_IR_PULSES];

esp_err_t sx_ir_service_init(const sx_ir_config_t *cfg) {
    if (s_initialized) {
        return ESP_OK;
    }
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_cfg = *cfg;
    
    if (s_cfg.tx_gpio >= 0) {
        // Phase 4: Initialize RMT TX channel for IR
        rmt_tx_channel_config_t tx_chan_cfg = {
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .gpio_num = s_cfg.tx_gpio,
            .mem_block_symbols = 64,
            .resolution_hz = 1000000, // 1MHz = 1us per tick
            .trans_queue_depth = 4,
            .flags = {
                .invert_out = false,
                .with_dma = false,
            },
        };
        
        esp_err_t ret = rmt_new_tx_channel(&tx_chan_cfg, &s_rmt_tx_chan);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "rmt_new_tx_channel failed: %s", esp_err_to_name(ret));
            s_initialized = true;
            return ret;
        }
        
        // Configure carrier (38kHz modulation)
        rmt_carrier_config_t carrier_cfg = {
            .duty_cycle = 0.33,  // 33% duty cycle
            .frequency_hz = s_cfg.carrier_hz,
            .flags = {
                .polarity_active_low = false,
            },
        };
        ret = rmt_apply_carrier(s_rmt_tx_chan, &carrier_cfg);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "rmt_apply_carrier failed: %s", esp_err_to_name(ret));
        }
        
        // Create bytes encoder for NEC protocol
        rmt_bytes_encoder_config_t bytes_encoder_cfg = {
            .bit0 = {
                .level0 = 1,
                .duration0 = 560,  // 560us mark
                .level1 = 0,
                .duration1 = 560,  // 560us space
            },
            .bit1 = {
                .level0 = 1,
                .duration0 = 560,  // 560us mark
                .level1 = 0,
                .duration1 = 1690, // 1690us space
            },
            .flags = {
                .msb_first = 1,
            },
        };
        ret = rmt_new_bytes_encoder(&bytes_encoder_cfg, &s_rmt_bytes_encoder);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "rmt_new_bytes_encoder failed: %s", esp_err_to_name(ret));
            rmt_del_channel(s_rmt_tx_chan);
            s_rmt_tx_chan = NULL;
            s_initialized = true;
            return ret;
        }
        
        // Enable RMT channel
        ret = rmt_enable(s_rmt_tx_chan);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "rmt_enable failed: %s", esp_err_to_name(ret));
            rmt_del_encoder(s_rmt_bytes_encoder);
            s_rmt_bytes_encoder = NULL;
            rmt_del_channel(s_rmt_tx_chan);
            s_rmt_tx_chan = NULL;
            s_initialized = true;
            return ret;
        }
        
        ESP_LOGI(TAG, "IR RMT TX initialized: gpio=%d carrier=%lu Hz", s_cfg.tx_gpio, (unsigned long)s_cfg.carrier_hz);
    }
    
    s_initialized = true;
    return ESP_OK;
}

esp_err_t sx_ir_service_start(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "IR service started (stub)");
    return ESP_OK;
}

esp_err_t sx_ir_service_stop(void) {
    ESP_LOGI(TAG, "IR service stopped (stub)");
    return ESP_OK;
}

esp_err_t sx_ir_send_raw(const uint16_t *pulses_us, size_t count) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (pulses_us == NULL || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_rmt_tx_chan == NULL) {
        ESP_LOGE(TAG, "RMT TX channel not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Thread safety: acquire mutex
    if (s_ir_mutex != NULL) {
        if (xSemaphoreTake(s_ir_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire IR mutex (timeout)");
            return ESP_ERR_TIMEOUT;
        }
    }
    
    esp_err_t ret = ESP_OK;
    
    // Use static buffer instead of dynamic allocation
    if (count > MAX_IR_PULSES) {
        ESP_LOGE(TAG, "Pulse count %u exceeds maximum %u", (unsigned)count, MAX_IR_PULSES);
        ret = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }
    
    rmt_symbol_word_t *rmt_symbols = s_tx_symbol_buffer;
    size_t symbol_count = count;
    
    // Convert pulses_us to RMT symbols with carrier modulation
    for (size_t i = 0; i < symbol_count; i++) {
        uint16_t pulse_us = pulses_us[i];
        rmt_symbols[i].level0 = (i % 2 == 0) ? 1 : 0; // Alternate high/low
        rmt_symbols[i].duration0 = pulse_us; // Duration in microseconds
        rmt_symbols[i].level1 = 0;
        rmt_symbols[i].duration1 = 0;
    }
    
    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0,
        .flags = {
            .eot_level = 0,
        },
    };
    
    // Use copy encoder for raw pulses (need to create it if not exists)
    rmt_encoder_handle_t copy_encoder = NULL;
    rmt_copy_encoder_config_t copy_encoder_cfg = {};
    ret = rmt_new_copy_encoder(&copy_encoder_cfg, &copy_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create copy encoder: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    
    ret = rmt_transmit(s_rmt_tx_chan, copy_encoder, rmt_symbols, symbol_count * sizeof(rmt_symbol_word_t), &tx_cfg);
    rmt_del_encoder(copy_encoder);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "rmt_transmit failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    
    ESP_LOGI(TAG, "IR send raw: %u pulses", (unsigned)count);
    
cleanup:
    // Release mutex
    if (s_ir_mutex != NULL) {
        xSemaphoreGive(s_ir_mutex);
    }
    
    return ret;
}

// Send NEC protocol command
esp_err_t sx_ir_send_nec(uint8_t address, uint8_t command) {
    if (!s_initialized || s_rmt_tx_chan == NULL || s_rmt_bytes_encoder == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Thread safety: acquire mutex
    if (s_ir_mutex != NULL) {
        if (xSemaphoreTake(s_ir_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire IR mutex (timeout)");
            return ESP_ERR_TIMEOUT;
        }
    }
    
    esp_err_t ret = ESP_OK;
    
    // NEC protocol format: [header: 9ms mark + 4.5ms space] + [address] + [~address] + [command] + [~command]
    // Use static buffer instead of stack allocation
    uint16_t *nec_pulses = s_pulse_buffer;
    memset(nec_pulses, 0, MAX_IR_PULSES * sizeof(uint16_t));
    size_t idx = 0;
    
    // Header: 9ms mark + 4.5ms space
    nec_pulses[idx++] = 9000;  // Mark
    nec_pulses[idx++] = 4500;  // Space
    
    // Encode 4 bytes: address, ~address, command, ~command
    uint8_t nec_data[4] = {address, ~address, command, ~command};
    
    for (int byte_idx = 0; byte_idx < 4; byte_idx++) {
        uint8_t byte = nec_data[byte_idx];
        for (int bit = 7; bit >= 0; bit--) {
            nec_pulses[idx++] = 560;  // Mark (always 560us)
            if (byte & (1 << bit)) {
                nec_pulses[idx++] = 1690;  // Space for '1' (1690us)
            } else {
                nec_pulses[idx++] = 560;   // Space for '0' (560us)
            }
        }
    }
    
    // End pulse
    nec_pulses[idx++] = 560;  // Final mark
    
    // Send using raw pulses (note: sx_ir_send_raw will acquire mutex again, but that's OK with recursive mutex handling)
    // We need to release mutex first to avoid deadlock
    if (s_ir_mutex != NULL) {
        xSemaphoreGive(s_ir_mutex);
    }
    
    ret = sx_ir_send_raw(nec_pulses, idx);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "NEC command sent: address=0x%02X, command=0x%02X", address, command);
    }
    
    return ret;
}

// Send air conditioner command (Toshiba/Sharp) - using default codes
esp_err_t sx_ir_send_ac_command(ir_protocol_t protocol, ir_ac_command_t command, int temperature) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Default codes (fallback if model not found)
    uint8_t address = 0;
    uint8_t cmd = 0;
    esp_err_t ret = ESP_OK;
    
    switch (protocol) {
        case IR_PROTOCOL_TOSHIBA_AC:
            address = 0xBF;
            switch (command) {
                case IR_AC_POWER_ON: cmd = 0x02; break;
                case IR_AC_POWER_OFF: cmd = 0x03; break;
                case IR_AC_TEMP_UP: cmd = 0x10; break;
                case IR_AC_TEMP_DOWN: cmd = 0x11; break;
                case IR_AC_MODE_COOL: cmd = 0x20; break;
                case IR_AC_MODE_HEAT: cmd = 0x21; break;
                case IR_AC_MODE_AUTO: cmd = 0x22; break;
                case IR_AC_FAN_SPEED_LOW: cmd = 0x30; break;
                case IR_AC_FAN_SPEED_MEDIUM: cmd = 0x31; break;
                case IR_AC_FAN_SPEED_HIGH: cmd = 0x32; break;
                default: return ESP_ERR_INVALID_ARG;
            }
            break;
            
        case IR_PROTOCOL_SHARP_AC:
            address = 0x5F;
            switch (command) {
                case IR_AC_POWER_ON: cmd = 0x01; break;
                case IR_AC_POWER_OFF: cmd = 0x02; break;
                case IR_AC_TEMP_UP: cmd = 0x10; break;
                case IR_AC_TEMP_DOWN: cmd = 0x11; break;
                case IR_AC_MODE_COOL: cmd = 0x20; break;
                case IR_AC_MODE_HEAT: cmd = 0x21; break;
                case IR_AC_MODE_AUTO: cmd = 0x22; break;
                case IR_AC_FAN_SPEED_LOW: cmd = 0x40; break;
                case IR_AC_FAN_SPEED_MEDIUM: cmd = 0x41; break;
                case IR_AC_FAN_SPEED_HIGH: cmd = 0x42; break;
                default: return ESP_ERR_INVALID_ARG;
            }
            break;
            
        default:
            ret = ESP_ERR_NOT_SUPPORTED;
            goto cleanup;
    }
    
    // Release mutex before calling sx_ir_send_nec (it will acquire its own)
    if (s_ir_mutex != NULL) {
        xSemaphoreGive(s_ir_mutex);
    }
    
    ret = sx_ir_send_nec(address, cmd);
    return ret;
    
cleanup:
    // Release mutex on error
    if (s_ir_mutex != NULL) {
        xSemaphoreGive(s_ir_mutex);
    }
    return ret;
}

// Send command using registered model
esp_err_t sx_ir_send_ac_command_by_model(const char *brand, const char *model, ir_ac_command_t command, int temperature) {
    if (brand == NULL || model == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t address = 0;
    uint8_t code = 0;
    
    // Try to get code from database
    esp_err_t ret = sx_ir_get_code(brand, model, command, &address, &code);
    if (ret == ESP_OK) {
        // Found in database, use it
        ESP_LOGI(TAG, "Using code from database for %s %s", brand, model);
        return sx_ir_send_nec(address, code);
    }
    
    // Fallback to default protocol codes
    ESP_LOGW(TAG, "Model %s %s not found in database, using default codes", brand, model);
    ir_protocol_t protocol = sx_ir_get_protocol(brand, model);
    return sx_ir_send_ac_command(protocol, command, temperature);
}

// ===== Sharp AC Full Protocol Implementation =====

// Helper: XOR bytes
static uint8_t xor_bytes(const uint8_t *data, size_t length) {
    uint8_t result = 0;
    for (size_t i = 0; i < length; i++) {
        result ^= data[i];
    }
    return result;
}

// Helper: Get bits from byte
static uint8_t get_bits(uint8_t byte, uint8_t start, uint8_t count) {
    return (byte >> start) & ((1 << count) - 1);
}

// Helper: Set bits in byte
static void set_bits(uint8_t *byte, uint8_t start, uint8_t count, uint8_t value) {
    uint8_t mask = ((1 << count) - 1) << start;
    *byte = (*byte & ~mask) | ((value << start) & mask);
}

esp_err_t sx_ir_sharp_ac_init_state(sharp_ac_state_t *state, sharp_ac_model_t model) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Default state from IRremoteESP8266
    static const uint8_t default_state[SHARP_AC_STATE_LENGTH] = {
        0xAA, 0x5A, 0xCF, 0x10, 0x00, 0x01, 0x00, 0x00, 0x08, 0x80, 0x00, 0xE0, 0x01
    };
    
    memcpy(state->raw, default_state, SHARP_AC_STATE_LENGTH);
    
    // Set model bits
    switch (model) {
        case SHARP_AC_MODEL_A705:
        case SHARP_AC_MODEL_A903:
            state->raw[4] |= (1 << 4);  // Model bit = 1
            state->raw[11] |= (1 << 4); // Model2 bit = 1
            break;
        case SHARP_AC_MODEL_A907:
        default:
            state->raw[4] &= ~(1 << 4);  // Model bit = 0
            state->raw[11] &= ~(1 << 4);  // Model2 bit = 0
            break;
    }
    
    // Calculate checksum
    return sx_ir_sharp_ac_calc_checksum(state);
}

esp_err_t sx_ir_sharp_ac_set_power(sharp_ac_state_t *state, bool on, bool prev_on) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t power_special;
    if (on) {
        power_special = prev_on ? SHARP_AC_POWER_ON : SHARP_AC_POWER_ON_FROM_OFF;
    } else {
        power_special = SHARP_AC_POWER_OFF;
    }
    
    set_bits(&state->raw[5], 0, 4, power_special);
    state->raw[10] = 0x00; // Special = Power
    
    // Clear clean mode if power is changed
    state->raw[6] &= ~(1 << 3);
    
    return ESP_OK;
}

esp_err_t sx_ir_sharp_ac_set_temp(sharp_ac_state_t *state, uint8_t temp) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Clamp temperature to valid range (15-30°C)
    if (temp < 15) temp = 15;
    if (temp > 30) temp = 30;
    
    // Check if mode allows temp changes
    uint8_t mode = get_bits(state->raw[6], 0, 2);
    if (mode == SHARP_AC_MODE_AUTO || mode == SHARP_AC_MODE_DRY) {
        // Auto and Dry don't allow temp changes
        state->raw[4] = 0x00;
        return ESP_OK;
    }
    
    // Set temp base (0xC0 for A907, 0xD0 for A705)
    uint8_t model = get_bits(state->raw[4], 4, 1);
    state->raw[4] = (model == 0) ? 0xC0 : 0xD0;
    
    // Set temperature offset (temp - 15)
    uint8_t temp_offset = temp - 15;
    set_bits(&state->raw[4], 0, 4, temp_offset);
    
    // Set special to TempEcono
    state->raw[10] = 0x04;
    
    // Clear power special bits (keep only power on/off)
    uint8_t power = get_bits(state->raw[5], 0, 4);
    if (power == SHARP_AC_POWER_ON || power == SHARP_AC_POWER_OFF) {
        // Keep as is
    } else {
        set_bits(&state->raw[5], 0, 4, SHARP_AC_POWER_ON);
    }
    
    return ESP_OK;
}

esp_err_t sx_ir_sharp_ac_set_mode(sharp_ac_state_t *state, sharp_ac_mode_t mode) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t model = get_bits(state->raw[4], 4, 1);
    uint8_t model2 = get_bits(state->raw[11], 4, 1);
    
    // Check if model supports heat mode
    if (mode == SHARP_AC_MODE_HEAT && (model == 1 || model2 == 1)) {
        // A705 and A903 don't support heat, use Fan mode instead
        mode = SHARP_AC_MODE_FAN;
    }
    
    uint8_t real_mode = mode;
    if (mode == SHARP_AC_MODE_FAN && model == 0 && model2 == 0) {
        // A907 doesn't have Fan mode, use Auto
        real_mode = SHARP_AC_MODE_AUTO;
    }
    
    set_bits(&state->raw[6], 0, 2, real_mode);
    
    // Auto and Dry modes force fan to Auto
    if (real_mode == SHARP_AC_MODE_AUTO || real_mode == SHARP_AC_MODE_DRY) {
        set_bits(&state->raw[6], 4, 3, SHARP_AC_FAN_AUTO);
    }
    
    // Update temp (Dry/Auto have no temp setting)
    if (real_mode == SHARP_AC_MODE_AUTO || real_mode == SHARP_AC_MODE_DRY) {
        state->raw[4] = 0x00;
    }
    
    state->raw[10] = 0x00; // Special = Power
    
    return ESP_OK;
}

esp_err_t sx_ir_sharp_ac_set_fan(sharp_ac_state_t *state, sharp_ac_fan_t fan) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check if mode allows fan changes
    uint8_t mode = get_bits(state->raw[6], 0, 2);
    if (mode == SHARP_AC_MODE_AUTO || mode == SHARP_AC_MODE_DRY) {
        // Auto and Dry force fan to Auto
        fan = SHARP_AC_FAN_AUTO;
    }
    
    set_bits(&state->raw[6], 4, 3, fan);
    state->raw[10] = 0x05; // Special = Fan
    
    // Clear power special bits
    uint8_t power = get_bits(state->raw[5], 0, 4);
    if (power == SHARP_AC_POWER_ON || power == SHARP_AC_POWER_OFF) {
        // Keep as is
    } else {
        set_bits(&state->raw[5], 0, 4, SHARP_AC_POWER_ON);
    }
    
    return ESP_OK;
}

esp_err_t sx_ir_sharp_ac_set_swing(sharp_ac_state_t *state, uint8_t swing) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (swing > 7) {
        return ESP_ERR_INVALID_ARG;
    }
    
    set_bits(&state->raw[8], 0, 3, swing);
    state->raw[10] = 0x06; // Special = Swing
    
    return ESP_OK;
}

esp_err_t sx_ir_sharp_ac_set_turbo(sharp_ac_state_t *state, bool on) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (on) {
        // Turbo: Set fan to max and special bits
        set_bits(&state->raw[6], 4, 3, SHARP_AC_FAN_MAX);
        set_bits(&state->raw[5], 0, 4, 0x06); // PowerSpecial = SetSpecialOn
        state->raw[10] = 0x01; // Special = Turbo
    } else {
        // Turn off turbo
        set_bits(&state->raw[5], 0, 4, SHARP_AC_POWER_ON);
        state->raw[10] = 0x00; // Special = Power
    }
    
    return ESP_OK;
}

esp_err_t sx_ir_sharp_ac_calc_checksum(sharp_ac_state_t *state) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Sharp AC checksum: XOR all bytes except last, then XOR with nibbles
    uint8_t xorsum = xor_bytes(state->raw, SHARP_AC_STATE_LENGTH - 1);
    
    // XOR with low nibble of last byte
    uint8_t last_low = get_bits(state->raw[SHARP_AC_STATE_LENGTH - 1], 0, 4);
    xorsum ^= last_low;
    
    // XOR with high nibble of xorsum
    uint8_t xorsum_high = get_bits(xorsum, 4, 4);
    xorsum ^= xorsum_high;
    
    // Set checksum (low nibble of last byte)
    uint8_t checksum = get_bits(xorsum, 0, 4);
    set_bits(&state->raw[SHARP_AC_STATE_LENGTH - 1], 4, 4, checksum);
    
    return ESP_OK;
}

bool sx_ir_sharp_ac_validate_checksum(const sharp_ac_state_t *state) {
    if (state == NULL) {
        return false;
    }
    
    sharp_ac_state_t temp_state;
    memcpy(&temp_state, state, sizeof(sharp_ac_state_t));
    
    // Get current checksum
    uint8_t current_checksum = get_bits(temp_state.raw[SHARP_AC_STATE_LENGTH - 1], 4, 4);
    
    // Calculate expected checksum
    sx_ir_sharp_ac_calc_checksum(&temp_state);
    uint8_t expected_checksum = get_bits(temp_state.raw[SHARP_AC_STATE_LENGTH - 1], 4, 4);
    
    return current_checksum == expected_checksum;
}

esp_err_t sx_ir_sharp_ac_send(const sharp_ac_state_t *state, uint16_t repeat) {
    if (state == NULL || !s_initialized || s_rmt_tx_chan == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Thread safety: acquire mutex
    if (s_ir_mutex != NULL) {
        if (xSemaphoreTake(s_ir_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire IR mutex (timeout)");
            return ESP_ERR_TIMEOUT;
        }
    }
    
    esp_err_t ret = ESP_OK;
    
    // Validate checksum
    if (!sx_ir_sharp_ac_validate_checksum(state)) {
        ESP_LOGW(TAG, "Sharp AC checksum invalid, recalculating...");
        sharp_ac_state_t temp_state;
        memcpy(&temp_state, state, sizeof(sharp_ac_state_t));
        sx_ir_sharp_ac_calc_checksum(&temp_state);
        // Release mutex before recursive call
        if (s_ir_mutex != NULL) {
            xSemaphoreGive(s_ir_mutex);
        }
        return sx_ir_sharp_ac_send(&temp_state, repeat);
    }
    
    // Sharp AC timing constants (from IRremoteESP8266)
    const uint16_t hdr_mark = 3800;
    const uint16_t hdr_space = 1900;
    const uint16_t bit_mark = 470;
    const uint16_t zero_space = 500;
    const uint16_t one_space = 1400;
    const uint32_t gap = 100000; // 100ms gap
    
    // Calculate pulse count: header (2) + 13 bytes * 8 bits * 2 (mark+space) + gap
    size_t pulse_count = 2 + (SHARP_AC_STATE_LENGTH * 8 * 2) + 1;
    if (pulse_count > MAX_IR_PULSES) {
        ESP_LOGE(TAG, "Pulse count %u exceeds maximum %u", (unsigned)pulse_count, MAX_IR_PULSES);
        ret = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }
    
    // Use static buffer instead of dynamic allocation
    uint16_t *pulses = s_pulse_buffer;
    
    size_t idx = 0;
    
    // Header
    pulses[idx++] = hdr_mark;
    pulses[idx++] = hdr_space;
    
    // Encode 13 bytes
    for (int byte_idx = 0; byte_idx < SHARP_AC_STATE_LENGTH; byte_idx++) {
        uint8_t byte = state->raw[byte_idx];
        for (int bit = 7; bit >= 0; bit--) {
            pulses[idx++] = bit_mark;
            if (byte & (1 << bit)) {
                pulses[idx++] = one_space;
            } else {
                pulses[idx++] = zero_space;
            }
        }
    }
    
    // Gap
    // gap là uint32_t (100ms) có thể vượt UINT16_MAX -> clamp để tránh overflow và giữ an toàn
    uint16_t gap_value = (gap > UINT16_MAX) ? UINT16_MAX : (uint16_t)gap;
    pulses[idx++] = gap_value;
    
    // Release mutex before calling sx_ir_send_raw (it will acquire its own)
    if (s_ir_mutex != NULL) {
        xSemaphoreGive(s_ir_mutex);
    }
    
    // Send
    ret = sx_ir_send_raw(pulses, idx);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Sharp AC state sent (repeat=%u)", (unsigned)repeat);
    }
    
    return ret;
    
cleanup:
    // Release mutex on error
    if (s_ir_mutex != NULL) {
        xSemaphoreGive(s_ir_mutex);
    }
    return ret;
}

// ===== Toshiba AC Full Protocol Implementation =====

esp_err_t sx_ir_toshiba_ac_init_state(toshiba_ac_state_t *state, toshiba_ac_model_t model) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Default state from IRremoteESP8266
    memset(state->raw, 0, TOSHIBA_AC_STATE_LENGTH_LONG);
    state->raw[0] = 0xF2;
    state->raw[1] = 0x0D;  // Inverted pair
    state->raw[2] = 0x03;  // Length = 3 (payload after byte 4)
    state->raw[3] = 0xFC;  // Inverted length
    state->raw[4] = 0x01;  // Initial state
    
    state->length = TOSHIBA_AC_STATE_LENGTH;
    
    // Set model
    set_bits(&state->raw[2], 4, 4, model);
    
    // Set default temp to 22°C
    sx_ir_toshiba_ac_set_temp(state, 22);
    
    // Set default mode to Auto
    sx_ir_toshiba_ac_set_mode(state, TOSHIBA_AC_MODE_AUTO);
    
    // Calculate checksum
    return sx_ir_toshiba_ac_calc_checksum(state);
}

esp_err_t sx_ir_toshiba_ac_set_power(toshiba_ac_state_t *state, bool on) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (on) {
        // Power on: set mode to Auto if currently off
        uint8_t mode = get_bits(state->raw[6], 0, 3);
        if (mode == TOSHIBA_AC_MODE_OFF) {
            sx_ir_toshiba_ac_set_mode(state, TOSHIBA_AC_MODE_AUTO);
        }
    } else {
        // Power off: set mode to OFF
        sx_ir_toshiba_ac_set_mode(state, TOSHIBA_AC_MODE_OFF);
    }
    
    return ESP_OK;
}

esp_err_t sx_ir_toshiba_ac_set_temp(toshiba_ac_state_t *state, uint8_t temp) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Clamp temperature to valid range (17-30°C)
    if (temp < 17) temp = 17;
    if (temp > 30) temp = 30;
    
    // Set temperature offset (temp - 17)
    uint8_t temp_offset = temp - 17;
    set_bits(&state->raw[5], 0, 4, temp_offset);
    
    return ESP_OK;
}

esp_err_t sx_ir_toshiba_ac_set_mode(toshiba_ac_state_t *state, toshiba_ac_mode_t mode) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    set_bits(&state->raw[6], 0, 3, mode);
    
    // If mode changed, disable turbo/econo (use normal message length)
    if (state->length == TOSHIBA_AC_STATE_LENGTH_LONG) {
        state->raw[8] = 0x00; // Clear EcoTurbo
        state->length = TOSHIBA_AC_STATE_LENGTH;
        set_bits(&state->raw[2], 0, 4, state->length - TOSHIBA_AC_STATE_LENGTH_SHORT);
        state->raw[3] = ~state->raw[2]; // Inverted length
    }
    
    return ESP_OK;
}

esp_err_t sx_ir_toshiba_ac_set_fan(toshiba_ac_state_t *state, toshiba_ac_fan_t fan) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Toshiba fan encoding: Auto=0, Min=1, Med=3, Max=5
    // But internal encoding: Auto=0, then 2,3,4,5,6 for speeds
    uint8_t fan_encoded = fan;
    if (fan > TOSHIBA_AC_FAN_AUTO) {
        fan_encoded++; // Skip value 2
    }
    
    set_bits(&state->raw[6], 3, 3, fan_encoded);
    
    return ESP_OK;
}

esp_err_t sx_ir_toshiba_ac_set_swing(toshiba_ac_state_t *state, toshiba_ac_swing_t swing) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Swing commands use short message (5 bytes) with temp = min (17°C)
    if (state->length != TOSHIBA_AC_STATE_LENGTH_SHORT) {
        // Save current state
        uint8_t saved_mode = get_bits(state->raw[6], 0, 3);
        uint8_t saved_fan = get_bits(state->raw[6], 3, 3);
        
        // Switch to short message
        state->length = TOSHIBA_AC_STATE_LENGTH_SHORT;
        set_bits(&state->raw[2], 0, 4, 0); // Length = 0 (payload = 1 byte after byte 4)
        state->raw[3] = ~state->raw[2]; // Inverted length
        
        // Set temp to min for swing
        set_bits(&state->raw[5], 0, 4, 0); // Temp = 17°C (offset 0)
        
        // Restore mode and fan
        set_bits(&state->raw[6], 0, 3, saved_mode);
        set_bits(&state->raw[6], 3, 3, saved_fan);
    }
    
    set_bits(&state->raw[5], 4, 3, swing);
    
    return ESP_OK;
}

esp_err_t sx_ir_toshiba_ac_set_turbo(toshiba_ac_state_t *state, bool on) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (on) {
        // Turbo: Use long message
        state->length = TOSHIBA_AC_STATE_LENGTH_LONG;
        set_bits(&state->raw[2], 0, 4, state->length - TOSHIBA_AC_STATE_LENGTH_SHORT);
        state->raw[3] = ~state->raw[2]; // Inverted length
        state->raw[8] = 0x01; // Turbo on
    } else {
        // Turn off turbo: Check if econo is also off
        if (state->raw[8] != 0x03) { // Not econo
            state->length = TOSHIBA_AC_STATE_LENGTH;
            set_bits(&state->raw[2], 0, 4, state->length - TOSHIBA_AC_STATE_LENGTH_SHORT);
            state->raw[3] = ~state->raw[2];
            state->raw[8] = 0x00;
        } else {
            // Keep long message for econo
            state->raw[8] = 0x03;
        }
    }
    
    return ESP_OK;
}

esp_err_t sx_ir_toshiba_ac_set_econo(toshiba_ac_state_t *state, bool on) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (on) {
        // Econo: Use long message
        state->length = TOSHIBA_AC_STATE_LENGTH_LONG;
        set_bits(&state->raw[2], 0, 4, state->length - TOSHIBA_AC_STATE_LENGTH_SHORT);
        state->raw[3] = ~state->raw[2]; // Inverted length
        state->raw[8] = 0x03; // Econo on
    } else {
        // Turn off econo: Check if turbo is also off
        if (state->raw[8] != 0x01) { // Not turbo
            state->length = TOSHIBA_AC_STATE_LENGTH;
            set_bits(&state->raw[2], 0, 4, state->length - TOSHIBA_AC_STATE_LENGTH_SHORT);
            state->raw[3] = ~state->raw[2];
            state->raw[8] = 0x00;
        } else {
            // Keep long message for turbo
            state->raw[8] = 0x01;
        }
    }
    
    return ESP_OK;
}

esp_err_t sx_ir_toshiba_ac_calc_checksum(toshiba_ac_state_t *state) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Toshiba AC checksum: XOR all bytes except last
    uint8_t checksum = xor_bytes(state->raw, state->length - 1);
    state->raw[state->length - 1] = checksum;
    
    // Also need to set inverted byte pairs
    // Byte[0] and Byte[1] are inverted pair
    state->raw[1] = ~state->raw[0];
    // Byte[2] and Byte[3] are inverted pair
    state->raw[3] = ~state->raw[2];
    
    return ESP_OK;
}

bool sx_ir_toshiba_ac_validate_checksum(const toshiba_ac_state_t *state) {
    if (state == NULL) {
        return false;
    }
    
    // Check inverted pairs
    if (state->raw[1] != (uint8_t)~state->raw[0]) {
        return false;
    }
    if (state->raw[3] != (uint8_t)~state->raw[2]) {
        return false;
    }
    
    // Check checksum
    uint8_t expected_checksum = xor_bytes(state->raw, state->length - 1);
    return state->raw[state->length - 1] == expected_checksum;
}

esp_err_t sx_ir_toshiba_ac_send(const toshiba_ac_state_t *state, uint16_t repeat) {
    if (state == NULL || !s_initialized || s_rmt_tx_chan == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Thread safety: acquire mutex
    if (s_ir_mutex != NULL) {
        if (xSemaphoreTake(s_ir_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire IR mutex (timeout)");
            return ESP_ERR_TIMEOUT;
        }
    }
    
    esp_err_t ret = ESP_OK;
    
    // Validate checksum
    if (!sx_ir_toshiba_ac_validate_checksum(state)) {
        ESP_LOGW(TAG, "Toshiba AC checksum invalid, recalculating...");
        toshiba_ac_state_t temp_state;
        memcpy(&temp_state, state, sizeof(toshiba_ac_state_t));
        sx_ir_toshiba_ac_calc_checksum(&temp_state);
        // Release mutex before recursive call
        if (s_ir_mutex != NULL) {
            xSemaphoreGive(s_ir_mutex);
        }
        return sx_ir_toshiba_ac_send(&temp_state, repeat);
    }
    
    // Toshiba AC timing constants (from IRremoteESP8266)
    const uint16_t hdr_mark = 4400;
    const uint16_t hdr_space = 4300;
    const uint16_t bit_mark = 580;
    const uint16_t zero_space = 490;
    const uint16_t one_space = 1600;
    const uint16_t gap = 7400; // 7.4ms gap (or 4.6ms for some models)
    
    // Calculate pulse count: header (2) + state->length bytes * 8 bits * 2 (mark+space) + gap
    size_t pulse_count = 2 + (state->length * 8 * 2) + 1;
    if (pulse_count > MAX_IR_PULSES) {
        ESP_LOGE(TAG, "Pulse count %u exceeds maximum %u", (unsigned)pulse_count, MAX_IR_PULSES);
        ret = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }
    
    // Use static buffer instead of dynamic allocation
    uint16_t *pulses = s_pulse_buffer;
    
    size_t idx = 0;
    
    // Header
    pulses[idx++] = hdr_mark;
    pulses[idx++] = hdr_space;
    
    // Encode bytes
    for (int byte_idx = 0; byte_idx < state->length; byte_idx++) {
        uint8_t byte = state->raw[byte_idx];
        for (int bit = 7; bit >= 0; bit--) {
            pulses[idx++] = bit_mark;
            if (byte & (1 << bit)) {
                pulses[idx++] = one_space;
            } else {
                pulses[idx++] = zero_space;
            }
        }
    }
    
    // Gap
    // gap đã là uint16_t (7.4ms) nên không thể > UINT16_MAX; tránh warning -Wtype-limits
    pulses[idx++] = gap;
    
    // Release mutex before calling sx_ir_send_raw (it will acquire its own)
    if (s_ir_mutex != NULL) {
        xSemaphoreGive(s_ir_mutex);
    }
    
    // Send
    ret = sx_ir_send_raw(pulses, idx);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Toshiba AC state sent (length=%u, repeat=%u)", 
                 (unsigned)state->length, (unsigned)repeat);
    }
    
    return ret;
    
cleanup:
    // Release mutex on error
    if (s_ir_mutex != NULL) {
        xSemaphoreGive(s_ir_mutex);
    }
    return ret;
}

// ===== IR Receive Implementation =====

static bool IRAM_ATTR rmt_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data) {
    BaseType_t must_yield = pdFALSE;
    if (s_rx_queue != NULL) {
        xQueueSendFromISR(s_rx_queue, edata, &must_yield);
    }
    return must_yield == pdTRUE;
}

esp_err_t sx_ir_receive_start(const sx_ir_receive_config_t *config) {
    if (config == NULL || s_rmt_rx_chan == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Thread safety: acquire mutex
    if (s_ir_mutex != NULL) {
        if (xSemaphoreTake(s_ir_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire IR mutex (timeout)");
            return ESP_ERR_TIMEOUT;
        }
    }
    
    if (s_rx_active) {
        if (s_ir_mutex != NULL) {
            xSemaphoreGive(s_ir_mutex);
        }
        return ESP_ERR_INVALID_STATE; // Already receiving
    }
    
    s_rx_config = *config;
    
    // Register callback if provided
    if (config->callback != NULL) {
        rmt_rx_event_callbacks_t cbs = {
            .on_recv_done = rmt_rx_done_callback,
        };
        esp_err_t ret = rmt_rx_register_event_callbacks(s_rmt_rx_chan, &cbs, NULL);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    
    // Enable RX channel
    esp_err_t ret = rmt_enable(s_rmt_rx_chan);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Configure receive
    rmt_receive_config_t rx_cfg = {
        .signal_range_min_ns = 1000,      // 1us minimum pulse
        .signal_range_max_ns = 12000000, // 12ms maximum pulse (for headers)
    };
    
    // Start receiving (continuous)
    ret = rmt_receive(s_rmt_rx_chan, NULL, 0, &rx_cfg);
    if (ret != ESP_OK) {
        rmt_disable(s_rmt_rx_chan);
        return ret;
    }
    
    s_rx_active = true;
    ESP_LOGI(TAG, "IR receive started on GPIO %d", s_cfg.rx_gpio);
    
    // Release mutex
    if (s_ir_mutex != NULL) {
        xSemaphoreGive(s_ir_mutex);
    }
    
    return ESP_OK;
}

esp_err_t sx_ir_receive_stop(void) {
    // Thread safety: acquire mutex
    if (s_ir_mutex != NULL) {
        if (xSemaphoreTake(s_ir_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire IR mutex (timeout)");
            return ESP_ERR_TIMEOUT;
        }
    }
    
    if (!s_rx_active || s_rmt_rx_chan == NULL) {
        if (s_ir_mutex != NULL) {
            xSemaphoreGive(s_ir_mutex);
        }
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = rmt_disable(s_rmt_rx_chan);
    s_rx_active = false;
    
    // Free captured buffer if exists
    if (s_captured_pulses != NULL) {
        free(s_captured_pulses);
        s_captured_pulses = NULL;
        s_captured_count = 0;
    }
    
    ESP_LOGI(TAG, "IR receive stopped");
    
    // Release mutex
    if (s_ir_mutex != NULL) {
        xSemaphoreGive(s_ir_mutex);
    }
    
    return ret;
}

esp_err_t sx_ir_receive_get_last(uint16_t **pulses, size_t *count) {
    if (pulses == NULL || count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_rx_active || s_rx_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Check for received data
    rmt_rx_done_event_data_t rx_data;
    if (xQueueReceive(s_rx_queue, &rx_data, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_NOT_FOUND;
    }
    
    // Convert RMT symbols to pulse durations (microseconds)
    size_t symbol_count = rx_data.num_symbols;
    if (symbol_count == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    
    // Free old buffer if exists
    if (s_captured_pulses != NULL) {
        free(s_captured_pulses);
    }
    
    // Allocate new buffer
    s_captured_pulses = malloc(symbol_count * sizeof(uint16_t));
    if (s_captured_pulses == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    // Convert symbols to pulse durations
    for (size_t i = 0; i < symbol_count; i++) {
        rmt_symbol_word_t sym = rx_data.received_symbols[i];
        // Sum level0 and level1 durations (in ticks, convert to microseconds)
        uint32_t duration_ticks = sym.duration0 + sym.duration1;
        s_captured_pulses[i] = (uint16_t)(duration_ticks); // Already in microseconds (1MHz = 1us per tick)
    }
    
    s_captured_count = symbol_count;
    *pulses = s_captured_pulses;
    *count = s_captured_count;
    
    ESP_LOGI(TAG, "Captured %u IR pulses", (unsigned)symbol_count);
    
    // Restart receiving for next capture
    if (s_rx_active) {
        rmt_receive_config_t rx_cfg = {
            .signal_range_min_ns = 1000,
            .signal_range_max_ns = 12000000,
        };
        rmt_receive(s_rmt_rx_chan, NULL, 0, &rx_cfg);
    }
    
    return ESP_OK;
}

void sx_ir_receive_free_buffer(uint16_t *pulses) {
    if (pulses == s_captured_pulses) {
        free(s_captured_pulses);
        s_captured_pulses = NULL;
        s_captured_count = 0;
    }
}

// Helper: Match pulse pattern (with tolerance)
static bool match_pulse(const uint16_t *pulses, size_t count, size_t idx, uint16_t expected, uint16_t tolerance) {
    if (idx >= count) return false;
    uint16_t actual = pulses[idx];
    return (actual >= expected - tolerance) && (actual <= expected + tolerance);
}

// Helper: Decode bit from pulses
static bool decode_bit(const uint16_t *pulses, size_t count, size_t *idx, uint16_t mark, uint16_t zero_space, uint16_t one_space, uint16_t tolerance) {
    if (*idx + 1 >= count) return false;
    
    // Skip mark
    if (!match_pulse(pulses, count, *idx, mark, tolerance)) {
        return false;
    }
    (*idx)++;
    
    // Check space
    uint16_t space = pulses[*idx];
    (*idx)++;
    
    // Determine bit value
    uint16_t zero_diff = (space > zero_space) ? (space - zero_space) : (zero_space - space);
    uint16_t one_diff = (space > one_space) ? (space - one_space) : (one_space - space);
    
    return one_diff < zero_diff; // Return true for '1', false for '0'
}

esp_err_t sx_ir_decode_nec(const uint16_t *pulses, size_t count, uint8_t *address, uint8_t *command) {
    if (pulses == NULL || address == NULL || command == NULL || count < 68) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // NEC protocol: Header (9ms + 4.5ms) + 32 bits (address, ~address, command, ~command)
    size_t idx = 0;
    const uint16_t tolerance = 200; // 200us tolerance
    
    // Check header
    if (!match_pulse(pulses, count, idx, 9000, tolerance * 10)) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    idx++;
    if (!match_pulse(pulses, count, idx, 4500, tolerance * 10)) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    idx++;
    
    // Decode 32 bits
    uint32_t data = 0;
    for (int i = 0; i < 32; i++) {
        bool bit = decode_bit(pulses, count, &idx, 560, 560, 1690, tolerance);
        data |= (bit ? 1 : 0) << (31 - i);
    }
    
    // Extract address and command
    *address = (data >> 24) & 0xFF;
    uint8_t address_inv = (data >> 16) & 0xFF;
    *command = (data >> 8) & 0xFF;
    uint8_t command_inv = data & 0xFF;
    
    // Verify inverted bytes
    if ((*address ^ address_inv) != 0xFF || (*command ^ command_inv) != 0xFF) {
        ESP_LOGW(TAG, "NEC decode: inverted bytes mismatch");
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    ESP_LOGI(TAG, "NEC decoded: address=0x%02X, command=0x%02X", *address, *command);
    return ESP_OK;
}

esp_err_t sx_ir_decode_sharp_ac(const uint16_t *pulses, size_t count, sharp_ac_state_t *state) {
    if (pulses == NULL || state == NULL || count < 210) { // 2 header + 13 bytes * 16 pulses
        return ESP_ERR_INVALID_ARG;
    }
    
    // Sharp AC: Header (3.8ms + 1.9ms) + 13 bytes
    size_t idx = 0;
    const uint16_t tolerance = 100;
    
    // Check header
    if (!match_pulse(pulses, count, idx, 3800, tolerance * 10)) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    idx++;
    if (!match_pulse(pulses, count, idx, 1900, tolerance * 10)) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    idx++;
    
    // Decode 13 bytes
    memset(state->raw, 0, SHARP_AC_STATE_LENGTH);
    for (int byte_idx = 0; byte_idx < SHARP_AC_STATE_LENGTH; byte_idx++) {
        uint8_t byte = 0;
        for (int bit = 7; bit >= 0; bit--) {
            bool bit_val = decode_bit(pulses, count, &idx, 470, 500, 1400, tolerance);
            byte |= (bit_val ? 1 : 0) << bit;
        }
        state->raw[byte_idx] = byte;
    }
    
    // Validate checksum
    if (!sx_ir_sharp_ac_validate_checksum(state)) {
        ESP_LOGW(TAG, "Sharp AC decode: checksum invalid");
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    ESP_LOGI(TAG, "Sharp AC decoded successfully");
    return ESP_OK;
}

esp_err_t sx_ir_decode_toshiba_ac(const uint16_t *pulses, size_t count, toshiba_ac_state_t *state) {
    if (pulses == NULL || state == NULL || count < 82) { // 2 header + 5 bytes * 16 pulses (minimum)
        return ESP_ERR_INVALID_ARG;
    }
    
    // Toshiba AC: Header (4.4ms + 4.3ms) + variable bytes (5/6/9)
    size_t idx = 0;
    const uint16_t tolerance = 100;
    
    // Check header
    if (!match_pulse(pulses, count, idx, 4400, tolerance * 10)) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    idx++;
    if (!match_pulse(pulses, count, idx, 4300, tolerance * 10)) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    idx++;
    
    // Decode first byte to determine length
    uint8_t first_byte = 0;
    for (int bit = 7; bit >= 0; bit--) {
        bool bit_val = decode_bit(pulses, count, &idx, 580, 490, 1600, tolerance);
        first_byte |= (bit_val ? 1 : 0) << bit;
    }
    
    // Determine length from byte 2 (length nibble)
    // We need to decode byte 2 first
    uint8_t byte2 = 0;
    for (int bit = 7; bit >= 0; bit--) {
        bool bit_val = decode_bit(pulses, count, &idx, 580, 490, 1600, tolerance);
        byte2 |= (bit_val ? 1 : 0) << bit;
    }
    
    uint8_t length_nibble = byte2 & 0x0F;
    state->length = length_nibble + TOSHIBA_AC_STATE_LENGTH_SHORT;
    if (state->length > TOSHIBA_AC_STATE_LENGTH_LONG) {
        state->length = TOSHIBA_AC_STATE_LENGTH_LONG;
    }
    
    // Reset and decode all bytes
    idx = 2; // Skip header, start from byte 0
    memset(state->raw, 0, TOSHIBA_AC_STATE_LENGTH_LONG);
    state->raw[0] = first_byte;
    state->raw[1] = byte2;
    
    // Decode remaining bytes
    for (int byte_idx = 2; byte_idx < state->length; byte_idx++) {
        uint8_t byte = 0;
        for (int bit = 7; bit >= 0; bit--) {
            bool bit_val = decode_bit(pulses, count, &idx, 580, 490, 1600, tolerance);
            byte |= (bit_val ? 1 : 0) << bit;
        }
        state->raw[byte_idx] = byte;
    }
    
    // Validate checksum
    if (!sx_ir_toshiba_ac_validate_checksum(state)) {
        ESP_LOGW(TAG, "Toshiba AC decode: checksum invalid");
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    ESP_LOGI(TAG, "Toshiba AC decoded successfully (length=%u)", (unsigned)state->length);
    return ESP_OK;
}

ir_protocol_t sx_ir_detect_protocol(const uint16_t *pulses, size_t count) {
    if (pulses == NULL || count < 4) {
        return IR_PROTOCOL_RAW;
    }
    
    // Check header to determine protocol
    const uint16_t tolerance = 500;
    
    // NEC: 9ms + 4.5ms
    if (match_pulse(pulses, count, 0, 9000, tolerance * 10) && 
        match_pulse(pulses, count, 1, 4500, tolerance * 10)) {
        return IR_PROTOCOL_NEC;
    }
    
    // Sharp AC: 3.8ms + 1.9ms
    if (match_pulse(pulses, count, 0, 3800, tolerance * 10) && 
        match_pulse(pulses, count, 1, 1900, tolerance * 10)) {
        return IR_PROTOCOL_SHARP_AC;
    }
    
    // Toshiba AC: 4.4ms + 4.3ms
    if (match_pulse(pulses, count, 0, 4400, tolerance * 10) && 
        match_pulse(pulses, count, 1, 4300, tolerance * 10)) {
        return IR_PROTOCOL_TOSHIBA_AC;
    }
    
    return IR_PROTOCOL_RAW;
}

