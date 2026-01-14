# Phân Tích Cấu Hình Services từ Repo Mẫu

## 📋 Tổng Quan

Phân tích repo mẫu `D:\xiaozhi-esp32_vietnam_ref` để tìm cách cấu hình:
- STT Service (Speech-to-Text)
- TTS Service (Text-to-Speech)  
- Music Online Service
- Weather Service

---

## 🔍 Developer Tools Screens

**Có, hệ thống có 7 Developer Tools Screens:**

1. ✅ **screen_voice_settings.c** - Cài đặt voice (STT/TTS)
2. ✅ **screen_network_diagnostic.c** - Chẩn đoán mạng
3. ✅ **screen_snapshot_manager.c** - Quản lý snapshot
4. ✅ **screen_diagnostics.c** - Chẩn đoán hệ thống
5. ✅ **screen_introspection.c** - Xem runtime state
6. ✅ **screen_dev_console.c** - Console developer
7. ✅ **screen_touch_debug.c** - Debug touch

Tất cả đã được đăng ký trong `register_all_screens.c` và có UI hoàn chỉnh.

---

## 🌤️ Weather Service Configuration

### Cách Repo Mẫu Cấu Hình

**File:** `D:\xiaozhi-esp32_vietnam_ref\main\features\weather\weather_service.cc`

**Cấu hình qua Settings (NVS):**
```cpp
Settings weather_settings("wifi", false);
std::string city = weather_settings.GetString("weather_city", "");
std::string api_key = weather_settings.GetString("weather_api_key", "");

// Fallback to default
if (api_key.empty()) {
    api_key = OPEN_WEATHERMAP_API_KEY_DEFAULT; // "ae8d3c2fda691593ce3e84472ef25784"
}
```

**API Endpoints:**
- Weather API: `https://api.openweathermap.org/data/2.5/weather`
- IP Location API: `https://ipwho.is`
- Default City: `"Hanoi"` (nếu auto-detect thất bại)

**Settings Keys:**
- `weather_city` - Tên thành phố (hoặc "auto" để tự động detect)
- `weather_api_key` - OpenWeatherMap API key

**Cách Implement cho SimpleXL:**

1. **Thêm vào bootstrap** (`components/sx_core/sx_bootstrap.c`):
```c
// Weather Service
sx_weather_config_t weather_cfg = {
    .api_key = NULL,  // Sẽ load từ settings
    .city = NULL,      // Sẽ load từ settings hoặc auto-detect
    .update_interval_ms = 30 * 60 * 1000,  // 30 minutes
};
esp_err_t weather_ret = sx_weather_service_init(&weather_cfg);
if (weather_ret != ESP_OK) {
    ESP_LOGW(TAG, "Weather service init failed (non-critical): %s", esp_err_to_name(weather_ret));
} else {
    ESP_LOGI(TAG, "Weather service initialized");
}
```

2. **Cấu hình qua Settings:**
   - Key: `weather_api_key` - OpenWeatherMap API key
   - Key: `weather_city` - Tên thành phố (hoặc "auto")

3. **Default API Key:** `"ae8d3c2fda691593ce3e84472ef25784"` (demo key, có thể bị revoke)

---

## 🎤 STT Service Configuration

### Cách Repo Mẫu Cấu Hình

**Repo mẫu sử dụng WebSocket/MQTT Protocol** để giao tiếp với server, không có STT service riêng biệt. STT được xử lý ở server side.

**Tuy nhiên, SimpleXL đã có STT service riêng:**

**Cấu hình hiện tại trong `sx_bootstrap.c`:**
```c
char stt_endpoint[256] = {0};
char stt_api_key[128] = {0};

// Load from Kconfig
#ifdef CONFIG_SX_STT_ENDPOINT_URL
    if (strlen(CONFIG_SX_STT_ENDPOINT_URL) > 0) {
        strncpy(stt_endpoint, CONFIG_SX_STT_ENDPOINT_URL, sizeof(stt_endpoint) - 1);
    }
#endif
#ifdef CONFIG_SX_STT_API_KEY
    if (strlen(CONFIG_SX_STT_API_KEY) > 0) {
        strncpy(stt_api_key, CONFIG_SX_STT_API_KEY, sizeof(stt_api_key) - 1);
    }
#endif

// Fallback to Settings
if (stt_endpoint[0] == '\0') {
    sx_settings_get_string_default("stt_endpoint", stt_endpoint, sizeof(stt_endpoint), NULL);
}
if (stt_api_key[0] == '\0') {
    sx_settings_get_string_default("stt_api_key", stt_api_key, sizeof(stt_api_key), NULL);
}
```

**Cách Cấu Hình:**

1. **Qua Kconfig** (`sdkconfig` hoặc `sdkconfig.defaults`):
```
CONFIG_SX_STT_ENDPOINT_URL="https://api.example.com/stt"
CONFIG_SX_STT_API_KEY="your-api-key-here"
```

2. **Qua Settings Service:**
   - Key: `stt_endpoint` - STT endpoint URL
   - Key: `stt_api_key` - STT API key

3. **Qua Voice Settings Screen:**
   - Screen: `screen_voice_settings.c` (đã có UI)
   - Cần thêm input fields cho STT endpoint và API key

---

## 🔊 TTS Service Configuration

### Cách Repo Mẫu Cấu Hình

**Tương tự STT, repo mẫu sử dụng server-side TTS qua WebSocket/MQTT.**

**SimpleXL đã có TTS service riêng:**

**Cấu hình hiện tại trong `sx_bootstrap.c`:**
```c
char tts_endpoint[256] = {0};
char tts_api_key[128] = {0};

// Load from Kconfig
#ifdef CONFIG_SX_TTS_ENDPOINT_URL
    if (strlen(CONFIG_SX_TTS_ENDPOINT_URL) > 0) {
        strncpy(tts_endpoint, CONFIG_SX_TTS_ENDPOINT_URL, sizeof(tts_endpoint) - 1);
    }
#endif
#ifdef CONFIG_SX_TTS_API_KEY
    if (strlen(CONFIG_SX_TTS_API_KEY) > 0) {
        strncpy(tts_api_key, CONFIG_SX_TTS_API_KEY, sizeof(tts_api_key) - 1);
    }
#endif

// Fallback to Settings
if (tts_endpoint[0] == '\0') {
    sx_settings_get_string_default("tts_endpoint_url", tts_endpoint, sizeof(tts_endpoint), NULL);
}
if (tts_api_key[0] == '\0') {
    sx_settings_get_string_default("tts_api_key", tts_api_key, sizeof(tts_api_key), NULL);
}
```

**Cách Cấu Hình:**

1. **Qua Kconfig:**
```
CONFIG_SX_TTS_ENDPOINT_URL="https://api.example.com/tts"
CONFIG_SX_TTS_API_KEY="your-api-key-here"
```

2. **Qua Settings Service:**
   - Key: `tts_endpoint_url` - TTS endpoint URL
   - Key: `tts_api_key` - TTS API key

3. **Qua Voice Settings Screen:**
   - Cần thêm input fields cho TTS endpoint và API key

---

## 🎵 Music Online Service Configuration

### Cách Repo Mẫu Cấu Hình

**File:** `D:\xiaozhi-esp32_vietnam_ref\main\features\music\esp32_music.cc`

```cpp
Settings settings("wifi", false);
std::string url = settings.GetString("music_url");
```

**Settings Key:** `music_url` - URL của music service

**SimpleXL đã có Music Online Service:**

**Cấu hình hiện tại trong `sx_bootstrap.c`:**
```c
char music_auth_mac[64] = {0};
char music_auth_secret[128] = {0};

// Load from Kconfig
#ifdef CONFIG_SX_MUSIC_ONLINE_AUTH_MAC
    if (strlen(CONFIG_SX_MUSIC_ONLINE_AUTH_MAC) > 0) {
        strncpy(music_auth_mac, CONFIG_SX_MUSIC_ONLINE_AUTH_MAC, sizeof(music_auth_mac) - 1);
    }
#endif
#ifdef CONFIG_SX_MUSIC_ONLINE_AUTH_SECRET
    if (strlen(CONFIG_SX_MUSIC_ONLINE_AUTH_SECRET) > 0) {
        strncpy(music_auth_secret, CONFIG_SX_MUSIC_ONLINE_AUTH_SECRET, sizeof(music_auth_secret) - 1);
    }
#endif

// Fallback to Settings
if (music_auth_mac[0] == '\0') {
    sx_settings_get_string_default("music_online_auth_mac", music_auth_mac, sizeof(music_auth_mac), NULL);
}
if (music_auth_secret[0] == '\0') {
    sx_settings_get_string_default("music_online_auth_secret", music_auth_secret, sizeof(music_auth_secret), NULL);
}
```

**Cách Cấu Hình:**

1. **Qua Kconfig:**
```
CONFIG_SX_MUSIC_ONLINE_AUTH_MAC="your-mac-address"
CONFIG_SX_MUSIC_ONLINE_AUTH_SECRET="your-secret-key"
```

2. **Qua Settings Service:**
   - Key: `music_online_auth_mac` - MAC address cho authentication
   - Key: `music_online_auth_secret` - Secret key cho authentication

---

## 📝 Tóm Tắt Cấu Hình

### Weather Service
- **Settings Keys:**
  - `weather_api_key` - OpenWeatherMap API key
  - `weather_city` - Tên thành phố (hoặc "auto")
- **Default API Key:** `"ae8d3c2fda691593ce3e84472ef25784"`
- **Cần:** Thêm vào bootstrap (chưa được init)

### STT Service
- **Settings Keys:**
  - `stt_endpoint` - STT endpoint URL
  - `stt_api_key` - STT API key
- **Kconfig:**
  - `CONFIG_SX_STT_ENDPOINT_URL`
  - `CONFIG_SX_STT_API_KEY`
- **Cần:** Cấu hình endpoint và API key

### TTS Service
- **Settings Keys:**
  - `tts_endpoint_url` - TTS endpoint URL
  - `tts_api_key` - TTS API key
- **Kconfig:**
  - `CONFIG_SX_TTS_ENDPOINT_URL`
  - `CONFIG_SX_TTS_API_KEY`
- **Cần:** Cấu hình endpoint và API key

### Music Online Service
- **Settings Keys:**
  - `music_online_auth_mac` - MAC address
  - `music_online_auth_secret` - Secret key
- **Kconfig:**
  - `CONFIG_SX_MUSIC_ONLINE_AUTH_MAC`
  - `CONFIG_SX_MUSIC_ONLINE_AUTH_SECRET`
- **Cần:** Cấu hình auth credentials

---

## 🔧 Các Bước Triển Khai

### 1. Thêm Weather Service vào Bootstrap

Cần thêm vào `components/sx_core/sx_bootstrap.c` sau khi WiFi service được start.

### 2. Cải Thiện Voice Settings Screen

Thêm input fields cho:
- STT endpoint URL
- STT API key
- TTS endpoint URL
- TTS API key

### 3. Tạo Settings Screen cho Services

Hoặc thêm vào Settings screen hiện có:
- Weather settings (API key, city)
- Music Online settings (auth config)

### 4. Cấu Hình Qua Kconfig

Thêm vào `sdkconfig.defaults` hoặc `sdkconfig`:
```
CONFIG_SX_STT_ENDPOINT_URL="https://your-stt-api.com"
CONFIG_SX_STT_API_KEY="your-key"
CONFIG_SX_TTS_ENDPOINT_URL="https://your-tts-api.com"
CONFIG_SX_TTS_API_KEY="your-key"
CONFIG_SX_MUSIC_ONLINE_AUTH_MAC="your-mac"
CONFIG_SX_MUSIC_ONLINE_AUTH_SECRET="your-secret"
```

---

## 📌 Lưu Ý

1. **Weather Service:** Chưa được init trong bootstrap, cần thêm
2. **STT/TTS:** Cần endpoint và API key từ service provider
3. **Music Online:** Cần auth credentials từ service provider
4. **Settings:** Tất cả có thể cấu hình qua Settings service hoặc Kconfig
5. **Voice Settings Screen:** Đã có UI nhưng chưa kết nối với services




















