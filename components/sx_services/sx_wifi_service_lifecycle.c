#include "sx_service_if.h"
#include "sx_wifi_service.h"
#include <esp_log.h>

static const char *TAG = "wifi_lifecycle";

// Forward declarations from sx_wifi_service
extern esp_err_t sx_wifi_service_init(const sx_wifi_config_t *cfg);
extern esp_err_t sx_wifi_service_start(void);
extern esp_err_t sx_wifi_service_stop(void);

// Lifecycle functions (no config yet, use defaults)
static esp_err_t wifi_init_wrap(void) {
    return sx_wifi_service_init(NULL);
}
static esp_err_t wifi_start_wrap(void) {
    return sx_wifi_service_start();
}
static esp_err_t wifi_stop_wrap(void) {
    return sx_wifi_service_stop();
}
static esp_err_t wifi_deinit_wrap(void) { return ESP_OK; }
static esp_err_t wifi_on_event(const sx_event_t *evt) { (void)evt; return ESP_ERR_NOT_SUPPORTED; }

static const sx_service_if_t s_wifi_if = {
    .init      = wifi_init_wrap,
    .start     = wifi_start_wrap,
    .stop      = wifi_stop_wrap,
    .deinit    = wifi_deinit_wrap,
    .on_event  = wifi_on_event,
};

// Constructor attribute ensures registration at startup before main()
__attribute__((constructor)) static void register_wifi_service(void) {
    esp_err_t ret = sx_service_register("wifi", &s_wifi_if);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi service: %s", esp_err_to_name(ret));
    }
}