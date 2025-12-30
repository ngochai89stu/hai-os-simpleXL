# Đề xuất tích hợp tính năng và hoàn thiện

**Ngày tạo:** 2024-12-19  
**Mục đích:** Đề xuất các tính năng cần tích hợp từ repo mẫu và kho vật liệu, và các tính năng cần hoàn thiện trong repo chính

---

## 🎯 PHÂN LOẠI ĐỀ XUẤT

### P0 - CRITICAL (Cần thiết để chạy được)
- Tính năng có cấu trúc nhưng chưa hoàn chỉnh
- Tính năng cần enable libraries
- Tính năng cần configuration

### P1 - HIGH PRIORITY (Cải thiện đáng kể)
- Tính năng quan trọng từ repo mẫu
- Tính năng nâng cao từ kho vật liệu
- Tính năng thiếu trong repo chính

### P2 - MEDIUM PRIORITY (Nice to have)
- Tính năng optional
- Tính năng advanced
- Tính năng có thể tích hợp sau

---

## 🚨 P0 - TÍNH NĂNG CẦN HOÀN THIỆN ĐỂ CHẠY ĐƯỢC

> **Lưu ý:** Các tính năng P0 là critical - cần hoàn thiện để đảm bảo hệ thống chạy được đầy đủ. Một số có TODO items cần implement.

### 1. Opus Codec - Enable và Test ⚠️

**Trạng thái hiện tại:**
- ✅ Có cấu trúc đầy đủ (`sx_codec_opus.c/h`)
- ✅ C++ wrapper đã implement (`sx_codec_opus_wrapper.cpp`)
- ✅ Library đã thêm vào dependencies (`78/esp-opus-encoder`)
- ⚠️ Cần enable `CONFIG_SX_CODEC_OPUS_ENABLE`
- ❌ TODO: Reset encoder state
- ❌ TODO: Reset decoder state
- ❌ TODO: Cleanup decoder

**Cần làm:**
1. ✅ Đã thêm Kconfig option
2. ⚠️ Implement reset functions (encoder/decoder)
3. ⚠️ Implement cleanup functions
4. ⚠️ Test compilation với library enabled
5. ⚠️ Test encoder functionality
6. ⚠️ Test decoder functionality
7. ⚠️ Integrate vào STT service (nếu cần encode audio)

**Files:**
- `components/sx_services/sx_codec_opus.c`
- `components/sx_services/sx_codec_opus_wrapper.cpp`
- `components/sx_services/Kconfig.projbuild`

**Ưu tiên:** P0 - Cần test và verify

---

### 2. ESP-SR AFE - Enable và Test ⚠️

**Trạng thái hiện tại:**
- ✅ Có cấu trúc đầy đủ (`sx_audio_afe.c/h`)
- ✅ C++ wrapper đã implement (`sx_audio_afe_esp_sr.cpp`)
- ✅ Library đã thêm vào dependencies (`espressif/esp-sr`)
- ✅ Integration vào `sx_audio_afe.c` đã hoàn thành
- ⚠️ Cần enable `CONFIG_SX_AUDIO_AFE_ESP_SR_ENABLE`
- ⚠️ Cần ESP-SR model files
- ❌ TODO: Reset AFE state

**Cần làm:**
1. ✅ Đã thêm Kconfig option
2. ⚠️ Implement reset AFE state function
3. ⚠️ Thêm ESP-SR model files vào project
4. ⚠️ Test compilation với library enabled
5. ⚠️ Test AFE processing
6. ⚠️ Test VAD callback
7. ⚠️ Integrate vào audio recording pipeline

**Files:**
- `components/sx_services/sx_audio_afe.c`
- `components/sx_services/sx_audio_afe_esp_sr.cpp`
- `components/sx_services/Kconfig.projbuild`

**Model files cần:**
- Copy từ `managed_components/espressif__esp-sr/model/`
- Hoặc download từ ESP-SR repository
- Đặt vào partition hoặc filesystem

**Ưu tiên:** P0 - Cần model files và test

---

### 3. ESP-SR Wake Word - Enable và Test ⚠️

**Trạng thái hiện tại:**
- ✅ Có cấu trúc đầy đủ (`sx_wake_word_service.c/h`)
- ✅ Load configuration từ settings
- ⚠️ Cần enable `CONFIG_SX_WAKE_WORD_ESP_SR_ENABLE`
- ⚠️ Cần ESP-SR wakenet models
- ⚠️ Cần implement wake word detection logic
- ❌ TODO: Get audio from recording service or AFE
- ❌ TODO: Process audio through wake word engine
- ❌ TODO: Initialize wake word engine
- ❌ TODO: Feed audio to wake word engine

**Cần làm:**
1. ✅ Đã thêm Kconfig option
2. ⚠️ Implement wake word detection với ESP-SR
3. ⚠️ Get audio từ recording service hoặc AFE
4. ⚠️ Feed audio vào wake word engine
5. ⚠️ Integrate với AFE service
6. ⚠️ Test wake word detection
7. ⚠️ Dispatch events khi wake word detected

**Files:**
- `components/sx_services/sx_wake_word_service.c`
- `components/sx_services/Kconfig.projbuild`

**Ưu tiên:** P0 - Cần implement detection logic

---

### 4. STT Service - Configuration và Test ⚠️

**Trạng thái hiện tại:**
- ✅ Có cấu trúc đầy đủ
- ✅ Load configuration từ settings
- ✅ Integration với audio recording
- ⚠️ Cần test với actual STT server
- ⚠️ Cần verify error handling

**Cần làm:**
1. ⚠️ Test với actual STT endpoint
2. ⚠️ Verify JSON parsing
3. ⚠️ Test error handling và retry logic
4. ⚠️ Verify event dispatching

**Files:**
- `components/sx_services/sx_stt_service.c`

**Ưu tiên:** P0 - Cần test với actual server

---

### 5. Hardware Volume Control - I2C Test ⚠️

**Trạng thái hiện tại:**
- ✅ I2C implementation đã hoàn thành
- ✅ Chip detection với chip ID
- ✅ Volume control implementation
- ⚠️ Cần test với actual codec chips (ES8388, ES8311)

**Cần làm:**
1. ⚠️ Test với ES8388 codec chip
2. ⚠️ Test với ES8311 codec chip
3. ⚠️ Verify I2C communication
4. ⚠️ Verify volume range (0-100%)
5. ⚠️ Verify register addresses (có thể cần điều chỉnh)

**Files:**
- `components/sx_platform/sx_platform_volume.c`

**Ưu tiên:** P0 - Cần test với hardware

---

### 6. Power Service - Battery Monitoring ⚠️

**Trạng thái hiện tại:**
- ✅ Có cấu trúc đầy đủ (`sx_power_service.c/h`)
- ❌ TODO: Initialize ADC for battery monitoring
- ❌ TODO: Implement low power mode
- ❌ TODO: Read battery level from ADC

**Cần làm:**
1. ⚠️ Initialize ADC cho battery monitoring
2. ⚠️ Implement đọc battery level từ ADC
3. ⚠️ Implement low power mode
4. ⚠️ Dispatch events khi battery low
5. ⚠️ Update UI với battery level

**Files:**
- `components/sx_services/sx_power_service.c`

**Ưu tiên:** P0 - Cần implement battery monitoring

---

### 7. LED Service - WS2812 Support ⚠️

**Trạng thái hiện tại:**
- ✅ Có cấu trúc đầy đủ (`sx_led_service.c/h`)
- ✅ GPIO LED support
- ❌ TODO: Initialize RMT for WS2812
- ❌ TODO: Set WS2812 color
- ❌ TODO: Listen to device state events

**Cần làm:**
1. ⚠️ Initialize RMT cho WS2812
2. ⚠️ Implement set WS2812 color
3. ⚠️ Listen to device state events
4. ⚠️ Update LED theo device state

**Files:**
- `components/sx_services/sx_led_service.c`

**Ưu tiên:** P0 - Cần implement WS2812 support

---

## 🔄 P1 - TÍNH NĂNG CẦN TÍCH HỢP TỪ REPO MẪU

### 1. MCP Tools - SD Music Tools 🔄

**Từ repo mẫu:**
- `self.sdmusic.playback` - play/pause/stop/next/prev
- `self.sdmusic.mode` - shuffle/repeat mode control
- `self.sdmusic.track` - set/info/list/current track operations
- `self.sdmusic.directory` - play/list directories
- `self.sdmusic.search` - search/play by keyword
- `self.sdmusic.library` - count_dir/count_current/page tracks
- `self.sdmusic.suggest` - suggest next/similar tracks
- `self.sdmusic.progress` - get playback progress
- `self.sdmusic.genre` - genre search/play/play_index/next
- `self.sdmusic.genre_list` - list all genres

**Trạng thái hiện tại:**
- ✅ SD Music service đã có đầy đủ features
- ❌ MCP tools chưa được implement
- ❌ TODO: Register tools with MCP server

**Cần làm:**
1. Thêm MCP tool handlers trong `sx_mcp_tools.c`
2. Map tools đến SD Music service APIs
3. Register tools với MCP server (TODO hiện tại)
4. Test với MCP client

**Files cần cập nhật:**
- `components/sx_services/sx_mcp_tools.c` (TODO: Register tools)

**Ưu tiên:** P1 - Cải thiện MCP functionality

---

### 2. MCP Tools - Music Online Tools 🔄

**Từ repo mẫu:**
- `self.music.play_song` - Play online music (song_name, artist_name)
- `self.music.set_display_mode` - Set display mode (spectrum/lyrics)
- `self.music.pause` / `self.music.resume`

**Trạng thái hiện tại:**
- ✅ Music Online service đã có
- ❌ MCP tools chưa được implement

**Cần làm:**
1. Thêm MCP tool handlers
2. Map tools đến Music Online service APIs
3. Test với MCP client

**Files cần cập nhật:**
- `components/sx_services/sx_mcp_tools.c`

**Ưu tiên:** P1 - Cải thiện MCP functionality

---

### 3. MCP Tools - User-Only Tools 🔄

**Từ repo mẫu:**
- `self.get_system_info` - System information
- `self.reboot` - Reboot system
- `self.upgrade_firmware` - OTA upgrade from URL
- `self.screen.get_info` - Screen information
- `self.screen.snapshot` - Snapshot screen và upload
- `self.screen.preview_image` - Preview image on screen

**Trạng thái hiện tại:**
- ✅ OTA service đã có
- ✅ Image service đã có
- ❌ MCP tools chưa được implement

**Cần làm:**
1. Thêm MCP tool handlers
2. Implement system info gathering
3. Implement reboot functionality
4. Integrate với OTA service
5. Integrate với Image service

**Files cần cập nhật:**
- `components/sx_services/sx_mcp_tools.c`

**Ưu tiên:** P1 - Cải thiện MCP functionality

---

### 4. MP3 Decoder 🔄

**Từ repo mẫu:**
- mini-mp3 decoder cho MP3 streams
- Support cho online music MP3

**Trạng thái hiện tại:**
- ❌ Chưa có MP3 decoder
- ✅ AAC decoder đã có
- ❌ TODO trong radio service: MP3 decoder support

**Cần làm:**
1. Thêm mini-mp3 library vào dependencies
2. Implement MP3 decoder wrapper
3. Integrate vào music online service
4. Integrate vào radio service (TODO hiện tại)
5. Auto-detect MP3 format

**Files cần tạo:**
- `components/sx_services/sx_codec_mp3.c/h`

**Files cần cập nhật:**
- `components/sx_services/sx_radio_service.c` (TODO: MP3 decoder support)

**Ưu tiên:** P1 - Support thêm format

---

### 5. Online Music Authentication 🔄

**Từ repo mẫu:**
- MAC, Chip-ID, Timestamp, SHA256 key authentication
- Headers cho authentication

**Trạng thái hiện tại:**
- ⚠️ Một phần (chưa có authentication headers)

**Cần làm:**
1. Implement authentication header generation
2. Add headers vào HTTP requests
3. Load keys từ settings

**Files cần cập nhật:**
- `components/sx_services/sx_music_online_service.c`

**Ưu tiên:** P1 - Security và compatibility

---

## 🔄 P1 - TÍNH NĂNG CẦN TÍCH HỢP TỪ KHO VẬT LIỆU

### 1. FLAC Decoder 🔄

**Từ kho vật liệu:**
- FLAC decoder support
- Multi-codec support với FLAC

**Trạng thái hiện tại:**
- ❌ Chưa có FLAC decoder

**Cần làm:**
1. Thêm FLAC decoder library
2. Implement FLAC decoder wrapper
3. Integrate vào multi-codec support

**Files cần tạo:**
- `components/sx_services/sx_codec_flac.c/h`

**Ưu tiên:** P1 - Support thêm format

---

### 2. Smart Codec Engine 🔄

**Từ kho vật liệu:**
- Auto-detect codec type từ data/URL
- Select decoder dựa trên detected codec
- Content-Type parsing

**Trạng thái hiện tại:**
- ⚠️ Radio service có Content-Type detection
- ❌ Chưa có smart codec engine tổng quát

**Cần làm:**
1. Implement codec detector
2. Auto-select decoder
3. Integrate vào audio services

**Files cần tạo:**
- `components/sx_services/sx_codec_detector.c/h`

**Ưu tiên:** P1 - Cải thiện user experience

---

### 3. Audio Buffer Pool 🔄

**Từ kho vật liệu:**
- Thread-safe buffer pool
- Buffer allocation management
- PSRAM buffer helper

**Trạng thái hiện tại:**
- ❌ Chưa có buffer pool
- ✅ Buffer monitoring đã có

**Cần làm:**
1. Implement buffer pool
2. Thread-safe allocation
3. PSRAM support

**Files cần tạo:**
- `components/sx_services/sx_audio_buffer_pool.c/h`

**Ưu tiên:** P1 - Performance optimization

---

### 4. TTS (Text-to-Speech) Service 🔄

**Từ repo mẫu và kho vật liệu:**
- Server-based TTS
- TTS với priority
- Voice state machine

**Trạng thái hiện tại:**
- ❌ Chưa có TTS service

**Cần làm:**
1. Implement TTS service
2. HTTP API integration
3. Priority queue cho TTS requests
4. Integrate vào chatbot service

**Files cần tạo:**
- `components/sx_services/sx_tts_service.c/h`

**Ưu tiên:** P1 - Voice interaction

---

## 🔄 P2 - TÍNH NĂNG NÂNG CAO (Optional)

### 1. Audio Pipeline Profiler 🔄

**Từ kho vật liệu:**
- Performance profiling
- Bottleneck detection
- Metrics tracking

**Trạng thái hiện tại:**
- ❌ Chưa có

**Ưu tiên:** P2 - Optional, cho debugging

---

### 2. Frequency Analyzer 🔄

**Từ kho vật liệu:**
- FFT spectrum analysis
- Real-time frequency analysis

**Trạng thái hiện tại:**
- ❌ Chưa có (FFT spectrum đã hủy theo yêu cầu)

**Ưu tiên:** P2 - Optional, đã hủy theo yêu cầu

---

### 3. Audio Router 🔄

**Từ kho vật liệu:**
- Route audio giữa sources
- External audio bridge

**Trạng thái hiện tại:**
- ❌ Chưa có

**Ưu tiên:** P2 - Optional, advanced feature

---

### 4. OGG Parser 🔄

**Từ kho vật liệu:**
- OGG/Opus file parsing
- Extract Opus packets

**Trạng thái hiện tại:**
- ❌ Chưa có

**Ưu tiên:** P2 - Optional, nếu cần OGG file support

---

### 5. Bluetooth Service 🔄

**Từ kho vật liệu:**
- Bluetooth audio
- Bluetooth pairing

**Trạng thái hiện tại:**
- ❌ Chưa có

**Ưu tiên:** P2 - Optional, nếu cần Bluetooth

---

### 6. Telegram Service 🔄

**Từ kho vật liệu:**
- Telegram bot integration
- Send messages

**Trạng thái hiện tại:**
- ❌ Chưa có

**Ưu tiên:** P2 - Optional, nếu cần Telegram integration

---

### 7. Navigation Service 🔄

**Từ repo mẫu và kho vật liệu:**
- Route management
- Voice guidance
- Turn-by-turn directions

**Trạng thái hiện tại:**
- ❌ Chưa có

**Ưu tiên:** P2 - Optional, nếu cần navigation

---

## 📋 KẾ HOẠCH TÍCH HỢP CHI TIẾT

### Phase 1: Hoàn thiện P0 (1-2 tuần)

**Mục tiêu:** Đảm bảo tất cả tính năng P0 chạy được

1. **Opus Codec**
   - [ ] Enable CONFIG_SX_CODEC_OPUS_ENABLE
   - [ ] Test compilation
   - [ ] Test encoder
   - [ ] Integrate vào STT (nếu cần)

2. **ESP-SR AFE**
   - [ ] Thêm model files vào project
   - [ ] Enable CONFIG_SX_AUDIO_AFE_ESP_SR_ENABLE
   - [ ] Test compilation
   - [ ] Test AFE processing
   - [ ] Integrate vào recording pipeline

3. **ESP-SR Wake Word**
   - [ ] Enable CONFIG_SX_WAKE_WORD_ESP_SR_ENABLE
   - [ ] Implement wake word detection logic
   - [ ] Integrate với AFE
   - [ ] Test wake word detection
   - [ ] Dispatch events

4. **STT Service**
   - [ ] Test với actual STT server
   - [ ] Verify JSON parsing
   - [ ] Test error handling

5. **Hardware Volume**
   - [ ] Test với ES8388
   - [ ] Test với ES8311
   - [ ] Verify I2C communication

---

### Phase 2: Tích hợp P1 từ Repo mẫu (2-3 tuần)

**Mục tiêu:** Tích hợp các tính năng quan trọng từ repo mẫu

1. **MCP Tools - SD Music**
   - [ ] Implement 10 SD Music tools
   - [ ] Map đến SD Music service APIs
   - [ ] Test với MCP client

2. **MCP Tools - Music Online**
   - [ ] Implement Music Online tools
   - [ ] Map đến Music Online service APIs
   - [ ] Test với MCP client

3. **MCP Tools - User-Only**
   - [ ] Implement user-only tools
   - [ ] System info gathering
   - [ ] Reboot functionality
   - [ ] OTA integration
   - [ ] Image service integration

4. **MP3 Decoder**
   - [ ] Thêm mini-mp3 library
   - [ ] Implement MP3 decoder
   - [ ] Integrate vào music online service

5. **Online Music Authentication**
   - [ ] Implement authentication headers
   - [ ] Load keys từ settings
   - [ ] Test với authenticated servers

---

### Phase 3: Tích hợp P1 từ Kho vật liệu (2-3 tuần)

**Mục tiêu:** Tích hợp các tính năng nâng cao từ kho vật liệu

1. **FLAC Decoder**
   - [ ] Thêm FLAC decoder library
   - [ ] Implement FLAC decoder
   - [ ] Integrate vào multi-codec support

2. **Smart Codec Engine**
   - [ ] Implement codec detector
   - [ ] Auto-select decoder
   - [ ] Integrate vào audio services

3. **Audio Buffer Pool**
   - [ ] Implement buffer pool
   - [ ] Thread-safe allocation
   - [ ] PSRAM support

4. **TTS Service**
   - [ ] Implement TTS service
   - [ ] HTTP API integration
   - [ ] Priority queue
   - [ ] Integrate vào chatbot

---

### Phase 4: Tích hợp P2 (Optional, khi cần)

**Mục tiêu:** Tích hợp các tính năng optional

1. Audio Pipeline Profiler (nếu cần debugging)
2. Audio Router (nếu cần advanced routing)
3. OGG Parser (nếu cần OGG file support)
4. Bluetooth Service (nếu cần Bluetooth)
5. Telegram Service (nếu cần Telegram)
6. Navigation Service (nếu cần navigation)

---

## 🎯 TỔNG KẾT ĐỀ XUẤT

### P0 - Critical (Cần hoàn thiện ngay)
1. ⚠️ Opus Codec - Enable và test (TODO: reset/cleanup)
2. ⚠️ ESP-SR AFE - Enable và test (cần model files, TODO: reset)
3. ⚠️ ESP-SR Wake Word - Implement detection logic (TODO: audio feed)
4. ⚠️ STT Service - Test với actual server
5. ⚠️ Hardware Volume - Test với actual chips
6. ⚠️ Power Service - Battery monitoring (TODO: ADC, low power)
7. ⚠️ LED Service - WS2812 support (TODO: RMT, state events)

**Tổng:** 7 tính năng cần hoàn thiện

---

### P1 - High Priority (Tích hợp từ repo mẫu)
1. 🔄 MCP Tools - SD Music (10 tools)
2. 🔄 MCP Tools - Music Online (3 tools)
3. 🔄 MCP Tools - User-Only (6 tools)
4. 🔄 MP3 Decoder
5. 🔄 Online Music Authentication

**Tổng:** 5 nhóm tính năng cần tích hợp

---

### P1 - High Priority (Tích hợp từ kho vật liệu)
1. 🔄 FLAC Decoder
2. 🔄 Smart Codec Engine
3. 🔄 Audio Buffer Pool
4. 🔄 TTS Service

**Tổng:** 4 tính năng cần tích hợp

---

### P2 - Medium Priority (Optional)
1. 🔄 Audio Pipeline Profiler
2. 🔄 Audio Router
3. 🔄 OGG Parser
4. 🔄 Bluetooth Service
5. 🔄 Telegram Service
6. 🔄 Navigation Service

**Tổng:** 6 tính năng optional

---

## 📊 THỐNG KÊ

### Tổng số tính năng cần làm:
- **P0:** 5 tính năng (hoàn thiện)
- **P1:** 9 tính năng (tích hợp)
- **P2:** 6 tính năng (optional)

**Tổng:** 20 tính năng

### Ước tính thời gian:
- **Phase 1 (P0):** 1-2 tuần
- **Phase 2 (P1 từ repo mẫu):** 2-3 tuần
- **Phase 3 (P1 từ kho vật liệu):** 2-3 tuần
- **Phase 4 (P2):** Tùy nhu cầu

**Tổng:** ~5-8 tuần cho P0 + P1

---

## 🎯 KHUYẾN NGHỊ

### Ưu tiên ngay (P0):
1. **Opus Codec** - Enable và test, implement reset/cleanup (2-3 ngày)
2. **ESP-SR AFE** - Thêm model files, implement reset, test (3-4 ngày)
3. **ESP-SR Wake Word** - Implement detection logic, audio feed (4-5 ngày)
4. **STT Service** - Test với server (1-2 ngày)
5. **Hardware Volume** - Test với hardware (1-2 ngày)
6. **Power Service** - Implement battery monitoring (2-3 ngày)
7. **LED Service** - Implement WS2812 support (2-3 ngày)

**Tổng ước tính:** ~15-22 ngày (3-4 tuần)

### Sau khi P0 hoàn thành (P1):
1. **MCP Tools** - Tích hợp từ repo mẫu (1-2 tuần)
2. **MP3 Decoder** - Support thêm format (3-5 ngày)
3. **Smart Codec Engine** - Cải thiện UX (1 tuần)
4. **TTS Service** - Voice interaction (1 tuần)

---

**Cập nhật:** 2024-12-19  
**Trạng thái:** ✅ HOÀN THÀNH ĐỀ XUẤT

