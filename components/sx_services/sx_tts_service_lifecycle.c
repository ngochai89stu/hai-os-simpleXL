#include "sx_service_if.h"
#include "sx_tts_service.h"
#include <esp_log.h>

static const char *TAG = "tts_lifecycle";

static esp_err_t tts_init_wrap(void){
    return sx_tts_service_init(NULL);
}
static esp_err_t tts_start_wrap(void){ return ESP_OK; }
static esp_err_t tts_stop_wrap(void){ return ESP_OK; }
static esp_err_t tts_deinit_wrap(void){ return ESP_OK; }
static esp_err_t tts_on_event(const sx_event_t *evt){ (void)evt; return ESP_ERR_NOT_SUPPORTED; }

static const sx_service_if_t s_tts_if={
    .init=tts_init_wrap,
    .start=tts_start_wrap,
    .stop=tts_stop_wrap,
    .deinit=tts_deinit_wrap,
    .on_event=tts_on_event,
};
__attribute__((constructor)) static void register_tts_service(void){
    esp_err_t r=sx_service_register("tts",&s_tts_if);
    if(r!=ESP_OK){ESP_LOGE(TAG,"register tts fail:%s",esp_err_to_name(r));}
}