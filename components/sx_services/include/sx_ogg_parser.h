#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// OGG Parser Service
// Parse OGG/Opus files and extract Opus packets

// OGG page header structure
typedef struct {
    uint8_t capture_pattern[4];  // "OggS"
    uint8_t version;
    uint8_t header_type;
    uint64_t granule_position;
    uint32_t serial_number;
    uint32_t page_sequence;
    uint32_t checksum;
    uint8_t page_segments;
    uint8_t segment_table[255];
} sx_ogg_page_header_t;

// OGG parser handle (opaque)
typedef struct sx_ogg_parser sx_ogg_parser_t;

// Initialize OGG parser
// parser: Output parser handle
// Returns: ESP_OK on success
esp_err_t sx_ogg_parser_init(sx_ogg_parser_t **parser);

// Deinitialize OGG parser
// parser: Parser handle
void sx_ogg_parser_deinit(sx_ogg_parser_t *parser);

// Parse OGG page header
// data: OGG page data (at least 27 bytes)
// data_size: Size of data
// header: Output parsed header
// Returns: ESP_OK on success
esp_err_t sx_ogg_parser_parse_page_header(const uint8_t *data, size_t data_size,
                                          sx_ogg_page_header_t *header);

// Read OGG page from file
// parser: Parser handle
// file: File handle (FILE*)
// header: Output parsed header
// page_data: Output page data buffer
// page_size: Input/output - buffer size / actual page size
// Returns: ESP_OK on success
esp_err_t sx_ogg_parser_read_page(sx_ogg_parser_t *parser, void *file,
                                  sx_ogg_page_header_t *header,
                                  uint8_t *page_data, size_t *page_size);

// Extract Opus packets from OGG page
// page_data: OGG page data
// page_size: Size of page data
// opus_packets: Output array of Opus packet pointers
// packet_sizes: Output array of packet sizes
// max_packets: Maximum number of packets
// packet_count: Output actual number of packets
// Returns: ESP_OK on success
esp_err_t sx_ogg_parser_extract_opus_packets(const uint8_t *page_data, size_t page_size,
                                             const uint8_t **opus_packets, size_t *packet_sizes,
                                             size_t max_packets, size_t *packet_count);

// Check if file is OGG format
// file: File handle (FILE*)
// Returns: true if OGG format
bool sx_ogg_parser_is_ogg_file(void *file);

#ifdef __cplusplus
}
#endif











