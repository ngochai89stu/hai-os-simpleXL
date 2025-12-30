# Thiết Kế Navigation Qua MCP Chatbot + BLE

## Tổng Quan

Sử dụng MCP (Model Context Protocol) Chatbot để xử lý lệnh điều hướng, sau đó gọi BLE service để mở Google Maps trên điện thoại. Đây là cách tiếp cận thông minh hơn vì chatbot có thể hiểu ngữ cảnh và xử lý các biến thể của lệnh.

## Luồng Hoạt Động

```
┌─────────────────────────────────────────────────────────────┐
│ 1. Người dùng nói: "Chỉ đường đi từ nhà đến bến xe miền tây"│
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. STT Service nhận diện giọng nói                          │
│    → Chuyển text sang Chatbot Service                      │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. MCP Chatbot xử lý lệnh                                   │
│    → AI hiểu intent: navigation                             │
│    → Extract: origin="nhà", destination="bến xe miền tây"  │
│    → Gọi MCP Tool: self.navigation.open_google_maps        │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ 4. MCP Tool Handler xử lý                                   │
│    → mcp_tool_navigation_open_google_maps()                 │
│    → Kiểm tra BLE connection                                │
│    → Nếu chưa kết nối → Tự động kết nối                    │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ 5. BLE Navigation Service gửi lệnh                          │
│    → Gửi JSON qua BLE đến điện thoại                        │
│    → Command: "OPEN_GOOGLE_MAPS"                           │
│    → Data: origin, destination                              │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ 6. Điện thoại nhận lệnh và mở Google Maps                  │
│    → Parse JSON command                                      │
│    → Mở Google Maps app                                     │
│    → Nhập điều hướng                                        │
│    → Lấy route information                                  │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ 7. Điện thoại gửi kết quả về ESP32                          │
│    → Route data qua BLE                                     │
│    → JSON response với route information                    │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ 8. ESP32 nhận và xử lý kết quả                              │
│    → BLE callback nhận response                             │
│    → Parse route data                                       │
│    → Cập nhật Navigation Service                           │
│    → Chatbot trả lời user                                   │
│    → Hiển thị trên UI                                       │
│    → Phát TTS hướng dẫn                                      │
└─────────────────────────────────────────────────────────────┘
```

## Kiến Trúc Hệ Thống

### 1. MCP Tool: Navigation

#### 1.1. Tool Definition

**Tool Name:** `self.navigation.open_google_maps`

**Description:**
```
Open Google Maps on connected phone via BLE and get navigation directions.
Automatically connects to phone if not connected.
Returns route information including distance, duration, and turn-by-turn instructions.
```

**Parameters:**
```json
{
  "origin": "nhà",                    // Điểm xuất phát (string)
  "destination": "bến xe miền tây",   // Điểm đến (string, required)
  "use_current_location": false       // Dùng vị trí hiện tại làm origin (bool, optional)
}
```

**Response:**
```json
{
  "success": true,
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
    ]
  },
  "ble_status": "connected",
  "phone_response": "success"
}
```

#### 1.2. Code Implementation

**File:** `components/sx_services/sx_mcp_tools_navigation.c`

```c
// MCP Tools - Navigation
#include "sx_mcp_tools.h"
#include "sx_navigation_ble.h"  // BLE Navigation service
#include "sx_navigation_service.h"
#include "sx_tts_service.h"

#include <esp_log.h>
#include <string.h>
#include "cJSON.h"

static const char *TAG = "sx_mcp_tools_navigation";

// Use shared helpers
extern cJSON* mcp_tool_create_error(const char *id, int code, const char *message);
extern cJSON* mcp_tool_create_success(const char *id, cJSON *result);

// Callback for BLE navigation response
static void on_navigation_ble_response(const char *json_response, void *user_data);
static cJSON *s_pending_response = NULL;
static char s_pending_id[64] = {0};
static SemaphoreHandle_t s_response_mutex = NULL;

// self.navigation.open_google_maps
cJSON* mcp_tool_navigation_open_google_maps(cJSON *params, const char *id) {
    // Validate parameters
    cJSON *destination = cJSON_GetObjectItem(params, "destination");
    if (!destination || !cJSON_IsString(destination)) {
        return mcp_tool_create_error(id, -32602, "Invalid params: destination required (string)");
    }
    
    cJSON *origin = cJSON_GetObjectItem(params, "origin");
    cJSON *use_current = cJSON_GetObjectItem(params, "use_current_location");
    
    // Prepare request
    sx_nav_ble_request_t request = {0};
    
    if (use_current && cJSON_IsTrue(use_current)) {
        request.use_current_location = true;
        strncpy(request.origin, "current_location", sizeof(request.origin) - 1);
    } else if (origin && cJSON_IsString(origin)) {
        strncpy(request.origin, origin->valuestring, sizeof(request.origin) - 1);
    } else {
        // Default to "current location" if origin not specified
        request.use_current_location = true;
        strncpy(request.origin, "current_location", sizeof(request.origin) - 1);
    }
    
    strncpy(request.destination, destination->valuestring, sizeof(request.destination) - 1);
    
    // Check BLE connection
    if (!sx_navigation_ble_is_connected()) {
        ESP_LOGI(TAG, "BLE not connected, attempting to connect...");
        esp_err_t ret = sx_navigation_ble_connect();
        if (ret != ESP_OK) {
            cJSON *result = cJSON_CreateObject();
            cJSON_AddBoolToObject(result, "success", false);
            cJSON_AddStringToObject(result, "error", "Failed to connect to phone via BLE");
            cJSON_AddStringToObject(result, "ble_status", "disconnected");
            return mcp_tool_create_success(id, result);
        }
        
        // Wait for connection (with timeout)
        int retries = 10;
        while (!sx_navigation_ble_is_connected() && retries > 0) {
            vTaskDelay(pdMS_TO_TICKS(500));
            retries--;
        }
        
        if (!sx_navigation_ble_is_connected()) {
            cJSON *result = cJSON_CreateObject();
            cJSON_AddBoolToObject(result, "success", false);
            cJSON_AddStringToObject(result, "error", "BLE connection timeout");
            cJSON_AddStringToObject(result, "ble_status", "timeout");
            return mcp_tool_create_success(id, result);
        }
    }
    
    // Set up response callback
    if (!s_response_mutex) {
        s_response_mutex = xSemaphoreCreateMutex();
    }
    
    strncpy(s_pending_id, id ? id : "unknown", sizeof(s_pending_id) - 1);
    sx_navigation_ble_set_response_callback(on_navigation_ble_response, NULL);
    
    // Send request via BLE
    esp_err_t ret = sx_navigation_ble_send_request(&request);
    if (ret != ESP_OK) {
        cJSON *result = cJSON_CreateObject();
        cJSON_AddBoolToObject(result, "success", false);
        cJSON_AddStringToObject(result, "error", "Failed to send BLE request");
        cJSON_AddStringToObject(result, "ble_status", sx_navigation_ble_is_connected() ? "connected" : "disconnected");
        return mcp_tool_create_success(id, result);
    }
    
    // Wait for response (with timeout - 10 seconds)
    int timeout_ms = 10000;
    int waited_ms = 0;
    cJSON *response = NULL;
    
    while (waited_ms < timeout_ms) {
        if (xSemaphoreTake(s_response_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (s_pending_response != NULL) {
                response = s_pending_response;
                s_pending_response = NULL;
                xSemaphoreGive(s_response_mutex);
                break;
            }
            xSemaphoreGive(s_response_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        waited_ms += 100;
    }
    
    if (response == NULL) {
        // Timeout - return partial result
        cJSON *result = cJSON_CreateObject();
        cJSON_AddBoolToObject(result, "success", false);
        cJSON_AddStringToObject(result, "error", "Timeout waiting for phone response");
        cJSON_AddStringToObject(result, "ble_status", "timeout");
        cJSON_AddStringToObject(result, "message", "Request sent to phone, but no response received");
        return mcp_tool_create_success(id, result);
    }
    
    // Parse response and return
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", true);
    cJSON_AddStringToObject(result, "ble_status", "connected");
    
    // Copy route data from response
    cJSON *route = cJSON_GetObjectItem(response, "route");
    if (route) {
        cJSON_AddItemToObject(result, "route", cJSON_Duplicate(route, 1));
    }
    
    cJSON_Delete(response);
    return mcp_tool_create_success(id, result);
}

// Callback for BLE navigation response
static void on_navigation_ble_response(const char *json_response, void *user_data) {
    (void)user_data;
    
    ESP_LOGI(TAG, "Received BLE response: %s", json_response);
    
    if (xSemaphoreTake(s_response_mutex, portMAX_DELAY) == pdTRUE) {
        if (s_pending_response) {
            cJSON_Delete(s_pending_response);
        }
        
        // Parse JSON response
        s_pending_response = cJSON_Parse(json_response);
        if (!s_pending_response) {
            ESP_LOGE(TAG, "Failed to parse BLE response JSON");
            s_pending_response = cJSON_CreateObject();
            cJSON_AddStringToObject(s_pending_response, "error", "Invalid JSON response");
        }
        
        xSemaphoreGive(s_response_mutex);
    }
    
    // Update navigation service with route data
    cJSON *json = cJSON_Parse(json_response);
    if (json) {
        cJSON *route = cJSON_GetObjectItem(json, "route");
        if (route) {
            // Convert route data to navigation service format
            // This would update sx_navigation_service with the route
            // Implementation depends on navigation service API
        }
        cJSON_Delete(json);
    }
    
    // Speak notification via TTS
    sx_tts_speak_simple("Đã nhận được thông tin điều hướng từ điện thoại");
}

// Register navigation tools
esp_err_t sx_mcp_tools_navigation_register(void) {
    esp_err_t ret = ESP_OK;
    
    ret = sx_mcp_server_register_tool("self.navigation.open_google_maps",
        "Open Google Maps on connected phone via BLE and get navigation directions. "
        "Automatically connects to phone if not connected. "
        "Parameters: destination (required, string), origin (optional, string), "
        "use_current_location (optional, bool). "
        "Returns route information including distance, duration, and turn-by-turn instructions.",
        mcp_tool_navigation_open_google_maps, false);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register navigation tools");
        return ret;
    }
    
    ESP_LOGI(TAG, "Navigation MCP tools registered");
    return ESP_OK;
}
```

#### 1.3. Đăng Ký Tool

Cập nhật `sx_mcp_tools_register_all()` trong `sx_mcp_tools.c`:

```c
esp_err_t sx_mcp_tools_register_all(void) {
    // ... existing tools ...
    
    // Navigation tools
    extern esp_err_t sx_mcp_tools_navigation_register(void);
    sx_mcp_tools_navigation_register();
    
    return ESP_OK;
}
```

### 2. Tích Hợp với Chatbot Service

Chatbot service đã có sẵn cơ chế gọi MCP tools. Khi chatbot nhận lệnh điều hướng, nó sẽ tự động:

1. Parse lệnh và hiểu intent
2. Extract parameters (origin, destination)
3. Gọi MCP tool `self.navigation.open_google_maps`
4. Trả về kết quả cho user

**Ví dụ lệnh chatbot có thể xử lý:**
- "Chỉ đường đi từ nhà đến bến xe miền tây"
- "Điều hướng đến sân bay Tân Sơn Nhất"
- "Mở Google Maps đi từ đây đến chợ Bến Thành"
- "Navigation from home to central park"

### 3. BLE Navigation Service

Cần tạo BLE Navigation Service như đã thiết kế trong `THIET_KE_NAVIGATION_BLE.md`:

**File:** `components/sx_services/sx_navigation_ble.c`

Service này sẽ:
1. Quản lý BLE connection với điện thoại
2. Gửi commands qua BLE
3. Nhận responses từ điện thoại
4. Callback để thông báo khi có response

### 4. Cập Nhật Navigation Service

Cập nhật `sx_navigation_service.c` để tích hợp với BLE:

```c
// Trong sx_navigation_calculate_route()
esp_err_t sx_navigation_calculate_route(const sx_nav_coordinate_t *start,
                                         const sx_nav_coordinate_t *end,
                                         sx_nav_route_t *route) {
    // ...
    
    // Priority 1: Try BLE + Phone (via MCP tool)
    // This will be called from MCP tool, so we can skip here
    // or use it as fallback
    
    // Priority 2: Try API-based routing
    if (sx_wifi_is_connected()) {
        // ... existing API code ...
    }
    
    // Priority 3: Fallback to simple route
    // ... existing simple route code ...
}
```

## Ưu Điểm Của Phương Án MCP

### 1. **Thông Minh Hơn**
- Chatbot có thể hiểu nhiều biến thể của lệnh
- Có thể xử lý ngữ cảnh và câu hỏi follow-up
- Tự động extract parameters từ câu lệnh tự nhiên

### 2. **Linh Hoạt**
- Không cần hardcode pattern matching
- Có thể xử lý các lệnh phức tạp hơn
- Dễ mở rộng với các tính năng mới

### 3. **Tích Hợp Tốt**
- Sử dụng cơ chế MCP đã có sẵn
- Không cần tạo Intent Service riêng cho navigation
- Tận dụng chatbot infrastructure

### 4. **User Experience Tốt**
- Chatbot có thể trả lời và giải thích
- Có thể hỏi lại nếu thiếu thông tin
- Có thể xử lý lỗi một cách thông minh

## Ví Dụ Tương Tác

### Scenario 1: Lệnh Đầy Đủ

**User:** "Chỉ đường đi từ nhà đến bến xe miền tây"

**Chatbot:**
1. Hiểu intent: navigation
2. Extract: origin="nhà", destination="bến xe miền tây"
3. Gọi `self.navigation.open_google_maps` với params
4. Nhận response từ BLE
5. Trả lời: "Đã mở Google Maps trên điện thoại. Tuyến đường: 15.2 km, thời gian: 25 phút. Bắt đầu điều hướng..."

### Scenario 2: Lệnh Thiếu Thông Tin

**User:** "Chỉ đường đến sân bay"

**Chatbot:**
1. Hiểu intent: navigation
2. Extract: destination="sân bay", origin=null
3. Hỏi lại: "Bạn muốn đi từ đâu đến sân bay? (nhà / vị trí hiện tại / địa chỉ cụ thể)"
4. User trả lời: "từ nhà"
5. Gọi MCP tool với origin="nhà"

### Scenario 3: Lệnh Phức Tạp

**User:** "Tôi muốn đi từ công ty về nhà, mở Google Maps giúp tôi"

**Chatbot:**
1. Hiểu intent: navigation
2. Extract: origin="công ty", destination="nhà"
3. Gọi MCP tool
4. Trả lời: "Đã mở Google Maps với tuyến đường từ công ty về nhà..."

## Triển Khai Chi Tiết

### Bước 1: Tạo BLE Navigation Service

1. Tạo `sx_navigation_ble.h` và `sx_navigation_ble.c`
2. Implement BLE GATT Server
3. Implement TX/RX characteristics
4. Test BLE communication

### Bước 2: Tạo MCP Navigation Tool

1. Tạo `sx_mcp_tools_navigation.c`
2. Implement `mcp_tool_navigation_open_google_maps()`
3. Implement response callback
4. Đăng ký tool trong `sx_mcp_tools_register_all()`

### Bước 3: Tích Hợp với Chatbot

1. Chatbot đã tự động gọi MCP tools
2. Test với các lệnh điều hướng khác nhau
3. Xử lý edge cases

### Bước 4: Cập Nhật UI

1. Cập nhật `screen_google_navigation.c`
2. Hiển thị trạng thái BLE
3. Hiển thị route information từ chatbot response
4. Callback để cập nhật real-time

### Bước 5: Phát Triển App Điện Thoại

1. Android/iOS app với BLE support
2. Tích hợp Google Maps SDK
3. Xử lý commands từ ESP32
4. Gửi route information về ESP32

## So Sánh Với Phương Án Intent Service

| Tiêu Chí | Intent Service | MCP Chatbot |
|----------|----------------|-------------|
| **Độ thông minh** | Pattern matching cố định | AI hiểu ngữ cảnh |
| **Linh hoạt** | Cần hardcode patterns | Tự động adapt |
| **Xử lý lỗi** | Manual error handling | Chatbot tự xử lý |
| **User experience** | Cơ bản | Tự nhiên, có thể hỏi lại |
| **Tích hợp** | Cần tạo service riêng | Dùng infrastructure có sẵn |
| **Mở rộng** | Khó mở rộng | Dễ thêm tools mới |

## Kết Luận

Sử dụng MCP Chatbot để xử lý lệnh điều hướng là phương án tốt hơn vì:

1. ✅ **Tận dụng AI:** Chatbot hiểu ngữ cảnh và xử lý lệnh tự nhiên
2. ✅ **Dễ triển khai:** Sử dụng MCP infrastructure đã có
3. ✅ **User experience tốt:** Tương tác tự nhiên, có thể hỏi lại
4. ✅ **Dễ mở rộng:** Thêm tools mới dễ dàng
5. ✅ **Xử lý lỗi thông minh:** Chatbot tự động xử lý các trường hợp edge case

Phương án này kết hợp sức mạnh của AI chatbot với BLE communication để tạo ra một trải nghiệm điều hướng thông minh và tiện lợi.










