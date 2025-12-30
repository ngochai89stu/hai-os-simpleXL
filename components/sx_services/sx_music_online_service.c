#include "sx_music_online_service.h"
#include "sx_settings_service.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "esp_http_client.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_mac.h"
#include "mbedtls/sha256.h"
#include "cJSON.h"
#include "sx_wifi_service.h"

static const char *TAG = "sx_music_online";

static bool s_initialized = false;
static sx_music_online_display_mode_t s_display_mode = SX_MUSIC_ONLINE_DISPLAY_INFO;
static sx_music_online_track_t s_current_track = {0};
static sx_music_online_auth_config_t s_auth_config = {0};
static char s_auth_key[128] = {0}; // Buffer for auth key from settings

// HTTP event handler for lyrics download
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t sx_music_online_service_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    s_display_mode = SX_MUSIC_ONLINE_DISPLAY_INFO;
    memset(&s_current_track, 0, sizeof(s_current_track));
    memset(&s_auth_config, 0, sizeof(s_auth_config));
    memset(s_auth_key, 0, sizeof(s_auth_key));
    
    // Load authentication key from settings
    if (sx_settings_get_string_default("music_online_auth_key", s_auth_key,
                                       sizeof(s_auth_key), NULL) == ESP_OK &&
        s_auth_key[0] != '\0') {
        s_auth_config.enable_auth = true;
        s_auth_config.auth_key = s_auth_key;
        ESP_LOGI(TAG, "Music online authentication enabled (key loaded from settings)");
    } else {
        s_auth_config.enable_auth = false;
        s_auth_config.auth_key = NULL;
        ESP_LOGI(TAG, "Music online authentication disabled (no key in settings)");
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "Music online service initialized");
    return ESP_OK;
}

esp_err_t sx_music_online_set_display_mode(sx_music_online_display_mode_t mode) {
    if (mode >= 2) {  // Only 2 modes for now
        return ESP_ERR_INVALID_ARG;
    }
    s_display_mode = mode;
    ESP_LOGI(TAG, "Display mode set to %d", mode);
    return ESP_OK;
}

sx_music_online_display_mode_t sx_music_online_get_display_mode(void) {
    return s_display_mode;
}

// Download lyrics from a lyrics API (simplified - using a placeholder API)
esp_err_t sx_music_online_download_lyrics(const char *track_title, const char *artist, 
                                          sx_music_online_lyrics_t *lyrics) {
    if (!track_title || !artist || !lyrics) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!sx_wifi_is_connected()) {
        ESP_LOGW(TAG, "WiFi not connected, cannot download lyrics");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Initialize lyrics structure
    memset(lyrics, 0, sizeof(*lyrics));
    
    // Build API URL (using a placeholder - would need actual lyrics API)
    // Example: https://api.lyrics.ovh/v1/{artist}/{title}
    char url[512];
    // URL encode would be needed here
    snprintf(url, sizeof(url), "https://api.lyrics.ovh/v1/%s/%s", artist, track_title);
    
    ESP_LOGI(TAG, "Downloading lyrics from: %s", url);
    
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .timeout_ms = 10000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return ESP_ERR_NO_MEM;
    }
    
    // Add authentication headers if enabled
    sx_music_online_add_auth_headers(client);
    
    // Allocate buffer for response
    // Tối ưu: Giảm buffer size từ 8192 xuống 4096 để tiết kiệm memory
    char *response_buffer = (char *)malloc(4096);
    if (!response_buffer) {
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t ret = esp_http_client_perform(client);
    if (ret == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);
        
        if (status_code == 200 && content_length > 0) {
            // Tối ưu: Giảm buffer size từ 8191 xuống 4095 để phù hợp với buffer mới
            int data_read = esp_http_client_read_response(client, response_buffer, 4095);
            if (data_read > 0) {
                response_buffer[data_read] = '\0';
                
                // Parse JSON response
                cJSON *json = cJSON_Parse(response_buffer);
                if (json) {
                    cJSON *lyrics_json = cJSON_GetObjectItemCaseSensitive(json, "lyrics");
                    if (cJSON_IsString(lyrics_json)) {
                        const char *lyrics_text = lyrics_json->valuestring;
                        size_t lyrics_len = strlen(lyrics_text);
                        
                        lyrics->text = (char *)malloc(lyrics_len + 1);
                        if (lyrics->text) {
                            strcpy(lyrics->text, lyrics_text);
                            lyrics->text_len = lyrics_len;
                            lyrics->synced = false;  // Simple API doesn't provide synced lyrics
                            lyrics->timestamps = NULL;
                            
                            // Count lines
                            lyrics->line_count = 1;
                            for (size_t i = 0; i < lyrics_len; i++) {
                                if (lyrics->text[i] == '\n') {
                                    lyrics->line_count++;
                                }
                            }
                            
                            ESP_LOGI(TAG, "Downloaded lyrics: %zu bytes, %zu lines", lyrics_len, lyrics->line_count);
                            ret = ESP_OK;
                        } else {
                            ret = ESP_ERR_NO_MEM;
                        }
                    } else {
                        ESP_LOGW(TAG, "No lyrics field in response");
                        ret = ESP_ERR_NOT_FOUND;
                    }
                    cJSON_Delete(json);
                } else {
                    ESP_LOGW(TAG, "Failed to parse JSON response");
                    ret = ESP_FAIL;
                }
            } else {
                ESP_LOGW(TAG, "No data read from response");
                ret = ESP_FAIL;
            }
        } else {
            ESP_LOGW(TAG, "HTTP status %d, content length %d", status_code, content_length);
            ret = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(ret));
    }
    
    free(response_buffer);
    esp_http_client_cleanup(client);
    
    return ret;
}

void sx_music_online_free_lyrics(sx_music_online_lyrics_t *lyrics) {
    if (!lyrics) {
        return;
    }
    
    if (lyrics->text) {
        free(lyrics->text);
        lyrics->text = NULL;
    }
    
    if (lyrics->timestamps) {
        free(lyrics->timestamps);
        lyrics->timestamps = NULL;
    }
    
    lyrics->text_len = 0;
    lyrics->line_count = 0;
    lyrics->synced = false;
}

const sx_music_online_track_t* sx_music_online_get_current_track(void) {
    return &s_current_track;
}

esp_err_t sx_music_online_set_current_track(const sx_music_online_track_t *track) {
    if (!track) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(&s_current_track, track, sizeof(s_current_track));
    ESP_LOGI(TAG, "Current track set: %s - %s", track->artist, track->title);
    return ESP_OK;
}

esp_err_t sx_music_online_play_song(const char *song_name, const char *artist_name) {
    if (!s_initialized || !song_name) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!sx_wifi_is_connected()) {
        ESP_LOGW(TAG, "WiFi not connected, cannot play online music");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Build API URL to get song stream URL
    // This is a placeholder - actual implementation would call music API
    // to get the stream URL based on song_name and artist_name
    char api_url[512];
    if (artist_name && artist_name[0] != '\0') {
        snprintf(api_url, sizeof(api_url), "https://api.example.com/music/play?song=%s&artist=%s",
                 song_name, artist_name);
    } else {
        snprintf(api_url, sizeof(api_url), "https://api.example.com/music/play?song=%s",
                 song_name);
    }
    
    ESP_LOGI(TAG, "Requesting stream URL: %s", api_url);
    
    // Create HTTP client for API request
    esp_http_client_config_t config = {
        .url = api_url,
        .timeout_ms = 10000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return ESP_ERR_NO_MEM;
    }
    
    // Add authentication headers
    sx_music_online_add_auth_headers(client);
    
    // Perform HTTP request to get stream URL
    esp_err_t ret = esp_http_client_perform(client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get stream URL: %s", esp_err_to_name(ret));
        esp_http_client_cleanup(client);
        return ret;
    }
    
    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        ESP_LOGE(TAG, "API returned status code: %d", status_code);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    
    // Read response (JSON with stream URL)
    char response_buffer[1024];
    int data_read = esp_http_client_read_response(client, response_buffer, sizeof(response_buffer) - 1);
    if (data_read <= 0) {
        ESP_LOGE(TAG, "Failed to read API response");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    
    response_buffer[data_read] = '\0';
    esp_http_client_cleanup(client);
    
    // Parse JSON response to get stream URL
    cJSON *json = cJSON_Parse(response_buffer);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse API response JSON");
        return ESP_FAIL;
    }
    
    cJSON *stream_url_json = cJSON_GetObjectItemCaseSensitive(json, "stream_url");
    if (!cJSON_IsString(stream_url_json)) {
        ESP_LOGE(TAG, "No stream_url in API response");
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    const char *stream_url = stream_url_json->valuestring;
    
    // Update current track info
    strncpy(s_current_track.title, song_name, sizeof(s_current_track.title) - 1);
    s_current_track.title[sizeof(s_current_track.title) - 1] = '\0';
    if (artist_name) {
        strncpy(s_current_track.artist, artist_name, sizeof(s_current_track.artist) - 1);
        s_current_track.artist[sizeof(s_current_track.artist) - 1] = '\0';
    }
    strncpy(s_current_track.url, stream_url, sizeof(s_current_track.url) - 1);
    s_current_track.url[sizeof(s_current_track.url) - 1] = '\0';
    
    // Use radio service to play the stream URL
    // Note: This reuses radio service infrastructure for HTTP streaming
    extern esp_err_t sx_radio_play_url(const char *url);
    ret = sx_radio_play_url(stream_url);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Started playing: %s - %s", s_current_track.artist, s_current_track.title);
    } else {
        ESP_LOGE(TAG, "Failed to start playback: %s", esp_err_to_name(ret));
    }
    
    cJSON_Delete(json);
    return ret;
}

esp_err_t sx_music_online_set_auth_config(const sx_music_online_auth_config_t *config) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (config == NULL) {
        s_auth_config.enable_auth = false;
        s_auth_config.auth_key = NULL;
        return ESP_OK;
    }
    
    s_auth_config = *config;
    
    // If key is provided, copy to internal buffer
    if (config->auth_key != NULL && config->auth_key != s_auth_key) {
        strncpy(s_auth_key, config->auth_key, sizeof(s_auth_key) - 1);
        s_auth_key[sizeof(s_auth_key) - 1] = '\0';
        s_auth_config.auth_key = s_auth_key;
    }
    
    ESP_LOGI(TAG, "Authentication config updated: enabled=%d", s_auth_config.enable_auth);
    return ESP_OK;
}

esp_err_t sx_music_online_add_auth_headers(esp_http_client_handle_t client) {
    if (!s_initialized || client == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!s_auth_config.enable_auth) {
        // Authentication not enabled, skip
        return ESP_OK;
    }
    
    // Get MAC address
    uint8_t mac[6];
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MAC address: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Format MAC address as hex string
    char mac_str[18]; // "XX:XX:XX:XX:XX:XX\0"
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // Get Chip ID from MAC address
    // Use MAC address to generate a unique chip ID
    uint64_t chip_id = 0;
    for (int i = 0; i < 6; i++) {
        chip_id |= ((uint64_t)mac[i]) << (8 * (5 - i));
    }
    char chip_id_str[32];
    snprintf(chip_id_str, sizeof(chip_id_str), "%llu", (unsigned long long)chip_id);
    
    // Get Timestamp (Unix timestamp in seconds)
    time_t now = time(NULL);
    char timestamp_str[32];
    snprintf(timestamp_str, sizeof(timestamp_str), "%ld", (long)now);
    
    // Add headers
    esp_http_client_set_header(client, "X-MAC", mac_str);
    esp_http_client_set_header(client, "X-Chip-ID", chip_id_str);
    esp_http_client_set_header(client, "X-Timestamp", timestamp_str);
    
    // Generate SHA256 signature
    // Signature = SHA256(MAC + Chip-ID + Timestamp + Key)
    if (s_auth_config.auth_key != NULL && s_auth_config.auth_key[0] != '\0') {
        // Build string to hash: MAC + Chip-ID + Timestamp + Key
        char hash_input[512];
        snprintf(hash_input, sizeof(hash_input), "%s%s%s%s",
                 mac_str, chip_id_str, timestamp_str, s_auth_config.auth_key);
        
        // Calculate SHA256
        unsigned char hash[32];
        mbedtls_sha256_context sha256_ctx;
        mbedtls_sha256_init(&sha256_ctx);
        mbedtls_sha256_starts(&sha256_ctx, 0); // 0 = SHA256, 1 = SHA224
        mbedtls_sha256_update(&sha256_ctx, (const unsigned char *)hash_input, strlen(hash_input));
        mbedtls_sha256_finish(&sha256_ctx, hash);
        mbedtls_sha256_free(&sha256_ctx);
        
        // Convert hash to hex string
        char signature_str[65]; // 32 bytes * 2 + 1
        for (int i = 0; i < 32; i++) {
            snprintf(&signature_str[i * 2], 3, "%02x", hash[i]);
        }
        signature_str[64] = '\0';
        
        // Add signature header
        esp_http_client_set_header(client, "X-Signature", signature_str);
        
        ESP_LOGD(TAG, "Added auth headers: MAC=%s, Chip-ID=%s, Timestamp=%s, Signature=%s",
                 mac_str, chip_id_str, timestamp_str, signature_str);
    } else {
        ESP_LOGW(TAG, "Authentication enabled but no key provided");
    }
    
    return ESP_OK;
}
