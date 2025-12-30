#include "sx_led_service.h"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_state_manager.h"
#include "sx_state.h"
#include "sx_audio_service.h"
#include "sx_wifi_service.h"

#include <esp_log.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
// Note: RMT driver migration in progress
// Legacy API temporarily kept for WS2812 support
// TODO: Migrate to new RMT encoder API (rmt_tx.h) when WS2812 encoder is implemented
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#include "driver/rmt.h"  // Legacy API - will be migrated to rmt_tx.h + encoder
#pragma GCC diagnostic pop
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "sx_led";

// LED service state
static bool s_initialized = false;
static sx_led_config_t s_config = {0};
static sx_led_state_t s_current_state = {0};
static bool s_state_based = false;

// WS2812 support (requires RMT driver)
// Note: Using legacy RMT API temporarily
// TODO: Migrate to new RMT encoder API when WS2812 encoder is implemented
static rmt_channel_t s_rmt_channel = RMT_CHANNEL_0;
static bool s_ws2812_initialized = false;
static uint8_t *s_ws2812_buffer = NULL;
static size_t s_ws2812_buffer_size = 0;

static esp_err_t sx_led_gpio_set(bool on) {
    if (s_config.type != SX_LED_TYPE_GPIO && s_config.type != SX_LED_TYPE_SINGLE) {
        return ESP_ERR_INVALID_STATE;
    }
    
    gpio_set_level(s_config.gpio_pin, s_config.inverted ? !on : on);
    return ESP_OK;
}

static esp_err_t sx_led_gpio_set_brightness(uint8_t brightness) {
    if (s_config.type != SX_LED_TYPE_GPIO && s_config.type != SX_LED_TYPE_SINGLE) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Use LEDC for PWM brightness control
    ledc_channel_config_t ledc_channel = {
        .channel = LEDC_CHANNEL_0,
        .duty = (brightness * 8191) / 100, // 13-bit resolution
        .gpio_num = s_config.gpio_pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
    };
    
    ledc_channel_config(&ledc_channel);
    return ESP_OK;
}

static void sx_led_state_based_task(void *arg) {
    (void)arg;
    
    ESP_LOGI(TAG, "State-based LED task started");
    
    sx_state_t last_state = {0};
    bool state_changed = false;
    
    while (s_state_based) {
        // Get current device state
        sx_state_t current_state;
        sx_dispatcher_get_state(&current_state);
        
        // Check if state changed
        if (memcmp(&current_state, &last_state, sizeof(sx_state_t)) != 0) {
            state_changed = true;
            last_state = current_state;
        }
        
        // Update LED based on device state
        if (state_changed) {
            sx_led_state_t led_state = {0};
            
            // Map device state to LED color/brightness
            // Check audio playing state
            bool audio_playing = current_state.audio.playing;
            if (audio_playing) {
                // Playing: Green
                led_state.r = 0;
                led_state.g = 255;
                led_state.b = 0;
                led_state.brightness = 50;
                led_state.on = true;
            } else if (current_state.wifi.connected) {
                // WiFi connected: Blue
                led_state.r = 0;
                led_state.g = 0;
                led_state.b = 255;
                led_state.brightness = 30;
                led_state.on = true;
            } else {
                // Idle: Dim white
                led_state.r = 255;
                led_state.g = 255;
                led_state.b = 255;
                led_state.brightness = 10;
                led_state.on = true;
            }
            
            sx_led_set_state(&led_state);
            state_changed = false;
        }
        
        vTaskDelay(pdMS_TO_TICKS(500)); // Check every 500ms
    }
    
    vTaskDelete(NULL);
}

esp_err_t sx_led_service_init(const sx_led_config_t *config) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    s_config = *config;
    
    // Initialize based on LED type
    if (s_config.type == SX_LED_TYPE_GPIO || s_config.type == SX_LED_TYPE_SINGLE) {
        // Configure GPIO
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << s_config.gpio_pin),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&io_conf);
        
        // Initialize LEDC for PWM
        ledc_timer_config_t ledc_timer = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .timer_num = LEDC_TIMER_0,
            .duty_resolution = LEDC_TIMER_13_BIT,
            .freq_hz = 5000,
            .clk_cfg = LEDC_AUTO_CLK,
        };
        ledc_timer_config(&ledc_timer);
        
        ESP_LOGI(TAG, "GPIO LED initialized on pin %d", s_config.gpio_pin);
    } else if (s_config.type == SX_LED_TYPE_WS2812) {
        // Initialize RMT for WS2812
        // Note: Using legacy RMT API temporarily
        // TODO: Migrate to new RMT encoder API (rmt_tx.h + rmt_encoder.h) when WS2812 encoder is implemented
        rmt_config_t rmt_cfg = {
            .rmt_mode = RMT_MODE_TX,
            .channel = s_rmt_channel,
            .gpio_num = s_config.gpio_pin,
            .clk_div = 80, // 80MHz / 80 = 1MHz (1us per tick)
            .mem_block_num = 1,
            .tx_config = {
                .loop_en = false,
                .carrier_freq_hz = 0, // No carrier
                .carrier_duty_percent = 0,
                .carrier_level = RMT_CARRIER_LEVEL_LOW,
                .carrier_en = false,
                .idle_level = RMT_IDLE_LEVEL_LOW,
                .idle_output_en = true,
            }
        };
        
        esp_err_t ret = rmt_config(&rmt_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure RMT: %s", esp_err_to_name(ret));
            return ret;
        }
        
        ret = rmt_driver_install(s_rmt_channel, 0, 0);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to install RMT driver: %s", esp_err_to_name(ret));
            return ret;
        }
        
        // Allocate buffer for WS2812 data
        // Each LED needs 24 bits (3 bytes), plus reset time
        s_ws2812_buffer_size = s_config.led_count * 3;
        s_ws2812_buffer = (uint8_t *)malloc(s_ws2812_buffer_size);
        if (s_ws2812_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate WS2812 buffer");
            rmt_driver_uninstall(s_rmt_channel);
            return ESP_ERR_NO_MEM;
        }
        
        s_ws2812_initialized = true;
        ESP_LOGI(TAG, "WS2812 LED initialized: %d LEDs on GPIO %d, RMT channel %d",
                 s_config.led_count, s_config.gpio_pin, s_rmt_channel);
    }
    
    s_current_state = (sx_led_state_t){.r = 255, .g = 255, .b = 255, .brightness = 100, .on = false};
    s_initialized = true;
    
    ESP_LOGI(TAG, "LED service initialized (type: %d)", s_config.type);
    return ESP_OK;
}

esp_err_t sx_led_set_state(const sx_led_state_t *state) {
    if (!s_initialized || state == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    s_current_state = *state;
    
    if (s_config.type == SX_LED_TYPE_GPIO || s_config.type == SX_LED_TYPE_SINGLE) {
        if (state->on) {
            sx_led_gpio_set_brightness(state->brightness);
            sx_led_gpio_set(true);
        } else {
            sx_led_gpio_set(false);
        }
    } else if (s_config.type == SX_LED_TYPE_WS2812) {
        if (!s_ws2812_initialized || s_ws2812_buffer == NULL) {
            return ESP_ERR_INVALID_STATE;
        }
        
        // Set WS2812 color for all LEDs
        // WS2812 expects GRB order, not RGB
        uint8_t grb[3] = {state->g, state->r, state->b};
        
        // Apply brightness
        if (state->on && state->brightness > 0) {
            for (int i = 0; i < 3; i++) {
                grb[i] = (grb[i] * state->brightness) / 100;
            }
        } else {
            memset(grb, 0, 3);
        }
        
        // Fill buffer with color for all LEDs
        for (int i = 0; i < s_config.led_count; i++) {
            memcpy(&s_ws2812_buffer[i * 3], grb, 3);
        }
        
        // Convert to RMT items
        // WS2812 timing: 0 = 0.3us high + 0.9us low, 1 = 0.6us high + 0.6us low
        // With 1MHz clock: 0 = 0.3 ticks high + 0.9 ticks low, 1 = 0.6 ticks high + 0.6 ticks low
        // We'll use: 0 = 0.3us high + 0.9us low, 1 = 0.6us high + 0.6us low
        size_t rmt_items_size = s_config.led_count * 24; // 24 bits per LED
        rmt_item32_t *rmt_items = (rmt_item32_t *)malloc(rmt_items_size * sizeof(rmt_item32_t));
        if (rmt_items == NULL) {
            ESP_LOGE(TAG, "Failed to allocate RMT items");
            return ESP_ERR_NO_MEM;
        }
        
        int item_idx = 0;
        for (int led = 0; led < s_config.led_count; led++) {
            uint8_t *led_data = &s_ws2812_buffer[led * 3];
            for (int bit = 0; bit < 24; bit++) {
                int byte_idx = bit / 8;
                int bit_idx = 7 - (bit % 8);
                bool bit_val = (led_data[byte_idx] >> bit_idx) & 1;
                
                if (bit_val) {
                    // Bit 1: 0.6us high + 0.6us low
                    rmt_items[item_idx].level0 = 1;
                    rmt_items[item_idx].duration0 = 6; // 0.6us
                    rmt_items[item_idx].level1 = 0;
                    rmt_items[item_idx].duration1 = 6; // 0.6us
                } else {
                    // Bit 0: 0.3us high + 0.9us low
                    rmt_items[item_idx].level0 = 1;
                    rmt_items[item_idx].duration0 = 3; // 0.3us
                    rmt_items[item_idx].level1 = 0;
                    rmt_items[item_idx].duration1 = 9; // 0.9us
                }
                item_idx++;
            }
        }
        
        // Send RMT data
        esp_err_t ret = rmt_write_items(s_rmt_channel, rmt_items, rmt_items_size, true);
        free(rmt_items);
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write RMT items: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    
    return ESP_OK;
}

esp_err_t sx_led_set_color(uint8_t r, uint8_t g, uint8_t b) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    s_current_state.r = r;
    s_current_state.g = g;
    s_current_state.b = b;
    
    // For GPIO/Single LED, color is not supported (only brightness)
    // For WS2812, set RGB color
    if (s_config.type == SX_LED_TYPE_WS2812) {
        sx_led_state_t state = s_current_state;
        state.r = r;
        state.g = g;
        state.b = b;
        return sx_led_set_state(&state);
    }
    
    return ESP_OK;
}

esp_err_t sx_led_set_brightness(uint8_t brightness) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (brightness > 100) brightness = 100;
    s_current_state.brightness = brightness;
    
    if (s_config.type == SX_LED_TYPE_GPIO || s_config.type == SX_LED_TYPE_SINGLE) {
        return sx_led_gpio_set_brightness(brightness);
    }
    
    return ESP_OK;
}

esp_err_t sx_led_set_on(bool on) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    s_current_state.on = on;
    
    if (s_config.type == SX_LED_TYPE_GPIO || s_config.type == SX_LED_TYPE_SINGLE) {
        return sx_led_gpio_set(on);
    }
    
    return ESP_OK;
}

esp_err_t sx_led_get_state(sx_led_state_t *state) {
    if (!s_initialized || state == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    *state = s_current_state;
    return ESP_OK;
}

esp_err_t sx_led_set_state_based(bool enabled) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    s_state_based = enabled;
    
    if (enabled) {
        // Create state-based LED task
        xTaskCreate(sx_led_state_based_task, "led_state", 2048, NULL, 3, NULL);
    }
    
    return ESP_OK;
}


