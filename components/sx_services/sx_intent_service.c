#include "sx_intent_service.h"

#include <esp_log.h>
#include <string.h>
#include <ctype.h>
#include "sx_radio_service.h"
#include "sx_audio_service.h"
#include "sx_wifi_service.h"

static const char *TAG = "sx_intent";

// Phase 2: Intent Engine v2 - Increased handler capacity
#define MAX_INTENT_HANDLERS 32  // Increased from 16 to support more intent types
static sx_intent_handler_t s_handlers[MAX_INTENT_HANDLERS] = {0};
static bool s_initialized = false;

// Helper function to convert string to lowercase
static void str_to_lower(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

// Helper function to check if text contains keyword
static bool contains_keyword(const char *text, const char *keyword) {
    return strstr(text, keyword) != NULL;
}

// Extract entity from text (extract full text after pattern)
static void extract_entity(const char *text, const char *pattern, char *entity, size_t entity_size) {
    const char *found = strstr(text, pattern);
    if (found != NULL) {
        found += strlen(pattern);
        // Skip whitespace
        while (*found == ' ' || *found == '\t') {
            found++;
        }
        // Extract until end of string (full entity, not just one word)
        size_t i = 0;
        while (*found != '\0' && *found != '\n' && *found != '\r' && i < entity_size - 1) {
            entity[i++] = *found++;
        }
        entity[i] = '\0';
        // Trim trailing whitespace
        while (i > 0 && (entity[i-1] == ' ' || entity[i-1] == '\t')) {
            entity[--i] = '\0';
        }
    } else {
        entity[0] = '\0';
    }
}

esp_err_t sx_intent_service_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    memset(s_handlers, 0, sizeof(s_handlers));
    s_initialized = true;
    
    ESP_LOGI(TAG, "Intent service initialized");
    return ESP_OK;
}

esp_err_t sx_intent_register_handler(sx_intent_type_t type, sx_intent_handler_t handler) {
    if (!s_initialized || type >= SX_INTENT_MAX || type >= MAX_INTENT_HANDLERS || handler == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    s_handlers[type] = handler;
    ESP_LOGI(TAG, "Intent handler registered for type %d", type);
    return ESP_OK;
}

esp_err_t sx_intent_parse(const char *text, sx_intent_t *intent) {
    if (text == NULL || intent == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Create lowercase copy for matching
    char lower_text[256];
    size_t len = strlen(text);
    if (len >= sizeof(lower_text)) {
        len = sizeof(lower_text) - 1;
    }
    strncpy(lower_text, text, len);
    lower_text[len] = '\0';
    str_to_lower(lower_text);
    
    memset(intent, 0, sizeof(sx_intent_t));
    intent->type = SX_INTENT_UNKNOWN;
    
    // Music intents
    if (contains_keyword(lower_text, "play music") || contains_keyword(lower_text, "phát nhạc")) {
        intent->type = SX_INTENT_MUSIC_PLAY;
        extract_entity(lower_text, "play music", intent->entity, sizeof(intent->entity));
        if (intent->entity[0] == '\0') {
            extract_entity(lower_text, "phát nhạc", intent->entity, sizeof(intent->entity));
        }
        return ESP_OK;
    }
    
    if (contains_keyword(lower_text, "stop music") || contains_keyword(lower_text, "dừng nhạc")) {
        intent->type = SX_INTENT_MUSIC_STOP;
        return ESP_OK;
    }
    
    if (contains_keyword(lower_text, "pause music") || contains_keyword(lower_text, "tạm dừng")) {
        intent->type = SX_INTENT_MUSIC_PAUSE;
        return ESP_OK;
    }
    
    // Phase 2: Intent Engine v2 - Music navigation intents
    if (contains_keyword(lower_text, "next track") || contains_keyword(lower_text, "bài tiếp") || 
        contains_keyword(lower_text, "bài sau") || contains_keyword(lower_text, "tiếp theo")) {
        intent->type = SX_INTENT_MUSIC_NEXT;
        return ESP_OK;
    }
    
    if (contains_keyword(lower_text, "previous track") || contains_keyword(lower_text, "bài trước") ||
        contains_keyword(lower_text, "bài trước đó") || contains_keyword(lower_text, "lùi lại")) {
        intent->type = SX_INTENT_MUSIC_PREVIOUS;
        return ESP_OK;
    }
    
    if (contains_keyword(lower_text, "resume music") || contains_keyword(lower_text, "tiếp tục phát") ||
        contains_keyword(lower_text, "phát tiếp")) {
        intent->type = SX_INTENT_MUSIC_RESUME;
        return ESP_OK;
    }
    
    // Radio intents
    if (contains_keyword(lower_text, "play radio") || contains_keyword(lower_text, "phát radio")) {
        intent->type = SX_INTENT_RADIO_PLAY;
        extract_entity(lower_text, "play radio", intent->entity, sizeof(intent->entity));
        if (intent->entity[0] == '\0') {
            extract_entity(lower_text, "phát radio", intent->entity, sizeof(intent->entity));
        }
        return ESP_OK;
    }
    
    if (contains_keyword(lower_text, "stop radio") || contains_keyword(lower_text, "dừng radio")) {
        intent->type = SX_INTENT_RADIO_STOP;
        return ESP_OK;
    }
    
    // Volume intents
    if (contains_keyword(lower_text, "volume up") || contains_keyword(lower_text, "tăng âm lượng")) {
        intent->type = SX_INTENT_VOLUME_UP;
        return ESP_OK;
    }
    
    if (contains_keyword(lower_text, "volume down") || contains_keyword(lower_text, "giảm âm lượng")) {
        intent->type = SX_INTENT_VOLUME_DOWN;
        return ESP_OK;
    }
    
    // Phase 2: Intent Engine v2 - Volume mute/unmute intents
    if (contains_keyword(lower_text, "mute") || contains_keyword(lower_text, "tắt tiếng") ||
        contains_keyword(lower_text, "im lặng")) {
        intent->type = SX_INTENT_VOLUME_MUTE;
        return ESP_OK;
    }
    
    if (contains_keyword(lower_text, "unmute") || contains_keyword(lower_text, "bật tiếng") ||
        contains_keyword(lower_text, "mở tiếng")) {
        intent->type = SX_INTENT_VOLUME_UNMUTE;
        return ESP_OK;
    }
    
    if (contains_keyword(lower_text, "volume") || contains_keyword(lower_text, "âm lượng")) {
        // Try to extract volume level
        const char *vol_str = strstr(lower_text, "volume");
        if (vol_str == NULL) {
            vol_str = strstr(lower_text, "âm lượng");
        }
        if (vol_str != NULL) {
            // Look for number after "volume" or "âm lượng"
            const char *num_start = vol_str;
            if (strstr(vol_str, "volume") == vol_str) {
                num_start = vol_str + 6;  // "volume"
            } else {
                num_start = vol_str + 8;  // "âm lượng"
            }
            while (*num_start == ' ' || *num_start == '\t') {
                num_start++;
            }
            if (*num_start >= '0' && *num_start <= '9') {
                intent->type = SX_INTENT_VOLUME_SET;
                intent->value = atoi(num_start);
                if (intent->value > 100) intent->value = 100;
                if (intent->value < 0) intent->value = 0;
                return ESP_OK;
            }
        }
    }
    
    // Phase 2: Intent Engine v2 - Seek position intent
    if (contains_keyword(lower_text, "seek") || contains_keyword(lower_text, "nhảy đến") ||
        contains_keyword(lower_text, "chuyển đến")) {
        // Try to extract position (seconds or percentage)
        const char *seek_str = strstr(lower_text, "seek");
        if (seek_str == NULL) {
            seek_str = strstr(lower_text, "nhảy đến");
            if (seek_str == NULL) {
                seek_str = strstr(lower_text, "chuyển đến");
            }
        }
        if (seek_str != NULL) {
            const char *num_start = seek_str;
            if (strstr(seek_str, "seek") == seek_str) {
                num_start = seek_str + 4;  // "seek"
            } else if (strstr(seek_str, "nhảy đến") == seek_str) {
                num_start = seek_str + 8;  // "nhảy đến"
            } else {
                num_start = seek_str + 9;  // "chuyển đến"
            }
            while (*num_start == ' ' || *num_start == '\t') {
                num_start++;
            }
            if (*num_start >= '0' && *num_start <= '9') {
                intent->type = SX_INTENT_SEEK_POSITION;
                intent->value = atoi(num_start);
                // Check if percentage (ends with "%" or "phần trăm")
                const char *pct_check = num_start;
                while (*pct_check >= '0' && *pct_check <= '9') pct_check++;
                if (*pct_check == '%' || strstr(pct_check, "phần trăm") != NULL) {
                    // Percentage mode (value is 0-100)
                    intent->value = (intent->value > 100) ? 100 : intent->value;
                }
                return ESP_OK;
            }
        }
    }
    
    // WiFi intents
    if (contains_keyword(lower_text, "connect wifi") || contains_keyword(lower_text, "kết nối wifi")) {
        intent->type = SX_INTENT_WIFI_CONNECT;
        // Extract SSID and password (simplified)
        extract_entity(lower_text, "connect wifi", intent->entity, sizeof(intent->entity));
        if (intent->entity[0] == '\0') {
            extract_entity(lower_text, "kết nối wifi", intent->entity, sizeof(intent->entity));
        }
        return ESP_OK;
    }
    
    if (contains_keyword(lower_text, "disconnect wifi") || contains_keyword(lower_text, "ngắt wifi")) {
        intent->type = SX_INTENT_WIFI_DISCONNECT;
        return ESP_OK;
    }
    
    ESP_LOGD(TAG, "Unknown intent: %s", text);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t sx_intent_execute(const char *text) {
    if (text == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    sx_intent_t intent;
    esp_err_t ret = sx_intent_parse(text, &intent);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Execute default handlers if no custom handler registered
    if (s_handlers[intent.type] != NULL) {
        return s_handlers[intent.type](&intent);
    }
    
    // Default handlers
    switch (intent.type) {
        case SX_INTENT_MUSIC_PLAY:
        case SX_INTENT_RADIO_PLAY:
            if (intent.entity[0] != '\0') {
                // Build stream URL and play
                char stream_url[512];
                snprintf(stream_url, sizeof(stream_url), "http://14.225.204.77:5005/stream_pcm?song=%s", intent.entity);
                return sx_radio_play_station(stream_url);
            }
            break;
            
        case SX_INTENT_MUSIC_STOP:
        case SX_INTENT_RADIO_STOP:
            sx_radio_stop_playback();
            sx_audio_stop();
            return ESP_OK;
            
        case SX_INTENT_MUSIC_PAUSE:
            sx_radio_pause();
            sx_audio_pause();
            return ESP_OK;
        
        // Phase 2: Intent Engine v2 - Music navigation handlers
        case SX_INTENT_MUSIC_RESUME:
            sx_radio_resume();
            sx_audio_resume();
            return ESP_OK;
            
        case SX_INTENT_MUSIC_NEXT:
            return sx_playlist_next();
            
        case SX_INTENT_MUSIC_PREVIOUS:
            return sx_playlist_previous();
            
        case SX_INTENT_VOLUME_UP: {
            uint8_t vol = sx_audio_get_volume();
            if (vol < 100) {
                vol += 10;
                if (vol > 100) vol = 100;
                return sx_audio_set_volume(vol);
            }
            return ESP_OK;
        }
        
        case SX_INTENT_VOLUME_DOWN: {
            uint8_t vol = sx_audio_get_volume();
            if (vol > 0) {
                if (vol < 10) {
                    vol = 0;
                } else {
                    vol -= 10;
                }
                return sx_audio_set_volume(vol);
            }
            return ESP_OK;
        }
        
        case SX_INTENT_VOLUME_SET:
            return sx_audio_set_volume(intent.value);
        
        // Phase 2: Intent Engine v2 - Volume mute/unmute handlers
        // Note: Using volume 0/restore approach since sx_audio_set_mute() may not exist
        case SX_INTENT_VOLUME_MUTE: {
            // Store current volume and set to 0
            static uint8_t s_saved_volume = 50;  // Default if not saved
            s_saved_volume = sx_audio_get_volume();
            return sx_audio_set_volume(0);
        }
        
        case SX_INTENT_VOLUME_UNMUTE: {
            // Restore saved volume (or use default 50)
            static uint8_t s_saved_volume = 50;
            return sx_audio_set_volume(s_saved_volume);
        }
        
        // Phase 2: Intent Engine v2 - Seek position handler
        case SX_INTENT_SEEK_POSITION:
            // Note: sx_audio_seek() may return ESP_ERR_NOT_SUPPORTED if not implemented
            return sx_audio_seek(intent.value);
            
        case SX_INTENT_WIFI_CONNECT:
            if (intent.entity[0] != '\0') {
                // Connect to WiFi (password would need to be provided separately)
                return sx_wifi_connect(intent.entity, "");
            }
            break;
            
        case SX_INTENT_WIFI_DISCONNECT:
            return sx_wifi_disconnect();
            
        default:
            ESP_LOGW(TAG, "No handler for intent type %d", intent.type);
            return ESP_ERR_NOT_SUPPORTED;
    }
    
    return ESP_OK;
}

