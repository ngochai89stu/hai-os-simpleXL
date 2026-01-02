#include "sx_ota_full.hpp"

#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_event_string_pool.h"
#include "sx_settings_service.h"
#include "sx_wifi_service.h"

#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_app_desc.h>
#include <esp_system.h>
#include <esp_mac.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <esp_partition.h>
#include <esp_timer.h>
#include <cstring>
#include <cstdlib>

#ifdef SOC_HMAC_SUPPORTED
#include <esp_hmac.h>
#endif

#ifdef ESP_EFUSE_BLOCK_USR_DATA
#include <esp_efuse.h>
#include <esp_efuse_table.h>
#endif

#include <cJSON.h>

static const char *TAG = "sx_ota_full";

SxOtaFull &SxOtaFull::instance() {
    static SxOtaFull inst;
    return inst;
}

SxOtaFull::SxOtaFull() {
    // Load OTA URL from settings if available
    char buf[256] = {0};
    if (sx_settings_get_string_default("ota_url", buf, sizeof(buf), NULL) == ESP_OK && buf[0] != '\0') {
        ota_url_ = buf;
    }

#ifdef ESP_EFUSE_BLOCK_USR_DATA
    // Read Serial Number from efuse user_data (optional, like reference repo)
    uint8_t serial_buf[33] = {0};
    if (esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA, serial_buf, 32 * 8) == ESP_OK) {
        if (serial_buf[0] != 0) {
            serial_number_ = std::string(reinterpret_cast<char*>(serial_buf), 32);
            has_serial_number_ = true;
            ESP_LOGI(TAG, "Serial number from eFuse: %s", serial_number_.c_str());
        }
    }
#endif
}

// ------------------------------
// Utilities
// ------------------------------

std::string SxOtaFull::buildSystemInfoJson() {
    // Minimal system info JSON for OTA check.
    // The reference repo posts a much bigger payload (partition table, board info, etc.).
    // We keep minimal but sufficient fields, and can extend later.

    cJSON *root = cJSON_CreateObject();
    if (!root) return "{}";

    const esp_app_desc_t *app = esp_app_get_description();

    cJSON_AddStringToObject(root, "language", "vi-VN");
    cJSON_AddStringToObject(root, "application", "hai-os-simplexl");
    cJSON_AddStringToObject(root, "version", app ? app->version : "unknown");

    // WiFi / MAC
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    cJSON_AddStringToObject(root, "mac_address", mac_str);

    // network info
    cJSON *net = cJSON_CreateObject();
    cJSON_AddBoolToObject(net, "connected", sx_wifi_is_connected());
    if (sx_wifi_is_connected()) {
        const char *ssid = sx_wifi_get_ssid();
        const char *ip = sx_wifi_get_ip_address();
        if (ssid) cJSON_AddStringToObject(net, "ssid", ssid);
        if (ip) cJSON_AddStringToObject(net, "ip", ip);
        cJSON_AddNumberToObject(net, "rssi", sx_wifi_get_rssi());
        cJSON_AddNumberToObject(net, "channel", sx_wifi_get_channel());
    }
    cJSON_AddItemToObject(root, "network", net);

    char *json_str = cJSON_PrintUnformatted(root);
    std::string out = json_str ? json_str : "{}";
    if (json_str) cJSON_free(json_str);
    cJSON_Delete(root);
    return out;
}

esp_err_t SxOtaFull::httpPostJson(const std::string &url, const std::string &body, std::string &out_response) {
    esp_http_client_config_t cfg = {};
    memset(&cfg, 0, sizeof(cfg));
    cfg.url = url.c_str();
    cfg.timeout_ms = 15000;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_ERR_NO_MEM;

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept", "application/json");

    // ----- Extra headers like reference repo -----
    esp_http_client_set_header(client, "Accept-Language", "vi-VN");

    // Activation-Version (1 = no serial, 2 = has serial)
    esp_http_client_set_header(client, "Activation-Version", has_serial_number_ ? "2" : "1");

    // Device-Id (MAC address)
    uint8_t mac_hdr[6];
    esp_read_mac(mac_hdr, ESP_MAC_WIFI_STA);
    char mac_hdr_str[18];
    snprintf(mac_hdr_str, sizeof(mac_hdr_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_hdr[0], mac_hdr[1], mac_hdr[2], mac_hdr[3], mac_hdr[4], mac_hdr[5]);
    esp_http_client_set_header(client, "Device-Id", mac_hdr_str);

    // Client-Id (UUID saved in settings key "device_uuid")
    char uuid_buf[64] = {0};
    if (sx_settings_get_string_default("device_uuid", uuid_buf, sizeof(uuid_buf), NULL) == ESP_OK && uuid_buf[0] != '\0') {
        esp_http_client_set_header(client, "Client-Id", uuid_buf);
    }

    // Serial-Number (if available)
    if (has_serial_number_) {
        esp_http_client_set_header(client, "Serial-Number", serial_number_.c_str());
    }

    // User-Agent "hai-os-simplexl/<version>"
    const esp_app_desc_t *desc = esp_app_get_description();
    char ua_buf[64];
    snprintf(ua_buf, sizeof(ua_buf), "hai-os-simplexl/%s", desc ? desc->version : "unknown");
    esp_http_client_set_header(client, "User-Agent", ua_buf);


    esp_err_t ret = esp_http_client_set_post_field(client, body.c_str(), body.size());
    if (ret != ESP_OK) {
        esp_http_client_cleanup(client);
        return ret;
    }

    ret = esp_http_client_perform(client);
    if (ret != ESP_OK) {
        esp_http_client_cleanup(client);
        return ret;
    }

    int status = esp_http_client_get_status_code(client);
    
    // Read response first (even if status is not 200)
    out_response.clear();
    char buf[512];
    int read;
    while ((read = esp_http_client_read(client, buf, sizeof(buf))) > 0) {
        out_response.append(buf, read);
    }

    esp_http_client_cleanup(client);

    // Handle status codes: 200 = success, 202 = timeout (waiting for user input), others = error
    if (status == 200) {
        return ESP_OK;
    } else if (status == 202) {
        return ESP_ERR_TIMEOUT; // Activation waiting for user input (like reference repo)
    } else {
        ESP_LOGE(TAG, "HTTP POST failed with status %d, response: %s", status, out_response.c_str());
        return ESP_FAIL;
    }
}

void SxOtaFull::storeMqttConfigFromJson(cJSON *mqtt) {
    if (!cJSON_IsObject(mqtt)) return;

    // Map fields to SimpleXL settings keys (flat keys)
    // Reference keys: endpoint, client_id, username, password, publish_topic, subscribe_topic
    const struct { const char *json_key; const char *settings_key; } map[] = {
        {"endpoint", "mqtt_endpoint"},
        {"client_id", "mqtt_client_id"},
        {"username", "mqtt_username"},
        {"password", "mqtt_password"},
        {"publish_topic", "mqtt_publish_topic"},
        {"subscribe_topic", "mqtt_subscribe_topic"},
    };

    for (size_t i = 0; i < sizeof(map)/sizeof(map[0]); i++) {
        cJSON *v = cJSON_GetObjectItem(mqtt, map[i].json_key);
        if (cJSON_IsString(v) && v->valuestring) {
            sx_settings_set_string(map[i].settings_key, v->valuestring);
        }
    }
    sx_settings_commit();
}

void SxOtaFull::storeWebsocketConfigFromJson(cJSON *ws) {
    if (!cJSON_IsObject(ws)) return;

    cJSON *url = cJSON_GetObjectItem(ws, "url");
    cJSON *token = cJSON_GetObjectItem(ws, "token");

    if (cJSON_IsString(url) && url->valuestring) {
        sx_settings_set_string("websocket_url", url->valuestring);
    }
    if (cJSON_IsString(token) && token->valuestring) {
        sx_settings_set_string("websocket_token", token->valuestring);
    }
    sx_settings_commit();
}

// ------------------------------
// Version compare helper
// ------------------------------

bool SxOtaFull::isNewVersionAvailable(const std::string &current, const std::string &newer) {
    // Split by '.' and compare each numeric part
    int c1 = 0, c2 = 0, c3 = 0, c4 = 0;
    int n1 = 0, n2 = 0, n3 = 0, n4 = 0;

    sscanf(current.c_str(), "%d.%d.%d.%d", &c1, &c2, &c3, &c4);
    sscanf(newer.c_str(), "%d.%d.%d.%d", &n1, &n2, &n3, &n4);

    if (n1 != c1) return n1 > c1;
    if (n2 != c2) return n2 > c2;
    if (n3 != c3) return n3 > c3;
    return n4 > c4;
}

// ------------------------------
// Public APIs
// ------------------------------

esp_err_t SxOtaFull::checkVersion() {
    if (ota_url_.empty()) {
        // If no ota_url configured, do nothing.
        ESP_LOGW(TAG, "ota_url not configured in settings (key: ota_url)");
        return ESP_ERR_INVALID_STATE;
    }

    if (!sx_wifi_is_connected()) {
        ESP_LOGW(TAG, "WiFi not connected, skip OTA check");
        return ESP_ERR_INVALID_STATE;
    }

    std::string body = buildSystemInfoJson();
    std::string resp;

    ESP_LOGI(TAG, "OTA check POST %s", ota_url_.c_str());
    esp_err_t ret = httpPostJson(ota_url_, body, resp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OTA check failed: %s", esp_err_to_name(ret));
        sx_event_t evt = {
            .type = SX_EVT_OTA_ERROR,
            .priority = SX_EVT_PRIORITY_NORMAL,
            .arg0 = 0,
            .arg1 = 0,
            .ptr = sx_event_alloc_string("OTA check failed")
        };
        sx_dispatcher_post_event(&evt);
        return ret;
    }

    cJSON *root = cJSON_Parse(resp.c_str());
    if (!root) {
        sx_event_t evt = {
            .type = SX_EVT_OTA_ERROR,
            .priority = SX_EVT_PRIORITY_NORMAL,
            .arg0 = 0,
            .arg1 = 0,
            .ptr = sx_event_alloc_string("OTA response JSON parse failed")
        };
        sx_dispatcher_post_event(&evt);
        return ESP_FAIL;
    }

    // activation
    has_activation_code_ = false;
    has_activation_challenge_ = false;
    activation_code_.clear();
    activation_challenge_.clear();
    activation_message_.clear();

    cJSON *activation = cJSON_GetObjectItem(root, "activation");
    if (cJSON_IsObject(activation)) {
        cJSON *code = cJSON_GetObjectItem(activation, "code");
        cJSON *challenge = cJSON_GetObjectItem(activation, "challenge");
        cJSON *message = cJSON_GetObjectItem(activation, "message");

        if (cJSON_IsString(code) && code->valuestring) {
            activation_code_ = code->valuestring;
            has_activation_code_ = true;
        }
        if (cJSON_IsString(challenge) && challenge->valuestring) {
            activation_challenge_ = challenge->valuestring;
            has_activation_challenge_ = true;
        }
        if (cJSON_IsString(message) && message->valuestring) {
            activation_message_ = message->valuestring;
        }

        if (has_activation_code_) {
            sx_event_t evt = {
                .type = SX_EVT_ACTIVATION_REQUIRED,
                .priority = SX_EVT_PRIORITY_HIGH,
                .arg0 = 0,
                .arg1 = 0,
                .ptr = sx_event_alloc_string(activation_code_.c_str()),
            };
            sx_dispatcher_post_event(&evt);
        }
    }

    // mqtt / websocket config
    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
    if (cJSON_IsObject(mqtt)) {
        storeMqttConfigFromJson(mqtt);
    }

    cJSON *ws = cJSON_GetObjectItem(root, "websocket");
    if (cJSON_IsObject(ws)) {
        storeWebsocketConfigFromJson(ws);
    }

    // firmware info (match reference)
    const esp_app_desc_t *app = esp_app_get_description();
    const char *current_ver = app ? app->version : "0.0.0";
    const char *new_ver = NULL;
    const char *fw_url = NULL;

    cJSON *firmware = cJSON_GetObjectItem(root, "firmware");
    if (cJSON_IsObject(firmware)) {
        cJSON *v = cJSON_GetObjectItem(firmware, "version");
        cJSON *u = cJSON_GetObjectItem(firmware, "url");
        if (cJSON_IsString(v) && v->valuestring) new_ver = v->valuestring;
        if (cJSON_IsString(u) && u->valuestring) fw_url = u->valuestring;
    }

    bool should_upgrade = false;
    if (new_ver && fw_url && fw_url[0] != '\0') {
        should_upgrade = isNewVersionAvailable(current_ver, new_ver);
    }

    cJSON_Delete(root);

    // If activation required, UI flow continues.
    if (has_activation_code_) {
        return ESP_OK;
    }

    // If a new firmware is available, trigger upgrade.
    if (should_upgrade) {
        ESP_LOGI(TAG, "New firmware available: %s -> %s, url=%s", current_ver, new_ver, fw_url);
        // Start upgrade now. (If you want to delay, move to another layer)
        return startUpgrade(fw_url);
    }

    // Otherwise mark finished
    sx_event_t evt = {
        .type = SX_EVT_OTA_FINISHED,
        .priority = SX_EVT_PRIORITY_NORMAL,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = sx_event_alloc_string(current_ver),
    };
    sx_dispatcher_post_event(&evt);

    return ESP_OK;
}

esp_err_t SxOtaFull::startUpgrade(const std::string &url) {
    ESP_LOGI(TAG, "Starting OTA upgrade from URL: %s", url.c_str());

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Failed to get update partition");
        sx_event_t evt = {
            .type = SX_EVT_OTA_ERROR,
            .priority = SX_EVT_PRIORITY_NORMAL,
            .arg0 = 0,
            .arg1 = 0,
            .ptr = sx_event_alloc_string("No update partition found"),
        };
        sx_dispatcher_post_event(&evt);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Writing to partition %s at offset 0x%lx", update_partition->label, update_partition->address);

    esp_http_client_config_t cfg = {};
    memset(&cfg, 0, sizeof(cfg));
    cfg.url = url.c_str();
    cfg.timeout_ms = 30000;
    cfg.keep_alive_enable = true;
    cfg.keep_alive_idle = 5;
    cfg.keep_alive_interval = 5;
    cfg.keep_alive_count = 3;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_err_t ret = esp_http_client_open(client, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(ret));
        esp_http_client_cleanup(client);
        return ret;
    }

    int status_code = esp_http_client_fetch_headers(client);
    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP status code: %d", status_code);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int content_length = esp_http_client_get_content_length(client);
    if (content_length <= 0) {
        // Try to estimate from partition size
        content_length = update_partition->size;
        ESP_LOGW(TAG, "Content-Length not provided, using partition size: %d", content_length);
    }
    ESP_LOGI(TAG, "Firmware size: %d bytes", content_length);

    esp_ota_handle_t update_handle = 0;
    bool image_header_checked = false;
    char buffer[1024];
    size_t total_read = 0;
    size_t recent_read = 0;
    int64_t last_calc_time = esp_timer_get_time();

    while (true) {
        int read_len = esp_http_client_read(client, buffer, sizeof(buffer));
        if (read_len < 0) {
            ESP_LOGE(TAG, "Failed to read HTTP data: %s", esp_err_to_name(read_len));
            if (update_handle) esp_ota_abort(update_handle);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }

        if (read_len == 0) {
            break; // EOF
        }

        // Check image header on first read
        if (!image_header_checked && total_read + read_len >= sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
            esp_app_desc_t new_app_info;
            memcpy(&new_app_info, buffer + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t), sizeof(esp_app_desc_t));
            
            const esp_app_desc_t *current_app = esp_app_get_description();
            ESP_LOGI(TAG, "Current version: %s, New version: %s", 
                     current_app ? current_app->version : "unknown", new_app_info.version);

            ret = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to begin OTA: %s", esp_err_to_name(ret));
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return ret;
            }
            image_header_checked = true;
        }

        // Write to OTA partition
        if (image_header_checked) {
            ret = esp_ota_write(update_handle, buffer, read_len);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write OTA data: %s", esp_err_to_name(ret));
                esp_ota_abort(update_handle);
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return ret;
            }
        }

        total_read += read_len;
        recent_read += read_len;

        // Calculate and emit progress every second
        int64_t now = esp_timer_get_time();
        if (now - last_calc_time >= 1000000 || read_len == 0) {
            int progress = content_length > 0 ? (total_read * 100 / content_length) : 0;
            size_t speed_kbps = recent_read / 1024; // KB/s
            ESP_LOGI(TAG, "Progress: %d%% (%zu/%d), Speed: %zu KB/s", progress, total_read, content_length, speed_kbps);
            
            sx_event_t evt = {
                .type = SX_EVT_OTA_PROGRESS,
                .priority = SX_EVT_PRIORITY_NORMAL,
                .arg0 = (uint32_t)progress,
                .arg1 = (uint32_t)speed_kbps,
                .ptr = NULL,
            };
            sx_dispatcher_post_event(&evt);
            
            last_calc_time = now;
            recent_read = 0;
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (!image_header_checked) {
        ESP_LOGE(TAG, "Image header not checked, aborting");
        return ESP_FAIL;
    }

    ret = esp_ota_end(update_handle);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
            sx_event_t evt = {
                .type = SX_EVT_OTA_ERROR,
                .priority = SX_EVT_PRIORITY_NORMAL,
                .arg0 = 0,
                .arg1 = 0,
                .ptr = sx_event_alloc_string("Image validation failed"),
            };
            sx_dispatcher_post_event(&evt);
        } else {
            ESP_LOGE(TAG, "Failed to end OTA: %s", esp_err_to_name(ret));
        }
        return ret;
    }

    ret = esp_ota_set_boot_partition(update_partition);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set boot partition: %s", esp_err_to_name(ret));
        return ret;
    }

    const esp_app_desc_t *app = esp_app_get_description();
    ESP_LOGI(TAG, "Firmware upgrade successful");
    
    sx_event_t evt = {
        .type = SX_EVT_OTA_FINISHED,
        .priority = SX_EVT_PRIORITY_NORMAL,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = sx_event_alloc_string(app ? app->version : "unknown"),
    };
    sx_dispatcher_post_event(&evt);

    return ESP_OK;
}

bool SxOtaFull::hasActivation() const {
    return has_activation_code_;
}

const std::string &SxOtaFull::getActivationCode() const {
    return activation_code_;
}

std::string SxOtaFull::buildActivationPayload() {
    if (!has_serial_number_) {
        // No serial number, return minimal payload
        cJSON *p = cJSON_CreateObject();
        cJSON_AddStringToObject(p, "algorithm", "none");
        cJSON_AddStringToObject(p, "challenge", activation_challenge_.c_str());
        char *s = cJSON_PrintUnformatted(p);
        std::string out = s ? s : "{}";
        if (s) cJSON_free(s);
        cJSON_Delete(p);
        return out;
    }

    std::string hmac_hex;
#ifdef SOC_HMAC_SUPPORTED
    uint8_t hmac_result[32]; // SHA-256 output is 32 bytes
    
    // Use HMAC_KEY0 to calculate HMAC (like reference repo)
    esp_err_t ret = esp_hmac_calculate(HMAC_KEY0, 
                                        reinterpret_cast<const uint8_t*>(activation_challenge_.data()), 
                                        activation_challenge_.size(), 
                                        hmac_result);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HMAC calculation failed: %s", esp_err_to_name(ret));
        // Fallback to minimal payload
        cJSON *p = cJSON_CreateObject();
        cJSON_AddStringToObject(p, "algorithm", "none");
        cJSON_AddStringToObject(p, "challenge", activation_challenge_.c_str());
        char *s = cJSON_PrintUnformatted(p);
        std::string out = s ? s : "{}";
        if (s) cJSON_free(s);
        cJSON_Delete(p);
        return out;
    }

    // Convert HMAC result to hex string
    char hex_buf[3];
    for (size_t i = 0; i < sizeof(hmac_result); i++) {
        snprintf(hex_buf, sizeof(hex_buf), "%02x", hmac_result[i]);
        hmac_hex += hex_buf;
    }
#endif

    cJSON *payload = cJSON_CreateObject();
#ifdef SOC_HMAC_SUPPORTED
    if (!hmac_hex.empty()) {
        cJSON_AddStringToObject(payload, "algorithm", "hmac-sha256");
        cJSON_AddStringToObject(payload, "hmac", hmac_hex.c_str());
    } else {
        cJSON_AddStringToObject(payload, "algorithm", "none");
    }
#else
    cJSON_AddStringToObject(payload, "algorithm", "none");
#endif
    cJSON_AddStringToObject(payload, "serial_number", serial_number_.c_str());
    cJSON_AddStringToObject(payload, "challenge", activation_challenge_.c_str());

    char *json_str = cJSON_PrintUnformatted(payload);
    std::string json = json_str ? json_str : "{}";
    if (json_str) cJSON_free(json_str);
    cJSON_Delete(payload);

    ESP_LOGI(TAG, "Activation payload: %s", json.c_str());
    return json;
}

esp_err_t SxOtaFull::activate() {
    if (!has_activation_challenge_) {
        return ESP_ERR_INVALID_STATE;
    }

    if (ota_url_.empty()) {
        return ESP_ERR_INVALID_STATE;
    }

    std::string url = ota_url_;
    if (url.back() != '/') url += "/activate";
    else url += "activate";

    std::string payload = buildActivationPayload();
    std::string resp;

    ESP_LOGI(TAG, "Activation POST %s", url.c_str());
    esp_err_t ret = httpPostJson(url, payload, resp);

    // Check response status (like reference repo: 202 = timeout, 200 = success)
    if (ret == ESP_OK) {
        sx_event_t evt = {
            .type = SX_EVT_ACTIVATION_DONE,
            .priority = SX_EVT_PRIORITY_HIGH,
            .arg0 = 0,
            .arg1 = 0,
            .ptr = NULL,
        };
        sx_dispatcher_post_event(&evt);
        ESP_LOGI(TAG, "Activation successful");
    } else if (ret == ESP_ERR_TIMEOUT) {
        // 202 status: server is waiting for user input (polling scenario)
        ESP_LOGI(TAG, "Activation waiting for user input (status 202)");
        // Don't emit error event, just return timeout - caller can retry
        return ESP_ERR_TIMEOUT;
    } else {
        sx_event_t evt = {
            .type = SX_EVT_OTA_ERROR,
            .priority = SX_EVT_PRIORITY_NORMAL,
            .arg0 = 0,
            .arg1 = 0,
            .ptr = sx_event_alloc_string("Activation failed"),
        };
        sx_dispatcher_post_event(&evt);
        ESP_LOGE(TAG, "Activation failed: %s", esp_err_to_name(ret));
    }

    return ret;
}
