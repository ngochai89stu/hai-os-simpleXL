#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Audio protocol bridge service
// Kết nối audio service với protocol layer để gửi/nhận audio

// Initialize audio protocol bridge
esp_err_t sx_audio_protocol_bridge_init(void);

// Start audio protocol bridge
esp_err_t sx_audio_protocol_bridge_start(void);

// Stop audio protocol bridge
esp_err_t sx_audio_protocol_bridge_stop(void);

// Enable/disable audio sending (recording -> protocol)
esp_err_t sx_audio_protocol_bridge_enable_send(bool enable);

// Enable/disable audio receiving (protocol -> playback)
esp_err_t sx_audio_protocol_bridge_enable_receive(bool enable);

// Check if sending is enabled
bool sx_audio_protocol_bridge_is_sending_enabled(void);

// Check if receiving is enabled
bool sx_audio_protocol_bridge_is_receiving_enabled(void);

#ifdef __cplusplus
}
#endif

