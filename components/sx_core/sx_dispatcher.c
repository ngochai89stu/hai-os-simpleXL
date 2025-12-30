#include "sx_dispatcher.h"
#include "sx_event_string_pool.h"

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// Multi-producer event queue (services + UI), single-consumer (orchestrator).
static QueueHandle_t s_evt_q;

// State snapshot storage (single-writer, multi-reader with mutex for copy-out).
static sx_state_t s_state;
static StaticSemaphore_t s_state_mutex_buf;
static SemaphoreHandle_t s_state_mutex;

bool sx_dispatcher_init(void) {
    // Initialize event string pool for memory optimization
    sx_event_string_pool_init();
    
    if (s_evt_q == NULL) {
        // Optimized: Increased queue size from 32 to 64 for better throughput
        s_evt_q = xQueueCreate(64, sizeof(sx_event_t));
    }
    if (s_state_mutex == NULL) {
        s_state_mutex = xSemaphoreCreateMutexStatic(&s_state_mutex_buf);
    }
    if (s_state.seq == 0) {
        memset(&s_state, 0, sizeof(s_state));
        s_state.ui.device_state = SX_DEV_BOOTING;
        s_state.ui.status_text = "booting";
        s_state.ui.emotion_id = "neutral";
    }
    return (s_evt_q != NULL) && (s_state_mutex != NULL);
}

bool sx_dispatcher_post_event(const sx_event_t *evt) {
    if (!evt || s_evt_q == NULL) {
        return false;
    }
    return xQueueSend(s_evt_q, evt, 0) == pdTRUE;
}

bool sx_dispatcher_poll_event(sx_event_t *out_evt) {
    if (!out_evt || s_evt_q == NULL) {
        return false;
    }
    return xQueueReceive(s_evt_q, out_evt, 0) == pdTRUE;
}

void sx_dispatcher_set_state(const sx_state_t *state) {
    if (!state || s_state_mutex == NULL) {
        return;
    }
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    s_state = *state;
    xSemaphoreGive(s_state_mutex);
}

void sx_dispatcher_get_state(sx_state_t *out_state) {
    if (!out_state || s_state_mutex == NULL) {
        return;
    }
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    *out_state = s_state;
    xSemaphoreGive(s_state_mutex);
}
