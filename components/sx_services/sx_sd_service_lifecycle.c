#include "sx_service_if.h"
#include "sx_sd_service.h"
#include <esp_log.h>

static const char *TAG = "sd_lifecycle";

static esp_err_t sd_init_wrap(void) {
    // Default config: mount at /sdcard, host SPI3, example pins 47/21/12/10
    sx_sd_config_t cfg = {
        .mount_point = SX_SD_MOUNT_POINT,
        .spi_host    = SPI3_HOST,
        .miso_gpio   = 12,
        .mosi_gpio   = 47,
        .sclk_gpio   = 21,
        .cs_gpio     = 10,
    };
    return sx_sd_service_init(&cfg);
}
static esp_err_t sd_start_wrap(void) { return sx_sd_service_start(); }
static esp_err_t sd_stop_wrap(void)  { return sx_sd_service_stop(); }
static esp_err_t sd_deinit_wrap(void){ return ESP_OK; }
static esp_err_t sd_on_event(const sx_event_t *evt){ (void)evt; return ESP_ERR_NOT_SUPPORTED; }

static const sx_service_if_t s_sd_if = {
    .init     = sd_init_wrap,
    .start    = sd_start_wrap,
    .stop     = sd_stop_wrap,
    .deinit   = sd_deinit_wrap,
    .on_event = sd_on_event,
};
__attribute__((constructor)) static void register_sd_service(void){
    esp_err_t ret = sx_service_register("sd", &s_sd_if);
    if(ret!=ESP_OK){ESP_LOGE(TAG,"register sd fail:%s",esp_err_to_name(ret));}
}