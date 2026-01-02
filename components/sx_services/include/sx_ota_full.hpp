#pragma once

#include <string>
#include <functional>
#include <esp_err.h>

// Forward-declare for internal use
struct cJSON;

// OTA-full port (from xiaozhi-esp32_vietnam_ref) adapted to SimpleXL
// This class is used by sx_ota_full_service (C API) and posts SX_EVT_* events.
class SxOtaFull {
public:
    static SxOtaFull &instance();

    // Async check version (posts events)
    esp_err_t checkVersion();

    // Async upgrade from url
    esp_err_t startUpgrade(const std::string &url);

    // Activation
    bool hasActivation() const;
    const std::string &getActivationCode() const;
    esp_err_t activate();

private:
    SxOtaFull();

    // Internal helpers
    std::string buildSystemInfoJson();
    esp_err_t httpPostJson(const std::string &url, const std::string &body, std::string &out_response);
    void storeMqttConfigFromJson(cJSON *mqtt);
    void storeWebsocketConfigFromJson(cJSON *ws);

    // Activation
    std::string buildActivationPayload();

private:
    std::string ota_url_;
    std::string activation_message_;
    std::string activation_code_;
    std::string activation_challenge_;
    bool has_activation_code_ = false;
    bool has_activation_challenge_ = false;
};







