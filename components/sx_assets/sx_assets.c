#include "sx_assets.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "driver/gpio.h"
// KHÔNG include bootscreen_img.h/flashscreen_img.h ở đây vì chúng include lvgl.h
// Thay vào đó, access raw data trực tiếp từ generated files

static const char *TAG = "sx_assets";

// Phase 3: SD card configuration (SPI mode, sharing SPI bus with LCD)
#define SD_SPI_HOST_ID      SPI3_HOST  // Same as LCD
#define SD_SPI_MISO_PIN     12          // SDO/MISO
#define SD_SPI_CS_PIN       13          // CS pin (different from LCD CS)
#define SD_MOUNT_POINT      "/sdcard"

static bool s_sd_mounted = false;

// Phase 4: SD service integration hook
// We keep sx_assets independent; SD mount can be provided externally later.

// Note: SD card mount is handled by sx_sd_service
// This variable reserved for future Phase 3 implementation if direct card access needed
// static sdmmc_card_t *s_card = NULL;

// Asset structure
struct sx_asset {
    uint16_t *data;
    sx_asset_info_t info;
};

esp_err_t sx_assets_init(void) {
    if (s_sd_mounted) {
        ESP_LOGW(TAG, "SD card already marked mounted");
        return ESP_OK;
    }

    // Phase 4: SD mount is owned by sx_sd_service. sx_assets just checks availability.
    // For now, keep backward-compatible behavior: we do not mount here.
    ESP_LOGI(TAG, "Assets init: waiting for SD mount by sx_sd_service (mount point: %s)", SD_MOUNT_POINT);
    return ESP_OK;
}

sx_asset_handle_t sx_assets_load_rgb565(const char *path, sx_asset_info_t *info) {
    if (path == NULL || info == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return NULL;
    }
    
    if (!s_sd_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return NULL;
    }
    
    ESP_LOGI(TAG, "Loading RGB565 asset: %s", path);
    
    // Future: Phase 3 - Load RGB565 from SD card
    // Implementation plan:
    // 1. Open file from SD card using standard file I/O
    // 2. Read header (optional: width, height) or use fixed size
    // 3. Allocate buffer for RGB565 data (consider using sx_audio_buffer_pool pattern)
    // 4. Read data into buffer
    // 5. Return handle for asset management
    // 5. Return asset handle
    
    ESP_LOGW(TAG, "Asset loading stub - will be fully implemented in Phase 3");
    
    // Stub: return NULL for now
    return NULL;
}

const uint16_t* sx_assets_get_data(sx_asset_handle_t handle) {
    if (handle == NULL) {
        return NULL;
    }
    
    return handle->data;
}

void sx_assets_free(sx_asset_handle_t handle) {
    if (handle == NULL) {
        return;
    }
    
    if (handle->data != NULL) {
        free(handle->data);
    }
    free(handle);
}

bool sx_assets_sd_ready(void) {
    // Phase 4: This flag can be updated when sx_sd_service mounts successfully.
    return s_sd_mounted;
}

// Phase 4: Allow SD service to notify assets layer
void sx_assets_set_sd_ready(bool ready) {
    s_sd_mounted = ready;
}

// Boot screen image raw data getter
// Note: Raw data được expose, sx_ui sẽ wrap thành lv_img_dsc_t
// Tuân theo SIMPLEXL_ARCH v1.3 - sx_assets không include LVGL
// Generated images được access trực tiếp từ sx_ui component qua ui_assets_wrapper
// Hàm này giữ lại để tương thích, nhưng thực tế sx_ui sẽ dùng ui_assets_get_bootscreen_img()

const sx_embedded_img_data_t* sx_assets_get_bootscreen_data(void) {
    // Stub: Generated images được access từ sx_ui component
    // Vì generated files include lvgl.h, chúng không thể được compile trong sx_assets
    // sx_ui/ui_assets_wrapper.c sẽ access trực tiếp từ generated files
    static const sx_embedded_img_data_t s_bootscreen_data = {
        .data = NULL,  // Không thể access từ đây vì generated file include lvgl.h
        .width = 320,
        .height = 480,
        .data_size = 307200,  // 320 * 480 * 2 (RGB565)
        .color_format = 0x0B, // LV_COLOR_FORMAT_RGB565
    };
    return &s_bootscreen_data;
}

const sx_embedded_img_data_t* sx_assets_get_flashscreen_data(void) {
    // Stub: Generated images được access từ sx_ui component
    static const sx_embedded_img_data_t s_flashscreen_data = {
        .data = NULL,  // Không thể access từ đây vì generated file include lvgl.h
        .width = 320,
        .height = 480,
        .data_size = 307200,  // 320 * 480 * 2 (RGB565)
        .color_format = 0x0B, // LV_COLOR_FORMAT_RGB565
    };
    return &s_flashscreen_data;
}

