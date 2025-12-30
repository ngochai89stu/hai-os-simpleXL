# Phân Tích Android App Trong Repo Mẫu

## Tổng Quan

Android app trong repo mẫu là một ứng dụng **hoàn chỉnh và có thể sử dụng ngay**, được viết bằng **Kotlin** với Android SDK.

## Công Nghệ Sử Dụng

### 1. **Ngôn Ngữ & Framework**
- ✅ **Kotlin** (version 1.7.20)
- ✅ **Android SDK** (minSdk 31, targetSdk 32)
- ✅ **AndroidX Libraries**
- ✅ **Material Design Components**

### 2. **Dependencies Chính**

```gradle
// Core Android
- androidx.core:core-ktx:1.7.0
- androidx.appcompat:appcompat:1.5.1
- androidx.lifecycle:lifecycle-viewmodel:2.5.1

// Navigation
- androidx.navigation:navigation-fragment-ktx:2.5.3
- androidx.navigation:navigation-ui-ktx:2.5.3

// Kotlin
- kotlinx-coroutines-core:1.4.2
- kotlinx-serialization-json:1.4.1

// Utilities
- com.jakewharton.timber:timber:4.7.1 (Logging)
- com.google.android.material:material:1.7.0
```

### 3. **Package Name**
```
com.maisonsmd.catdrive
```

## Cấu Trúc App

### 1. **Services (Background Services)**

#### a. **GoogleMapNotificationListener**
- **Type:** `NotificationListenerService`
- **Chức năng:** Lắng nghe Google Maps notification
- **Permission:** `BIND_NOTIFICATION_LISTENER_SERVICE`
- **File:** `GoogleMapNotificationListener.kt`

#### b. **BleService** (BroadcastService)
- **Type:** `ForegroundService`
- **Chức năng:** 
  - Kết nối BLE với ESP32
  - Gửi navigation data qua BLE
  - GPS speed tracking
- **Permission:** `FOREGROUND_SERVICE` (location type)
- **File:** `service/BroadcastService.kt`

### 2. **Core Libraries**

#### a. **Navigation Parsing**
- `lib/GMapsNotification.kt` - Parse Google Maps notification
- `lib/NavigationData.kt` - Data structures
- `lib/ParserHelper.kt` - Text parsing utilities

#### b. **BLE Communication**
- `lib/BleCharacteristics.kt` - UUID definitions
- `lib/BleWriteQueue.kt` - Write queue management
- `lib/BitmapHelper.kt` - Icon compression

### 3. **UI Components**

- `MainActivity.kt` - Main activity
- `ui/home/HomeFragment.kt` - Home screen
- `ui/settings/SettingsFragment.kt` - Settings
- `ui/BleDeviceSelectionActivity.kt` - BLE device selection

## Có Thể Sử Dụng Luôn Không?

### ✅ **CÓ THỂ SỬ DỤNG** - Nhưng cần một số thay đổi

### 1. **Điều Kiện**

#### a. **Yêu Cầu Hệ Thống**
- ✅ Android 12+ (API 31+) - **Đã có sẵn**
- ✅ Kotlin 1.7.20 - **Đã có sẵn**
- ✅ Android Studio - **Cần cài đặt**

#### b. **Permissions Cần Thiết**
```xml
- BLUETOOTH
- BLUETOOTH_ADMIN
- BLUETOOTH_CONNECT (Android 12+)
- BLUETOOTH_SCAN (Android 12+)
- ACCESS_FINE_LOCATION
- ACCESS_COARSE_LOCATION
- BIND_NOTIFICATION_LISTENER_SERVICE
- FOREGROUND_SERVICE
```

### 2. **Những Gì Cần Thay Đổi**

#### a. **Package Name (Bắt Buộc)**
```gradle
// Hiện tại:
applicationId "com.maisonsmd.catdrive"

// Nên đổi thành:
applicationId "com.yourcompany.haios.navigation"
// hoặc
applicationId "com.haios.simplexl.navigation"
```

**Lý do:** Tránh conflict với app khác, tuân thủ naming convention.

#### b. **App Name & Icon (Tùy chọn)**
```xml
<!-- strings.xml -->
<string name="app_name">HAI-OS Navigation</string>

<!-- Thay icon trong res/mipmap-* -->
```

#### c. **BLE Service UUIDs (Có thể giữ nguyên)**
```kotlin
// BleCharacteristics.kt
const val SERVICE_UUID = "ec91d7ab-e87c-48d5-adfa-cc4b2951298a"
const val CHA_NAV = "0b11deef-1563-447f-aece-d3dfeb1c1f20"
// ... các UUID khác
```

**Lưu ý:** Nếu giữ nguyên UUIDs, ESP32 cũng phải dùng cùng UUIDs.

#### d. **SharedPreferences Key (Tùy chọn)**
```kotlin
// MainActivity.kt
const val SHARED_PREFERENCES_FILE = "${BuildConfig.APPLICATION_ID}.preferences"
// Sẽ tự động thay đổi theo applicationId mới
```

### 3. **Những Gì KHÔNG Cần Thay Đổi**

✅ **Core Logic:**
- Notification parsing logic
- BLE communication
- Data structures
- Icon compression

✅ **UI Structure:**
- Fragment structure
- Navigation flow
- Settings screen

## Hướng Dẫn Sử Dụng

### Bước 1: Clone/Copy App

```bash
# Option 1: Copy toàn bộ folder
cp -r D:\esp32-google-maps-reference\android-app D:\NEWESP32\hai-os-simplexl\android-app

# Option 2: Import vào Android Studio
# File → New → Import Project → Chọn android-app folder
```

### Bước 2: Thay Đổi Package Name

#### a. **Trong build.gradle:**
```gradle
android {
    namespace 'com.haios.simplexl.navigation'  // Đổi namespace
    
    defaultConfig {
        applicationId "com.haios.simplexl.navigation"  // Đổi applicationId
    }
}
```

#### b. **Refactor Package trong Android Studio:**
1. Right-click package `com.maisonsmd.catdrive`
2. Refactor → Rename
3. Đổi thành `com.haios.simplexl.navigation`
4. Android Studio sẽ tự động update tất cả imports

#### c. **Update Manifest:**
```xml
<!-- AndroidManifest.xml -->
<manifest package="com.haios.simplexl.navigation">
    <!-- ... -->
</manifest>
```

### Bước 3: Thay Đổi App Name

```xml
<!-- app/src/main/res/values/strings.xml -->
<string name="app_name">HAI-OS Navigation</string>
```

### Bước 4: Build & Install

```bash
# Trong Android Studio:
# Build → Make Project
# Run → Run 'app'

# Hoặc command line:
cd android-app
./gradlew assembleDebug
adb install app/build/outputs/apk/debug/app-debug.apk
```

### Bước 5: Cấp Quyền

Sau khi cài đặt:
1. Mở Settings → Apps → HAI-OS Navigation
2. Permissions → Cấp quyền:
   - Location
   - Bluetooth
   - Notification Access (quan trọng!)

3. **Notification Access (Bắt buộc):**
   - Settings → Apps → Special access → Notification access
   - Bật "HAI-OS Navigation"

## Tích Hợp Với ESP32

### 1. **ESP32 Phải Dùng Cùng UUIDs**

```c
// Trong sx_navigation_ble.h
#define NAV_SERVICE_UUID     "ec91d7ab-e87c-48d5-adfa-cc4b2951298a"
#define NAV_CHAR_UUID        "0b11deef-1563-447f-aece-d3dfeb1c1f20"
#define NAV_ICON_CHAR_UUID   "d4d8fcca-16b2-4b8e-8ed5-90137c44a8ad"
#define GPS_SPEED_CHAR_UUID  "98b6073a-5cf3-4e73-b6d3-f8e05fa018a9"
```

### 2. **ESP32 Phải Parse Key-Value Format**

```c
// Format nhận từ Android:
// "nextRd=Đường ABC\ndistToNext=200 m\neta=25 min"
```

## Lưu Ý Quan Trọng

### 1. **NotificationListenerService**

- ✅ **Hoạt động:** App đọc Google Maps notification
- ⚠️ **Hạn chế:** 
  - Cần user enable notification access manually
  - Phụ thuộc vào Google Maps notification format
  - Có thể bị Google thay đổi format

### 2. **Android Version**

- ✅ **Min SDK 31:** Chỉ chạy trên Android 12+
- ⚠️ **Lý do:** Sử dụng các API mới (BLUETOOTH_CONNECT, etc.)

### 3. **Google Maps**

- ✅ **Không cần:** Google Maps SDK
- ✅ **Chỉ cần:** Google Maps app đã cài đặt
- ✅ **Cách hoạt động:** Đọc notification khi đang navigation

## Tóm Tắt

### ✅ **CÓ THỂ SỬ DỤNG LUÔN**

**Điều kiện:**
1. ✅ App code hoàn chỉnh, có thể build ngay
2. ⚠️ Cần thay đổi package name
3. ⚠️ Cần cấp quyền notification access
4. ⚠️ ESP32 phải dùng cùng BLE UUIDs

**Thời gian setup:**
- Copy & refactor: ~30 phút
- Build & test: ~15 phút
- **Tổng: ~45 phút**

**Khuyến nghị:**
- ✅ Sử dụng app này làm base
- ✅ Thay đổi package name và branding
- ✅ Giữ nguyên core logic (notification parsing, BLE)
- ✅ Tích hợp với ESP32 bằng cách dùng cùng UUIDs

## Kết Luận

Android app trong repo mẫu là một **ứng dụng hoàn chỉnh và production-ready**, có thể sử dụng ngay sau khi:
1. Thay đổi package name
2. Cấp quyền notification access
3. Đảm bảo ESP32 dùng cùng BLE UUIDs

Đây là một giải pháp tốt vì:
- ✅ Code đã được test và hoạt động
- ✅ Logic parse notification đã được implement
- ✅ BLE communication đã stable
- ✅ UI đã có sẵn

**Có thể bắt đầu sử dụng ngay!** 🚀



