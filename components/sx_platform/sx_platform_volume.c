#include "sx_platform_volume.h"

#include <esp_log.h>
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sx_platform_vol";

// Hardware volume state
static bool s_initialized = false;
static sx_hw_codec_chip_t s_detected_codec = SX_HW_CODEC_NONE;
static uint8_t s_current_volume = 50;
static bool s_hw_volume_available = false;

// I2C configuration for codec chips
// GPIO 22 đã tắt - không sử dụng I2C hardware volume
// System sẽ dùng software volume thay vì hardware volume
#define I2C_MASTER_SCL_IO           -1  // Đã tắt - không sử dụng GPIO 22
#define I2C_MASTER_SDA_IO           -1  // Đã tắt - không sử dụng GPIO 21
#define I2C_MASTER_FREQ_HZ          100000

// I2C master bus handle
static i2c_master_bus_handle_t s_i2c_bus_handle = NULL;
// Device handles for different codec chips
static i2c_master_dev_handle_t s_i2c_dev_es8388 = NULL;
static i2c_master_dev_handle_t s_i2c_dev_es8311 = NULL;

// ES8388 I2C address
#define ES8388_ADDR                 0x10

// ES8311 I2C address
#define ES8311_ADDR                 0x18

static esp_err_t i2c_master_init(void) {
    // I2C đã bị disable - GPIO 22/21 không sử dụng
    if (I2C_MASTER_SCL_IO < 0 || I2C_MASTER_SDA_IO < 0) {
        ESP_LOGI(TAG, "I2C hardware volume disabled - using software volume only");
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    if (s_i2c_bus_handle != NULL) {
        return ESP_OK; // Already initialized
    }
    
    // Validate GPIO pins - ESP32-S3 GPIO range: 0-48
    if (I2C_MASTER_SCL_IO > 48 || I2C_MASTER_SDA_IO > 48) {
        ESP_LOGE(TAG, "Invalid GPIO pin numbers: SCL=%d, SDA=%d (max 48)", 
                 I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Configure I2C master bus
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = true,
        },
    };
    
    esp_err_t ret = i2c_new_master_bus(&i2c_bus_config, &s_i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "GPIO SCL=%d, SDA=%d - Kiểm tra lại hardware pin mapping", 
                 I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO);
        return ret;
    }
    
    return ESP_OK;
}

// ES8388 register addresses for volume control
#define ES8388_REG_DACVOLUME1     0x1F  // DAC Volume Control 1
#define ES8388_REG_DACVOLUME2     0x20  // DAC Volume Control 2
#define ES8388_REG_DACVOLUME3     0x21  // DAC Volume Control 3
#define ES8388_REG_DACVOLUME4     0x22  // DAC Volume Control 4
#define ES8388_REG_DACVOLUME5     0x23  // DAC Volume Control 5
#define ES8388_REG_DACVOLUME6     0x24  // DAC Volume Control 6
#define ES8388_REG_HPVOL          0x25  // Headphone Volume Control
#define ES8388_REG_CHIPID         0xFD  // Chip ID Register

// ES8311 register addresses for volume control
#define ES8311_REG_DAC_VOL        0x1F  // DAC Volume Control
#define ES8311_REG_HP_VOL         0x20  // Headphone Volume Control
#define ES8311_REG_CHIPID1        0xFD  // Chip ID Register 1
#define ES8311_REG_CHIPID2        0xFE  // Chip ID Register 2

// I2C helper functions
static esp_err_t i2c_get_device(uint8_t addr, i2c_master_dev_handle_t *dev_handle) {
    if (s_i2c_bus_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Check if device handle already exists for this address
    if (addr == ES8388_ADDR && s_i2c_dev_es8388 != NULL) {
        *dev_handle = s_i2c_dev_es8388;
        return ESP_OK;
    }
    if (addr == ES8311_ADDR && s_i2c_dev_es8311 != NULL) {
        *dev_handle = s_i2c_dev_es8311;
        return ESP_OK;
    }
    
    // Create device configuration
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t ret = i2c_master_bus_add_device(s_i2c_bus_handle, &dev_cfg, dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device (addr 0x%02X): %s", addr, esp_err_to_name(ret));
        return ret;
    }
    
    // Store device handle based on address
    if (addr == ES8388_ADDR) {
        s_i2c_dev_es8388 = *dev_handle;
    } else if (addr == ES8311_ADDR) {
        s_i2c_dev_es8311 = *dev_handle;
    }
    
    return ESP_OK;
}

static esp_err_t i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t value) {
    i2c_master_dev_handle_t dev_handle;
    esp_err_t ret = i2c_get_device(addr, &dev_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    uint8_t write_buf[2] = {reg, value};
    return i2c_master_transmit(dev_handle, write_buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *value) {
    i2c_master_dev_handle_t dev_handle;
    esp_err_t ret = i2c_get_device(addr, &dev_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Write register address
    ret = i2c_master_transmit(dev_handle, &reg, 1, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Read register value
    return i2c_master_receive(dev_handle, value, 1, pdMS_TO_TICKS(100));
}

static bool detect_es8388(void) {
    uint8_t chip_id = 0;
    esp_err_t ret = i2c_read_reg(ES8388_ADDR, ES8388_REG_CHIPID, &chip_id);
    if (ret == ESP_OK && chip_id == 0x01) {  // ES8388 chip ID is 0x01
        ESP_LOGI(TAG, "ES8388 detected (chip ID: 0x%02X)", chip_id);
        return true;
    }
    return false;
}

static bool detect_es8311(void) {
    uint8_t chip_id1 = 0, chip_id2 = 0;
    esp_err_t ret1 = i2c_read_reg(ES8311_ADDR, ES8311_REG_CHIPID1, &chip_id1);
    esp_err_t ret2 = i2c_read_reg(ES8311_ADDR, ES8311_REG_CHIPID2, &chip_id2);
    if (ret1 == ESP_OK && ret2 == ESP_OK && chip_id1 == 0x83 && chip_id2 == 0x11) {
        ESP_LOGI(TAG, "ES8311 detected (chip ID: 0x%02X%02X)", chip_id1, chip_id2);
        return true;
    }
    return false;
}

sx_hw_codec_chip_t sx_platform_detect_codec(void) {
    if (s_detected_codec != SX_HW_CODEC_NONE) {
        return s_detected_codec; // Already detected
    }
    
    // I2C đã bị disable - không detect codec, dùng software volume
    if (I2C_MASTER_SCL_IO < 0 || I2C_MASTER_SDA_IO < 0) {
        s_detected_codec = SX_HW_CODEC_PCM5102A;
        ESP_LOGI(TAG, "I2C disabled (GPIO 22/21) - using PCM5102A (software volume only)");
        return SX_HW_CODEC_PCM5102A;
    }
    
    // Initialize I2C
    esp_err_t ret = i2c_master_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C init failed: %s", esp_err_to_name(ret));
        s_detected_codec = SX_HW_CODEC_PCM5102A;
        return SX_HW_CODEC_PCM5102A;
    }
    
    // Try to detect ES8388
    if (detect_es8388()) {
        s_detected_codec = SX_HW_CODEC_ES8388;
        return SX_HW_CODEC_ES8388;
    }
    
    // Try to detect ES8311
    if (detect_es8311()) {
        s_detected_codec = SX_HW_CODEC_ES8311;
        return SX_HW_CODEC_ES8311;
    }
    
    // No codec detected, assume PCM5102A (no hardware volume)
    s_detected_codec = SX_HW_CODEC_PCM5102A;
    ESP_LOGI(TAG, "No codec chip detected, using PCM5102A (software volume only)");
    return SX_HW_CODEC_PCM5102A;
}

esp_err_t sx_platform_hw_volume_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    // Detect codec
    sx_hw_codec_chip_t codec = sx_platform_detect_codec();
    
    if (codec == SX_HW_CODEC_ES8388 || codec == SX_HW_CODEC_ES8311) {
        s_hw_volume_available = true;
        // Set initial volume to 50%
        sx_platform_hw_volume_set(50);
        ESP_LOGI(TAG, "Hardware volume control initialized (codec: %d)", codec);
    } else {
        s_hw_volume_available = false;
        ESP_LOGI(TAG, "Hardware volume not available, using software volume");
    }
    
    s_initialized = true;
    return ESP_OK;
}

esp_err_t sx_platform_hw_volume_set(uint8_t volume) {
    if (!s_initialized || !s_hw_volume_available) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (volume > 100) volume = 100;
    s_current_volume = volume;
    
    esp_err_t ret = ESP_OK;
    
    if (s_detected_codec == SX_HW_CODEC_ES8388) {
        // ES8388 volume control: 0-100 maps to 0-192 (0x00-0xC0)
        // Volume register: 0x00 = mute, 0xC0 = 0dB, higher = attenuation
        // We invert: 0 = 0xC0 (0dB), 100 = 0x00 (mute) - but we want opposite
        // Actually: 0 = mute (0xFF), 100 = max (0x00)
        // Better: 0 = 0xC0 (mute), 100 = 0x00 (max)
        // ES8388 uses inverted scale: 0x00 = +12dB, 0xFF = -96dB
        // We'll use: 0 = 0xC0 (0dB), 100 = 0x00 (max)
        uint8_t vol_reg = 0xC0 - ((volume * 0xC0) / 100);
        
        // Set DAC volume (stereo: left and right)
        ret = i2c_write_reg(ES8388_ADDR, ES8388_REG_DACVOLUME1, vol_reg);
        if (ret == ESP_OK) {
            ret = i2c_write_reg(ES8388_ADDR, ES8388_REG_DACVOLUME2, vol_reg);
        }
        if (ret == ESP_OK) {
            ret = i2c_write_reg(ES8388_ADDR, ES8388_REG_HPVOL, vol_reg);
        }
    } else if (s_detected_codec == SX_HW_CODEC_ES8311) {
        // ES8311 volume control: 0-100 maps to 0-192 (0x00-0xC0)
        // Similar to ES8388
        uint8_t vol_reg = 0xC0 - ((volume * 0xC0) / 100);
        
        // Set DAC volume
        ret = i2c_write_reg(ES8311_ADDR, ES8311_REG_DAC_VOL, vol_reg);
        if (ret == ESP_OK) {
            ret = i2c_write_reg(ES8311_ADDR, ES8311_REG_HP_VOL, vol_reg);
        }
    }
    
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "Hardware volume set to %d%% (codec: %d)", volume, s_detected_codec);
    } else {
        ESP_LOGW(TAG, "Failed to set hardware volume: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

uint8_t sx_platform_hw_volume_get(void) {
    return s_current_volume;
}

bool sx_platform_hw_volume_available(void) {
    return s_hw_volume_available;
}


