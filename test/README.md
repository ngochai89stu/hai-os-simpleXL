# Unit Tests for hai-os-simplexl

## Tổng Quan

Thư mục này chứa unit tests cho các core modules của hai-os-simplexl sử dụng ESP-IDF Unity test framework.

## Cấu Trúc

```
test/
├── CMakeLists.txt          # Test framework configuration
├── unit_test/
│   ├── CMakeLists.txt      # Unit test component
│   ├── test_dispatcher.c   # Tests cho sx_dispatcher
│   ├── test_dispatcher.h
│   ├── test_state.c        # Tests cho sx_state
│   ├── test_state.h
│   └── main/
│       ├── CMakeLists.txt
│       └── test_runner.c    # Test runner main
└── README.md
```

## Chạy Tests

### Cách 1: Sử dụng idf.py

```bash
# Build và flash tests
idf.py build
idf.py flash monitor
```

### Cách 2: Sử dụng test runner riêng

```bash
# Build test project
cd test
idf.py build
idf.py flash monitor
```

## Test Coverage

### Dispatcher Tests
- ✅ Initialization
- ✅ Event posting and polling
- ✅ State get/set operations
- ✅ Queue full scenario (drop events)
- ✅ Thread safety (basic)
- ✅ Invalid input handling

### State Tests
- ✅ Initial state values
- ✅ State sequence increment
- ✅ Device state transitions
- ✅ UI state fields
- ✅ Message buffers
- ✅ State immutability pattern
- ✅ WiFi and Audio state structures

## Thêm Tests Mới

1. Tạo test file mới trong `test/unit_test/`
2. Thêm test functions với prefix `test_`
3. Sử dụng Unity macros: `TEST_ASSERT_*`
4. Thêm test declarations vào header file
5. Register test trong `test_runner.c`

## Ví Dụ Test

```c
void test_example(void) {
    // Setup
    sx_state_t state;
    
    // Test
    sx_dispatcher_get_state(&state);
    
    // Assert
    TEST_ASSERT_EQUAL(0, state.seq);
}
```

## Lưu Ý

- Tests chạy trên target hardware (ESP32-S3)
- Cần flash và monitor để xem kết quả
- Unity framework tự động report pass/fail
- Tests được chạy tuần tự

## Tài Liệu Tham Khảo

- [ESP-IDF Unit Testing](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-unit-test.html)
- [Unity Test Framework](https://github.com/ThrowTheSwitch/Unity)

