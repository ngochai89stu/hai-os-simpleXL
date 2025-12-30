# Phân Tích Sâu Repo Mẫu: esp32-google-maps

## Tổng Quan

Repo mẫu: [maisonsmd/esp32-google-maps](https://github.com/maisonsmd/esp32-google-maps)

Đây là một dự án hiển thị Google Maps navigation trên màn hình nhỏ gắn vào xe máy. Android app đọc dữ liệu navigation từ Google Maps notification và gửi qua BLE đến ESP32.

## Kiến Trúc Tổng Thể

```
┌─────────────────────────────────────────────────────────────┐
│                    Google Maps App                          │
│  (Đang chạy navigation, hiển thị notification)             │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│              Android App (CatDrive)                         │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ NavigationListener (NotificationListenerService)     │  │
│  │  - Lắng nghe Google Maps notification                │  │
│  │  - Parse notification data                           │  │
│  └────────────────────┬─────────────────────────────────┘  │
│                       │                                      │
│  ┌────────────────────▼─────────────────────────────────┐  │
│  │ BroadcastService (BLE Service)                       │  │
│  │  - Kết nối BLE với ESP32                             │  │
│  │  - Gửi navigation data qua BLE                       │  │
│  └────────────────────┬─────────────────────────────────┘  │
└───────────────────────┼────────────────────────────────────┘
                        │ BLE
                        ▼
┌─────────────────────────────────────────────────────────────┐
│                    ESP32 (Arduino)                          │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ BLE GATT Server                                       │  │
│  │  - Nhận data từ Android                              │  │
│  │  - Parse key-value format                            │  │
│  └────────────────────┬─────────────────────────────────┘  │
│                       │                                      │
│  ┌────────────────────▼─────────────────────────────────┐  │
│  │ UI (LVGL)                                            │  │
│  │  - Hiển thị navigation info                          │  │
│  │  - Hiển thị speed, ETA, next turn                    │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Phân Tích Chi Tiết

### 1. Android App - Cách Đọc Google Maps Notification

#### 1.1. NavigationListener Service

**File:** `NavigationListener.kt`

**Cách hoạt động:**
1. Extends `NotificationListenerService` - Android service để lắng nghe notifications
2. Lọc Google Maps notifications:
   ```kotlin
   private fun isGoogleMapsNotification(sbn: StatusBarNotification?): Boolean {
       if (!enabled || sbn == null) return false
       if (!sbn.isOngoing || GMAPS_PACKAGE !in sbn.packageName) return false
       return (sbn.id == 1)  // Google Maps navigation notification có ID = 1
   }
   ```
3. Parse notification khi có thay đổi:
   ```kotlin
   override fun onNotificationPosted(sbn: StatusBarNotification?) {
       if (isGoogleMapsNotification(sbn))
           handleGoogleNotification(sbn!!)
   }
   ```

#### 1.2. GMapsNotification Parser

**File:** `GMapsNotification.kt`

**Cách parse:**
1. Lấy RemoteViews từ notification:
   ```kotlin
   val remoteViews = Notification.Builder.recoverBuilder(mContext, mNotification)
       .createBigContentView()
   ```
2. Inflate RemoteViews thành ViewGroup:
   ```kotlin
   val viewGroup = layoutInflater.inflate(remoteViews.layoutId, null) as ViewGroup
   remoteViews.reapply(mAppSourceContext, viewGroup)
   ```
3. Tìm các TextView/ImageView bằng resource name:
   ```kotlin
   val directionText = findChildByName(group, "text") as TextView?
   val etaText = findChildByName(group, "header_text") as TextView?
   val rightIcon = findChildByName(group, "right_icon") as ImageView?
   ```
4. Extract data:
   - **Next Road:** Từ `directionText` (phân tích Spanned text với Typeface.BOLD)
   - **Distance to next turn:** Từ `titleText`
   - **ETA/ETE:** Từ `etaText` (format: "ETE · Distance · ETA")
   - **Turn icon:** Từ `rightIcon` bitmap

**Điểm quan trọng:**
- Sử dụng introspection để tìm views bằng resource name
- Parse Spanned text để tách road name (bold) và description (normal)
- Extract bitmap từ ImageView để gửi icon qua BLE

#### 1.3. NavigationData Structure

**File:** `NavigationData.kt`

```kotlin
data class NavigationData(
    var nextDirection: NavigationDirection = NavigationDirection(),  // Next road, description, distance
    var eta: NavigationEta = NavigationEta(),                        // ETA, ETE, total distance
    var actionIcon: NavigationIcon = NavigationIcon(),               // Turn icon bitmap
    var postTime: NavigationTimestamp = NavigationTimestamp()        // Timestamp
)
```

### 2. Android App - BLE Communication

#### 2.1. BroadcastService (BLE Service)

**File:** `BroadcastService.kt`

**Các tính năng chính:**

1. **BLE Connection Management:**
   ```kotlin
   fun connect(device: BluetoothDevice) {
       mBluetoothGatt = device.connectGatt(this, false, gattCallback, 
                                            BluetoothDevice.TRANSPORT_LE)
   }
   ```

2. **Auto-reconnect:**
   ```kotlin
   inner class ReconnectTask : TimerTask() {
       override fun run() {
           if (mConnectionState == BluetoothProfile.STATE_DISCONNECTED) {
               connectToLastDevice()  // Tự động kết nối lại
           }
       }
   }
   ```

3. **Ping Timer (25s):**
   ```kotlin
   inner class PingTask : TimerTask() {
       override fun run() {
           if (mLastNavigationData != null) {
               sendToDevice(mLastNavigationData)  // Gửi lại data mỗi 25s
           }
       }
   }
   ```

4. **Write Queue:**
   - Sử dụng `BleWriteQueue` để queue các write requests
   - Chỉ gửi khi BLE không busy
   - Tự động retry nếu fail

#### 2.2. Data Format - Key-Value String

**Format gửi qua BLE:**
```
nextRd=Đường ABC
nextRdDesc=Rẽ phải
distToNext=200 m
totalDist=15.2 km
eta=25 min
ete=10:30 AM
iconHash=abc123def4
```

**Implementation:**
```kotlin
fun toKeyValString(map: Map<String, String>): String {
    var result = ""
    for ((key, value) in map) {
        result += "$key=$value"
        if (count < map.size) {
            result += "\n"
        }
    }
    return result
}
```

#### 2.3. Icon Transmission

**Cách gửi icon:**
1. Compress bitmap thành 64x62, black & white
2. Tính MD5 hash (10 ký tự cuối)
3. Gửi hash trước trong NAV characteristic
4. Nếu ESP32 chưa có icon → gửi bitmap qua NAV_TBT_ICON characteristic
5. Format: `"hash;bitmap_data"`

### 3. ESP32 - BLE GATT Server

#### 3.1. BLE Setup

**File:** `ble.h`

**Service UUID:** `ec91d7ab-e87c-48d5-adfa-cc4b2951298a`

**Characteristics:**
- `CHA_SETTINGS` (9d37a346-...): Settings (theme, brightness, speed limit)
- `CHA_NAV` (0b11deef-...): Navigation data (key-value string)
- `CHA_NAV_TBT_ICON` (d4d8fcca-...): Turn-by-turn icon (hash + bitmap)
- `CHA_GPS_SPEED` (98b6073a-...): GPS speed from phone

**Implementation:**
```cpp
void initBle() {
    BLEDevice::init("CatDrive");
    BLEDevice::setMTU(240);  // Tăng MTU để gửi data lớn hơn
    
    server.bleServer = BLEDevice::createServer();
    server.bleServer->setCallbacks(new ServerCallbacks());
    
    // Create service and characteristics
    for (auto& serviceConfig : server.services) {
        serviceConfig.bleService = server.bleServer->createService(...);
        for (auto& characteristicConfig : serviceConfig.characteristics) {
            characteristicConfig.bleCharacteristic = 
                serviceConfig.bleService->createCharacteristic(..., 
                    BLECharacteristic::PROPERTY_WRITE);
        }
    }
}
```

#### 3.2. Data Processing

**File:** `esp32.ino`

**Callback khi nhận data:**
```cpp
void onCharacteristicWrite(const String& uuid, uint8_t* data, size_t length) {
    if (uuid == CHA_NAV) {
        navigationQueue.push(String((char*)data));  // Queue để xử lý
        pongNavigation();
    }
    
    if (uuid == CHA_NAV_TBT_ICON) {
        // Parse: "hash;bitmap_data"
        String iconHash = String((char*)data, semicolonIndex);
        Data::receiveNewIcon(iconHash, data + semicolonIndex + 1);
    }
}
```

**Parse key-value:**
```cpp
void processQueue() {
    const auto& data = navigationQueue.front();
    const auto kv = kvParseMultiline(data);  // Parse "key=value\nkey=value"
    
    if (kv.contains("nextRd")) {
        Data::setNextRoad(kv.getOrDefault("nextRd"));
    }
    if (kv.contains("distToNext")) {
        Data::setDistanceToNextTurn(kv.getOrDefault("distToNext"));
    }
    // ... parse các fields khác
}
```

### 4. ESP32 - UI Display

**File:** `ui.h`

**Hiển thị:**
- Speed (từ GPS)
- Next road name
- Next road description
- Distance to next turn
- Total distance
- ETA/ETE
- Turn icon (bitmap)

**Update UI:**
```cpp
void UI::update() {
    // Update labels với data mới
    lv_label_set_text(lblNextRoad, Data::nextRoad().c_str());
    lv_label_set_text(lblDistanceToNextRoad, Data::distanceToNextTurn().c_str());
    // ...
}
```

## So Sánh Với Repo Hiện Tại (hai-os-simplexl)

### Điểm Khác Biệt

| Tiêu Chí | Repo Mẫu | Repo Hiện Tại |
|----------|-----------|---------------|
| **Framework** | Arduino + LVGL | ESP-IDF + LVGL |
| **BLE** | BLE GATT Server (Arduino BLE) | Chưa có BLE navigation |
| **Data Source** | Đọc Google Maps notification | Chưa có (cần tích hợp) |
| **Data Format** | Key-value string | JSON (dự kiến) |
| **Architecture** | Monolithic (Arduino) | Modular (ESP-IDF services) |
| **UI** | Simple LVGL screen | Screen registry system |

### Điểm Tương Đồng

1. ✅ Cả hai đều dùng LVGL
2. ✅ Cả hai đều có navigation service structure
3. ✅ Cả hai đều cần BLE communication

## Áp Dụng Vào Repo Hiện Tại

### Phương Án 1: Đọc Google Maps Notification (Giống Repo Mẫu)

#### Ưu Điểm:
- ✅ Không cần mở Google Maps app
- ✅ Tự động lấy data khi user đang navigation
- ✅ Real-time updates

#### Nhược Điểm:
- ❌ Cần NotificationListenerService permission
- ❌ Phụ thuộc vào Google Maps notification format
- ❌ Có thể bị Google thay đổi format

#### Triển Khai:

1. **Tạo Android App tương tự:**
   - `NavigationListener` service
   - `GMapsNotification` parser
   - BLE service để gửi data

2. **ESP32 BLE Service:**
   - Tạo `sx_navigation_ble.h/c`
   - Implement GATT Server
   - Parse key-value hoặc JSON

3. **Tích hợp với Navigation Service:**
   - Cập nhật `sx_navigation_service.c` để nhận data từ BLE
   - Update route khi có data mới

### Phương Án 2: Mở Google Maps App (Như Thiết Kế Trước)

#### Ưu Điểm:
- ✅ User control (mở app khi cần)
- ✅ Không cần notification permission
- ✅ Có thể mở với destination cụ thể

#### Nhược Điểm:
- ❌ Cần user mở app manually
- ❌ Phức tạp hơn (cần intent handling)

### Phương Án 3: Kết Hợp (Đề Xuất)

**Kết hợp cả hai:**
1. **MCP Chatbot** → Gọi tool để mở Google Maps với destination
2. **Android App** → Đọc notification và gửi data qua BLE
3. **ESP32** → Nhận và hiển thị

## Kế Hoạch Triển Khai Chi Tiết

### Phase 1: BLE Navigation Service (ESP32)

1. **Tạo `sx_navigation_ble.h/c`:**
   ```c
   // Tương tự repo mẫu nhưng dùng ESP-IDF BLE
   - BLE GATT Server
   - Characteristics: NAV, NAV_ICON, GPS_SPEED
   - Parse key-value hoặc JSON
   ```

2. **Tích hợp với Navigation Service:**
   ```c
   // Trong sx_navigation_update_position()
   // Nếu có BLE data → update route
   ```

### Phase 2: Android App

1. **NotificationListener:**
   - Copy logic từ repo mẫu
   - Adapt cho repo hiện tại

2. **BLE Service:**
   - Kết nối với ESP32
   - Gửi navigation data
   - Auto-reconnect

### Phase 3: MCP Integration

1. **MCP Tool:**
   - `self.navigation.open_google_maps`
   - Gửi lệnh mở Google Maps qua BLE

2. **Chatbot:**
   - Parse lệnh điều hướng
   - Gọi MCP tool

### Phase 4: UI Updates

1. **Cập nhật `screen_google_navigation.c`:**
   - Hiển thị data từ BLE
   - Real-time updates
   - Hiển thị turn icon

## Code Mẫu - BLE Navigation Service

### ESP32 Side (ESP-IDF)

```c
// components/sx_services/sx_navigation_ble.h
#pragma once

#include <esp_err.h>
#include <stdbool.h>

// BLE Characteristics (giống repo mẫu)
#define NAV_SERVICE_UUID     "ec91d7ab-e87c-48d5-adfa-cc4b2951298a"
#define NAV_CHAR_UUID        "0b11deef-1563-447f-aece-d3dfeb1c1f20"
#define NAV_ICON_CHAR_UUID   "d4d8fcca-16b2-4b8e-8ed5-90137c44a8ad"
#define GPS_SPEED_CHAR_UUID  "98b6073a-5cf3-4e73-b6d3-f8e05fa018a9"

// Navigation data callback
typedef void (*sx_nav_ble_data_cb_t)(const char *next_road, 
                                      const char *distance,
                                      const char *eta,
                                      void *user_data);

esp_err_t sx_navigation_ble_init(void);
esp_err_t sx_navigation_ble_start(void);
bool sx_navigation_ble_is_connected(void);
void sx_navigation_ble_set_data_callback(sx_nav_ble_data_cb_t callback, void *user_data);
```

### Android Side (Kotlin)

```kotlin
// Tương tự BroadcastService.kt từ repo mẫu
// Nhưng adapt cho ESP-IDF BLE characteristics
```

## Kết Luận

Repo mẫu cung cấp một giải pháp hoàn chỉnh để:
1. ✅ Đọc Google Maps notification
2. ✅ Gửi data qua BLE
3. ✅ Hiển thị trên ESP32

**Để áp dụng vào repo hiện tại:**
1. Adapt BLE code từ Arduino sang ESP-IDF
2. Tích hợp với navigation service hiện có
3. Tạo Android app tương tự
4. Kết hợp với MCP chatbot để mở Google Maps

**Lợi ích:**
- Real-time navigation data
- Không cần API key
- Tận dụng Google Maps trên điện thoại
- User experience tốt










