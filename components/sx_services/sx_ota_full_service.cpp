#include "sx_ota_full_service.h"  // C API
// C++ implementation wrapper
#include "sx_ota_full.hpp"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_event_string_pool.h"
#include <esp_log.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sx_ota_full_srv";
static bool s_initialized = false;

// Activation polling task
static TaskHandle_t s_activation_task = NULL;
static const int s_activation_max_attempts = 30; // ~ up to ~2-3 minutes depending on backoff

static void activation_poll_task(void *arg) {
    (void)arg;
    int attempt = 0;
    int delay_ms = 2000; // start with 2s

    ESP_LOGI(TAG, "Activation polling started");

    while (attempt < s_activation_max_attempts) {
        attempt++;
        esp_err_t ret = SxOtaFull::instance().activate();

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Activation polling success (attempt %d)", attempt);
            break;
        }
        if (ret != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "Activation polling failed (attempt %d): %s", attempt, esp_err_to_name(ret));
            // activate() already emits SX_EVT_OTA_ERROR on failure
            break;
        }

        // Emit UI event: activation pending
        sx_event_t pending_evt = {
            .type = SX_EVT_ACTIVATION_PENDING,
            .priority = SX_EVT_PRIORITY_NORMAL,
            .arg0 = (uint32_t)attempt,
            .arg1 = (uint32_t)delay_ms,
            .ptr = NULL,
        };
        sx_dispatcher_post_event(&pending_evt);

        ESP_LOGI(TAG, "Activation still pending (202), retry in %d ms (attempt %d/%d)", delay_ms, attempt, s_activation_max_attempts);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));

        // Exponential backoff capped at 10s
        if (delay_ms < 10000) {
            delay_ms = delay_ms * 2;
            if (delay_ms > 10000) delay_ms = 10000;
        }
    }

    if (attempt >= s_activation_max_attempts) {
        ESP_LOGW(TAG, "Activation polling reached max attempts");
        sx_event_t evt = {
            .type = SX_EVT_ACTIVATION_TIMEOUT,
            .priority = SX_EVT_PRIORITY_HIGH,
            .arg0 = (uint32_t)attempt,
            .arg1 = 0,
            .ptr = NULL,
        };
        sx_dispatcher_post_event(&evt);
    }

    s_activation_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t sx_ota_full_service_init(void) {
    if (s_initialized) return ESP_OK;
    s_initialized = true;
    ESP_LOGI(TAG, "OTA full service init");
    return ESP_OK;
}

esp_err_t sx_ota_full_check_version(void) {
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    return SxOtaFull::instance().checkVersion();
}

esp_err_t sx_ota_full_start_upgrade(const char *url) {
    if (!s_initialized || !url) return ESP_ERR_INVALID_ARG;
    return SxOtaFull::instance().startUpgrade(url);
}

bool sx_ota_full_has_activation(void) {
    return SxOtaFull::instance().hasActivation();
}

const char *sx_ota_full_get_activation_code(void) {
    return SxOtaFull::instance().getActivationCode().c_str();
}

esp_err_t sx_ota_full_activate(void) {
    esp_err_t ret = SxOtaFull::instance().activate();

    // If server returns 202 (mapped to ESP_ERR_TIMEOUT), start polling in background
    if (ret == ESP_ERR_TIMEOUT) {
        if (s_activation_task == NULL) {
            BaseType_t ok = xTaskCreate(
                activation_poll_task,
                "sx_act_poll",
                4096,
                NULL,
                tskIDLE_PRIORITY + 1,
                &s_activation_task
            );
            if (ok != pdPASS) {
                s_activation_task = NULL;
                ESP_LOGE(TAG, "Failed to start activation polling task");
                return ESP_ERR_NO_MEM;
            }
        }
    }

    return ret;
}

