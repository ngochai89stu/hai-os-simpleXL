#pragma once
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* TaskHandle_t;

typedef void (*TaskFunction_t)(void *);

static inline int xTaskCreate(TaskFunction_t pxTaskCode, const char *const pcName,
                              unsigned short usStackDepth, void *pvParameters,
                              unsigned portBASE_TYPE uxPriority, TaskHandle_t *pxCreatedTask) {
    // No-op stub
    (void)pxTaskCode; (void)pcName; (void)usStackDepth; (void)pvParameters;
    (void)uxPriority; (void)pxCreatedTask;
    return 1; // Success
}

static inline void vTaskDelay(unsigned int ticks) { (void)ticks; }

#ifdef __cplusplus
}
#endif
