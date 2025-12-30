#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 4: SD service (mount + file ops) - separated from sx_assets (which loads RGB565 assets)

#define SX_SD_MOUNT_POINT "/sdcard"

typedef struct {
    const char *mount_point; // default: /sdcard
    int spi_host;            // SPI host id (e.g., SPI3_HOST)
    int miso_gpio;
    int mosi_gpio;
    int sclk_gpio;
    int cs_gpio;
} sx_sd_config_t;

esp_err_t sx_sd_service_init(const sx_sd_config_t *cfg);
esp_err_t sx_sd_service_start(void);
esp_err_t sx_sd_service_stop(void);

bool sx_sd_is_mounted(void);
const char *sx_sd_get_mount_point(void);

// Basic file ops helpers
esp_err_t sx_sd_read_file(const char *path, void *out_buf, size_t buf_size, size_t *out_read);
esp_err_t sx_sd_get_file_size(const char *path, size_t *out_size);

// List files in directory (simple implementation)
// Caller provides buffer and max_count, fills out entries array
typedef struct {
    char name[64];
    bool is_dir;
    size_t size;
} sx_sd_file_entry_t;
esp_err_t sx_sd_list_files(const char *dir_path, sx_sd_file_entry_t *entries, size_t max_count, size_t *out_count);

#ifdef __cplusplus
}
#endif

