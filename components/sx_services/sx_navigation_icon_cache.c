// Icon cache service for navigation icons
// Stores icons in SPIFFS to avoid re-downloading

#include "sx_navigation_icon_cache.h"
#include <esp_log.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "sx_nav_icon_cache";
static bool s_initialized = false;
static SemaphoreHandle_t s_mutex = NULL;

// Initialize SPIFFS for icon cache
static esp_err_t init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS: %d KB total, %d KB used", total / 1024, used / 1024);
    }
    
    // Create nav_icons directory if it doesn't exist
    struct stat st = {0};
    if (stat(SX_NAV_ICON_CACHE_PATH, &st) == -1) {
        // Directory doesn't exist, create it
        // Note: mkdir may not work directly, we'll create files with full path
        ESP_LOGI(TAG, "Icon cache directory will be created on first save");
    }
    
    return ESP_OK;
}

esp_err_t sx_nav_icon_cache_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t ret = init_spiffs();
    if (ret != ESP_OK) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ret;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "Icon cache service initialized");
    return ESP_OK;
}

void sx_nav_icon_cache_deinit(void) {
    if (!s_initialized) {
        return;
    }
    
    if (s_mutex) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }
    
    s_initialized = false;
    ESP_LOGI(TAG, "Icon cache service deinitialized");
}

// Build icon file path
static void build_icon_path(const char *icon_hash, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s.bin", SX_NAV_ICON_CACHE_PATH, icon_hash);
}

bool sx_nav_icon_cache_exists(const char *icon_hash) {
    if (!s_initialized || !icon_hash) {
        return false;
    }
    
    char path[128];
    build_icon_path(icon_hash, path, sizeof(path));
    
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return true;
    }
    
    return false;
}

esp_err_t sx_nav_icon_cache_save(const char *icon_hash, const uint8_t *bitmap, size_t size) {
    if (!s_initialized || !icon_hash || !bitmap || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check if already exists
    if (sx_nav_icon_cache_exists(icon_hash)) {
        ESP_LOGD(TAG, "Icon %s already cached, skipping", icon_hash);
        return ESP_OK;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    char path[128];
    build_icon_path(icon_hash, path, sizeof(path));
    
    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", path);
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    size_t written = fwrite(bitmap, 1, size, f);
    fclose(f);
    
    xSemaphoreGive(s_mutex);
    
    if (written != size) {
        ESP_LOGE(TAG, "Failed to write icon: %s (written: %d, expected: %d)", 
                 icon_hash, written, size);
        return ESP_ERR_INVALID_SIZE;
    }
    
    ESP_LOGI(TAG, "Icon saved to cache: %s (%d bytes)", icon_hash, size);
    return ESP_OK;
}

esp_err_t sx_nav_icon_cache_load(const char *icon_hash, uint8_t *bitmap, size_t size) {
    if (!s_initialized || !icon_hash || !bitmap || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    char path[128];
    build_icon_path(icon_hash, path, sizeof(path));
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    size_t read = fread(bitmap, 1, size, f);
    fclose(f);
    
    xSemaphoreGive(s_mutex);
    
    if (read != size) {
        ESP_LOGW(TAG, "Icon size mismatch: %s (read: %d, expected: %d)", 
                 icon_hash, read, size);
        return ESP_ERR_INVALID_SIZE;
    }
    
    ESP_LOGD(TAG, "Icon loaded from cache: %s (%d bytes)", icon_hash, size);
    return ESP_OK;
}

size_t sx_nav_icon_cache_list(char hashes[][17], size_t max_count) {
    if (!s_initialized || !hashes || max_count == 0) {
        return 0;
    }
    
    size_t count = 0;
    DIR *dir = opendir(SX_NAV_ICON_CACHE_PATH);
    if (!dir) {
        ESP_LOGW(TAG, "Failed to open icon cache directory");
        return 0;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max_count) {
        // Check if file ends with .bin
        size_t len = strlen(entry->d_name);
        if (len > 4 && strcmp(entry->d_name + len - 4, ".bin") == 0) {
            // Extract hash (filename without .bin extension)
            strncpy(hashes[count], entry->d_name, len - 4);
            hashes[count][len - 4] = '\0';
            if (strlen(hashes[count]) <= 16) {
                count++;
            }
        }
    }
    
    closedir(dir);
    return count;
}

esp_err_t sx_nav_icon_cache_clear_all(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    DIR *dir = opendir(SX_NAV_ICON_CACHE_PATH);
    if (!dir) {
        xSemaphoreGive(s_mutex);
        return ESP_OK;  // Directory doesn't exist, nothing to clear
    }
    
    struct dirent *entry;
    size_t removed = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            // Buffer size: SX_NAV_ICON_CACHE_PATH (18) + "/" (1) + filename (max 255) + null (1) = 275
            char path[275];
            int len = snprintf(path, sizeof(path), "%s/%s", SX_NAV_ICON_CACHE_PATH, entry->d_name);
            if (len < 0 || len >= (int)sizeof(path)) {
                ESP_LOGW(TAG, "Path truncated for file: %s", entry->d_name);
                continue;  // Skip files with too long names
            }
            if (remove(path) == 0) {
                removed++;
            }
        }
    }
    
    closedir(dir);
    xSemaphoreGive(s_mutex);
    
    ESP_LOGI(TAG, "Cleared %d cached icons", removed);
    return ESP_OK;
}

