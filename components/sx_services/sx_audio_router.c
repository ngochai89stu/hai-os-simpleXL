#include "sx_audio_router.h"
#include "sx_audio_service.h"
#include "sx_bluetooth_service.h"
#include <esp_log.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "sx_audio_router";

// Router state
static bool s_initialized = false;
static sx_audio_route_t s_routes[SX_AUDIO_SOURCE_COUNT] = {0};
static SemaphoreHandle_t s_mutex = NULL;

// Source names
static const char *s_source_names[SX_AUDIO_SOURCE_COUNT] = {
    "NONE",
    "SD_MUSIC",
    "RADIO",
    "ONLINE_MUSIC",
    "TTS",
    "EXTERNAL",
};

// Sink names
static const char *s_sink_names[SX_AUDIO_SINK_COUNT] = {
    "NONE",
    "I2S",
    "EXTERNAL",
    "BOTH",
};

esp_err_t sx_audio_router_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize default routes
    for (int i = 0; i < SX_AUDIO_SOURCE_COUNT; i++) {
        s_routes[i].source = (sx_audio_source_t)i;
        s_routes[i].sink = SX_AUDIO_SINK_I2S; // Default to I2S
        s_routes[i].enabled = true;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "Audio router initialized");
    return ESP_OK;
}

void sx_audio_router_deinit(void) {
    if (!s_initialized) {
        return;
    }
    
    if (s_mutex) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }
    
    s_initialized = false;
    ESP_LOGI(TAG, "Audio router deinitialized");
}

esp_err_t sx_audio_router_set_route(sx_audio_source_t source, sx_audio_sink_t sink) {
    if (!s_initialized || source >= SX_AUDIO_SOURCE_COUNT || sink >= SX_AUDIO_SINK_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    s_routes[source].sink = sink;
    
    xSemaphoreGive(s_mutex);
    
    ESP_LOGI(TAG, "Route set: %s -> %s", s_source_names[source], s_sink_names[sink]);
    return ESP_OK;
}

esp_err_t sx_audio_router_get_route(sx_audio_source_t source, sx_audio_sink_t *sink) {
    if (!s_initialized || source >= SX_AUDIO_SOURCE_COUNT || !sink) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    *sink = s_routes[source].sink;
    
    xSemaphoreGive(s_mutex);
    
    return ESP_OK;
}

esp_err_t sx_audio_router_enable_route(sx_audio_source_t source, bool enabled) {
    if (!s_initialized || source >= SX_AUDIO_SOURCE_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    s_routes[source].enabled = enabled;
    
    xSemaphoreGive(s_mutex);
    
    ESP_LOGI(TAG, "Route %s: %s", enabled ? "enabled" : "disabled", s_source_names[source]);
    return ESP_OK;
}

bool sx_audio_router_is_route_enabled(sx_audio_source_t source) {
    if (!s_initialized || source >= SX_AUDIO_SOURCE_COUNT) {
        return false;
    }
    
    bool enabled = false;
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        enabled = s_routes[source].enabled;
        xSemaphoreGive(s_mutex);
    }
    
    return enabled;
}

esp_err_t sx_audio_router_route_audio(sx_audio_source_t source, const int16_t *pcm,
                                      size_t samples, uint32_t sample_rate) {
    if (!s_initialized || source >= SX_AUDIO_SOURCE_COUNT || !pcm || samples == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    sx_audio_sink_t sink;
    bool enabled;
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    sink = s_routes[source].sink;
    enabled = s_routes[source].enabled;
    
    xSemaphoreGive(s_mutex);
    
    if (!enabled) {
        return ESP_OK; // Route disabled, silently ignore
    }
    
    // Route to appropriate sink
    if (sink == SX_AUDIO_SINK_I2S || sink == SX_AUDIO_SINK_BOTH) {
        // Route to I2S (internal audio service)
        extern esp_err_t sx_audio_service_feed_pcm(const int16_t *pcm, size_t samples, uint32_t sample_rate);
        esp_err_t ret = sx_audio_service_feed_pcm(pcm, samples, sample_rate);
        if (ret != ESP_OK && sink == SX_AUDIO_SINK_I2S) {
            return ret; // If I2S only and it failed, return error
        }
    }
    
    if (sink == SX_AUDIO_SINK_EXTERNAL || sink == SX_AUDIO_SINK_BOTH) {
        // Route to external sink (e.g., Bluetooth, USB audio)
        // External audio bridge implementation
        extern esp_err_t sx_bluetooth_service_feed_audio(const int16_t *pcm, size_t samples, uint32_t sample_rate);
        
        // Try Bluetooth first (if available and connected)
        extern bool sx_bluetooth_is_enabled(void);
        extern sx_bluetooth_state_t sx_bluetooth_get_state(void);
        
        if (sx_bluetooth_is_enabled() && sx_bluetooth_get_state() == SX_BT_STATE_CONNECTED) {
            esp_err_t bt_ret = sx_bluetooth_service_feed_audio(pcm, samples, sample_rate);
            if (bt_ret == ESP_OK) {
                ESP_LOGD(TAG, "Routed audio to Bluetooth (%zu samples)", samples);
            } else {
                ESP_LOGW(TAG, "Bluetooth audio feed failed: %s", esp_err_to_name(bt_ret));
            }
        } else {
            // Other external sinks (USB audio, etc.) can be added here
            ESP_LOGD(TAG, "External audio sink not available (Bluetooth not connected)");
        }
    }
    
    return ESP_OK;
}

const char *sx_audio_router_source_name(sx_audio_source_t source) {
    if (source >= SX_AUDIO_SOURCE_COUNT) {
        return "UNKNOWN";
    }
    return s_source_names[source];
}

const char *sx_audio_router_sink_name(sx_audio_sink_t sink) {
    if (sink >= SX_AUDIO_SINK_COUNT) {
        return "UNKNOWN";
    }
    return s_sink_names[sink];
}

