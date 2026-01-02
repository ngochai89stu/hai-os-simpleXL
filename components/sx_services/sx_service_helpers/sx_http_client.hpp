#pragma once

#include <string>
#include <map>
#include "esp_err.h"

/**
 * @brief HTTP JSON Client helper class
 * 
 * Provides a simple interface for making HTTP POST requests with JSON payloads.
 * Extracted from sx_ota_full to be reusable by other services.
 */
class SxHttpClient {
public:
    /**
     * @brief Perform HTTP POST request with JSON body
     * 
     * @param url URL to POST to
     * @param body JSON body as string
     * @param headers Optional custom headers (map of header name -> value)
     * @param out_response Output parameter for response body
     * @param http_status Output parameter for HTTP status code
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    static esp_err_t postJson(
        const std::string &url,
        const std::string &body,
        const std::map<std::string, std::string> *headers,
        std::string &out_response,
        int &http_status
    );
    
    /**
     * @brief Perform HTTP POST request with JSON body (no custom headers)
     * 
     * @param url URL to POST to
     * @param body JSON body as string
     * @param out_response Output parameter for response body
     * @param http_status Output parameter for HTTP status code
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    static esp_err_t postJson(
        const std::string &url,
        const std::string &body,
        std::string &out_response,
        int &http_status
    ) {
        return postJson(url, body, nullptr, out_response, http_status);
    }
};

