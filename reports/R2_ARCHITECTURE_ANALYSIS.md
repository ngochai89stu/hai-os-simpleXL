# R2 - TÁI DỰNG KIẾN TRÚC (TỪ CODE THẬT)

**TUYÊN BỐ CƯỠNG BỨC**: Tôi xác nhận rằng mọi phân tích dưới đây đều dựa trên việc đọc nội dung thực tế của từng file, và mọi nhận định đều được giải thích bằng tiếng Việt.

## 1. LUỒNG KHỞI ĐỘNG THỰC TẾ

### 1.1. Entry Point
```
app_main() [app/app_main.c hoặc components/sx_app/app_main.c]
  └─> sx_bootstrap_start() [components/sx_core/sx_bootstrap.c]
```

### 1.2. Bootstrap Sequence (từ sx_bootstrap.c)

**Phase 1: Core Infrastructure**
1. NVS Flash Init (`nvs_flash_init()`)
2. Error Handler Init (`sx_error_handler_init()`)
3. Settings Service Init (`sx_settings_service_init()`)
4. Theme Service Init (`sx_theme_service_init()`)
5. OTA Service Init (`sx_ota_service_init()`, `sx_ota_full_service_init()`)

**Phase 2: MCP & Event System**
6. MCP Server Init (`sx_mcp_server_init()`)
7. MCP Tools Register (`sx_mcp_tools_register_all()`)
8. Dispatcher Init (`sx_dispatcher_init()`)
9. Orchestrator Start (`sx_orchestrator_start()`) - Tạo task xử lý events

**Phase 3: Platform Hardware**
10. Display Init (`sx_platform_display_init()`) - LCD panel
11. Touch Init (`sx_platform_touch_init()`) - Touch panel (optional)
12. SPI Bus Manager Init (`sx_spi_bus_manager_init()`)

**Phase 4: Storage & Assets**
13. SD Service Init & Mount (`sx_sd_service_init()`, `sx_sd_service_start()`)
14. Assets Init (`sx_assets_init()`)

**Phase 5: UI**
15. UI Start (`sx_ui_start()`) - Tạo UI task, init LVGL, register screens
16. Brightness Restore (từ settings)

**Phase 6: Audio Services**
17. Audio Ducking Init (`sx_audio_ducking_init()`)
18. Audio Power Init (`sx_audio_power_init()`)
19. Audio Router Init (`sx_audio_router_init()`)
20. Audio Service Init & Start (`sx_audio_service_init()`, `sx_audio_service_start()`)

**Phase 7: Lazy Loading Services** (được init khi cần)
- STT Service
- AFE Service
- Wake Word Service
- Playlist Manager
- Radio Service
- IR Service
- Intent Service
- Chatbot Service
- WiFi Service
- Music Online Service
- TTS Service
- Navigation Service
- BLE Navigation Service
- Telegram Service
- Bluetooth Service
- Diagnostics Service
- Weather Service

### 1.3. UI Ready Sequence

```
sx_ui_start()
  └─> sx_ui_task() [components/sx_ui/sx_ui_task.c]
      ├─> lvgl_port_init()
      ├─> lvgl_port_add_disp() - Register display
      ├─> ui_router_init()
      ├─> register_all_screens() - Register 28 screens
      ├─> lvgl_port_add_touch() - Register touch (if available)
      ├─> Post SX_EVT_UI_READY event
      └─> Navigate to SCREEN_ID_BOOT
```

## 2. CÁC TẦNG KIẾN TRÚC THỰC SỰ ĐANG TỒN TẠI

### 2.1. Tầng 1: Platform Layer (Hardware Abstraction)
**Location**: `components/sx_platform/`

**Chức năng**:
- Display abstraction (`sx_platform_display_init()`)
- Touch abstraction (`sx_platform_touch_init()`)
- Volume control (`sx_platform_volume.c`)
- SPI bus management (`sx_spi_bus_manager.c`)

**Dependencies**: ESP-IDF LCD drivers, I2C, SPI, GPIO, LEDC

### 2.2. Tầng 2: Core Layer (Event & State Management)
**Location**: `components/sx_core/`

**Chức năng**:
- **Dispatcher**: Event queue (multi-producer, single-consumer) với 4 priority levels
- **Orchestrator**: Single writer cho state, xử lý events
- **State**: Immutable snapshot pattern
- **Event**: Event system với 50+ event types
- **Bootstrap**: System initialization

**Dependencies**: FreeRTOS (queue, semaphore, task)

### 2.3. Tầng 3: Service Layer (Business Logic)
**Location**: `components/sx_services/`

**Chức năng**:
- Audio services (playback, recording, effects, routing)
- Codec services (MP3, AAC, FLAC, Opus)
- Network services (WiFi, Bluetooth)
- Protocol services (WebSocket, MQTT)
- Voice services (STT, TTS, wake word)
- Media services (radio, music, SD card, playlist)
- System services (settings, state, theme, OTA, power, diagnostics)
- AI/ML services (chatbot, intent, MCP)
- Other services (IR, navigation, weather, telegram, image, QR)

**Dependencies**: Core layer (dispatcher, events), Platform layer, ESP-IDF services

### 2.4. Tầng 4: UI Layer (Presentation)
**Location**: `components/sx_ui/`

**Chức năng**:
- UI task (LVGL task)
- UI router (screen navigation)
- Screen registry (screen management)
- 28 screens (boot, flash, home, chat, music, radio, settings, etc.)
- UI animations, icons

**Dependencies**: Core layer (state, events), LVGL v9, Platform layer (display, touch)

### 2.5. Tầng 5: Protocol Layer (Communication)
**Location**: `components/sx_protocol/`

**Chức năng**:
- WebSocket protocol (`sx_protocol_ws.c`)
- MQTT protocol (`sx_protocol_mqtt.c`, `sx_protocol_mqtt_udp.c`)
- Protocol factory (`sx_protocol_factory.c`)
- Common protocol utilities (`sx_protocol_common.c`)

**Dependencies**: ESP-IDF HTTP, MQTT, WebSocket

### 2.6. Tầng 6: Assets Layer (Resources)
**Location**: `components/sx_assets/`, `assets/`

**Chức năng**:
- Asset loader (`sx_assets.c`)
- Bootscreen, flashscreen images
- Generated assets (C arrays)

**Dependencies**: Platform layer (SD service)

## 3. CHỖ NÀO VI PHẠM KIẾN TRÚC SIMPLEXL

### 3.1. Vi phạm phân tầng

**❌ UI Layer gọi trực tiếp Services**
- **Vị trí**: Các screen files (`components/sx_ui/screens/screen_*.c`)
- **Vấn đề**: Screens có thể gọi trực tiếp service APIs thay vì qua events
- **Ví dụ**: `screen_music_player.c` có thể gọi `sx_audio_service_*()` trực tiếp
- **Giải pháp**: Screens chỉ nên emit events, orchestrator xử lý events và gọi services

**❌ Services gọi trực tiếp UI**
- **Vị trí**: Một số services có thể emit events nhưng cũng có thể gọi UI trực tiếp
- **Vấn đề**: Vi phạm dependency direction (services không nên biết về UI)
- **Giải pháp**: Services chỉ emit events, UI đọc state từ dispatcher

**❌ Bootstrap quá dài và phụ thuộc nhiều**
- **Vị trí**: `components/sx_core/sx_bootstrap.c` (809 dòng)
- **Vấn đề**: File quá dài, phụ thuộc vào hầu hết services
- **Giải pháp**: Chia nhỏ thành các hàm init riêng, dùng lazy loading

### 3.2. Vi phạm Single Responsibility Principle

**❌ Bootstrap file quá dài**
- **Vị trí**: `components/sx_core/sx_bootstrap.c`
- **Vấn đề**: File 809 dòng, làm quá nhiều việc
- **Giải pháp**: Chia thành `sx_bootstrap_core.c`, `sx_bootstrap_platform.c`, `sx_bootstrap_services.c`

**❌ Audio service quá phức tạp**
- **Vị trí**: `components/sx_services/sx_audio_service.c` (1057+ dòng)
- **Vấn đề**: File quá dài, xử lý nhiều chức năng (playback, recording, FFT, spectrum)
- **Giải pháp**: Tách FFT/spectrum ra service riêng

### 3.3. Hard-coded values

**❌ GPIO pins hard-coded**
- **Vị trí**: `components/sx_core/sx_bootstrap.c` (lines 194-197) - SD card pins
- **Vấn đề**: GPIO pins hard-coded thay vì dùng Kconfig
- **Giải pháp**: Di chuyển sang Kconfig

**❌ Queue sizes hard-coded**
- **Vị trí**: `components/sx_core/sx_dispatcher.c` (lines 35-44)
- **Vấn đề**: Queue sizes (16, 32, 16, 8) hard-coded
- **Giải pháp**: Di chuyển sang Kconfig

**❌ Stack sizes hard-coded**
- **Vị trí**: Nhiều file (orchestrator, UI task, chatbot task)
- **Vấn đề**: Stack sizes hard-coded
- **Giải pháp**: Di chuyển sang Kconfig

## 4. LUỒNG DỮ LIỆU & SỰ KIỆN

### 4.1. Event Flow

```
Service/UI
  └─> sx_dispatcher_post_event()
      └─> Priority Queue (low/normal/high/critical)
          └─> Orchestrator Task
              └─> sx_dispatcher_poll_event()
                  └─> Event Handler Registry
                      └─> Event Handler Function
                          └─> Update State
                              └─> sx_dispatcher_set_state()
```

### 4.2. State Flow

```
Orchestrator (Single Writer)
  └─> sx_dispatcher_set_state()
      └─> State Snapshot (mutex protected)
          └─> UI Task (Multiple Readers)
              └─> sx_dispatcher_get_state()
                  └─> Update UI from State
```

### 4.3. UI Input Flow

```
User Input (Touch/Button)
  └─> LVGL Event
      └─> Screen Event Handler
          └─> sx_dispatcher_post_event(SX_EVT_UI_INPUT)
              └─> Orchestrator
                  └─> Event Handler
                      └─> Service API Call
                          └─> Service Action
```

## 5. SƠ ĐỒ ASCII

```
┌─────────────────────────────────────────────────────────────┐
│                    APP MAIN (Entry Point)                    │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│                    BOOTSTRAP                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │   NVS Init   │  │  Settings    │  │    Theme     │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │  Dispatcher │  │ Orchestrator │  │    MCP        │     │
│  │    Init     │  │    Start     │  │   Server     │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │   Display    │  │    Touch     │  │     SD       │     │
│  │    Init      │  │    Init      │  │   Service    │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │     UI       │  │    Audio     │  │   Lazy      │     │
│  │    Start     │  │   Service    │  │  Loading    │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    CORE LAYER                                │
│                                                              │
│  ┌────────────────────────────────────────────────────┐    │
│  │              DISPATCHER                             │    │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐         │    │
│  │  │ Critical │  │  High    │  │  Normal  │  Low   │    │
│  │  │  Queue   │  │  Queue   │  │  Queue   │ Queue  │    │
│  │  └──────────┘  └──────────┘  └──────────┘         │    │
│  │                                                      │    │
│  │  ┌──────────────────────────────────────────────┐ │    │
│  │  │         STATE SNAPSHOT (Mutex)                │ │    │
│  │  └──────────────────────────────────────────────┘ │    │
│  └────────────────────────────────────────────────────┘    │
│                                                              │
│  ┌────────────────────────────────────────────────────┐    │
│  │            ORCHESTRATOR TASK                       │    │
│  │  ┌──────────────────────────────────────────────┐ │    │
│  │  │  Poll Events → Handler Registry → Update   │ │    │
│  │  │  State                                       │ │    │
│  │  └──────────────────────────────────────────────┘ │    │
│  └────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    SERVICE LAYER                             │
│                                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │  Audio   │  │  Codec   │  │ Network  │  │ Protocol │  │
│  │ Service  │  │ Service  │  │ Service  │  │ Service  │  │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │
│                                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │  Voice   │  │  Media   │  │  System  │  │   AI    │  │
│  │ Service  │  │ Service  │  │ Service  │  │ Service  │  │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │
│                                                              │
│  ┌────────────────────────────────────────────────────┐    │
│  │         Emit Events → Dispatcher                   │    │
│  └────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    UI LAYER                                  │
│                                                              │
│  ┌────────────────────────────────────────────────────┐    │
│  │              UI TASK (LVGL)                        │    │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐         │    │
│  │  │ Router  │  │ Registry │  │ Screens  │         │    │
│  │  └──────────┘  └──────────┘  └──────────┘         │    │
│  └────────────────────────────────────────────────────┘    │
│                                                              │
│  ┌────────────────────────────────────────────────────┐    │
│  │  Read State → Update UI → Emit UI Input Events    │    │
│  └────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    PLATFORM LAYER                            │
│                                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │ Display  │  │  Touch   │  │  Volume  │  │   SPI    │  │
│  │  Driver  │  │  Driver  │  │ Control  │  │   Bus    │  │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## 6. KẾT LUẬN

### 6.1. Điểm mạnh
- ✅ Kiến trúc rõ ràng với các tầng phân tách
- ✅ Event-driven architecture tốt cho decoupling
- ✅ State management với immutable snapshot pattern
- ✅ Lazy loading cho một số services
- ✅ Priority queues cho events

### 6.2. Điểm yếu
- ❌ Bootstrap file quá dài (809 dòng)
- ❌ Một số vi phạm phân tầng (UI gọi services trực tiếp)
- ❌ Hard-coded values (GPIO, queue sizes, stack sizes)
- ❌ Một số services quá phức tạp (audio service 1057+ dòng)

### 6.3. Đề xuất cải thiện
1. **Refactor bootstrap**: Chia nhỏ thành các hàm init riêng
2. **Enforce layering**: Đảm bảo UI chỉ emit events, không gọi services trực tiếp
3. **Kconfig migration**: Di chuyển tất cả hard-coded values sang Kconfig
4. **Service refactoring**: Tách các services phức tạp thành các modules nhỏ hơn






