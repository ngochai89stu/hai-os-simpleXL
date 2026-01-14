# 🎵 SimpleXL OS - ESP32 Smart Assistant

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.1+-green.svg)](https://docs.espressif.com/projects/esp-idf/en/latest/)
[![Platform](https://img.shields.io/badge/platform-ESP32-orange.svg)](https://www.espressif.com/en/products/socs/esp32)

> Hệ điều hành thông minh cho ESP32 với giao diện người dùng đẹp mắt, hỗ trợ AI, điều khiển giọng nói và nhiều tính năng hiện đại.

## 📖 Giới Thiệu

**SimpleXL OS** là một hệ điều hành nhúng được thiết kế cho ESP32, cung cấp:

- 🎨 **Giao diện người dùng đẹp mắt** với LVGL
- 🎤 **Điều khiển bằng giọng nói** (Wake Word, STT, TTS)
- 🤖 **Trợ lý AI thông minh** (Chatbot)
- 🎵 **Trình phát nhạc** với nhiều định dạng (MP3, FLAC, Opus)
- 📻 **Radio trực tuyến**
- 🌐 **Kết nối mạng** (WiFi, MQTT, WebSocket)
- 🎮 **Điều khiển IR** cho thiết bị gia đình
- 📱 **Điều hướng BLE** từ ứng dụng Android
- 🔄 **OTA Updates** tự động

## ✨ Tính Năng Chính

### 🎯 Core Features
- ✅ Event-driven architecture
- ✅ Service layer pattern
- ✅ Lazy loading components
- ✅ Error handling & recovery
- ✅ Metrics & monitoring

### 🎨 UI/UX
- ✅ 30+ màn hình được tối ưu
- ✅ Animations mượt mà
- ✅ Theme system
- ✅ Touch screen support
- ✅ Music player với spectrum analyzer

### 🎵 Audio
- ✅ Multi-format decoder (MP3, FLAC, Opus)
- ✅ Audio effects (EQ, Reverb, Ducking)
- ✅ Buffer pool management
- ✅ Streaming support

### 🤖 AI & Voice
- ✅ Wake word detection (ESP-SR)
- ✅ Speech-to-Text
- ✅ Text-to-Speech
- ✅ Chatbot integration

### 🌐 Network
- ✅ WiFi management
- ✅ MQTT protocol
- ✅ WebSocket support
- ✅ HTTP client

## 🚀 Bắt Đầu Nhanh

### Yêu Cầu Hệ Thống

- **ESP-IDF** v5.1 hoặc mới hơn
- **Python** 3.8+
- **CMake** 3.16+
- **ESP32** development board
- **Display** (ST7796U hoặc tương thích)
- **Touch screen** (tùy chọn)

### Cài Đặt

1. **Clone repository:**
```bash
git clone https://github.com/ngochai89stu/hai-os-simpleXL.git
cd hai-os-simpleXL
```

2. **Setup ESP-IDF:**
```powershell
# Windows (PowerShell)
D:\esp\esp-idf\export.ps1

# Linux/Mac
. $HOME/esp/esp-idf/export.sh
```

3. **Build project:**
```bash
idf.py build
```

4. **Flash to device:**
```bash
idf.py -p COM_PORT flash monitor
```

### Quick Build Script

Sử dụng script tự động:
```powershell
.\quick_build_test.ps1
```

## 📁 Cấu Trúc Dự Án

```
hai-os-simpleXL/
├── app/                    # Main application
├── components/             # ESP-IDF components
│   ├── sx_core/           # Core system (dispatcher, events, state)
│   ├── sx_services/       # Services (audio, chatbot, wifi, etc.)
│   ├── sx_ui/             # UI components & screens
│   ├── sx_platform/        # Platform abstraction
│   └── sx_protocol/        # Network protocols
├── assets/                 # Resources (images, fonts)
├── docs/                   # Documentation
├── reports/                # Project reports
├── scripts/                # Build & utility scripts
├── test/                   # Unit & integration tests
└── tools/                  # Development tools
```

## 🛠️ Build & Development

### Build Options

```bash
# Standard build
idf.py build

# Build with verbose output
idf.py build -v

# Clean build
idf.py fullclean
idf.py build

# Build specific component
idf.py build --component sx_ui
```

### Configuration

```bash
# Open menuconfig
idf.py menuconfig

# Set specific config
idf.py set-target esp32
```

Xem thêm: [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md)

## 📚 Tài Liệu

- [📖 Hướng dẫn Build](BUILD_INSTRUCTIONS.md)
- [📋 Roadmap](ROADMAP.md)
- [🧪 Test Plan](TEST_PLAN.md)
- [📊 Project Reports](reports/)
- [📝 API Documentation](docs/API_DOCUMENTATION.md)

## 🧪 Testing

```bash
# Run unit tests
cd test/unit_test
idf.py build
idf.py flash monitor

# Run integration tests
cd test/integration_test
idf.py build
```

## 🤝 Đóng Góp

Chúng tôi hoan nghênh mọi đóng góp! Vui lòng xem [CONTRIBUTING.md](CONTRIBUTING.md) để biết chi tiết.

### Quy Trình Đóng Góp

1. Fork repository
2. Tạo feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to branch (`git push origin feature/AmazingFeature`)
5. Mở Pull Request

## 📝 License

Dự án này được phân phối dưới giấy phép MIT. Xem [LICENSE](LICENSE) để biết thêm chi tiết.

## 👥 Tác Giả

- **ngochai89stu** - [GitHub](https://github.com/ngochai89stu)

## 🙏 Lời Cảm Ơn

- [ESP-IDF](https://github.com/espressif/esp-idf) - ESP32 development framework
- [LVGL](https://lvgl.io/) - Graphics library
- [ESP-SR](https://github.com/espressif/esp-sr) - Speech recognition
- Tất cả các contributors và cộng đồng open source

## 📞 Liên Hệ

- **Issues:** [GitHub Issues](https://github.com/ngochai89stu/hai-os-simpleXL/issues)
- **Discussions:** [GitHub Discussions](https://github.com/ngochai89stu/hai-os-simpleXL/discussions)

## 📈 Roadmap

Xem [ROADMAP.md](ROADMAP.md) để biết kế hoạch phát triển chi tiết.

## ⚠️ Trạng Thái Dự Án

Dự án đang trong giai đoạn phát triển tích cực. Một số tính năng có thể chưa hoàn thiện.

---

⭐ Nếu dự án này hữu ích, hãy cho chúng tôi một star!



