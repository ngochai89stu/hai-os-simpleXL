# Hướng Dẫn Đóng Góp

Cảm ơn bạn đã quan tâm đến việc đóng góp cho SimpleXL OS! 🎉

## 📋 Mục Lục

- [Code of Conduct](#code-of-conduct)
- [Cách Đóng Góp](#cách-đóng-góp)
- [Quy Trình Phát Triển](#quy-trình-phát-triển)
- [Coding Standards](#coding-standards)
- [Commit Message Convention](#commit-message-convention)
- [Testing](#testing)
- [Pull Request Process](#pull-request-process)

## Code of Conduct

Dự án này tuân theo [Contributor Covenant Code of Conduct](CODE_OF_CONDUCT.md). Bằng cách tham gia, bạn cam kết duy trì tiêu chuẩn này.

## Cách Đóng Góp

### Báo Cáo Bug

Nếu bạn tìm thấy bug, vui lòng:

1. Kiểm tra xem bug đã được báo cáo chưa trong [Issues](https://github.com/ngochai89stu/hai-os-simpleXL/issues)
2. Nếu chưa, tạo issue mới với:
   - Mô tả rõ ràng về bug
   - Các bước để reproduce
   - Kết quả mong đợi vs kết quả thực tế
   - Environment (ESP-IDF version, board, etc.)
   - Logs/error messages nếu có

### Đề Xuất Tính Năng

1. Kiểm tra [Issues](https://github.com/ngochai89stu/hai-os-simpleXL/issues) và [Roadmap](ROADMAP.md)
2. Tạo issue mới với label `enhancement`
3. Mô tả chi tiết:
   - Tính năng bạn muốn
   - Use case
   - Lợi ích
   - Có thể kèm mockup/screenshot

## Quy Trình Phát Triển

### 1. Fork Repository

```bash
# Fork repository trên GitHub, sau đó:
git clone https://github.com/YOUR_USERNAME/hai-os-simpleXL.git
cd hai-os-simpleXL
```

### 2. Tạo Branch

```bash
# Tạo branch mới từ main
git checkout -b feature/your-feature-name

# Hoặc cho bug fix
git checkout -b fix/your-bug-fix
```

### 3. Setup Development Environment

```bash
# Activate ESP-IDF
D:\esp\esp-idf\export.ps1  # Windows
# hoặc
. $HOME/esp/esp-idf/export.sh  # Linux/Mac

# Build để kiểm tra
idf.py build
```

### 4. Make Changes

- Tuân theo [Coding Standards](#coding-standards)
- Viết code rõ ràng, có comments
- Thêm tests nếu cần
- Cập nhật documentation

### 5. Test Your Changes

```bash
# Build project
idf.py build

# Run tests
cd test/unit_test
idf.py build
idf.py flash monitor
```

### 6. Commit Changes

Tuân theo [Commit Message Convention](#commit-message-convention)

### 7. Push và Tạo Pull Request

```bash
git push origin feature/your-feature-name
```

Sau đó tạo Pull Request trên GitHub.

## Coding Standards

### C Style

- Sử dụng 4 spaces cho indentation (không dùng tabs)
- Tên biến: `snake_case`
- Tên hàm: `snake_case`
- Tên struct/typedef: `snake_case_t`
- Tên constants: `UPPER_SNAKE_CASE`
- Tên macros: `UPPER_SNAKE_CASE`

### File Organization

```
component_name/
├── CMakeLists.txt
├── include/
│   └── component_name.h
├── component_name.c
└── README.md (nếu cần)
```

### Comments

```c
/**
 * @brief Mô tả ngắn gọn về hàm
 * 
 * @param param1 Mô tả tham số 1
 * @param param2 Mô tả tham số 2
 * @return Giá trị trả về
 */
int example_function(int param1, int param2);
```

### Error Handling

Luôn kiểm tra return values và xử lý lỗi:

```c
esp_err_t ret = some_function();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error: %s", esp_err_to_name(ret));
    return ret;
}
```

### Memory Management

- Luôn free memory đã allocate
- Sử dụng ESP-IDF memory management functions
- Kiểm tra NULL pointers

## Commit Message Convention

Chúng tôi sử dụng [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <subject>

<body>

<footer>
```

### Types

- `feat`: Tính năng mới
- `fix`: Sửa lỗi
- `docs`: Thay đổi documentation
- `style`: Formatting, missing semicolons, etc.
- `refactor`: Refactoring code
- `test`: Thêm/sửa tests
- `chore`: Maintenance tasks

### Examples

```
feat(audio): Thêm support cho FLAC decoder

- Implement FLAC decoder
- Add buffer management
- Update audio service

Closes #123
```

```
fix(ui): Sửa lỗi crash khi switch screen

- Fix memory leak trong screen cleanup
- Add null pointer check

Fixes #456
```

## Testing

### Unit Tests

- Viết unit tests cho các functions quan trọng
- Đặt trong `test/unit_test/`
- Sử dụng Unity test framework

### Integration Tests

- Test các components tương tác với nhau
- Đặt trong `test/integration_test/`

### Test Coverage

Cố gắng đạt ít nhất 70% code coverage cho các components mới.

## Pull Request Process

### Before Submitting

- [ ] Code tuân theo coding standards
- [ ] Đã test trên hardware thực tế (nếu có thể)
- [ ] Đã cập nhật documentation
- [ ] Commit messages rõ ràng
- [ ] Không có warnings khi build
- [ ] Đã rebase với main branch

### PR Template

Khi tạo PR, vui lòng điền:

- **Mô tả**: Mô tả ngắn gọn về thay đổi
- **Loại thay đổi**: Bug fix, Feature, Documentation, etc.
- **Testing**: Cách test thay đổi
- **Checklist**: Đánh dấu các mục đã hoàn thành

### Review Process

1. Maintainer sẽ review code
2. Có thể yêu cầu thay đổi
3. Sau khi approve, PR sẽ được merge

## Questions?

Nếu có câu hỏi, vui lòng:
- Tạo issue với label `question`
- Hoặc tham gia [Discussions](https://github.com/ngochai89stu/hai-os-simpleXL/discussions)

## License

Bằng cách đóng góp, bạn đồng ý rằng đóng góp của bạn sẽ được cấp phép dưới [MIT License](LICENSE).

---

Cảm ơn bạn đã đóng góp! 🙏

