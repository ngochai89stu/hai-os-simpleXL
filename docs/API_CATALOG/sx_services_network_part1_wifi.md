# API Catalog: sx_services - Network & AI Part 1: WiFi Service

## Tổng Quan

**sx_wifi_service** quản lý kết nối WiFi Station mode, bao gồm:
- Init WiFi stack & event loop
- Scan AP
- Connect/Disconnect
- Auto-reconnect
- Expose state getters (SSID, RSSI, channel, IP)
- Phát sự kiện qua dispatcher (TODO – chưa thấy trong code đoạn đầu)
- Tối ưu hoá năng lượng mạng qua `sx_network_optimizer`

---

## 1. sx_wifi_service.h / sx_wifi_service.c

### A) Vai Trò File
- Wrap ESP-IDF WiFi STA APIs
- Dùng FreeRTOS event group để đồng bộ trạng thái
- Đăng ký WiFi & IP event handler → cập nhật state và phát event

### B) Public API (từ header)
```
esp_err_t sx_wifi_service_init(const sx_wifi_config_t *cfg);
esp_err_t sx_wifi_service_start(void);
esp_err_t sx_wifi_service_stop(void);
int        sx_wifi_scan(sx_wifi_network_info_t *nets,uint8_t max);
esp_err_t sx_wifi_connect(const char* ssid,const char* pass);
esp_err_t sx_wifi_disconnect(void);
bool       sx_wifi_is_connected(void);
const char*sx_wifi_get_ssid(void);
int8_t     sx_wifi_get_rssi(void);
uint8_t    sx_wifi_get_channel(void);
const char*sx_wifi_get_ip_address(void);
```

### C) Data Model
- `s_initialized`, `s_started`, `s_connected`
- Config `s_cfg` (auto_reconnect, interval)
- Connection info: ssid, password, ip string, rssi, channel
- EventGroup `s_wifi_event_group` bits CONNECTED/FAIL
- Retry counter `s_retry_num`

### D) Concurrency
- Event handlers run in ESP event loop task
- Public APIs callable from any task (no mutex in current code ⇒ ⚠️ race risk when read/write global state)
- `s_wifi_event_group` provides sync for connect flow

### E) Memory Ownership
- Strings stored in static buffers (33/65/16 bytes)
- Caller must keep password pointer until connect returns (copied internally?)

### F) Side Effects
1. Calls ESP-IDF WiFi & netif
2. Registers event handlers (`WIFI_EVENT`, `IP_EVENT_STA_GOT_IP`)
3. Creates FreeRTOS event group
4. Initializes `sx_network_optimizer`

### G) Call Sites
- bootstrap: init & start
- UI settings: scan/connect/disconnect
- Network optimizer uses wifi RSSI maybe

### H) Issues / Risks
1. P1 No mutex around shared state (`s_current_ssid` etc.) – concurrent access race.
2. P1 Fixed password buffer length 65 – potential overflow if >64 (unlikely)
3. P1 Auto-reconnect logic TBD (retry up to MAX_RETRY=5 then wait)
4. P2 IPv6 not handled
5. P2 No WPA3 Sae explicit support mapping (auth_mode just stored)

### I) Đề xuất cải thiện
- Add mutex for state getters/setters
- Expose async connect result via events rather than blocking loops
- Support IPv6 address retrieval
- Configurable MAX_RETRY & scanning params through Kconfig

---
