#include "sx_event_handler.h"
#include "sx_playlist_manager.h"
#include <esp_log.h>

static const char *TAG = "evt_handler_audio";

bool sx_event_handler_audio_playback_stopped(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_AUDIO_PLAYBACK_STOPPED) {
        return false;
    }
    
    // Check if should auto-play next track
    if (sx_playlist_should_auto_play_next()) {
        ESP_LOGI(TAG, "Auto-playing next track");
        esp_err_t ret = sx_playlist_next();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Auto-play next track failed: %s", esp_err_to_name(ret));
        }
    }
    return true;
}









