#pragma once
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int esp_log_level_t;
#define ESP_LOGI(tag, fmt, ...) printf("%s I: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) printf("%s E: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("%s W: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) printf("%s D: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI_MEMORY(tag, msg, data, len)
#define ESP_LOG_BUFFER_HEXDUMP(tag, buffer, len, level)

#ifdef __cplusplus
}
#endif
