# API Catalog: sx_services – Network & AI Part 2: Network Optimizer

## Tổng quan

`sx_network_optimizer` cung cấp lớp tiện ích “adaptive retry / connection optimization” cho các dịch vụ mạng.  
Mục tiêu: quyết định khoảng trễ reconnect/bật lại dựa trên thống kê tỉ lệ thành công, hỗ trợ exponential back-off và theo dõi thống kê kết nối.

---

## 1. sx_network_optimizer.h / sx_network_optimizer.c

### A) Vai trò file
* Lưu thống kê kết nối (tổng, thành công, thất bại, số lần reconnect).
* Tính khoảng delay retry dựa trên số lần thử + cấu hình back-off.
* API ghi nhận mỗi lần kết nối thành công/thất bại hoặc reconnect.
* Được Wi-Fi service và các dịch vụ TCP/HTTP khác sử dụng để cải thiện tiêu thụ năng lượng và time-to-connect.

### B) Public API (header)
```
esp_err_t sx_network_optimizer_init(void);
uint32_t  sx_network_optimizer_get_retry_delay(uint32_t attempt,
                                              const sx_retry_config_t *cfg);
void      sx_network_optimizer_record_connection(bool success);
void      sx_network_optimizer_record_reconnect(void);
float     sx_network_optimizer_get_success_rate(void);
void      sx_network_optimizer_reset_stats(void);
```
Cấu hình `sx_retry_config_t`:
```
uint32_t initial_delay_ms;     // default 1000
uint32_t max_delay_ms;         // default 30000
uint32_t max_attempts;         // 0 = unlimited (chưa dùng trong code)
bool     exponential_backoff;  // true
float    backoff_multiplier;   // 1.5
```

### C) Data model
* `s_initialized` – cờ init.  
* Counters: `s_total_connections`, `s_successful_connections`, `s_failed_connections`, `s_reconnect_attempts` (uint32).

Invariants:
* Counters chỉ tăng; reset bằng `sx_network_optimizer_reset_stats()`.

### D) Concurrency
* Tất cả hàm không dùng mutex; counters là biến global 32-bit – ghi/đọc không nguyên tử trên Xtensa (32-bit) nhưng thường an toàn vì word-aligned. Tuy nhiên có risk race khi nhiều task ghi đồng thời.
* Init idempotent; không bảo vệ bằng mutex – call lặp lại OK.

### E) Memory ownership
Không cấp phát động; chỉ các biến static.

### F) Side-effects
* Logging qua `ESP_LOG*` khi init, ghi nhận kết nối, reset.

### G) Call-sites
* `sx_wifi_service_init()` gọi `sx_network_optimizer_init()`.
* Wi-Fi event handler (chưa trích) sẽ gọi `record_connection` / `record_reconnect` (dự kiến).
* Có thể được các dịch vụ khác (MQTT, HTTP) dùng để chọn delay.

### H) Issues / Risks
1. **P1 – Thiếu mutex**: Counters cập nhật từ nhiều task (Wi-Fi event + logic reconnect) có thể race.
2. **P2 – `max_attempts` không được sử dụng** khi tính delay ⇒ cấu hình thừa.
3. **P2 – Hàm delay không hạn chế attempt_num** nếu `max_attempts` != 0.
4. **P2 – Không có hàm save/restore thống kê qua reboot** ⇒ mất dữ liệu dài hạn.

### I) Đề xuất cải thiện
* Thêm `portENTER_CRITICAL` hoặc mutex nhỏ bảo vệ counters.
* Sử dụng `max_attempts` để clamp attempt trong tính delay.
* Cân nhắc lưu thống kê vào NVS để theo dõi dài hạn.
* API trả về struct thống kê chi tiết thay vì success rate đơn lẻ.

---

