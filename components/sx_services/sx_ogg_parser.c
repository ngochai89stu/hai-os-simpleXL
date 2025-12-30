#include "sx_ogg_parser.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "sx_ogg_parser";

// OGG parser structure
struct sx_ogg_parser {
    uint32_t serial_number;
    uint32_t page_sequence;
    bool initialized;
};

// OGG capture pattern
static const uint8_t OGG_CAPTURE_PATTERN[4] = {'O', 'g', 'g', 'S'};

esp_err_t sx_ogg_parser_init(sx_ogg_parser_t **parser) {
    if (!parser) {
        return ESP_ERR_INVALID_ARG;
    }
    
    sx_ogg_parser_t *new_parser = (sx_ogg_parser_t *)malloc(sizeof(sx_ogg_parser_t));
    if (!new_parser) {
        return ESP_ERR_NO_MEM;
    }
    
    memset(new_parser, 0, sizeof(*new_parser));
    new_parser->initialized = true;
    
    *parser = new_parser;
    ESP_LOGI(TAG, "OGG parser initialized");
    return ESP_OK;
}

void sx_ogg_parser_deinit(sx_ogg_parser_t *parser) {
    if (!parser) {
        return;
    }
    
    free(parser);
    ESP_LOGI(TAG, "OGG parser deinitialized");
}

esp_err_t sx_ogg_parser_parse_page_header(const uint8_t *data, size_t data_size,
                                          sx_ogg_page_header_t *header) {
    if (!data || !header || data_size < 27) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check capture pattern
    if (memcmp(data, OGG_CAPTURE_PATTERN, 4) != 0) {
        return ESP_ERR_INVALID_STATE;
    }
    
    memset(header, 0, sizeof(*header));
    
    // Parse header
    memcpy(header->capture_pattern, data, 4);
    header->version = data[4];
    header->header_type = data[5];
    
    // Granule position (64-bit little-endian)
    header->granule_position = ((uint64_t)data[6]) |
                               ((uint64_t)data[7] << 8) |
                               ((uint64_t)data[8] << 16) |
                               ((uint64_t)data[9] << 24) |
                               ((uint64_t)data[10] << 32) |
                               ((uint64_t)data[11] << 40) |
                               ((uint64_t)data[12] << 48) |
                               ((uint64_t)data[13] << 56);
    
    // Serial number (32-bit little-endian)
    header->serial_number = data[14] | (data[15] << 8) | (data[16] << 16) | (data[17] << 24);
    
    // Page sequence (32-bit little-endian)
    header->page_sequence = data[18] | (data[19] << 8) | (data[20] << 16) | (data[21] << 24);
    
    // Checksum (32-bit little-endian)
    header->checksum = data[22] | (data[23] << 8) | (data[24] << 16) | (data[25] << 24);
    
    // Page segments
    header->page_segments = data[26];
    
    // Read segment table
    if (data_size < 27 + header->page_segments) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    memcpy(header->segment_table, data + 27, header->page_segments);
    
    return ESP_OK;
}

esp_err_t sx_ogg_parser_read_page(sx_ogg_parser_t *parser, void *file,
                                  sx_ogg_page_header_t *header,
                                  uint8_t *page_data, size_t *page_size) {
    if (!parser || !file || !header || !page_data || !page_size) {
        return ESP_ERR_INVALID_ARG;
    }
    
    FILE *f = (FILE *)file;
    
    // Read page header (27 bytes + segment table)
    uint8_t header_buf[27 + 255];
    size_t header_read = fread(header_buf, 1, 27, f);
    if (header_read != 27) {
        return ESP_FAIL;
    }
    
    // Parse header
    esp_err_t ret = sx_ogg_parser_parse_page_header(header_buf, 27, header);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Read segment table
    if (header->page_segments > 0) {
        size_t seg_read = fread(header_buf + 27, 1, header->page_segments, f);
        if (seg_read != header->page_segments) {
            return ESP_FAIL;
        }
        memcpy(header->segment_table, header_buf + 27, header->page_segments);
    }
    
    // Calculate total page data size
    size_t total_size = 0;
    for (uint8_t i = 0; i < header->page_segments; i++) {
        total_size += header->segment_table[i];
    }
    
    if (total_size > *page_size) {
        *page_size = total_size;
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Read page data
    size_t data_read = fread(page_data, 1, total_size, f);
    if (data_read != total_size) {
        return ESP_FAIL;
    }
    
    *page_size = total_size;
    
    // Update parser state
    parser->serial_number = header->serial_number;
    parser->page_sequence = header->page_sequence;
    
    return ESP_OK;
}

esp_err_t sx_ogg_parser_extract_opus_packets(const uint8_t *page_data, size_t page_size,
                                             const uint8_t **opus_packets, size_t *packet_sizes,
                                             size_t max_packets, size_t *packet_count) {
    if (!page_data || !opus_packets || !packet_sizes || !packet_count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // OGG pages contain one or more packets
    // Packets can span multiple segments
    // We need to reconstruct packets from segments
    
    size_t offset = 0;
    size_t packet_idx = 0;
    
    while (offset < page_size && packet_idx < max_packets) {
        // Find packet start (Opus packets start with TOC byte)
        // For simplicity, we'll treat each segment as a potential packet
        // In practice, we'd need to parse the OGG packet structure more carefully
        
        if (offset < page_size) {
            opus_packets[packet_idx] = page_data + offset;
            
            // Try to find packet end (this is simplified)
            // Real implementation would need to parse OGG packet structure
            size_t packet_size = page_size - offset;
            if (packet_size > 0) {
                packet_sizes[packet_idx] = packet_size;
                offset += packet_size;
                packet_idx++;
            } else {
                break;
            }
        } else {
            break;
        }
    }
    
    *packet_count = packet_idx;
    
    if (packet_idx == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    
    return ESP_OK;
}

bool sx_ogg_parser_is_ogg_file(void *file) {
    if (!file) {
        return false;
    }
    
    FILE *f = (FILE *)file;
    long pos = ftell(f);
    
    uint8_t header[4];
    if (fread(header, 1, 4, f) != 4) {
        fseek(f, pos, SEEK_SET);
        return false;
    }
    
    fseek(f, pos, SEEK_SET);
    
    return memcmp(header, OGG_CAPTURE_PATTERN, 4) == 0;
}




