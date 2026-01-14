# Hướng Dẫn Build Android APK

## Tổng Quan

Hướng dẫn này sẽ giúp bạn build APK từ Android app trong repo mẫu `esp32-google-maps-reference`.

## 📋 Yêu Cầu

1. ✅ Android Studio đã được cài đặt
2. ✅ Android SDK đã được cấu hình
3. ✅ JDK 8 hoặc cao hơn

## 🔧 Bước 1: Cấu Hình Android SDK Path

### Kiểm Tra Android SDK Location

Mở PowerShell và chạy:

```powershell
# Kiểm tra Android SDK location
if (Test-Path "$env:LOCALAPPDATA\Android\Sdk") {
    Write-Host "Android SDK found at: $env:LOCALAPPDATA\Android\Sdk"
} elseif (Test-Path "C:\Users\$env:USERNAME\AppData\Local\Android\Sdk") {
    Write-Host "Android SDK found at: C:\Users\$env:USERNAME\AppData\Local\Android\Sdk"
} else {
    Write-Host "Android SDK not found in default location"
    Write-Host "Please find your SDK path in Android Studio: File > Settings > Appearance & Behavior > System Settings > Android SDK"
}
```

### Cập Nhật local.properties

Mở file `D:\esp32-google-maps-reference\android-app\local.properties` và cập nhật SDK path:

```properties
## This file must *NOT* be checked into Version Control Systems,
# as it contains information specific to your local configuration.
#
# Location of the SDK. This is only used by Gradle.
sdk.dir=C\:\\Users\\YOUR_USERNAME\\AppData\\Local\\Android\\Sdk
```

**Lưu ý:** Thay `YOUR_USERNAME` bằng tên user của bạn, và escape backslashes với `\\`

---

## 🎯 Bước 2: Build APK - Phương Pháp 1: Android Studio (GUI)

### 2.1. Mở Project trong Android Studio

1. Mở Android Studio
2. Chọn **File > Open**
3. Chọn folder: `D:\esp32-google-maps-reference\android-app`
4. Đợi Android Studio sync project (download dependencies nếu cần)

### 2.2. Cấu Hình Build

1. Chọn **Build > Select Build Variant**
2. Chọn **debug** hoặc **release** (khuyến nghị: **debug** cho lần đầu)

### 2.3. Build APK

**Cách 1: Build từ Menu**
- Chọn **Build > Build Bundle(s) / APK(s) > Build APK(s)**
- Đợi build hoàn tất
- APK sẽ nằm tại: `android-app\app\build\outputs\apk\debug\app-debug.apk`

**Cách 2: Build và Install trực tiếp**
- Kết nối điện thoại Android qua USB
- Bật **USB Debugging** trên điện thoại
- Chọn **Run > Run 'app'** hoặc nhấn `Shift + F10`
- APK sẽ được build và cài đặt tự động

---

## 🚀 Bước 3: Build APK - Phương Pháp 2: Command Line (Script)

### 3.1. Sử Dụng Script Tự Động

Tôi đã tạo script `build_android_apk.ps1` để build tự động. Chạy:

```powershell
cd D:\esp32-google-maps-reference\android-app
.\build_android_apk.ps1
```

### 3.2. Build Thủ Công với Gradle

```powershell
cd D:\esp32-google-maps-reference\android-app

# Build debug APK
.\gradlew.bat assembleDebug

# Build release APK (cần signing config)
.\gradlew.bat assembleRelease

# APK sẽ nằm tại:
# android-app\app\build\outputs\apk\debug\app-debug.apk
# hoặc
# android-app\app\build\outputs\apk\release\app-release.apk
```

---

## 📱 Bước 4: Cài Đặt APK vào Điện Thoại

### Cách 1: Qua USB (ADB)

```powershell
# Kết nối điện thoại qua USB
# Bật USB Debugging trên điện thoại

# Cài đặt APK
adb install D:\esp32-google-maps-reference\android-app\app\build\outputs\apk\debug\app-debug.apk
```

### Cách 2: Copy File và Cài Thủ Công

1. Copy file `app-debug.apk` vào điện thoại (qua USB, email, hoặc cloud)
2. Trên điện thoại, mở file APK
3. Cho phép "Install from Unknown Sources" nếu được hỏi
4. Cài đặt APK

---

## 🔍 Troubleshooting

### Lỗi: "SDK location not found"

**Giải pháp:**
1. Kiểm tra file `local.properties` có đúng path không
2. Tạo file `local.properties` nếu chưa có:
   ```properties
   sdk.dir=C\:\\Users\\YOUR_USERNAME\\AppData\\Local\\Android\\Sdk
   ```

### Lỗi: "Gradle sync failed"

**Giải pháp:**
1. Mở Android Studio
2. Chọn **File > Invalidate Caches / Restart**
3. Chọn **Invalidate and Restart**
4. Đợi Android Studio restart và sync lại

### Lỗi: "JDK not found"

**Giải pháp:**
1. Android Studio thường đi kèm với JDK
2. Hoặc cài JDK 8+ từ Oracle/OpenJDK
3. Cấu hình JDK trong Android Studio: **File > Settings > Build, Execution, Deployment > Build Tools > Gradle**

### Lỗi: "Min SDK version too high"

**Giải pháp:**
- App yêu cầu minSdk 31 (Android 12)
- Đảm bảo điện thoại chạy Android 12 trở lên
- Hoặc giảm minSdk trong `app/build.gradle` (không khuyến nghị)

---

## 📝 Lưu Ý Quan Trọng

1. **Notification Listener Permission:**
   - App cần quyền "Notification Listener" để đọc Google Maps notifications
   - Sau khi cài, vào **Settings > Apps > CatDrive > Notifications > Notification access** và bật

2. **Location Permission:**
   - App cần quyền Location để lấy GPS speed
   - Cho phép khi được hỏi

3. **Bluetooth Permission:**
   - App cần quyền Bluetooth để kết nối với ESP32
   - Cho phép khi được hỏi

---

## ✅ Checklist

- [ ] Android Studio đã cài đặt
- [ ] Android SDK path đã cấu hình trong `local.properties`
- [ ] Project đã sync thành công trong Android Studio
- [ ] APK đã được build thành công
- [ ] APK đã được cài đặt vào điện thoại
- [ ] Notification Listener permission đã được cấp
- [ ] Location permission đã được cấp
- [ ] Bluetooth permission đã được cấp

---

## 🎉 Hoàn Thành

Sau khi build và cài đặt APK thành công, bạn có thể:

1. Mở app "CatDrive" trên điện thoại
2. Kết nối với ESP32 qua BLE
3. Mở Google Maps và bắt đầu navigation
4. App sẽ tự động đọc notification và gửi data đến ESP32

**Chúc bạn thành công!** 🚀




















