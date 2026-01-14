# MODULE INVENTORY – Repo Gốc (hai-os-simplexl)

_Định dạng theo taxonomy M0..M10_

| M# | Module | Vai trò (1 câu) | Trạng thái hiện tại (0=none,1=stub,2=partial,3=complete) | File gốc chính |
|----|--------|-----------------|-----------------------------------------|-----------------|
| M0 | Build/Config | sdkconfig.defaults, partitions, CMakeLists | 3 | sdkconfig.defaults, partitions.csv |
| M1 | Core contracts | Dispatcher + Orchestrator + Service registry | 3 | components/sx_core/
| M2 | Platform/HAL | LCD, Touch, SPI, I2C, SD | 2 | components/sx_platform/ |
| M3 | UI framework | sx_ui task, router, screen lifecycle | 2 | components/sx_ui/ |
| M4 | Audio playback | Decode + router + underrun handling | 2 | components/sx_services/sx_audio_service.c |
| M5 | Audio record/voice | MIC capture, AFE (ESP-SR) | 2 | components/sx_services/sx_audio_afe*.c |
| M6 | Network | WiFi connect/reconnect | 2 | components/sx_services/sx_wifi_service.c |
| M7 | Protocol/Transport | WebSocket / MQTT-UDP factory | 2 | components/sx_protocol/ |
| M8 | AI pipeline | Chatbot, STT, TTS services | 1 | components/sx_services/sx_chatbot_service.c etc. |
| M9 | Storage | Settings (NVS), SD, Assets | 3 | components/sx_services/sx_settings_service.c |
| M10| Observability | Metrics, self-test, watchdog | 2 | components/sx_core/sx_metrics.c |

(Chi tiết sẽ được điền dần theo từng batch)

