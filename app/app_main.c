#include <esp_err.h>
#include <esp_log.h>

#include "sx_bootstrap.h"

static const char *TAG = "app_main";

void app_main(void) {
    ESP_LOGI(TAG, "hai-os-simplexl starting...");
    ESP_ERROR_CHECK(sx_bootstrap_start());
}

