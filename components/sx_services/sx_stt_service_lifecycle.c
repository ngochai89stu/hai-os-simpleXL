#include "sx_service_if.h"
#include "sx_stt_service.h"
#include <esp_log.h>

static const char *TAG = "stt_lifecycle";

static esp_err_t stt_init_wrap(void){ return sx_stt_service_init(NULL); }
static esp_err_t stt_start_wrap(void){ return ESP_OK; }
static esp_err_t stt_stop_wrap(void){ return sx_stt_stop_session(); }
static esp_err_t stt_deinit_wrap(void){ return ESP_OK; }
static esp_err_t stt_on_event(const sx_event_t *evt){ (void)evt; return ESP_ERR_NOT_SUPPORTED; }

static const sx_service_if_t s_stt_if={
    .init=stt_init_wrap,
    .start=stt_start_wrap,
    .stop=stt_stop_wrap,
    .deinit=stt_deinit_wrap,
    .on_event=stt_on_event,
};
__attribute__((constructor)) static void register_stt_service(void){
    esp_err_t r=sx_service_register("stt",&s_stt_if);
    if(r!=ESP_OK){ ESP_LOGE(TAG,"register stt fail:%s",esp_err_to_name(r)); }
}