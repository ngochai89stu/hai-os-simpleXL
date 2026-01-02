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

// Update frame duration from server hello message (optimization: dynamic frame duration)
esp_err_t sx_audio_protocol_bridge_update_frame_duration(uint32_t frame_duration_ms);

// Get error statistics (optimization: error monitoring)
typedef struct {
    uint32_t send_errors;        // Number of send failures
    uint32_t receive_drops;      // Number of dropped packets (queue full)
    uint32_t decode_errors;     // Number of decode failures
} sx_audio_bridge_stats_t;
sx_audio_bridge_stats_t sx_audio_protocol_bridge_get_stats(void);
void sx_audio_protocol_bridge_reset_stats(void);

#ifdef __cplusplus
}
#endif

