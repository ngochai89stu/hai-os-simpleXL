#include "sx_event_handler.h"
#include "sx_dispatcher.h"
#include "sx_event_payloads.h"
#include "sx_wifi_service.h"
#include "sx_event_string_pool.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "evt_handler_wifi";

// Phase 1: Handle WiFi scan request from UI
uint32_t sx_event_handler_wifi_scan_request(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_WIFI_SCAN_REQUEST) {
        return 0;
    }
    
    ESP_LOGI(TAG, "WiFi scan request received");
    
    // Perform scan
    sx_wifi_network_info_t networks[20];
    int count = sx_wifi_scan(networks, 20);
    
    if (count > 0) {
        ESP_LOGI(TAG, "WiFi scan found %d networks", count);
    } else {
        ESP_LOGW(TAG, "WiFi scan found no networks");
    }
    
    // Note: Scan results will be handled by UI subscribing to SX_EVT_WIFI_SCAN_COMPLETE
    // For now, we just post the event. Full results can be stored in a separate cache if needed.
    
    // Post scan complete event
    sx_event_t scan_evt = {
        .type = SX_EVT_WIFI_SCAN_COMPLETE,
        .arg0 = count,
        .arg1 = 0,
        .ptr = NULL
    };
    sx_dispatcher_post_event(&scan_evt);
    
    // Phase 3: Return dirty_mask instead of bool
    return SX_STATE_DIRTY_WIFI; // WiFi domain changed
}

// Phase 1: Handle WiFi connect request from UI
uint32_t sx_event_handler_wifi_connect_request(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_WIFI_CONNECT_REQUEST) {
        return 0;
    }
    
    const sx_wifi_connect_request_payload_t *payload = (const sx_wifi_connect_request_payload_t *)evt->ptr;
    if (payload == NULL) {
        ESP_LOGE(TAG, "WiFi connect request: payload is NULL");
        return 0;
    }
    
    ESP_LOGI(TAG, "WiFi connect request: SSID=%s, has_password=%d", payload->ssid, payload->has_password);
    
    // Connect to WiFi
    const char *password = payload->has_password ? payload->password : NULL;
    esp_err_t ret = sx_wifi_connect(payload->ssid, password);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi connect initiated");
        state->ui.wifi_connected = false; // Will be updated when actually connected
        strncpy(state->ui.wifi_ssid, payload->ssid, sizeof(state->ui.wifi_ssid) - 1);
        state->ui.wifi_ssid[sizeof(state->ui.wifi_ssid) - 1] = '\0';
    } else {
        ESP_LOGE(TAG, "WiFi connect failed: %s", esp_err_to_name(ret));
    }
    
    // Free payload
    free((void *)evt->ptr);
    
    // Phase 3: Return dirty_mask instead of bool
    return SX_STATE_DIRTY_WIFI; // WiFi domain changed
}

// Phase 1: Handle WiFi state update (from service to UI)
uint32_t sx_event_handler_wifi_state_update(const sx_event_t *evt, sx_state_t *state) {
    if (evt->type != SX_EVT_WIFI_STATE_UPDATE) {
        return 0;
    }
    
    bool connected = (evt->arg0 != 0);
    const char *ip_or_ssid = (const char *)evt->ptr;  // Can be IP address or SSID
    
    state->ui.wifi_connected = connected;
    
    // Update SSID from WiFi service
    const char *ssid = sx_wifi_get_ssid();
    if (ssid != NULL) {
        strncpy(state->ui.wifi_ssid, ssid, sizeof(state->ui.wifi_ssid) - 1);
        state->ui.wifi_ssid[sizeof(state->ui.wifi_ssid) - 1] = '\0';
    } else {
        state->ui.wifi_ssid[0] = '\0';
    }
    
    // Update IP address if provided (when connected)
    if (connected && ip_or_ssid != NULL && strlen(ip_or_ssid) > 0) {
        // Check if it's an IP address (contains dots) or SSID
        if (strchr(ip_or_ssid, '.') != NULL) {
            // It's an IP address
            strncpy(state->ui.wifi_ip_address, ip_or_ssid, sizeof(state->ui.wifi_ip_address) - 1);
            state->ui.wifi_ip_address[sizeof(state->ui.wifi_ip_address) - 1] = '\0';
        } else {
            // It's SSID, get IP from service
            const char *ip = sx_wifi_get_ip_address();
            if (ip != NULL) {
                strncpy(state->ui.wifi_ip_address, ip, sizeof(state->ui.wifi_ip_address) - 1);
                state->ui.wifi_ip_address[sizeof(state->ui.wifi_ip_address) - 1] = '\0';
            } else {
                state->ui.wifi_ip_address[0] = '\0';
            }
        }
    } else {
        state->ui.wifi_ip_address[0] = '\0';
    }
    
    // Update RSSI if available
    if (connected) {
        state->ui.wifi_rssi = sx_wifi_get_rssi();
    } else {
        state->ui.wifi_rssi = 0;
    }
    
    // Phase 3: Return dirty_mask instead of bool
    return SX_STATE_DIRTY_WIFI; // WiFi domain changed
}
