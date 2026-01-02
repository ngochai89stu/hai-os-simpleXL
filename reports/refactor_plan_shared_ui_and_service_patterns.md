# KẾ HOẠCH REFACTOR UI/SERVICE PATTERNS (THEO PR NHỎ)

> **Mục tiêu:** Đề xuất lộ trình refactor chi tiết, chia thành các Pull Request (PR) nhỏ, độc lập để trích các pattern và component dùng chung từ Music Demo và OTA service.  
> **Nguyên tắc:** An toàn, không phá vỡ, không thay đổi behavior/style của Music Player và OTA hiện tại.

---

## 1. Nguyên tắc Refactor

1.  **Không phá vỡ (Non-breaking):** Mỗi PR sau khi merge phải đảm bảo Music Player và OTA hoạt động y hệt như cũ.
2.  **Tương thích ngược (Backward Compatibility):** Các module cũ (music/ota) sẽ được cập nhật để gọi code dùng chung mới, nhưng không được thay đổi logic của chúng.
3.  **Độc lập (Independent):** Mỗi PR nên nhỏ, tập trung vào một mục tiêu duy nhất (ví dụ: chỉ trích button, chỉ trích helper animation).
4.  **Không tạo Abstraction thừa:** Chỉ trích những gì thực sự cần dùng chung, không tạo ra các lớp trừu tượng phức tạp không cần thiết.

---

## 2. Lộ trình Refactor theo PR

### PR-0: Chuẩn bị cấu trúc thư mục

-   **Mục tiêu:** Tạo các thư mục cho code dùng chung.
-   **Hành động:**
    1.  Tạo thư mục `components/sx_ui/ui_components/` (cho các widget tái sử dụng).
    2.  Tạo thư mục `components/sx_ui/ui_helpers/` (cho các hàm tiện ích UI).
    3.  Tạo thư mục `components/sx_services/sx_service_helpers/` (cho các tiện ích service).
    4.  Cập nhật `CMakeLists.txt` của `sx_ui` và `sx_services` để thêm các thư mục mới vào `SRCS` hoặc `INCLUDE_DIRS`.
-   **Rủi ro:** Rất thấp. Chỉ là thay đổi cấu trúc file.

---

### PR-1: Trích xuất UI Animation Helper (Rủi ro thấp nhất)

-   **Mục tiêu:** Trích pattern "Staggered Fade-In" thành một helper function.
-   **File hiện tại:** `components/sx_ui/screens/screen_music_player.c` (logic trong `on_create`).
-   **Vị trí mới đề xuất:**
    -   `components/sx_ui/ui_helpers/ui_animation_helpers.c`
    -   `components/sx_ui/ui_helpers/ui_animation_helpers.h`
-   **API tối thiểu:**
    ```c
    // ui_animation_helpers.h
    void ui_helper_fade_in_staggered(lv_obj_t **objs, size_t count, uint32_t base_delay_ms, uint32_t step_ms);
    ```
-   **Cách đảm bảo tương thích:**
    -   `screen_music_player.c` sẽ `#include "ui_helpers/ui_animation_helpers.h"`.
    -   Phần code `lv_obj_fade_in` lặp đi lặp lại trong `on_create` sẽ được thay bằng một lời gọi đến `ui_helper_fade_in_staggered()`.
    -   Behavior không đổi.
-   **Rủi ro:** Thấp. Đây là hàm tiện ích không có state.

---

### PR-2: Trích xuất UI Components - Buttons (Rủi ro thấp)

-   **Mục tiêu:** Trích `ImageButton` và `CheckableImageButton` thành component chung.
-   **File hiện tại:** `components/sx_ui/screens/screen_music_player.c`.
-   **Vị trí mới đề xuất:**
    -   `components/sx_ui/ui_components/ui_buttons.c`
    -   `components/sx_ui/ui_components/ui_buttons.h`
-   **API tối thiểu:**
    ```c
    // ui_buttons.h
    lv_obj_t * ui_image_button_create(lv_obj_t *parent, const void *src, lv_event_cb_t cb);
    lv_obj_t * ui_checkable_image_button_create(lv_obj_t *parent, const void *src_rel, const void *src_chk, lv_event_cb_t cb);
    ```
-   **Cách đảm bảo tương thích:**
    -   `screen_music_player.c` sẽ `#include "ui_components/ui_buttons.h"`.
    -   Các lời gọi `lv_img_create` và `lv_imagebutton_create` để tạo nút sẽ được thay bằng hàm helper mới.
    -   Các style đặc thù của Music Player (size, padding...) vẫn được set trong `screen_music_player.c` sau khi tạo button.
-   **Rủi ro:** Thấp. Các hàm helper chỉ đóng gói việc tạo widget.

---

### PR-3: Trích xuất Service Helper - HTTP JSON Client (Rủi ro thấp-trung bình)

-   **Mục tiêu:** Trích `httpPostJson` từ OTA thành helper dùng chung.
-   **File hiện tại:** `components/sx_services/sx_ota_full.cpp`.
-   **Vị trí mới đề xuất:**
    -   `components/sx_services/sx_service_helpers/sx_http_client.cpp`
    -   `components/sx_services/sx_service_helpers/sx_http_client.hpp`
-   **API tối thiểu:**
    ```cpp
    // sx_http_client.hpp
    class SxHttpClient {
    public:
        static esp_err_t postJson(const std::string &url, const std::string &body, std::string &out_response, int &http_status);
    };
    ```
-   **Cách đảm bảo tương thích:**
    -   `sx_ota_full.cpp` sẽ `#include "sx_service_helpers/sx_http_client.hpp"`.
    -   Hàm `httpPostJson` nội bộ của `SxOtaFull` sẽ gọi đến `SxHttpClient::postJson()`.
    -   **Quan trọng:** Logic set các header đặc thù của OTA (`Activation-Version`, `Device-Id`...) vẫn nằm trong `sx_ota_full.cpp` và được truyền vào helper dưới dạng một struct hoặc map tùy chọn.
-   **Rủi ro:** Trung bình. Cần đảm bảo helper đủ linh hoạt để các service khác có thể thêm header tùy chỉnh.

---

### PR-4: Trích xuất UI Components - List & Slider (Rủi ro trung bình)

-   **Mục tiêu:** Trích `UiScrollableList`, `UiListItemTwoLine`, `UiGradientSlider`.
-   **File hiện tại:** `screen_music_player.c`, `screen_music_player_list.c`.
-   **Vị trí mới đề xuất:**
    -   `components/sx_ui/ui_components/ui_list.c/.h`
    -   `components/sx_ui/ui_components/ui_slider.c/.h`
-   **API tối thiểu:** (Như trong `ui_components_to_extract.md`)
-   **Cách đảm bảo tương thích:**
    -   **Adapter/Wrapper:** Các style đặc thù của Music List (màu nền pressed/checked, font...) sẽ được định nghĩa trong `screen_music_player_list.c` và truyền vào hàm tạo component mới.
    -   Ví dụ: `ui_list_item_create(..., &music_list_item_style)`.
    -   Music Player sẽ không bị ảnh hưởng vì nó vẫn cung cấp style riêng của mình.
-   **Rủi ro:** Trung bình. Việc tách style và logic có thể phức tạp. Phải đảm bảo API của component đủ linh hoạt để nhận style từ bên ngoài.

---

### PR-5: Chuẩn hóa UI Theme Tokens + Style Standardization (Rủi ro thấp-trung bình)

-   **Mục tiêu:** Tạo một module "tokens" chứa các giá trị màu sắc, font, spacing dùng chung, để các screen mới có thể dùng ngay mà không cần hardcode.
-   **File hiện tại:** Các giá trị hardcode trong nhiều screen:
    -   `components/sx_ui/screens/screen_music_player.c` (màu `0x1a1a1a`, `0x5b7fff`, font `montserrat_22/16/12`)
    -   `components/sx_ui/screens/screen_radio.c`, `screen_wifi_setup.c`, `screen_ota.c`... (cùng pattern)
-   **Vị trí mới đề xuất:**
    -   `components/sx_ui/ui_helpers/ui_theme_tokens.h` (chỉ header, không có .c)
    -   Hoặc `components/sx_ui/include/ui_theme_tokens.h` (nếu muốn expose cho toàn bộ sx_ui)
-   **API tối thiểu:**
    ```c
    // ui_theme_tokens.h
    // Color tokens
    #define UI_COLOR_BG_PRIMARY      lv_color_hex(0x1a1a1a)
    #define UI_COLOR_BG_SECONDARY    lv_color_hex(0x2a2a2a)
    #define UI_COLOR_BG_PRESSED      lv_color_hex(0x3a3a3a)
    #define UI_COLOR_PRIMARY         lv_color_hex(0x5b7fff)
    #define UI_COLOR_TEXT_PRIMARY    lv_color_hex(0xFFFFFF)
    #define UI_COLOR_TEXT_SECONDARY  lv_color_hex(0x888888)
    
    // Font tokens (pointers to LVGL fonts)
    #define UI_FONT_SMALL            (&lv_font_montserrat_12)
    #define UI_FONT_MEDIUM           (&lv_font_montserrat_14)
    #define UI_FONT_LARGE           (&lv_font_montserrat_16)
    #define UI_FONT_XLARGE           (&lv_font_montserrat_22)
    
    // Spacing tokens (pixels)
    #define UI_SPACE_XS              4
    #define UI_SPACE_SM              8
    #define UI_SPACE_MD              12
    #define UI_SPACE_LG              16
    #define UI_SPACE_XL              20
    #define UI_SPACE_XXL              24
    ```
-   **Cách đảm bảo tương thích:**
    -   **Không bắt buộc migrate:** Music Player và các screen cũ vẫn có thể dùng hardcode values như cũ. Tokens chỉ là "suggested values" cho screen mới.
    -   **Optional migration:** Nếu muốn, có thể tạo một PR riêng sau để migrate Music Player sang dùng tokens (thay `lv_color_hex(0x1a1a1a)` → `UI_COLOR_BG_PRIMARY`), nhưng không bắt buộc.
    -   **Screen mới bắt buộc dùng tokens:** Từ PR-5 trở đi, tất cả screen mới (Settings, Chatbot, AC...) phải dùng tokens thay vì hardcode.
-   **Rủi ro:** Thấp-trung bình. Nếu tokens không đủ linh hoạt, các screen có thể vẫn phải override. Cần đảm bảo tokens cover 80% use cases.
-   **Lợi ích:**
    -   Screen mới tạo nhanh hơn (không cần nhớ màu/font codes).
    -   Dễ thay đổi theme toàn OS (chỉ sửa tokens.h).
    -   UI consistency tự động (tất cả screen dùng cùng màu/font).

---

### PR-6: Chuẩn hóa Async Job / Progress Event Contract (Rủi ro thấp)

-   **Mục tiêu:** Tạo một "contract" (không phải code, mà là documentation + optional helper) để các service async (OTA, Download, Chat upload...) emit event progress/error/finished theo cùng một pattern.
-   **File hiện tại:**
    -   `components/sx_services/sx_ota_full.cpp` → `SX_EVT_OTA_PROGRESS` (arg0=percent, arg1=speed_kbps), `SX_EVT_OTA_FINISHED`, `SX_EVT_OTA_ERROR`
    -   `components/sx_core/include/sx_event.h` → Đã có các event types này
-   **Vị trí mới đề xuất:**
    -   `docs/EVENT_CONTRACT_ASYNC_JOBS.md` (documentation)
    -   `components/sx_services/sx_service_helpers/sx_async_job_helper.h` (optional helper macros/functions)
-   **API tối thiểu (documentation):**
    ```markdown
    # Async Job Event Contract
    
    ## Pattern chuẩn cho service async (OTA, Download, Upload...)
    
    ### Progress Event
    - Type: `SX_EVT_<SERVICE>_PROGRESS`
    - arg0: percent (0-100)
    - arg1: speed (KB/s) hoặc bytes_transferred
    - ptr: NULL hoặc optional status string
    
    ### Finished Event
    - Type: `SX_EVT_<SERVICE>_FINISHED`
    - arg0: 0 (reserved)
    - arg1: 0 (reserved)
    - ptr: result string (version, file path, etc.)
    
    ### Error Event
    - Type: `SX_EVT_<SERVICE>_ERROR`
    - arg0: error code (esp_err_t cast to uint32_t)
    - arg1: 0 (reserved)
    - ptr: error message string
    ```
-   **Optional Helper (nếu muốn):**
    ```c
    // sx_async_job_helper.h
    #define SX_ASYNC_JOB_EMIT_PROGRESS(evt_type, percent, speed) \
        do { \
            sx_event_t evt = { \
                .type = evt_type, \
                .priority = SX_EVT_PRIORITY_NORMAL, \
                .arg0 = (uint32_t)(percent), \
                .arg1 = (uint32_t)(speed), \
                .ptr = NULL \
            }; \
        sx_dispatcher_post_event(&evt); \
        } while(0)
    
    #define SX_ASYNC_JOB_EMIT_FINISHED(evt_type, result_str) \
        do { \
            sx_event_t evt = { \
                .type = evt_type, \
                .priority = SX_EVT_PRIORITY_NORMAL, \
                .arg0 = 0, \
                .arg1 = 0, \
                .ptr = sx_event_alloc_string(result_str) \
            }; \
            sx_dispatcher_post_event(&evt); \
        } while(0)
    
    #define SX_ASYNC_JOB_EMIT_ERROR(evt_type, err_code, err_msg) \
        do { \
            sx_event_t evt = { \
                .type = evt_type, \
                .priority = SX_EVT_PRIORITY_NORMAL, \
                .arg0 = (uint32_t)(err_code), \
                .arg1 = 0, \
                .ptr = sx_event_alloc_string(err_msg) \
            }; \
            sx_dispatcher_post_event(&evt); \
        } while(0)
    ```
-   **Cách đảm bảo tương thích:**
    -   **OTA không cần thay đổi:** OTA đã emit event đúng pattern, không cần sửa code.
    -   **Documentation only:** PR-6 chủ yếu là tạo documentation để các service mới (Download, Chat upload...) biết cách emit event đúng.
    -   **Optional helper:** Nếu muốn, các service mới có thể dùng helper macros để code ngắn gọn hơn, nhưng không bắt buộc.
-   **Rủi ro:** Rất thấp. Đây chỉ là documentation + optional helper, không thay đổi behavior của OTA.
-   **Lợi ích:**
    -   UI screen (Settings, OTA screen...) có thể xử lý progress/error từ nhiều service khác nhau theo cùng một pattern.
    -   Dễ tạo "generic progress dialog" component (PR-4 có thể mở rộng thêm).
    -   Code review dễ hơn (biết ngay service nào emit event đúng/không đúng contract).

---

## 3. Roadmap áp dụng vào các màn hình mới

Sau khi các PR trên được merge, việc tạo các màn hình mới sẽ nhanh hơn đáng kể:

### Màn hình Settings:
1.  Dùng `screen_common_create_top_bar_with_back()` (PR-0)
2.  Dùng `ui_scrollable_list_create()` (PR-4)
3.  Dùng `ui_list_item_two_line_create()` để tạo các mục cài đặt (PR-4)
4.  Dùng `ui_gradient_slider_create()` cho mục Brightness (PR-4)
5.  Dùng `ui_checkable_image_button_create()` (hoặc switch LVGL) cho các toggle (PR-2)

### Màn hình Chatbot:
1.  Dùng `screen_common_create_top_bar_with_back()` (PR-0)
2.  Dùng `ui_scrollable_list_create()` cho lịch sử chat (PR-4)
3.  Dùng `ui_list_item_two_line_create()` (customized) để hiển thị tin nhắn (PR-4)
4.  Dùng `ui_checkable_image_button_create()` cho nút bật/tắt micro (PR-2)

### Màn hình AC Control:
1.  Dùng `screen_common_create_top_bar_with_back()` (PR-0)
2.  Dùng `ui_gradient_slider_create()` cho thanh nhiệt độ (PR-4)
3.  Dùng `ui_image_button_create()` cho các nút chế độ (làm mát, quạt...) (PR-2)
4.  Dùng `ui_checkable_image_button_create()` cho nút nguồn (PR-2)

> **Kết luận:** Lộ trình này đảm bảo việc tái cấu trúc diễn ra an toàn, có kiểm soát, và mang lại giá trị ngay lập tức bằng cách tăng tốc độ phát triển các màn hình mới trong khi vẫn giữ nguyên vẹn màn hình Music Player hiện có.

