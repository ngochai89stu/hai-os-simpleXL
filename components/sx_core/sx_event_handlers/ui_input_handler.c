#include "sx_event_handler.h"
#include "sx_chatbot_service.h"
#include "sx_event_string_pool.h"
#include <esp_log.h>
#include <string.h>

static const char *TAG = "evt_handler_ui_input";

bool sx_event_handler_ui_input(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_UI_INPUT) {
        return false;
    }
    
    state->seq++;
    
    if (evt->ptr != NULL) {
        const char *message = (const char *)evt->ptr;
        ESP_LOGI(TAG, "UI input message: %s", message);
        
        // Route to chatbot service (if ready)
        if (sx_chatbot_is_ready()) {
            esp_err_t ret = sx_chatbot_send_message(message);
            if (ret == ESP_OK) {
                state->ui.status_text = "sending...";
            } else {
                state->ui.status_text = "chat_error";
            }
        } else {
            state->ui.status_text = "chat_not_ready";
        }
        
        // Free message copy allocated by ChatScreen (may be from pool or malloc)
        sx_event_free_string((char *)evt->ptr);
    } else {
        state->ui.status_text = "ui_input";
    }
    
    return true;
}









