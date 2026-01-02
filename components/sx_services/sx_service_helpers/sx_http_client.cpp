#include "sx_http_client.hpp"
#include <esp_http_client.h>
#include <esp_log.h>
#include <map>
#include <string>
#include <cstring>

static const char *TAG = "sx_http_client";

esp_err_t SxHttpClient::postJson(
    const std::string &url,
    const std::string &body,
    const std::map<std::string, std::string> *headers,
    std::string &out_response,
    int &http_status
) {
    esp_http_client_config_t cfg = {};
    memset(&cfg, 0, sizeof(cfg));
    cfg.url = url.c_str();
    cfg.timeout_ms = 15000;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept", "application/json");

    // Add custom headers if provided
    if (headers != nullptr) {
        for (const auto &pair : *headers) {
            esp_http_client_set_header(client, pair.first.c_str(), pair.second.c_str());
        }
    }

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

    http_status = esp_http_client_get_status_code(client);
    
    // Read response
    out_response.clear();
    char buf[512];
    int read;
    while ((read = esp_http_client_read(client, buf, sizeof(buf))) > 0) {
        out_response.append(buf, read);
    }

    esp_http_client_cleanup(client);

    // Handle status codes: 200 = success, 202 = timeout (waiting for user input), others = error
    if (http_status == 200) {
        return ESP_OK;
    } else if (http_status == 202) {
        return ESP_ERR_TIMEOUT; // Activation waiting for user input
    } else {
        ESP_LOGE(TAG, "HTTP POST failed with status %d, response: %s", http_status, out_response.c_str());
        return ESP_FAIL;
    }
}

