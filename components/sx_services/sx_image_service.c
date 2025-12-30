#include "sx_image_service.h"
#include "sx_sd_service.h"

#include <esp_log.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>

// Include LVGL for image descriptor
#include "lvgl.h"

// Include lodepng if available (from LVGL)
#if LV_USE_LODEPNG
#include "../../managed_components/lvgl__lvgl/src/libs/lodepng/lodepng.h"
#define LODEPNG_AVAILABLE 1
#else
#define LODEPNG_AVAILABLE 0
#endif

// Include tjpgd if available (from LVGL)
#if LV_USE_TJPGD
#include "../../managed_components/lvgl__lvgl/src/libs/tjpgd/tjpgd.h"
#define TJPGD_AVAILABLE 1
#else
#define TJPGD_AVAILABLE 0
#endif

static const char *TAG = "sx_image";

// Image service state
static bool s_initialized = false;

// Helper function to parse PNG header and get dimensions
static esp_err_t parse_png_header(const uint8_t *data, size_t data_len, uint16_t *width, uint16_t *height) {
    if (data == NULL || data_len < 24 || width == NULL || height == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // PNG signature: 0x89 0x50 0x4E 0x47 0x0D 0x0A 0x1A 0x0A
    if (memcmp(data, "\x89PNG\r\n\x1a\n", 8) != 0) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    // IHDR chunk starts at offset 16 (after signature)
    // Width: bytes 16-19 (big-endian)
    // Height: bytes 20-23 (big-endian)
    *width = (uint16_t)((data[16] << 24) | (data[17] << 16) | (data[18] << 8) | data[19]);
    *height = (uint16_t)((data[20] << 24) | (data[21] << 16) | (data[22] << 8) | data[23]);
    
    return ESP_OK;
}

// Helper function to parse JPEG header and get dimensions
static esp_err_t parse_jpeg_header(const uint8_t *data, size_t data_len, uint16_t *width, uint16_t *height) {
    if (data == NULL || data_len < 20 || width == NULL || height == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // JPEG signature: 0xFF 0xD8 0xFF
    if (data[0] != 0xFF || data[1] != 0xD8 || data[2] != 0xFF) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    // Search for SOF (Start of Frame) marker: 0xFF 0xC0, 0xFF 0xC1, or 0xFF 0xC2
    size_t pos = 2;
    while (pos < data_len - 8) {
        if (data[pos] == 0xFF) {
            uint8_t marker = data[pos + 1];
            if (marker >= 0xC0 && marker <= 0xC3) {
                // Found SOF marker
                // Height: bytes pos+5, pos+6 (big-endian)
                // Width: bytes pos+7, pos+8 (big-endian)
                *height = (uint16_t)((data[pos + 5] << 8) | data[pos + 6]);
                *width = (uint16_t)((data[pos + 7] << 8) | data[pos + 8]);
                return ESP_OK;
            }
            pos += 2;
        } else {
            pos++;
        }
    }
    
    return ESP_ERR_NOT_FOUND;
}

// Convert RGB888 to RGB565
static uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

// JPEG decoder context for tjpgd
#if TJPGD_AVAILABLE
typedef struct {
    const uint8_t *data;
    size_t data_size;
    size_t data_pos;
    uint8_t *output;
    uint16_t width;
    uint16_t height;
    size_t output_pos;
} jpeg_decode_ctx_t;

// JPEG input function for tjpgd
static size_t jpeg_input_func(JDEC *jd, uint8_t *buff, size_t ndata) {
    jpeg_decode_ctx_t *ctx = (jpeg_decode_ctx_t *)jd->device;
    
    if (ctx->data_pos >= ctx->data_size) {
        return 0; // End of data
    }
    
    size_t remaining = ctx->data_size - ctx->data_pos;
    size_t to_read = (ndata < remaining) ? ndata : remaining;
    
    if (buff != NULL) {
        memcpy(buff, ctx->data + ctx->data_pos, to_read);
    }
    
    ctx->data_pos += to_read;
    return to_read;
}

// JPEG output function for tjpgd (convert to RGB565)
static int jpeg_output_func(JDEC *jd, void *bitmap, JRECT *rect) {
    jpeg_decode_ctx_t *ctx = (jpeg_decode_ctx_t *)jd->device;
    uint8_t *rgb888 = (uint8_t *)bitmap;
    
    // rect contains the MCU (Minimum Coded Unit) region
    // bitmap contains RGB888 data for this region
    uint16_t rect_width = rect->right - rect->left + 1;
    uint16_t rect_height = rect->bottom - rect->top + 1;
    
    // Convert RGB888 to RGB565 and write to output buffer
    for (uint16_t y = 0; y < rect_height; y++) {
        for (uint16_t x = 0; x < rect_width; x++) {
            uint16_t img_x = rect->left + x;
            uint16_t img_y = rect->top + y;
            
            // Check bounds
            if (img_x >= ctx->width || img_y >= ctx->height) {
                continue;
            }
            
            // Calculate indices
            size_t rgb888_idx = (y * rect_width + x) * 3;
            size_t rgb565_idx = (img_y * ctx->width + img_x) * 2;
            
            // Convert RGB888 to RGB565
            uint8_t r = rgb888[rgb888_idx];
            uint8_t g = rgb888[rgb888_idx + 1];
            uint8_t b = rgb888[rgb888_idx + 2];
            uint16_t rgb565 = rgb888_to_rgb565(r, g, b);
            
            // Write RGB565 (little-endian)
            ctx->output[rgb565_idx] = rgb565 & 0xFF;
            ctx->output[rgb565_idx + 1] = (rgb565 >> 8) & 0xFF;
        }
    }
    
    return 1; // Continue
}
#endif

// Decode JPEG to RGB565
static esp_err_t decode_jpeg_to_rgb565(const uint8_t *jpeg_data, size_t jpeg_size,
                                       uint16_t width, uint16_t height,
                                       uint8_t **out_data, size_t *out_size) {
#if TJPGD_AVAILABLE
    JDEC jd;
    jpeg_decode_ctx_t ctx;
    
    // Initialize context
    ctx.data = jpeg_data;
    ctx.data_size = jpeg_size;
    ctx.data_pos = 0;
    ctx.width = width;
    ctx.height = height;
    
    // Allocate output buffer
    size_t rgb565_size = width * height * 2;
    ctx.output = (uint8_t *)malloc(rgb565_size);
    if (ctx.output == NULL) {
        return ESP_ERR_NO_MEM;
    }
    memset(ctx.output, 0, rgb565_size);
    
    // Allocate work buffer for tjpgd (recommended: 3100 bytes)
    size_t workbuf_size = 3100;
    void *workbuf = malloc(workbuf_size);
    if (workbuf == NULL) {
        free(ctx.output);
        return ESP_ERR_NO_MEM;
    }
    
    // Prepare JPEG decoder
    JRESULT res = jd_prepare(&jd, jpeg_input_func, workbuf, workbuf_size, &ctx);
    if (res != JDR_OK) {
        ESP_LOGE(TAG, "JPEG prepare failed: %d", res);
        free(workbuf);
        free(ctx.output);
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    // Update dimensions from JPEG header
    ctx.width = jd.width;
    ctx.height = jd.height;
    
    // Reallocate output buffer if dimensions changed
    if (ctx.width != width || ctx.height != height) {
        free(ctx.output);
        rgb565_size = ctx.width * ctx.height * 2;
        ctx.output = (uint8_t *)malloc(rgb565_size);
        if (ctx.output == NULL) {
            free(workbuf);
            return ESP_ERR_NO_MEM;
        }
        memset(ctx.output, 0, rgb565_size);
    }
    
    // Decompress JPEG
    res = jd_decomp(&jd, jpeg_output_func, 0); // scale = 0 means no scaling
    free(workbuf);
    
    if (res != JDR_OK) {
        ESP_LOGE(TAG, "JPEG decompress failed: %d", res);
        free(ctx.output);
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    *out_data = ctx.output;
    *out_size = rgb565_size;
    return ESP_OK;
#else
    ESP_LOGW(TAG, "JPEG decoding not available (LV_USE_TJPGD not enabled)");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

// Decode PNG to RGB565
static esp_err_t decode_png_to_rgb565(const uint8_t *png_data, size_t png_size, 
                                      uint16_t width, uint16_t height,
                                      uint8_t **out_data, size_t *out_size) {
#if LODEPNG_AVAILABLE
    unsigned int w = width, h = height;
    unsigned char *image = NULL;
    unsigned error = lodepng_decode24(&image, &w, &h, png_data, png_size);
    
    if (error != 0) {
        ESP_LOGE(TAG, "PNG decode error %u: %s", error, lodepng_error_text(error));
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    // Convert RGB888 to RGB565
    size_t rgb565_size = width * height * 2; // 2 bytes per pixel
    uint8_t *rgb565_data = (uint8_t *)malloc(rgb565_size);
    if (rgb565_data == NULL) {
        free(image);
        return ESP_ERR_NO_MEM;
    }
    
    uint16_t *rgb565_ptr = (uint16_t *)rgb565_data;
    uint8_t *rgb888_ptr = image;
    
    for (size_t i = 0; i < width * height; i++) {
        uint8_t r = rgb888_ptr[i * 3];
        uint8_t g = rgb888_ptr[i * 3 + 1];
        uint8_t b = rgb888_ptr[i * 3 + 2];
        rgb565_ptr[i] = rgb888_to_rgb565(r, g, b);
    }
    
    free(image);
    *out_data = rgb565_data;
    *out_size = rgb565_size;
    return ESP_OK;
#else
    ESP_LOGW(TAG, "PNG decoding not available (LV_USE_LODEPNG not enabled)");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t sx_image_service_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "Image service initialized");
    return ESP_OK;
}

sx_image_type_t sx_image_detect_type(const char *file_path) {
    if (file_path == NULL) {
        return SX_IMAGE_TYPE_UNKNOWN;
    }
    
    // Check file extension
    const char *ext = strrchr(file_path, '.');
    if (ext == NULL) {
        return SX_IMAGE_TYPE_UNKNOWN;
    }
    
    ext++; // Skip '.'
    
    // Convert to lowercase for comparison
    char ext_lower[16] = {0};
    for (int i = 0; i < 15 && ext[i] != '\0'; i++) {
        ext_lower[i] = tolower((unsigned char)ext[i]);
    }
    
    if (strcmp(ext_lower, "gif") == 0) {
        return SX_IMAGE_TYPE_GIF;
    } else if (strcmp(ext_lower, "jpg") == 0 || strcmp(ext_lower, "jpeg") == 0) {
        return SX_IMAGE_TYPE_JPEG;
    } else if (strcmp(ext_lower, "png") == 0) {
        return SX_IMAGE_TYPE_PNG;
    } else if (strcmp(ext_lower, "cbin") == 0) {
        return SX_IMAGE_TYPE_CBIN;
    }
    
    return SX_IMAGE_TYPE_UNKNOWN;
}

sx_image_type_t sx_image_detect_type_from_memory(const uint8_t *data, size_t data_len) {
    if (data == NULL || data_len < 4) {
        return SX_IMAGE_TYPE_UNKNOWN;
    }
    
    // Check magic bytes
    // GIF: "GIF8" (GIF87a or GIF89a)
    if (data_len >= 6 && memcmp(data, "GIF8", 4) == 0) {
        return SX_IMAGE_TYPE_GIF;
    }
    
    // JPEG: 0xFF 0xD8 0xFF
    if (data_len >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
        return SX_IMAGE_TYPE_JPEG;
    }
    
    // PNG: 0x89 0x50 0x4E 0x47 0x0D 0x0A 0x1A 0x0A
    if (data_len >= 8 && memcmp(data, "\x89PNG\r\n\x1a\n", 8) == 0) {
        return SX_IMAGE_TYPE_PNG;
    }
    
    // CBIN: Custom format (check for custom header)
    // For now, assume unknown if not recognized
    return SX_IMAGE_TYPE_UNKNOWN;
}

esp_err_t sx_image_load_from_file(const char *file_path, sx_image_info_t *info, uint8_t **data) {
    if (!s_initialized || file_path == NULL || info == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Detect image type
    sx_image_type_t type = sx_image_detect_type(file_path);
    if (type == SX_IMAGE_TYPE_UNKNOWN) {
        ESP_LOGW(TAG, "Unknown image type for file: %s", file_path);
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    // Open file
    FILE *f = fopen(file_path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open image file: %s", file_path);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size <= 0 || file_size > 10 * 1024 * 1024) { // Max 10MB
        fclose(f);
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Allocate buffer
    uint8_t *buffer = (uint8_t *)malloc(file_size);
    if (buffer == NULL) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    
    // Read file
    size_t read = fread(buffer, 1, file_size, f);
    fclose(f);
    
    if (read != (size_t)file_size) {
        free(buffer);
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    // Verify type from memory
    sx_image_type_t detected_type = sx_image_detect_type_from_memory(buffer, read);
    if (detected_type != type && detected_type != SX_IMAGE_TYPE_UNKNOWN) {
        type = detected_type; // Use detected type if different
    }
    
    // Parse image header to get dimensions
    uint16_t width = 0, height = 0;
    esp_err_t parse_ret = ESP_OK;
    
    if (type == SX_IMAGE_TYPE_PNG) {
        parse_ret = parse_png_header(buffer, read, &width, &height);
    } else if (type == SX_IMAGE_TYPE_JPEG) {
        parse_ret = parse_jpeg_header(buffer, read, &width, &height);
    }
    
    if (parse_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to parse image header, dimensions unknown");
    }
    
    // Decode image to RGB565 if supported
    uint8_t *decoded_data = NULL;
    size_t decoded_size = 0;
    bool is_decoded = false;
    
    if (type == SX_IMAGE_TYPE_PNG) {
        if (width > 0 && height > 0) {
            esp_err_t decode_ret = decode_png_to_rgb565(buffer, read, width, height, &decoded_data, &decoded_size);
            if (decode_ret == ESP_OK) {
                free(buffer); // Free original PNG data
                buffer = decoded_data;
                read = decoded_size;
                is_decoded = true;
                ESP_LOGI(TAG, "PNG decoded to RGB565: %dx%d, %zu bytes", width, height, decoded_size);
            } else {
                ESP_LOGW(TAG, "PNG decode failed, returning raw data");
            }
        }
    }
    // Decode JPEG to RGB565 if supported
    if (type == SX_IMAGE_TYPE_JPEG) {
        if (width > 0 && height > 0) {
            esp_err_t decode_ret = decode_jpeg_to_rgb565(buffer, read, width, height, &decoded_data, &decoded_size);
            if (decode_ret == ESP_OK) {
                free(buffer); // Free original JPEG data
                buffer = decoded_data;
                read = decoded_size;
                is_decoded = true;
                ESP_LOGI(TAG, "JPEG decoded to RGB565: %dx%d, %zu bytes", width, height, decoded_size);
            } else {
                ESP_LOGW(TAG, "JPEG decode failed, returning raw data");
            }
        }
    }
    
    info->type = type;
    info->width = width;
    info->height = height;
    info->has_alpha = (type == SX_IMAGE_TYPE_PNG || type == SX_IMAGE_TYPE_GIF) && !is_decoded;
    info->data_size = read;
    
    *data = buffer;
    
    ESP_LOGI(TAG, "Loaded image: %s (type=%d, %dx%d, size=%zu, decoded=%d)", 
             file_path, type, width, height, read, is_decoded);
    return ESP_OK;
}

esp_err_t sx_image_load_from_memory(const uint8_t *data, size_t data_len, sx_image_info_t *info, uint8_t **out_data) {
    if (!s_initialized || data == NULL || info == NULL || out_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Detect image type
    sx_image_type_t type = sx_image_detect_type_from_memory(data, data_len);
    if (type == SX_IMAGE_TYPE_UNKNOWN) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    // Parse image header to get dimensions
    uint16_t width = 0, height = 0;
    esp_err_t parse_ret = ESP_OK;
    
    if (type == SX_IMAGE_TYPE_PNG) {
        parse_ret = parse_png_header(data, data_len, &width, &height);
    } else if (type == SX_IMAGE_TYPE_JPEG) {
        parse_ret = parse_jpeg_header(data, data_len, &width, &height);
    }
    
    if (parse_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to parse image header from memory, dimensions unknown");
    }
    
    // Decode image to RGB565 if supported
    uint8_t *decoded_data = NULL;
    size_t decoded_size = 0;
    bool is_decoded = false;
    
    if (type == SX_IMAGE_TYPE_PNG) {
        if (width > 0 && height > 0) {
            esp_err_t decode_ret = decode_png_to_rgb565(data, data_len, width, height, &decoded_data, &decoded_size);
            if (decode_ret == ESP_OK) {
                is_decoded = true;
                *out_data = decoded_data;
                info->type = type;
                info->width = width;
                info->height = height;
                info->has_alpha = false; // RGB565 has no alpha
                info->data_size = decoded_size;
                ESP_LOGI(TAG, "PNG decoded to RGB565 from memory: %dx%d, %zu bytes", width, height, decoded_size);
                return ESP_OK;
            }
        }
    } else if (type == SX_IMAGE_TYPE_JPEG) {
        if (width > 0 && height > 0) {
            esp_err_t decode_ret = decode_jpeg_to_rgb565(data, data_len, width, height, &decoded_data, &decoded_size);
            if (decode_ret == ESP_OK) {
                is_decoded = true;
                *out_data = decoded_data;
                info->type = type;
                info->width = width;
                info->height = height;
                info->has_alpha = false; // RGB565 has no alpha
                info->data_size = decoded_size;
                ESP_LOGI(TAG, "JPEG decoded to RGB565 from memory: %dx%d, %zu bytes", width, height, decoded_size);
                return ESP_OK;
            }
        }
    }
    
    (void)is_decoded; // Suppress unused variable warning if not decoded
    
    // Fallback: copy raw data
    uint8_t *buffer = (uint8_t *)malloc(data_len);
    if (buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    memcpy(buffer, data, data_len);
    
    info->type = type;
    info->width = width;
    info->height = height;
    info->has_alpha = (type == SX_IMAGE_TYPE_PNG || type == SX_IMAGE_TYPE_GIF);
    info->data_size = data_len;
    
    *out_data = buffer;
    
    return ESP_OK;
}

void sx_image_free(uint8_t *data) {
    if (data != NULL) {
        free(data);
    }
}

// Create LVGL image descriptor from RGB565 data
sx_lvgl_image_t* sx_image_create_lvgl_rgb565(uint8_t *data, uint16_t width, uint16_t height) {
    if (data == NULL || width == 0 || height == 0) {
        return NULL;
    }
    
    // Allocate structure
    sx_lvgl_image_t *img = (sx_lvgl_image_t *)malloc(sizeof(sx_lvgl_image_t));
    if (img == NULL) {
        return NULL;
    }
    
    // Allocate LVGL image descriptor
    lv_image_dsc_t *img_dsc = (lv_image_dsc_t *)malloc(sizeof(lv_image_dsc_t));
    if (img_dsc == NULL) {
        free(img);
        return NULL;
    }
    
    // Initialize image descriptor
    img_dsc->header.w = width;
    img_dsc->header.h = height;
    img_dsc->header.cf = LV_COLOR_FORMAT_RGB565;
    img_dsc->data_size = width * height * 2; // 2 bytes per pixel (RGB565)
    img_dsc->data = data;
    
    img->img_dsc = img_dsc;
    img->data = data;
    
    ESP_LOGI(TAG, "Created LVGL image descriptor: %dx%d, %zu bytes", width, height, img_dsc->data_size);
    
    return img;
}

// Free LVGL image descriptor and data
void sx_image_free_lvgl(sx_lvgl_image_t *img) {
    if (img == NULL) {
        return;
    }
    
    if (img->img_dsc != NULL) {
        free(img->img_dsc);
    }
    
    if (img->data != NULL) {
        free(img->data);
    }
    
    free(img);
}



