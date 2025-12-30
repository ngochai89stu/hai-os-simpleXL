#pragma once

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Audio stream packet structure (C version)
typedef struct {
    uint32_t sample_rate;      // Sample rate in Hz
    uint32_t frame_duration;   // Frame duration in milliseconds
    uint32_t timestamp;        // Timestamp in milliseconds
    uint8_t *payload;          // Opus encoded audio data
    size_t payload_size;       // Payload size in bytes
} sx_audio_stream_packet_t;

// Binary protocol v2 structure (network byte order)
typedef struct __attribute__((packed)) {
    uint16_t version;          // Protocol version (network byte order)
    uint16_t type;             // Message type (0: OPUS, 1: JSON)
    uint32_t reserved;         // Reserved for future use
    uint32_t timestamp;        // Timestamp in milliseconds (network byte order)
    uint32_t payload_size;     // Payload size in bytes (network byte order)
    uint8_t payload[];         // Payload data
} sx_binary_protocol_v2_t;

// Binary protocol v3 structure
typedef struct __attribute__((packed)) {
    uint8_t type;              // Message type
    uint8_t reserved;          // Reserved
    uint16_t payload_size;      // Payload size in bytes (network byte order)
    uint8_t payload[];         // Payload data
} sx_binary_protocol_v3_t;

// Audio callback function type
typedef void (*sx_protocol_audio_callback_t)(const sx_audio_stream_packet_t *packet);

#ifdef __cplusplus
}
#endif

