# Thiết Kế Tính Năng Navigation Qua BLE với Điện Thoại

## Tổng Quan

Tính năng này cho phép ESP32 kết nối với điện thoại qua BLE, tự động mở Google Maps trên điện thoại, và nhận thông tin điều hướng trả về.

## Luồng Hoạt Động

```
┌─────────────────────────────────────────────────────────────┐
│ 1. Người dùng nói: "Chỉ đường đi từ nhà đến bến xe miền tây"│
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. STT Service nhận diện lệnh                               │
│    → Parse intent: navigation                               │
│    → Extract: origin="nhà", destination="bến xe miền tây"  │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. Navigation Service xử lý                                 │
│    → Kiểm tra BLE connection                                │
│    → Nếu chưa kết nối → Tự động kết nối với điện thoại      │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ 4. Gửi lệnh qua BLE đến điện thoại                          │
│    → Command: "OPEN_GOOGLE_MAPS"                            │
│    → Data: {                                                 │
│         "origin": "nhà",                                     │
│         "destination": "bến xe miền tây",                    │
│         "action": "directions"                               │
│       }                                                      │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ 5. Điện thoại nhận lệnh                                      │
│    → Mở Google Maps app                                      │
│    → Nhập điều hướng: nhà → bến xe miền tây                 │
│    → Lấy kết quả điều hướng từ Google Maps                  │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ 6. Điện thoại gửi kết quả về ESP32 qua BLE                  │
│    → Route data: {                                           │
│         "distance": "15.2 km",                               │
│         "duration": "25 min",                                 │
│         "steps": [...],                                      │
│         "polyline": "encoded_polyline_string"                │
│       }                                                      │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ 7. ESP32 nhận và xử lý kết quả                              │
│    → Parse JSON response                                     │
│    → Cập nhật navigation service                            │
│    → Hiển thị trên UI                                        │
│    → Phát TTS hướng dẫn                                      │
└─────────────────────────────────────────────────────────────┘
```

## Kiến Trúc Hệ Thống

### 1. BLE Communication Protocol

#### Service UUID
```
Service: 0000ff00-0000-1000-8000-00805f9b34fb (Custom)
```

#### Characteristics

**TX Characteristic (ESP32 → Phone):**
```
UUID: 0000ff01-0000-1000-8000-00805f9b34fb
Properties: WRITE, WRITE_NO_RESPONSE
```

**RX Characteristic (Phone → ESP32):**
```
UUID: 0000ff02-0000-1000-8000-00805f9b34fb
Properties: NOTIFY, INDICATE
```

#### Message Format

**Request (ESP32 → Phone):**
```json
{
  "command": "OPEN_GOOGLE_MAPS",
  "data": {
    "origin": "nhà",
    "destination": "bến xe miền tây",
    "action": "directions"
  },
  "timestamp": 1234567890
}
```

**Response (Phone → ESP32):**
```json
{
  "status": "success",
  "command": "OPEN_GOOGLE_MAPS",
  "data": {
    "route": {
      "distance": "15.2 km",
      "distance_m": 15200,
      "duration": "25 min",
      "duration_s": 1500,
      "steps": [
        {
          "instruction": "Head north on Đường ABC",
          "distance": "200 m",
          "distance_m": 200,
          "type": "straight"
        },
        {
          "instruction": "Turn right onto Đường XYZ",
          "distance": "500 m",
          "distance_m": 500,
          "type": "turn_right"
        }
      ],
      "polyline": "encoded_polyline_string"
    }
  },
  "timestamp": 1234567891
}
```

### 2. Cấu Trúc Code

#### 2.1. BLE Navigation Service Header

```c
// components/sx_services/include/sx_navigation_ble.h

#pragma once

#include <esp_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// BLE Navigation command types
typedef enum {
    SX_NAV_BLE_CMD_OPEN_GOOGLE_MAPS = 0,
    SX_NAV_BLE_CMD_GET_ROUTE,
    SX_NAV_BLE_CMD_STOP_NAVIGATION,
} sx_nav_ble_cmd_t;

// BLE Navigation state
typedef enum {
    SX_NAV_BLE_STATE_DISCONNECTED = 0,
    SX_NAV_BLE_STATE_CONNECTING,
    SX_NAV_BLE_STATE_CONNECTED,
    SX_NAV_BLE_STATE_SENDING,
    SX_NAV_BLE_STATE_RECEIVING,
    SX_NAV_BLE_STATE_ERROR,
} sx_nav_ble_state_t;

// Navigation request
typedef struct {
    char origin[128];          // Điểm xuất phát
    char destination[128];     // Điểm đến
    bool use_current_location; // Dùng vị trí hiện tại làm điểm xuất phát
} sx_nav_ble_request_t;

// Navigation response callback
typedef void (*sx_nav_ble_response_cb_t)(const char *json_response, void *user_data);

// Initialize BLE Navigation service
esp_err_t sx_navigation_ble_init(void);

// Deinitialize BLE Navigation service
void sx_navigation_ble_deinit(void);

// Connect to phone (auto-connect to last paired device)
esp_err_t sx_navigation_ble_connect(void);

// Disconnect from phone
esp_err_t sx_navigation_ble_disconnect(void);

// Check if connected
bool sx_navigation_ble_is_connected(void);

// Send navigation request
esp_err_t sx_navigation_ble_send_request(const sx_nav_ble_request_t *request);

// Set response callback
void sx_navigation_ble_set_response_callback(sx_nav_ble_response_cb_t callback, void *user_data);

// Get current state
sx_nav_ble_state_t sx_navigation_ble_get_state(void);

#ifdef __cplusplus
}
#endif
```

#### 2.2. Navigation Service Integration

Cần cập nhật `sx_navigation_service.c` để tích hợp với BLE:

```c
// Trong sx_navigation_calculate_route()
esp_err_t sx_navigation_calculate_route(const sx_nav_coordinate_t *start,
                                         const sx_nav_coordinate_t *end,
                                         sx_nav_route_t *route) {
    // ...
    
    // Try BLE + Phone first
    if (sx_navigation_ble_is_connected()) {
        sx_nav_ble_request_t ble_request = {0};
        
        // Convert coordinates to address strings (if needed)
        // Or use place names directly from intent
        
        if (sx_navigation_ble_send_request(&ble_request) == ESP_OK) {
            // Wait for response (async callback)
            // Response will be processed in callback
            return ESP_OK;
        }
    }
    
    // Fallback to API or simple route
    // ...
}
```

#### 2.3. Intent Service Integration

Cần cập nhật Intent Service để nhận diện lệnh điều hướng:

```c
// Trong sx_intent_service.c

// Intent: navigation
// Pattern: "chỉ đường đi từ {origin} đến {destination}"
// Example: "chỉ đường đi từ nhà đến bến xe miền tây"

typedef struct {
    char intent[32];          // "navigation"
    char origin[128];         // "nhà"
    char destination[128];    // "bến xe miền tây"
} navigation_intent_t;

// Parse navigation intent
static esp_err_t parse_navigation_intent(const char *text, navigation_intent_t *intent) {
    // Pattern matching:
    // "chỉ đường đi từ {origin} đến {destination}"
    // "điều hướng từ {origin} đến {destination}"
    // "navigation from {origin} to {destination}"
    
    // Extract origin and destination
    // ...
    
    return ESP_OK;
}

// Handle navigation intent
static void handle_navigation_intent(const navigation_intent_t *intent) {
    // Trigger navigation service
    sx_nav_ble_request_t request = {0};
    strncpy(request.origin, intent->origin, sizeof(request.origin) - 1);
    strncpy(request.destination, intent->destination, sizeof(request.destination) - 1);
    request.use_current_location = (strcmp(intent->origin, "nhà") == 0 || 
                                   strcmp(intent->origin, "here") == 0);
    
    sx_navigation_ble_send_request(&request);
}
```

### 3. Ứng Dụng Điện Thoại (Android/iOS)

#### 3.1. Android App Requirements

**Manifest:**
```xml
<uses-permission android:name="android.permission.BLUETOOTH" />
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN" />
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
<uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION" />
```

**Main Activity:**
```kotlin
class NavigationBLEActivity : AppCompatActivity() {
    private lateinit var bluetoothGatt: BluetoothGatt
    
    // BLE Service UUID
    private val SERVICE_UUID = UUID.fromString("0000ff00-0000-1000-8000-00805f9b34fb")
    private val TX_CHAR_UUID = UUID.fromString("0000ff01-0000-1000-8000-00805f9b34fb")
    private val RX_CHAR_UUID = UUID.fromString("0000ff02-0000-1000-8000-00805f9b34fb")
    
    // Handle incoming command
    private fun handleCommand(jsonString: String) {
        val json = JSONObject(jsonString)
        val command = json.getString("command")
        
        when (command) {
            "OPEN_GOOGLE_MAPS" -> {
                val data = json.getJSONObject("data")
                val origin = data.getString("origin")
                val destination = data.getString("destination")
                
                // Open Google Maps
                openGoogleMaps(origin, destination)
            }
        }
    }
    
    private fun openGoogleMaps(origin: String, destination: String) {
        // Convert place names to coordinates (if needed)
        val originCoords = geocodeAddress(origin)
        val destCoords = geocodeAddress(destination)
        
        // Create Google Maps intent
        val uri = Uri.parse("google.navigation:q=${destCoords.lat},${destCoords.lng}")
        val mapIntent = Intent(Intent.ACTION_VIEW, uri)
        mapIntent.setPackage("com.google.android.apps.maps")
        
        // Start activity
        startActivity(mapIntent)
        
        // Get route information
        val routeInfo = getRouteInfo(originCoords, destCoords)
        
        // Send response back to ESP32
        sendResponse(routeInfo)
    }
    
    private fun getRouteInfo(origin: LatLng, destination: LatLng): RouteInfo {
        // Use Google Maps Directions API
        // Or use Google Maps SDK to get route
        // Return route information
    }
    
    private fun sendResponse(routeInfo: RouteInfo) {
        val json = JSONObject().apply {
            put("status", "success")
            put("command", "OPEN_GOOGLE_MAPS")
            put("data", JSONObject().apply {
                put("route", JSONObject().apply {
                    put("distance", routeInfo.distance)
                    put("distance_m", routeInfo.distanceMeters)
                    put("duration", routeInfo.duration)
                    put("duration_s", routeInfo.durationSeconds)
                    put("steps", routeInfo.steps)
                    put("polyline", routeInfo.polyline)
                })
            })
        }
        
        // Send via BLE
        bluetoothGatt.writeCharacteristic(rxCharacteristic, json.toString().toByteArray())
    }
}
```

#### 3.2. iOS App Requirements

Tương tự Android, sử dụng CoreBluetooth framework và Google Maps SDK.

### 4. Cập Nhật UI

Cần cập nhật `screen_google_navigation.c` để:

1. **Hiển thị trạng thái BLE:**
   - "Đang kết nối với điện thoại..."
   - "Đã kết nối"
   - "Đang gửi lệnh..."
   - "Đang nhận kết quả..."

2. **Hiển thị thông tin điều hướng:**
   - Khoảng cách
   - Thời gian
   - Các bước điều hướng
   - Bản đồ (nếu có polyline)

3. **Xử lý callback từ BLE:**
   ```c
   static void on_navigation_ble_response(const char *json_response, void *user_data) {
       // Parse JSON
       // Update UI
       // Update navigation service
   }
   ```

## Triển Khai Chi Tiết

### Bước 1: Tạo BLE Navigation Service

1. Tạo file `sx_navigation_ble.c` và `sx_navigation_ble.h`
2. Implement BLE GATT Server với custom service
3. Implement TX/RX characteristics
4. Implement JSON parsing

### Bước 2: Tích Hợp với Navigation Service

1. Cập nhật `sx_navigation_calculate_route()` để ưu tiên BLE
2. Thêm callback để xử lý response từ điện thoại
3. Parse route data từ JSON response

### Bước 3: Tích Hợp với Intent Service

1. Thêm pattern matching cho lệnh điều hướng
2. Extract origin và destination từ câu lệnh
3. Gọi navigation service với thông tin đã parse

### Bước 4: Cập Nhật UI

1. Thêm hiển thị trạng thái BLE
2. Đăng ký callback để cập nhật UI
3. Hiển thị route information từ điện thoại

### Bước 5: Phát Triển App Điện Thoại

1. Android app với BLE support
2. Tích hợp Google Maps SDK
3. Xử lý commands từ ESP32
4. Gửi route information về ESP32

## Lợi Ích Của Giải Pháp Này

1. ✅ **Không cần API key:** Sử dụng Google Maps trên điện thoại
2. ✅ **Tận dụng sức mạnh điện thoại:** Xử lý phức tạp trên điện thoại
3. ✅ **Offline support:** Có thể cache route trên điện thoại
4. ✅ **Dễ bảo trì:** Logic điều hướng trên điện thoại, ESP32 chỉ hiển thị
5. ✅ **Tích hợp tốt:** Sử dụng Google Maps native app

## Hạn Chế và Giải Pháp

### Hạn Chế:
1. Cần app điện thoại riêng
2. Phụ thuộc vào kết nối BLE
3. Cần điện thoại luôn bật và kết nối

### Giải Pháp:
1. Tạo app đơn giản, dễ cài đặt
2. Auto-reconnect khi mất kết nối
3. Fallback về simple route nếu không có BLE

## Kế Hoạch Triển Khai

### Phase 1: BLE Communication (1-2 tuần)
- [ ] Implement BLE GATT Server
- [ ] Implement TX/RX characteristics
- [ ] Test BLE communication

### Phase 2: Navigation Integration (1 tuần)
- [ ] Tích hợp với navigation service
- [ ] Parse JSON request/response
- [ ] Test end-to-end flow

### Phase 3: Intent Integration (1 tuần)
- [ ] Thêm pattern matching cho lệnh điều hướng
- [ ] Extract origin/destination
- [ ] Test với STT

### Phase 4: UI Updates (1 tuần)
- [ ] Cập nhật UI với BLE status
- [ ] Hiển thị route information
- [ ] Test UI flow

### Phase 5: Mobile App (2-3 tuần)
- [ ] Develop Android app
- [ ] Integrate Google Maps
- [ ] Test với ESP32

## Kết Luận

Giải pháp này cho phép ESP32 tận dụng sức mạnh của điện thoại để xử lý điều hướng, trong khi ESP32 chỉ cần hiển thị kết quả. Đây là cách tiếp cận hiệu quả và dễ triển khai hơn so với việc gọi API trực tiếp từ ESP32.



