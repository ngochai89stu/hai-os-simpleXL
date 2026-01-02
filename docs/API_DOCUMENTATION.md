# API Documentation Guide

## Tổng Quan

Dự án hai-os-simplexl sử dụng Doxygen để generate API documentation tự động từ source code comments.

## Cài Đặt Doxygen

### Windows
```powershell
# Download từ https://www.doxygen.nl/download.html
# Hoặc dùng chocolatey
choco install doxygen
choco install graphviz  # For diagrams
```

### Linux/Mac
```bash
sudo apt-get install doxygen graphviz  # Ubuntu/Debian
brew install doxygen graphviz          # macOS
```

## Generate Documentation

```bash
# Generate HTML documentation
doxygen Doxyfile

# Output sẽ ở: docs/doxygen/html/
# Mở index.html trong browser
```

## Cách Viết Documentation Comments

### Function Documentation
```c
/**
 * @brief Initialize the dispatcher system
 * 
 * Initializes the event queue and state mutex for cross-module communication.
 * Must be called before using any dispatcher functions.
 * 
 * @return true if initialization successful, false otherwise
 * 
 * @note This function is thread-safe and can be called multiple times
 * @warning Must be called from main task before other services start
 */
bool sx_dispatcher_init(void);
```

### Struct Documentation
```c
/**
 * @brief Application state snapshot
 * 
 * Immutable snapshot-style state for cross-module communication.
 * - Single writer: orchestrator (sx_core)
 * - Multiple readers: UI task and services
 * 
 * @note State is protected by mutex for thread-safe access
 */
typedef struct {
    uint32_t seq;                    ///< Monotonically increasing sequence number
    sx_wifi_state_t wifi;            ///< WiFi connection state
    sx_audio_state_t audio;          ///< Audio playback state
    sx_ui_state_t ui;                ///< UI display state
} sx_state_t;
```

### Enum Documentation
```c
/**
 * @brief Device operational state
 */
typedef enum {
    SX_DEV_UNKNOWN = 0,  ///< Unknown state
    SX_DEV_BOOTING,      ///< System is booting
    SX_DEV_IDLE,         ///< System is idle, ready for input
    SX_DEV_BUSY,         ///< System is processing (audio, network, etc.)
    SX_DEV_ERROR,        ///< System error state
} sx_device_state_t;
```

## Documentation Tags

### Common Tags
- `@brief` - Brief description (first line)
- `@param` - Parameter description
- `@return` - Return value description
- `@note` - Additional notes
- `@warning` - Warnings
- `@see` - Cross-references
- `@todo` - TODO items

### Example với Parameters
```c
/**
 * @brief Post an event to the dispatcher queue
 * 
 * @param evt Pointer to event structure to post
 * @return true if event posted successfully, false if queue full
 * 
 * @note Events are queued and processed by orchestrator task
 * @warning Event pointer (evt->ptr) must remain valid until processed
 */
bool sx_dispatcher_post_event(const sx_event_t *evt);
```

## View Documentation

Sau khi generate:
1. Mở `docs/doxygen/html/index.html` trong browser
2. Navigate đến modules/classes/functions
3. Search cho APIs cần tìm

## Best Practices

1. **Document public APIs:** Tất cả public functions nên có documentation
2. **Keep it concise:** Brief description ngắn gọn, chi tiết trong detailed description
3. **Use examples:** Thêm code examples khi cần
4. **Cross-reference:** Dùng `@see` để link related functions
5. **Update regularly:** Cập nhật documentation khi code thay đổi

## Integration với CI/CD

Có thể integrate Doxygen vào build process:

```cmake
# In CMakeLists.txt
find_package(Doxygen)
if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating API documentation"
    )
endif()
```

## Tham Khảo

- [Doxygen Manual](https://www.doxygen.nl/manual/index.html)
- [Doxygen C++ Style Guide](https://www.doxygen.nl/manual/docblocks.html)









