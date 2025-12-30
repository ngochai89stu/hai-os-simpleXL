#include "sx_sd_service.h"

#include <esp_log.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "sdmmc_cmd.h"
#include "sx_spi_bus_manager.h"

static const char *TAG = "sx_sd";

static bool s_initialized = false;
static bool s_mounted = false;
static sx_sd_config_t s_cfg = {0};
static sdmmc_card_t *s_card = NULL;

esp_err_t sx_sd_service_init(const sx_sd_config_t *cfg) {
    if (s_initialized) {
        return ESP_OK;
    }
    if (cfg == NULL || cfg->mount_point == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_cfg = *cfg;
    s_initialized = true;
    ESP_LOGI(TAG, "SD service init: mount_point=%s spi_host=%d", s_cfg.mount_point, s_cfg.spi_host);
    return ESP_OK;
}

esp_err_t sx_sd_service_start(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_mounted) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Mounting SD card (SPI)...");
    ESP_LOGI(TAG, "SD card config: CS=%d, MOSI=%d, MISO=%d, SCLK=%d, Host=%d", 
             s_cfg.cs_gpio, s_cfg.mosi_gpio, s_cfg.miso_gpio, s_cfg.sclk_gpio, s_cfg.spi_host);

    // Note: SPI bus is already initialized by LCD with MISO=12, MOSI=47, SCLK=21
    // SD card uses the same SPI bus (SPI3_HOST) with different CS pin (10 vs 41 for LCD)
    // esp_vfs_fat_sdspi_mount() will automatically use the existing SPI bus if it matches
    // This approach is similar to LovyanGFX + SD card sharing SPI bus

    // Initialize CS pin as output and set high (inactive)
    // This is the only GPIO setup needed - no complex reset sequence required
    gpio_reset_pin(s_cfg.cs_gpio);
    gpio_set_direction(s_cfg.cs_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(s_cfg.cs_gpio, 1); // CS high = inactive
    vTaskDelay(pdMS_TO_TICKS(50)); // Brief delay for GPIO to stabilize

    // Configure SDSPI host and device (similar to repo mẫu)
    sdspi_device_config_t device_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    device_config.host_id = s_cfg.spi_host;
    device_config.gpio_cs = s_cfg.cs_gpio;
    device_config.gpio_wp = GPIO_NUM_NC; // No write protect pin
    device_config.gpio_cd = GPIO_NUM_NC; // No card detect pin

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = device_config.host_id;
    // Start with default frequency (20MHz), esp_vfs_fat_sdspi_mount will handle initialization
    host.max_freq_khz = SDMMC_FREQ_DEFAULT; // 20MHz default

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false
    };

    // Acquire SPI bus lock before mounting (LCD and SD card share the same bus)
    ESP_LOGI(TAG, "Acquiring SPI bus lock...");
    sx_spi_bus_lock();
    
    // Brief delay to ensure SPI bus is stable
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Mount SD card - esp_vfs_fat_sdspi_mount() will use the existing SPI bus
    // No need for complex retry logic or reset sequences - keep it simple like repo mẫu
    ESP_LOGI(TAG, "Mounting filesystem at %s", s_cfg.mount_point);
    esp_err_t ret = esp_vfs_fat_sdspi_mount(s_cfg.mount_point, &host, &device_config, &mount_cfg, &s_card);
    
    sx_spi_bus_unlock();
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set format_if_mount_failed option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }
        ESP_LOGW(TAG, "SD card may be missing, damaged, or incompatible. Continuing without SD card.");
        return ret;
    }

    s_mounted = true;
    ESP_LOGI(TAG, "Filesystem mounted successfully");
    
    // Print card information (similar to repo mẫu)
    if (s_card != NULL) {
        sdmmc_card_print_info(stdout, s_card);
    }
    
    ESP_LOGI(TAG, "SD card mounted at %s", s_cfg.mount_point);
    return ESP_OK;
}

esp_err_t sx_sd_service_stop(void) {
    if (!s_mounted) {
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Unmounting SD card");
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(s_cfg.mount_point, s_card);
    s_card = NULL;
    s_mounted = false;
    return ret;
}

bool sx_sd_is_mounted(void) {
    return s_mounted;
}

const char *sx_sd_get_mount_point(void) {
    return (s_cfg.mount_point != NULL) ? s_cfg.mount_point : SX_SD_MOUNT_POINT;
}

static esp_err_t make_full_path(const char *path, char *out, size_t out_sz) {
    if (path == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (path[0] == '/') {
        snprintf(out, out_sz, "%s", path);
    } else {
        snprintf(out, out_sz, "%s/%s", sx_sd_get_mount_point(), path);
    }
    return ESP_OK;
}

esp_err_t sx_sd_get_file_size(const char *path, size_t *out_size) {
    if (!s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    if (out_size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    char full[256];
    make_full_path(path, full, sizeof(full));
    sx_spi_bus_lock();
    FILE *f = fopen(full, "rb");
    if (!f) {
        sx_spi_bus_unlock();
        return ESP_FAIL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    sx_spi_bus_unlock();
    if (sz < 0) {
        return ESP_FAIL;
    }
    *out_size = (size_t)sz;
    return ESP_OK;
}

esp_err_t sx_sd_read_file(const char *path, void *out_buf, size_t buf_size, size_t *out_read) {
    if (!s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    if (path == NULL || out_buf == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    char full[256];
    make_full_path(path, full, sizeof(full));
    sx_spi_bus_lock();
    FILE *f = fopen(full, "rb");
    if (!f) {
        sx_spi_bus_unlock();
        return ESP_FAIL;
    }
    size_t n = fread(out_buf, 1, buf_size, f);
    fclose(f);
    if (out_read) {
        *out_read = n;
    }
    sx_spi_bus_unlock();
    return ESP_OK;
}

esp_err_t sx_sd_list_files(const char *dir_path, sx_sd_file_entry_t *entries, size_t max_count, size_t *out_count) {
    if (!s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    if (entries == NULL || max_count == 0 || out_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char full[256];
    make_full_path(dir_path, full, sizeof(full));
    
    sx_spi_bus_lock();
    DIR *dir = opendir(full);
    if (dir == NULL) {
        sx_spi_bus_unlock();
        return ESP_FAIL;
    }
    
    size_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max_count) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        strncpy(entries[count].name, entry->d_name, sizeof(entries[count].name) - 1);
        entries[count].name[sizeof(entries[count].name) - 1] = '\0';
        
        // Check if directory (use larger buffer to avoid truncation warning)
        char entry_path[512];
        int len = snprintf(entry_path, sizeof(entry_path), "%s/%s", full, entry->d_name);
        if (len > 0 && len < (int)sizeof(entry_path)) {
            struct stat st;
            if (stat(entry_path, &st) == 0) {
                entries[count].is_dir = S_ISDIR(st.st_mode);
                entries[count].size = st.st_size;
            } else {
                entries[count].is_dir = (entry->d_type == DT_DIR);
                entries[count].size = 0;
            }
        } else {
            // Path too long or error, use dirent type
            entries[count].is_dir = (entry->d_type == DT_DIR);
            entries[count].size = 0;
        }
        
        count++;
    }
    
    closedir(dir);
    *out_count = count;
    sx_spi_bus_unlock();
    return ESP_OK;
}

