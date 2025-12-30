# Hướng Dẫn Build Android App

## Tình Trạng Hiện Tại

Sau khi kiểm tra, môi trường build **chưa sẵn sàng**:
- ❌ Java/JDK chưa được cài đặt hoặc chưa có trong PATH
- ❌ Android SDK chưa được cấu hình (ANDROID_HOME chưa set)
- ✅ Gradle wrapper có sẵn trong repo

## Các Phương Án Build

### Phương Án 1: Build Bằng Android Studio (Khuyến Nghị)

#### Bước 1: Cài Đặt Android Studio

1. **Download Android Studio:**
   - Link: https://developer.android.com/studio
   - File: `android-studio-*.exe` (~1GB)

2. **Cài đặt:**
   - Chạy installer
   - Chọn "Standard" installation
   - Android Studio sẽ tự động cài:
     - JDK (OpenJDK)
     - Android SDK
     - Gradle

#### Bước 2: Mở Project

1. Mở Android Studio
2. **File → Open**
3. Chọn folder: `D:\esp32-google-maps-reference\android-app`
4. Đợi Android Studio sync project (lần đầu sẽ lâu)

#### Bước 3: Build APK

1. **Build → Make Project** (hoặc `Ctrl+F9`)
2. Đợi build hoàn tất
3. **Build → Build Bundle(s) / APK(s) → Build APK(s)**
4. APK sẽ được tạo tại:
   ```
   android-app/app/build/outputs/apk/debug/app-debug.apk
   ```

#### Bước 4: Cài Đặt APK

```bash
# Kết nối điện thoại qua USB
# Enable USB Debugging trên điện thoại
adb install app/build/outputs/apk/debug/app-debug.apk
```

### Phương Án 2: Build Bằng Command Line (Nâng Cao)

#### Yêu Cầu

1. **Cài JDK 11+**
   - Download: https://adoptium.net/
   - Cài đặt và thêm vào PATH

2. **Cài Android SDK**
   - Download Android Studio (chỉ cần SDK)
   - Hoặc dùng command line tools:
     ```bash
     # Download SDK command line tools
     # https://developer.android.com/studio#command-tools
     ```

3. **Set Environment Variables**
   ```powershell
   # Set JAVA_HOME
   $env:JAVA_HOME = "C:\Program Files\Java\jdk-11"
   
   # Set ANDROID_HOME
   $env:ANDROID_HOME = "$env:LOCALAPPDATA\Android\Sdk"
   
   # Add to PATH
   $env:PATH += ";$env:JAVA_HOME\bin;$env:ANDROID_HOME\platform-tools"
   ```

#### Build Command

```bash
cd D:\esp32-google-maps-reference\android-app

# Windows
.\gradlew.bat assembleDebug

# APK sẽ ở:
# app\build\outputs\apk\debug\app-debug.apk
```

### Phương Án 3: Sử Dụng GitHub Actions (Tự Động)

Nếu bạn có GitHub account, có thể tạo GitHub Actions workflow để build tự động:

```yaml
# .github/workflows/build-apk.yml
name: Build APK

on:
  workflow_dispatch:

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-java@v3
        with:
          java-version: '11'
      - uses: android-actions/setup-android@v2
      - run: cd android-app && ./gradlew assembleDebug
      - uses: actions/upload-artifact@v3
        with:
          name: app-debug
          path: android-app/app/build/outputs/apk/debug/app-debug.apk
```

## Tôi Có Thể Giúp Gì?

### ✅ **Có Thể:**
1. Tạo script build tự động
2. Hướng dẫn chi tiết từng bước
3. Tạo GitHub Actions workflow
4. Kiểm tra và fix lỗi build

### ❌ **Không Thể:**
1. Cài đặt Android Studio cho bạn (cần download và cài thủ công)
2. Build trực tiếp nếu không có JDK/Android SDK

## Khuyến Nghị

**Cách nhanh nhất:**
1. ✅ Cài Android Studio (1 lần, ~30 phút)
2. ✅ Mở project trong Android Studio
3. ✅ Build APK (5 phút)
4. ✅ Cài vào điện thoại

**Sau khi cài Android Studio, tôi có thể:**
- Hướng dẫn build chi tiết
- Giúp fix lỗi nếu có
- Tạo script tự động

## Script Build Tự Động (Sau Khi Có Android Studio)

Tôi có thể tạo script PowerShell để build tự động:

```powershell
# build-apk.ps1
cd D:\esp32-google-maps-reference\android-app
.\gradlew.bat assembleDebug

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build successful!"
    Write-Host "APK location: app\build\outputs\apk\debug\app-debug.apk"
} else {
    Write-Host "Build failed!"
}
```

Bạn muốn tôi:
1. ✅ Tạo script build tự động?
2. ✅ Tạo hướng dẫn chi tiết hơn?
3. ✅ Kiểm tra và fix lỗi sau khi bạn cài Android Studio?










