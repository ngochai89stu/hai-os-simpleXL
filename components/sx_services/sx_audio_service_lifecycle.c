/**
 * @file sx_audio_service_lifecycle.c
 * @brief Audio service lifecycle interface implementation (OPT-2)
 * 
 * This file provides an example migration of sx_audio_service to use
 * the lifecycle interface (sx_service_if.h) defined in P1.4.
 */

#include "sx_audio_service.h"
#include "sx_service_if.h"
#include "sx_event.h"
#include <esp_log.h>

static const char *TAG = "sx_audio_lifecycle";

// Lifecycle interface implementation
static esp_err_t audio_service_init(void) {
    ESP_LOGI(TAG, "Audio service init (lifecycle)");
    return sx_audio_service_init();
}

static esp_err_t audio_service_start(void) {
    ESP_LOGI(TAG, "Audio service start (lifecycle)");
    return sx_audio_service_start();
}

static esp_err_t audio_service_stop(void) {
    ESP_LOGI(TAG, "Audio service stop (lifecycle)");
    // Note: sx_audio_service doesn't have a stop function yet
    // This is a placeholder - can be implemented if needed
    return ESP_OK;
}

static esp_err_t audio_service_deinit(void) {
    ESP_LOGI(TAG, "Audio service deinit (lifecycle)");
    // Note: sx_audio_service doesn't have a deinit function yet
    // This is a placeholder - can be implemented if needed
    return ESP_OK;
}

static esp_err_t audio_service_on_event(const sx_event_t *evt) {
    if (!evt) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Audio service can handle events here if needed
    // For now, audio service handles events internally
    // This is a placeholder for future event handling
    (void)evt;
    return ESP_ERR_NOT_SUPPORTED;
}

// Service interface vtable
static const sx_service_if_t s_audio_service_if = {
    .init = audio_service_init,
    .start = audio_service_start,
    .stop = audio_service_stop,
    .deinit = audio_service_deinit,
    .on_event = audio_service_on_event,
};

// Registration function (call this during bootstrap)
esp_err_t sx_audio_service_register_lifecycle(void) {
    return sx_service_register("audio", &s_audio_service_if);
}






