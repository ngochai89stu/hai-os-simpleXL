# Integration Tests

## Tổng Quan

Integration tests kiểm tra các services hoạt động cùng nhau, bao gồm:
- Audio service với hardware
- Chatbot service với network
- Protocol services (WebSocket, MQTT)
- UI services

## Cấu Trúc

```
test/integration_test/
├── CMakeLists.txt
├── test_audio_service.c
├── test_chatbot_service.c
└── README.md
```

## Chạy Tests

```bash
cd test
idf.py build
idf.py flash monitor
```

## Lưu Ý

- Integration tests yêu cầu hardware thực tế
- Một số tests có thể fail nếu không có network/hardware
- Tests được thiết kế để graceful fail (không crash)

## Test Coverage

### Audio Service
- ✅ Initialization
- ⏳ Volume control (placeholder)
- ⏳ Playback state (placeholder)

### Chatbot Service
- ✅ Initialization
- ⏳ Ready state check (placeholder)
- ⏳ Message sending (placeholder)
- ⏳ JSON message handling (placeholder)

## Mở Rộng

Để thêm integration test mới:

1. Tạo test file mới trong `test/integration_test/`
2. Thêm vào `CMakeLists.txt`
3. Implement test functions
4. Register trong test runner









