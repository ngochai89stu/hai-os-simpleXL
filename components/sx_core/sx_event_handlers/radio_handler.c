#include "sx_event_handler.h"
#include "sx_event_string_pool.h"
#include <esp_log.h>
#include <string.h>

static const char *TAG = "evt_handler_radio";

bool sx_event_handler_radio_error(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_RADIO_ERROR) {
        return false;
    }
    
    const char *error_msg = (const char *)evt->ptr;
    if (error_msg != NULL) {
        ESP_LOGE(TAG, "Radio error: %s", error_msg);
        // Update state with error message
        state->seq++;
        state->ui.has_error = true;
        strncpy(state->ui.error_message, error_msg, SX_UI_ERROR_MSG_MAX_LEN - 1);
        state->ui.error_message[SX_UI_ERROR_MSG_MAX_LEN - 1] = '\0';
        state->ui.error_code = evt->arg0; // Use arg0 for error code if provided
        // Free error message copy (may be from pool or malloc)
        sx_event_free_string((char *)evt->ptr);
        return true;
    }
    return false;
}

