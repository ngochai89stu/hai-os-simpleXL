#pragma once

#include <string>
#include <functional>
#include <esp_err.h>

// Forward declare cJSON without including cJSON.h in header
typedef struct cJSON cJSON;

class SxOtaFull {
public:
    static SxOtaFull &instance();

    // OTA check
    esp_err_t checkVersion();

    // Firmware upgrade
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

    // Version compare
    static bool isNewVersionAvailable(const std::string &current, const std::string &newer);

private:
    std::string ota_url_;
    std::string activation_message_;
    std::string activation_code_;
    std::string activation_challenge_;
    std::string serial_number_;
    bool has_activation_code_ = false;
    bool has_activation_challenge_ = false;
    bool has_serial_number_ = false;
};
