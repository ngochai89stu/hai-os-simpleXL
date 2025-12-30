#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include "sx_codec_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Phase 5: Hardware Volume Control

// Hardware codec chip types are defined in sx_codec_common.h

// Detect hardware codec chip
sx_hw_codec_chip_t sx_platform_detect_codec(void);

// Initialize hardware volume control
esp_err_t sx_platform_hw_volume_init(void);

// Set hardware volume (0-100)
esp_err_t sx_platform_hw_volume_set(uint8_t volume);

// Get hardware volume (0-100)
uint8_t sx_platform_hw_volume_get(void);

// Check if hardware volume is available
bool sx_platform_hw_volume_available(void);

#ifdef __cplusplus
}
#endif
