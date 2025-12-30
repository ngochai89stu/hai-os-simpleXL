#include "sx_assets.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "driver/gpio.h"
#include "bootscreen_img.h"
#include "flashscreen_img.h"

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

// Boot screen image getter
const lv_img_dsc_t* sx_assets_get_bootscreen_img(void) {
    return &bootscreen_img;
}

// Flash screen background image getter
const lv_img_dsc_t* sx_assets_get_flashscreen_img(void) {
    return &flashscreen_img;
}

