#include "sx_platform.h"

#include <esp_log.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7796.h"
#include "esp_lcd_touch_ft5x06.h"

// Include LCD drivers based on Kconfig selection
#if defined(CONFIG_LCD_ST7789_240X320) || defined(CONFIG_LCD_ST7789_240X240)
#include "esp_lcd_panel_vendor.h"
#endif

#if defined(CONFIG_LCD_ILI9341_240X320)
#include "esp_lcd_ili9341.h"
#endif

// GPIO mapping từ Kconfig
#define LCD_HOST_ID         SPI3_HOST

// LCD pins từ Kconfig
#define LCD_PIN_NUM_MOSI    CONFIG_HAI_LCD_PIN_MOSI
#define LCD_PIN_NUM_CLK     CONFIG_HAI_LCD_PIN_CLK
#define LCD_PIN_NUM_CS      CONFIG_HAI_LCD_PIN_CS
#define LCD_PIN_NUM_DC      CONFIG_HAI_LCD_PIN_DC
#define LCD_PIN_NUM_RST     CONFIG_HAI_LCD_PIN_RST
#define LCD_PIN_NUM_BK_LIGHT CONFIG_HAI_LCD_PIN_BK_LIGHT

// CTP (Capacitive Touch Panel) GPIO mapping từ Kconfig
#if CONFIG_HAI_TOUCH_ENABLE
#define CTP_I2C_SDA_GPIO    CONFIG_HAI_TOUCH_PIN_SDA
#define CTP_I2C_SCL_GPIO    CONFIG_HAI_TOUCH_PIN_SCL
#define CTP_RST_GPIO        CONFIG_HAI_TOUCH_PIN_RST
#define CTP_INT_GPIO        CONFIG_HAI_TOUCH_PIN_INT
#define CTP_I2C_BUS_NUM     I2C_NUM_1  // I2C bus cho touch (port 1, volume dùng port 0)
#define CTP_I2C_FREQ_HZ     400000  // 400kHz cho touch controller
#endif

// LCD resolution từ Kconfig hoặc mặc định theo loại LCD
#if defined(CONFIG_LCD_ST7796_320X480)
#define LCD_H_RES           320
#define LCD_V_RES           480
#elif defined(CONFIG_LCD_ST7789_240X320)
#define LCD_H_RES           240
#define LCD_V_RES           320
#elif defined(CONFIG_LCD_ST7789_240X240)
#define LCD_H_RES           240
#define LCD_V_RES           240
#elif defined(CONFIG_LCD_ILI9341_240X320)
#define LCD_H_RES           240
#define LCD_V_RES           320
#elif defined(CONFIG_LCD_CUSTOM)
#define LCD_H_RES           CONFIG_HAI_LCD_WIDTH
#define LCD_V_RES           CONFIG_HAI_LCD_HEIGHT
#else
// Default fallback
#define LCD_H_RES           320
#define LCD_V_RES           480
#endif

// LEDC configuration for PWM backlight
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // 8192 steps
#define LEDC_FREQUENCY          5000              // 5kHz PWM frequency

static const char *TAG = "sx_platform";
static bool s_backlight_initialized = false;
static uint8_t s_current_brightness = 100; // Default 100%

// ST7796U custom init sequence from display_test_standalone/main.c
static const st7796_lcd_init_cmd_t st7796u_init_cmds[] = {
    {0x11, (uint8_t[]){0x00}, 0, 120},  // SLPOUT - Sleep out, delay 120ms
    {0x3A, (uint8_t[]){0x55}, 1, 0},   // COLMOD - RGB565 (0x55)
    {0xF0, (uint8_t[]){0xC3}, 1, 0},   // Command Set Control 1
    {0xF0, (uint8_t[]){0x96}, 1, 0},   // Command Set Control 2
    {0xB4, (uint8_t[]){0x01}, 1, 0},   // Display Inversion Control
    {0xB7, (uint8_t[]){0xC6}, 1, 0},   // Gate Control
    {0xC0, (uint8_t[]){0x80, 0x45}, 2, 0},  // Power Control 1
    {0xC1, (uint8_t[]){0x13}, 1, 0},   // Power Control 2
    {0xC2, (uint8_t[]){0xA7}, 1, 0},   // Power Control 3
    {0xC5, (uint8_t[]){0x0A}, 1, 0},   // VCOM Control
    {0xE8, (uint8_t[]){0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33}, 8, 0},  // Power Control 4
    {0xE0, (uint8_t[]){0xD0, 0x08, 0x0F, 0x06, 0x06, 0x33, 0x30, 0x33, 0x47, 0x17, 0x13, 0x13, 0x2B, 0x31}, 14, 0},  // Positive Gamma
    {0xE1, (uint8_t[]){0xD0, 0x0A, 0x11, 0x0B, 0x09, 0x07, 0x2F, 0x33, 0x47, 0x38, 0x15, 0x16, 0x2C, 0x32}, 14, 0},  // Negative Gamma
    {0xF0, (uint8_t[]){0x3C}, 1, 0},   // Command Set Control 3
    {0xF0, (uint8_t[]){0x69}, 1, 120}, // Command Set Control 4, delay 120ms
    {0x36, (uint8_t[]){0x48}, 1, 0},   // MADCTL - BGR=ON (0x48)
    {0x21, (uint8_t[]){0x00}, 0, 0},   // INVON - Display inversion on
    {0x29, (uint8_t[]){0x00}, 0, 20},  // DISPON - Display on, delay 20ms
};

sx_display_handles_t sx_platform_display_init(void) {
    ESP_LOGI(TAG, "Initializing display platform...");

    sx_display_handles_t handles = {0};

    // Initialize PWM for backlight control
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LCD_PIN_NUM_BK_LIGHT,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    s_backlight_initialized = true;
    s_current_brightness = 100; // Default to 100%
    
    // Set initial brightness to 100%
    sx_platform_set_brightness(100);

    // SPI bus configuration
    // Note: MISO pin (12) is shared with SD card, so we initialize it here
    // to allow both LCD and SD card to use the same SPI bus
    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_PIN_NUM_CLK,
        .mosi_io_num = LCD_PIN_NUM_MOSI,
        .miso_io_num = 12,  // SD card MISO pin (shared with LCD SPI bus)
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST_ID, &buscfg, SPI_DMA_CH_AUTO));

    // Panel IO configuration
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = LCD_PIN_NUM_CS,
        .dc_gpio_num = LCD_PIN_NUM_DC,
        .spi_mode = 0,
        .pclk_hz = 20 * 1000 * 1000, // 20MHz from reference
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    esp_err_t ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST_ID, &io_config, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel IO: %s", esp_err_to_name(ret));
        // Note: SPI bus was initialized but panel IO creation failed
        // SPI bus resources will remain allocated (ESP-IDF limitation)
        // This is acceptable as init failures are rare and system will reboot
        s_backlight_initialized = false; // Reset backlight state on failure
        return handles; // Return empty handles on error
    }
    handles.io_handle = io_handle;

    // Panel driver configuration - select based on Kconfig
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_NUM_RST,
        .bits_per_pixel = 16,
    };
    
#if defined(CONFIG_LCD_ST7796_320X480)
    // ST7796 configuration
    ESP_LOGI(TAG, "Initializing ST7796 LCD (320x480)");
    panel_config.rgb_ele_order = ESP_LCD_COLOR_SPACE_BGR;
    
    st7796_vendor_config_t vendor_config = {
        .init_cmds = st7796u_init_cmds,
        .init_cmds_size = sizeof(st7796u_init_cmds) / sizeof(st7796_lcd_init_cmd_t),
    };
    panel_config.vendor_config = &vendor_config;

    ret = esp_lcd_new_panel_st7796(io_handle, &panel_config, &panel_handle);
    
#elif defined(CONFIG_LCD_ST7789_240X320)
    // ST7789 240x320 configuration
    ESP_LOGI(TAG, "Initializing ST7789 LCD (240x320)");
    panel_config.rgb_ele_order = ESP_LCD_COLOR_SPACE_RGB;
    
    ret = esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle);
    
#elif defined(CONFIG_LCD_ST7789_240X240)
    // ST7789 240x240 configuration
    ESP_LOGI(TAG, "Initializing ST7789 LCD (240x240)");
    panel_config.rgb_ele_order = ESP_LCD_COLOR_SPACE_RGB;
    
    ret = esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle);
    
#elif defined(CONFIG_LCD_ILI9341_240X320)
    // ILI9341 configuration
    ESP_LOGI(TAG, "Initializing ILI9341 LCD (240x320)");
    panel_config.rgb_ele_order = ESP_LCD_COLOR_SPACE_BGR;
    
    ret = esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle);
    
#elif defined(CONFIG_LCD_CUSTOM)
    // Custom LCD - default to ST7796 for now
    ESP_LOGI(TAG, "Initializing Custom LCD (%dx%d) - using ST7796 driver", LCD_H_RES, LCD_V_RES);
    panel_config.rgb_ele_order = ESP_LCD_COLOR_SPACE_BGR;
    
    st7796_vendor_config_t vendor_config = {
        .init_cmds = st7796u_init_cmds,
        .init_cmds_size = sizeof(st7796u_init_cmds) / sizeof(st7796_lcd_init_cmd_t),
    };
    panel_config.vendor_config = &vendor_config;

    ret = esp_lcd_new_panel_st7796(io_handle, &panel_config, &panel_handle);
    
#else
    // Default fallback to ST7796
    ESP_LOGW(TAG, "No LCD type selected, defaulting to ST7796");
    panel_config.rgb_ele_order = ESP_LCD_COLOR_SPACE_BGR;
    
    st7796_vendor_config_t vendor_config = {
        .init_cmds = st7796u_init_cmds,
        .init_cmds_size = sizeof(st7796u_init_cmds) / sizeof(st7796_lcd_init_cmd_t),
    };
    panel_config.vendor_config = &vendor_config;

    ret = esp_lcd_new_panel_st7796(io_handle, &panel_config, &panel_handle);
#endif

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel: %s", esp_err_to_name(ret));
        // Cleanup: Delete panel IO handle if panel creation failed
        if (io_handle != NULL) {
            esp_lcd_panel_io_del(io_handle);
            handles.io_handle = NULL;
        }
        s_backlight_initialized = false; // Reset backlight state on failure
        return handles; // Return handles with io_handle cleaned up
    }
    handles.panel_handle = panel_handle;

    // Reset and initialize the panel
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // Turn on display
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Display platform initialized successfully.");
    return handles;
}

esp_err_t sx_platform_set_brightness(uint8_t percent) {
    if (!s_backlight_initialized) {
        ESP_LOGW(TAG, "Backlight not initialized, using GPIO fallback");
        // Fallback to GPIO if PWM not initialized
        gpio_config_t bk_gpio_config = {
            .pin_bit_mask = 1ULL << LCD_PIN_NUM_BK_LIGHT,
            .mode = GPIO_MODE_OUTPUT,
        };
        gpio_config(&bk_gpio_config);
        gpio_set_level(LCD_PIN_NUM_BK_LIGHT, (percent > 0) ? 1 : 0);
        s_current_brightness = percent;
        return ESP_OK;
    }

    if (percent > 100) {
        percent = 100;
    }

    // Convert percent (0-100) to duty cycle (0-8191 for 13-bit resolution)
    uint32_t duty = (uint32_t)((percent * ((1 << LEDC_DUTY_RES) - 1)) / 100);
    
    esp_err_t ret = ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LEDC duty: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update LEDC duty: %s", esp_err_to_name(ret));
        return ret;
    }

    s_current_brightness = percent;
    ESP_LOGI(TAG, "Brightness set to %d%% (duty: %lu)", percent, duty);
    return ESP_OK;
}

uint8_t sx_platform_get_brightness(void) {
    return s_current_brightness;
}

// Static I2C bus handle for touch controller
static i2c_master_bus_handle_t s_touch_i2c_bus_handle = NULL;

// Phase 3: Platform initialization for touch hardware
esp_err_t sx_platform_touch_init(sx_touch_handles_t *touch_handles) {
    if (touch_handles == NULL) {
        ESP_LOGE(TAG, "[TOUCH] ERROR: touch_handles is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Zero-initialize handles to ensure clean state
    touch_handles->touch_handle = NULL;
    
#if !CONFIG_HAI_TOUCH_ENABLE
    ESP_LOGI(TAG, "[TOUCH] Touch panel disabled in Kconfig, skipping initialization");
    return ESP_OK;
#endif
    
    ESP_LOGI(TAG, "[TOUCH] Starting touch panel initialization...");
    ESP_LOGI(TAG, "[TOUCH] Touch config: SDA=%d, SCL=%d, INT=%d, RST=%d, I2C_BUS=%d, FREQ=%d Hz",
             CTP_I2C_SDA_GPIO, CTP_I2C_SCL_GPIO, CTP_INT_GPIO, CTP_RST_GPIO, 
             CTP_I2C_BUS_NUM, CTP_I2C_FREQ_HZ);
    
    // Initialize I2C bus for touch controller (GPIO 8/11)
    if (s_touch_i2c_bus_handle == NULL) {
        i2c_master_bus_config_t i2c_bus_config = {
            .i2c_port = CTP_I2C_BUS_NUM,
            .sda_io_num = CTP_I2C_SDA_GPIO,
            .scl_io_num = CTP_I2C_SCL_GPIO,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags = {
                .enable_internal_pullup = true,
            },
        };
        
        ESP_LOGI(TAG, "[TOUCH] Creating I2C master bus for touch (port=%d, SDA=%d, SCL=%d, freq=%d Hz)...", 
                 i2c_bus_config.i2c_port, i2c_bus_config.sda_io_num, i2c_bus_config.scl_io_num, CTP_I2C_FREQ_HZ);
        esp_err_t ret = i2c_new_master_bus(&i2c_bus_config, &s_touch_i2c_bus_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[TOUCH] ERROR: Failed to create I2C master bus for touch: %s (error code: 0x%x)", 
                     esp_err_to_name(ret), ret);
            ESP_LOGE(TAG, "[TOUCH] ERROR: I2C config: port=%d, sda=%d, scl=%d, glitch_ignore=%d", 
                     i2c_bus_config.i2c_port, i2c_bus_config.sda_io_num, i2c_bus_config.scl_io_num, 
                     i2c_bus_config.glitch_ignore_cnt);
            return ret;
        }
        ESP_LOGI(TAG, "[TOUCH] Touch I2C bus initialized successfully (handle=%p, GPIO SDA=%d, SCL=%d)", 
                 (void*)s_touch_i2c_bus_handle, CTP_I2C_SDA_GPIO, CTP_I2C_SCL_GPIO);
    } else {
        ESP_LOGI(TAG, "[TOUCH] Touch I2C bus already initialized (handle=%p), reusing existing bus", 
                 (void*)s_touch_i2c_bus_handle);
    }
    
    // Configure touch panel IO (match repo mẫu xiaozhi-esp32)
    ESP_LOGI(TAG, "[TOUCH] Configuring touch panel IO (FT5x06)...");
    
    // Verify macro is defined (compile-time check)
    #ifndef ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS
    #error "ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS is not defined! Check esp_lcd_touch_ft5x06.h include"
    #endif
    
    ESP_LOGI(TAG, "[TOUCH] FT5x06 I2C address: 0x%02X", ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS);
    
    // Use macro like repo mẫu to ensure all fields are initialized correctly
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    // Override I2C clock frequency (required for ESP-IDF v5)
    io_config.scl_speed_hz = CTP_I2C_FREQ_HZ;
    
    ESP_LOGI(TAG, "[TOUCH] Touch IO config: dev_addr=0x%02X, control_phase_bytes=%d, lcd_cmd_bits=%d, scl_speed_hz=%d", 
             io_config.dev_addr, io_config.control_phase_bytes, io_config.lcd_cmd_bits, io_config.scl_speed_hz);
    
    esp_lcd_panel_io_handle_t touch_io_handle = NULL;
    ESP_LOGI(TAG, "[TOUCH] Creating touch panel IO (i2c_bus_handle=%p)...", (void*)s_touch_i2c_bus_handle);
    if (s_touch_i2c_bus_handle == NULL) {
        ESP_LOGE(TAG, "[TOUCH] ERROR: I2C bus handle is NULL! I2C bus creation must have failed earlier.");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = esp_lcd_new_panel_io_i2c(s_touch_i2c_bus_handle, &io_config, &touch_io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[TOUCH] ERROR: Failed to create touch panel IO: %s (error code: 0x%x)", 
                 esp_err_to_name(ret), ret);
        ESP_LOGE(TAG, "[TOUCH] ERROR: I2C bus handle=%p, io_config.dev_addr=0x%02X", 
                 (void*)s_touch_i2c_bus_handle, io_config.dev_addr);
        // Cleanup I2C bus on failure
        if (s_touch_i2c_bus_handle != NULL) {
            i2c_del_master_bus(s_touch_i2c_bus_handle);
            s_touch_i2c_bus_handle = NULL;
        }
        return ret;
    }
    ESP_LOGI(TAG, "[TOUCH] Touch panel IO created successfully (handle=%p)", (void*)touch_io_handle);
    
    // Configure touch panel
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = CTP_RST_GPIO,
        .int_gpio_num = CTP_INT_GPIO,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    
    // Create FT5x06 touch panel handle
    ESP_LOGI(TAG, "[TOUCH] Creating FT5x06 touch panel driver (x_max=%d, y_max=%d, rst_gpio=%d, int_gpio=%d)...", 
             tp_cfg.x_max, tp_cfg.y_max, tp_cfg.rst_gpio_num, tp_cfg.int_gpio_num);
    esp_lcd_touch_handle_t touch_handle = NULL;
    ret = esp_lcd_touch_new_i2c_ft5x06(touch_io_handle, &tp_cfg, &touch_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[TOUCH] ERROR: Failed to create FT5x06 touch panel: %s (error code: 0x%x)", 
                 esp_err_to_name(ret), ret);
        ESP_LOGE(TAG, "[TOUCH] ERROR: touch_io_handle=%p, tp_cfg: x_max=%d, y_max=%d, rst=%d, int=%d", 
                 (void*)touch_io_handle, tp_cfg.x_max, tp_cfg.y_max, tp_cfg.rst_gpio_num, tp_cfg.int_gpio_num);
        // Cleanup on failure (match repo mẫu xiaozhi-esp32)
        if (touch_io_handle != NULL) {
            esp_lcd_panel_io_del(touch_io_handle);
        }
        if (s_touch_i2c_bus_handle != NULL) {
            i2c_del_master_bus(s_touch_i2c_bus_handle);
            s_touch_i2c_bus_handle = NULL;
        }
        return ret;
    }
    
    touch_handles->touch_handle = touch_handle;
    
    ESP_LOGI(TAG, "[TOUCH] ✓ Touch driver initialized successfully - FT5x06");
    ESP_LOGI(TAG, "[TOUCH]   Touch handle: %p", (void*)touch_handle);
    ESP_LOGI(TAG, "[TOUCH]   GPIO pins: INT=%d, SDA=%d, RST=%d, SCL=%d",
             CTP_INT_GPIO, CTP_I2C_SDA_GPIO, CTP_RST_GPIO, CTP_I2C_SCL_GPIO);
    ESP_LOGI(TAG, "[TOUCH]   Screen size: %dx%d", tp_cfg.x_max, tp_cfg.y_max);
    ESP_LOGI(TAG, "[TOUCH]   I2C bus: port=%d, freq=%d Hz", CTP_I2C_BUS_NUM, CTP_I2C_FREQ_HZ);
    ESP_LOGE(TAG, "[TOUCH] ===== sx_platform_touch_init() SUCCESS EXIT =====");
    return ESP_OK;
}
