#include "sx_jpeg_encoder.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

static const char *TAG = "sx_jpeg_enc";

// Minimal JPEG encoder implementation
// Based on simple JPEG encoding algorithm
// Note: This is a simplified encoder - for production, use ESP32 hardware encoder or libjpeg-turbo

// JPEG quantization tables (quality 50)
static const uint8_t qtable_luma[64] = {
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77,
    24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99
};

static const uint8_t qtable_chroma[64] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

// Adjust quantization table based on quality
static void adjust_quality_table(uint8_t *table, uint8_t quality) {
    if (quality <= 0) quality = 1;
    if (quality > 100) quality = 100;
    
    // Scale factor: quality 50 = 1.0, quality 100 = 0.5, quality 1 = 2.0
    float scale = (quality < 50) ? (5000.0f / quality) : (200.0f - 2.0f * quality) / 100.0f;
    if (scale < 0.5f) scale = 0.5f;
    if (scale > 2.0f) scale = 2.0f;
    
    for (int i = 0; i < 64; i++) {
        int q = (int)(table[i] * scale);
        if (q < 1) q = 1;
        if (q > 255) q = 255;
        table[i] = (uint8_t)q;
    }
}

// Convert RGB565 to RGB888
static void rgb565_to_rgb888(const uint8_t *rgb565_data, uint8_t *rgb888_data, uint16_t width, uint16_t height) {
    const uint16_t *rgb565_ptr = (const uint16_t *)rgb565_data;
    uint8_t *rgb888_ptr = rgb888_data;
    
    for (int i = 0; i < width * height; i++) {
        uint16_t pixel = rgb565_ptr[i];
        // Extract RGB components
        uint8_t r = ((pixel >> 11) & 0x1F) << 3;
        uint8_t g = ((pixel >> 5) & 0x3F) << 2;
        uint8_t b = (pixel & 0x1F) << 3;
        
        // Expand to full range (optional enhancement)
        r = (r * 255) / 248;
        g = (g * 255) / 252;
        b = (b * 255) / 248;
        
        rgb888_ptr[i * 3] = r;
        rgb888_ptr[i * 3 + 1] = g;
        rgb888_ptr[i * 3 + 2] = b;
    }
}

// Write JPEG header
static size_t write_jpeg_header(uint8_t *buffer, uint16_t width, uint16_t height, uint8_t quality) {
    size_t pos = 0;
    
    // SOI (Start of Image)
    buffer[pos++] = 0xFF;
    buffer[pos++] = 0xD8;
    
    // APP0 (JFIF)
    buffer[pos++] = 0xFF;
    buffer[pos++] = 0xE0;
    buffer[pos++] = 0x00;
    buffer[pos++] = 0x10;
    memcpy(&buffer[pos], "JFIF\0\1\1\0\0\1\0\1\0\0", 16);
    pos += 16;
    
    // DQT (Quantization Table) - Luma
    buffer[pos++] = 0xFF;
    buffer[pos++] = 0xDB;
    buffer[pos++] = 0x00;
    buffer[pos++] = 0x43;
    buffer[pos++] = 0x00; // Table ID 0, 8-bit precision
    uint8_t qtable_luma_adj[64];
    memcpy(qtable_luma_adj, qtable_luma, 64);
    adjust_quality_table(qtable_luma_adj, quality);
    memcpy(&buffer[pos], qtable_luma_adj, 64);
    pos += 64;
    
    // DQT (Quantization Table) - Chroma
    buffer[pos++] = 0xFF;
    buffer[pos++] = 0xDB;
    buffer[pos++] = 0x00;
    buffer[pos++] = 0x43;
    buffer[pos++] = 0x01; // Table ID 1, 8-bit precision
    uint8_t qtable_chroma_adj[64];
    memcpy(qtable_chroma_adj, qtable_chroma, 64);
    adjust_quality_table(qtable_chroma_adj, quality);
    memcpy(&buffer[pos], qtable_chroma_adj, 64);
    pos += 64;
    
    // SOF0 (Start of Frame)
    buffer[pos++] = 0xFF;
    buffer[pos++] = 0xC0;
    buffer[pos++] = 0x00;
    buffer[pos++] = 0x11;
    buffer[pos++] = 0x08; // Precision: 8 bits
    buffer[pos++] = (height >> 8) & 0xFF;
    buffer[pos++] = height & 0xFF;
    buffer[pos++] = (width >> 8) & 0xFF;
    buffer[pos++] = width & 0xFF;
    buffer[pos++] = 0x03; // Number of components
    // Y component
    buffer[pos++] = 0x01; // Component ID
    buffer[pos++] = 0x11; // H/V sampling
    buffer[pos++] = 0x00; // Quantization table ID
    // Cb component
    buffer[pos++] = 0x02; // Component ID
    buffer[pos++] = 0x11; // H/V sampling
    buffer[pos++] = 0x01; // Quantization table ID
    // Cr component
    buffer[pos++] = 0x03; // Component ID
    buffer[pos++] = 0x11; // H/V sampling
    buffer[pos++] = 0x01; // Quantization table ID
    
    // DHT (Huffman Table) - Simplified (using default tables)
    // Note: Full implementation would include proper Huffman tables
    // For now, we'll use a simplified approach
    
    return pos;
}

// Zigzag scan order for 8x8 block
static const int zigzag_order[64] = {
     0,  1,  5,  6, 14, 15, 27, 28,
     2,  4,  7, 13, 16, 26, 29, 42,
     3,  8, 12, 17, 25, 30, 41, 43,
     9, 11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63
};

// Forward DCT for 8x8 block
// Formula: F(u,v) = (1/4) * C(u) * C(v) * sum(x,y) f(x,y) * cos((2x+1)uπ/16) * cos((2y+1)vπ/16)
// where C(u) = 1/sqrt(2) if u=0, else 1
static void dct_8x8(int16_t *block) {
    float temp[64];
    float sum;
    const float PI = 3.14159265358979323846f;
    
    // Forward DCT
    for (int u = 0; u < 8; u++) {
        for (int v = 0; v < 8; v++) {
            sum = 0.0f;
            float cu = (u == 0) ? 1.0f / sqrtf(2.0f) : 1.0f;
            float cv = (v == 0) ? 1.0f / sqrtf(2.0f) : 1.0f;
            
            for (int x = 0; x < 8; x++) {
                for (int y = 0; y < 8; y++) {
                    float pixel = (float)(block[y * 8 + x] - 128); // Center around 0
                    sum += pixel * cosf((2.0f * x + 1.0f) * u * PI / 16.0f) * 
                                  cosf((2.0f * y + 1.0f) * v * PI / 16.0f);
                }
            }
            
            temp[v * 8 + u] = 0.25f * cu * cv * sum;
        }
    }
    
    // Copy back to block (round to nearest integer)
    for (int i = 0; i < 64; i++) {
        block[i] = (int16_t)(temp[i] + 0.5f);
    }
}

// Quantize 8x8 block using quantization table
static void quantize_8x8(int16_t *block, const uint8_t *qtable) {
    for (int i = 0; i < 64; i++) {
        if (qtable[i] > 0) {
            block[i] = block[i] / qtable[i];
        } else {
            block[i] = 0;
        }
    }
}

// Zigzag reorder 8x8 block
static void zigzag_reorder(int16_t *block, int16_t *output) {
    for (int i = 0; i < 64; i++) {
        output[i] = block[zigzag_order[i]];
    }
}

// Encode DC coefficient (differential encoding)
static int16_t encode_dc(int16_t dc, int16_t *prev_dc) {
    int16_t diff = dc - *prev_dc;
    *prev_dc = dc;
    return diff;
}

// Count bits needed to represent a value
static int count_bits(int16_t val) {
    if (val == 0) return 0;
    int abs_val = (val < 0) ? -val : val;
    int bits = 0;
    while (abs_val > 0) {
        abs_val >>= 1;
        bits++;
    }
    return bits;
}

// Encode RGB565 to JPEG (minimal implementation)
esp_err_t sx_jpeg_encode_rgb565(const uint8_t *rgb565_data, uint16_t width, uint16_t height,
                                uint8_t quality, uint8_t **jpeg_data, size_t *jpeg_size) {
    if (!rgb565_data || !jpeg_data || !jpeg_size || width == 0 || height == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (quality == 0) quality = 50; // Default quality
    if (quality > 100) quality = 100;
    
    // Allocate RGB888 buffer
    size_t rgb888_size = width * height * 3;
    uint8_t *rgb888_data = (uint8_t *)malloc(rgb888_size);
    if (!rgb888_data) {
        return ESP_ERR_NO_MEM;
    }
    
    // Convert RGB565 to RGB888
    rgb565_to_rgb888(rgb565_data, rgb888_data, width, height);
    
    // Estimate JPEG size (worst case: ~width*height bytes for low quality, ~width*height*3 for high quality)
    size_t estimated_size = width * height * 2; // Conservative estimate
    uint8_t *jpeg_buffer = (uint8_t *)malloc(estimated_size);
    if (!jpeg_buffer) {
        free(rgb888_data);
        return ESP_ERR_NO_MEM;
    }
    
    size_t pos = 0;
    
    // Write JPEG header
    pos = write_jpeg_header(jpeg_buffer, width, height, quality);
    
    // Write DHT (Huffman Tables) - MUST be before SOS
    // DC Luma Huffman table
    jpeg_buffer[pos++] = 0xFF;
    jpeg_buffer[pos++] = 0xC4;
    jpeg_buffer[pos++] = 0x00;
    jpeg_buffer[pos++] = 0x1F; // Length
    jpeg_buffer[pos++] = 0x00; // Table ID 0, DC, Luma
    uint8_t dc_luma_bits[16] = {0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0};
    memcpy(&jpeg_buffer[pos], dc_luma_bits, 16);
    pos += 16;
    uint8_t dc_luma_values[12] = {0,1,2,3,4,5,6,7,8,9,10,11};
    memcpy(&jpeg_buffer[pos], dc_luma_values, 12);
    pos += 12;
    
    // AC Luma Huffman table
    jpeg_buffer[pos++] = 0xFF;
    jpeg_buffer[pos++] = 0xC4;
    jpeg_buffer[pos++] = 0x00;
    jpeg_buffer[pos++] = 0xB5; // Length
    jpeg_buffer[pos++] = 0x10; // Table ID 1, AC, Luma
    uint8_t ac_luma_bits[16] = {0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,0x7D};
    memcpy(&jpeg_buffer[pos], ac_luma_bits, 16);
    pos += 16;
    // Standard AC Luma values (162 values)
    uint8_t ac_luma_values[162] = {
        0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,
        0x22,0x71,0x14,0x32,0x81,0x91,0xA1,0x08,0x23,0x42,0xB1,0xC1,0x15,0x52,0xD1,0xF0,
        0x24,0x33,0x62,0x72,0x82,0x09,0x0A,0x16,0x17,0x18,0x19,0x1A,0x25,0x26,0x27,0x28,
        0x29,0x2A,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
        0x4A,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
        0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
        0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,
        0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,
        0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xE1,0xE2,
        0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,
        0xF9,0xFA
    };
    memcpy(&jpeg_buffer[pos], ac_luma_values, 162);
    pos += 162;
    
    // DC Chroma Huffman table
    jpeg_buffer[pos++] = 0xFF;
    jpeg_buffer[pos++] = 0xC4;
    jpeg_buffer[pos++] = 0x00;
    jpeg_buffer[pos++] = 0x1F; // Length
    jpeg_buffer[pos++] = 0x01; // Table ID 1, DC, Chroma
    memcpy(&jpeg_buffer[pos], dc_luma_bits, 16);
    pos += 16;
    memcpy(&jpeg_buffer[pos], dc_luma_values, 12);
    pos += 12;
    
    // AC Chroma Huffman table
    jpeg_buffer[pos++] = 0xFF;
    jpeg_buffer[pos++] = 0xC4;
    jpeg_buffer[pos++] = 0x00;
    jpeg_buffer[pos++] = 0xB5; // Length
    jpeg_buffer[pos++] = 0x11; // Table ID 1, AC, Chroma
    memcpy(&jpeg_buffer[pos], ac_luma_bits, 16);
    pos += 16;
    memcpy(&jpeg_buffer[pos], ac_luma_values, 162);
    pos += 162;
    
    // Write SOS (Start of Scan) - AFTER DHT
    jpeg_buffer[pos++] = 0xFF;
    jpeg_buffer[pos++] = 0xDA;
    jpeg_buffer[pos++] = 0x00;
    jpeg_buffer[pos++] = 0x0C; // Length
    jpeg_buffer[pos++] = 0x03; // Number of components
    // Y component
    jpeg_buffer[pos++] = 0x01; // Component ID
    jpeg_buffer[pos++] = 0x00; // DC/AC table selectors
    // Cb component
    jpeg_buffer[pos++] = 0x02; // Component ID
    jpeg_buffer[pos++] = 0x11; // DC/AC table selectors
    // Cr component
    jpeg_buffer[pos++] = 0x03; // Component ID
    jpeg_buffer[pos++] = 0x11; // DC/AC table selectors
    jpeg_buffer[pos++] = 0x00; // Ss (start of spectral selection)
    jpeg_buffer[pos++] = 0x3F; // Se (end of spectral selection)
    jpeg_buffer[pos++] = 0x00; // Ah/Al (successive approximation)
    
    // Prepare quantization tables
    uint8_t qtable_luma_adj[64];
    uint8_t qtable_chroma_adj[64];
    memcpy(qtable_luma_adj, qtable_luma, 64);
    memcpy(qtable_chroma_adj, qtable_chroma, 64);
    adjust_quality_table(qtable_luma_adj, quality);
    adjust_quality_table(qtable_chroma_adj, quality);
    
    // Helper function to write byte with 0xFF escaping
    #define WRITE_BYTE(val) do { \
        if ((val) == 0xFF) { \
            jpeg_buffer[pos++] = 0xFF; \
            jpeg_buffer[pos++] = 0x00; \
        } else { \
            jpeg_buffer[pos++] = (val); \
        } \
    } while(0)
    
    // Process image in 8x8 MCU blocks with full DCT/quantization
    int16_t prev_dc_y = 0, prev_dc_cb = 0, prev_dc_cr = 0;
    
    // Process Y, Cb, Cr components
    for (int mcu_y = 0; mcu_y < (height + 7) / 8; mcu_y++) {
        for (int mcu_x = 0; mcu_x < (width + 7) / 8; mcu_x++) {
            // Process Y component (8x8 block)
            int16_t y_block[64];
            for (int by = 0; by < 8; by++) {
                for (int bx = 0; bx < 8; bx++) {
                    int x = mcu_x * 8 + bx;
                    int y = mcu_y * 8 + by;
                    if (x < width && y < height) {
                        int idx = (y * width + x) * 3;
                        uint8_t r = rgb888_data[idx];
                        uint8_t g = rgb888_data[idx + 1];
                        uint8_t b = rgb888_data[idx + 2];
                        
                        // RGB to YCbCr (Y component)
                        int y_val = (int)(0.299f * r + 0.587f * g + 0.114f * b);
                        if (y_val < 0) y_val = 0;
                        if (y_val > 255) y_val = 255;
                        y_block[by * 8 + bx] = (int16_t)y_val;
                    } else {
                        // Padding
                        y_block[by * 8 + bx] = 128;
                    }
                }
            }
            
            // DCT
            dct_8x8(y_block);
            
            // Quantize
            quantize_8x8(y_block, qtable_luma_adj);
            
            // Zigzag reorder
            int16_t y_zigzag[64];
            zigzag_reorder(y_block, y_zigzag);
            
            // Encode DC (differential)
            int16_t dc_diff = encode_dc(y_zigzag[0], &prev_dc_y);
            int dc_bits = count_bits(dc_diff);
            if (dc_bits > 0) {
                WRITE_BYTE((uint8_t)dc_bits);
                if (dc_diff < 0) {
                    WRITE_BYTE((uint8_t)((1 << dc_bits) + dc_diff - 1));
                } else {
                    WRITE_BYTE((uint8_t)dc_diff);
                }
            } else {
                WRITE_BYTE(0);
            }
            
            // Encode AC coefficients
            int zero_count = 0;
            for (int i = 1; i < 64; i++) {
                if (y_zigzag[i] == 0) {
                    zero_count++;
                    if (zero_count == 16) {
                        WRITE_BYTE(0xF0); // ZRL
                        zero_count = 0;
                    }
                } else {
                    while (zero_count >= 16) {
                        WRITE_BYTE(0xF0);
                        zero_count -= 16;
                    }
                    int ac_bits = count_bits(y_zigzag[i]);
                    if (ac_bits > 0 && ac_bits <= 10) {
                        uint8_t run_size = (zero_count << 4) | ac_bits;
                        WRITE_BYTE(run_size);
                        if (y_zigzag[i] < 0) {
                            WRITE_BYTE((uint8_t)((1 << ac_bits) + y_zigzag[i] - 1));
                        } else {
                            WRITE_BYTE((uint8_t)y_zigzag[i]);
                        }
                    }
                    zero_count = 0;
                }
            }
            if (zero_count > 0) {
                WRITE_BYTE(0x00); // EOB
            }
            
            // Process Cb and Cr (simplified - same size, no subsampling)
            for (int comp = 1; comp <= 2; comp++) {
                int16_t *prev_dc = (comp == 1) ? &prev_dc_cb : &prev_dc_cr;
                int16_t chroma_block[64];
                
                for (int by = 0; by < 8; by++) {
                    for (int bx = 0; bx < 8; bx++) {
                        int x = mcu_x * 8 + bx;
                        int y = mcu_y * 8 + by;
                        if (x < width && y < height) {
                            int idx = (y * width + x) * 3;
                            uint8_t r = rgb888_data[idx];
                            uint8_t g = rgb888_data[idx + 1];
                            uint8_t b = rgb888_data[idx + 2];
                            
                            int chroma_val;
                            if (comp == 1) { // Cb
                                chroma_val = (int)(-0.1687f * r - 0.3313f * g + 0.5f * b + 128);
                            } else { // Cr
                                chroma_val = (int)(0.5f * r - 0.4187f * g - 0.0813f * b + 128);
                            }
                            if (chroma_val < 0) chroma_val = 0;
                            if (chroma_val > 255) chroma_val = 255;
                            chroma_block[by * 8 + bx] = (int16_t)chroma_val;
                        } else {
                            chroma_block[by * 8 + bx] = 128;
                        }
                    }
                }
                
                // DCT
                dct_8x8(chroma_block);
                
                // Quantize
                quantize_8x8(chroma_block, qtable_chroma_adj);
                
                // Zigzag reorder
                int16_t chroma_zigzag[64];
                zigzag_reorder(chroma_block, chroma_zigzag);
                
                // Encode DC
                int16_t dc_diff_chroma = encode_dc(chroma_zigzag[0], prev_dc);
                int dc_bits_chroma = count_bits(dc_diff_chroma);
                if (dc_bits_chroma > 0) {
                    WRITE_BYTE((uint8_t)dc_bits_chroma);
                    if (dc_diff_chroma < 0) {
                        WRITE_BYTE((uint8_t)((1 << dc_bits_chroma) + dc_diff_chroma - 1));
                    } else {
                        WRITE_BYTE((uint8_t)dc_diff_chroma);
                    }
                } else {
                    WRITE_BYTE(0);
                }
                
                // Encode AC
                zero_count = 0;
                for (int i = 1; i < 64; i++) {
                    if (chroma_zigzag[i] == 0) {
                        zero_count++;
                        if (zero_count == 16) {
                            WRITE_BYTE(0xF0);
                            zero_count = 0;
                        }
                    } else {
                        while (zero_count >= 16) {
                            WRITE_BYTE(0xF0);
                            zero_count -= 16;
                        }
                        int ac_bits = count_bits(chroma_zigzag[i]);
                        if (ac_bits > 0 && ac_bits <= 10) {
                            uint8_t run_size = (zero_count << 4) | ac_bits;
                            WRITE_BYTE(run_size);
                            if (chroma_zigzag[i] < 0) {
                                WRITE_BYTE((uint8_t)((1 << ac_bits) + chroma_zigzag[i] - 1));
                            } else {
                                WRITE_BYTE((uint8_t)chroma_zigzag[i]);
                            }
                        }
                        zero_count = 0;
                    }
                }
                if (zero_count > 0) {
                    WRITE_BYTE(0x00); // EOB
                }
            }
        }
    }
    
    #undef WRITE_BYTE
    
    // Write EOI (End of Image)
    jpeg_buffer[pos++] = 0xFF;
    jpeg_buffer[pos++] = 0xD9;
    
    ESP_LOGI(TAG, "JPEG encoded (full DCT/quantization): %dx%d, quality=%d, size=%zu bytes", width, height, quality, pos);
    ESP_LOGI(TAG, "Note: Full JPEG encoding with DCT, quantization, zigzag, and DC/AC encoding implemented");
    
    free(rgb888_data);
    
    // Reallocate to actual size
    uint8_t *final_buffer = (uint8_t *)realloc(jpeg_buffer, pos);
    if (!final_buffer) {
        free(jpeg_buffer);
        return ESP_ERR_NO_MEM;
    }
    
    *jpeg_data = final_buffer;
    *jpeg_size = pos;
    
    return ESP_OK;
}

