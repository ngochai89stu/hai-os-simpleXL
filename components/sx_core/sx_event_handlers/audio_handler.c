#include "sx_event_handler.h"
#include "sx_playlist_manager.h"
#include "sx_audio_service.h"  // Phase 2: Gapless playback - direct play
#include <esp_log.h>

static const char *TAG = "evt_handler_audio";

uint32_t sx_event_handler_audio_playback_stopped(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_AUDIO_PLAYBACK_STOPPED) {
        return 0;
    }
    
    // Phase 2: Gapless playback - use preloaded track if available
    const char *preloaded_track = sx_playlist_get_preloaded_track();
    if (preloaded_track != NULL && sx_playlist_should_auto_play_next()) {
        ESP_LOGI(TAG, "Gapless: Playing preloaded next track: %s", preloaded_track);
        // Play preloaded track directly (minimize delay)
        esp_err_t ret = sx_audio_play_file(preloaded_track);
        if (ret == ESP_OK) {
            // Update playlist index to match preloaded track
            sx_playlist_next(); // This will update current_index
            ESP_LOGI(TAG, "Gapless: Seamless transition to next track");
        } else {
            ESP_LOGW(TAG, "Gapless: Failed to play preloaded track, falling back to normal next");
            sx_playlist_next();
        }
    } else if (sx_playlist_should_auto_play_next()) {
        // Fallback: normal auto-play (no preload)
        ESP_LOGI(TAG, "Auto-playing next track");
        esp_err_t ret = sx_playlist_next();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Auto-play next track failed: %s", esp_err_to_name(ret));
        }
    }
    // Phase 3: Return dirty_mask instead of bool
    return SX_STATE_DIRTY_AUDIO; // Audio domain changed
}











