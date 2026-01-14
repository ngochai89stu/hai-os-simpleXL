#include "sx_event_handler.h"
#include "sx_dispatcher.h"
#include "sx_tts_service.h"
#include "sx_event_string_pool.h"
#include <esp_log.h>
#include <string.h>

static const char *TAG = "evt_handler_stt_tts";

// Phase 1: Handle STT state update (from service to UI)
uint32_t sx_event_handler_stt_state_update(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_STT_STATE_UPDATE) {
        return 0;
    }
    
    bool active = (evt->arg0 != 0);
    state->ui.stt_active = active;
    
    // Phase 3: Return dirty_mask instead of bool
    return SX_STATE_DIRTY_UI; // STT state affects UI
}

// Phase 1: Handle TTS state update (from service to UI)
uint32_t sx_event_handler_tts_state_update(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_TTS_STATE_UPDATE) {
        return 0;
    }
    
    bool speaking = (evt->arg0 != 0);
    state->ui.tts_speaking = speaking;
    
    // Phase 3: Return dirty_mask instead of bool
    return SX_STATE_DIRTY_UI; // TTS state affects UI
}

// Phase 1: Handle TTS speak request from UI
uint32_t sx_event_handler_tts_speak_request(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_TTS_SPEAK_REQUEST) {
        return 0;
    }
    
    const char *text = (const char *)evt->ptr;
    if (text == NULL) {
        ESP_LOGE(TAG, "TTS speak request: text is NULL");
        return 0;
    }
    
    ESP_LOGI(TAG, "TTS speak request: %s", text);
    
    // Call TTS service
    esp_err_t ret = sx_tts_speak_simple(text);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TTS speak failed: %s", esp_err_to_name(ret));
    }
    
    // Free text string
    sx_event_free_string((char *)evt->ptr);
    
    // Phase 3: Return dirty_mask (0 = no state update)
    return 0; // State not updated here, only triggers TTS service
}
