# ĐỀ XUẤT CÁC UI COMPONENT NÊN TRÍCH RA DÙNG CHUNG

> **Mục tiêu:** Liệt kê các thành phần UI cụ thể đang nằm trong Music Demo có giá trị dùng chung cao, đề xuất vị trí mới và API tối thiểu để các màn hình khác sử dụng mà không phụ thuộc vào file "music".

---

## 1) `UiImageButton`

- **Component name đề xuất:** `UiImageButton`
- **Hiện đang nằm ở:**
  - `components/sx_ui/screens/screen_music_player.c` (prev/next buttons tạo bằng `lv_img_create`)
- **Đề xuất vị trí mới:**
  - `components/sx_ui/ui_components/ui_image_button.c/.h`
- **API tối thiểu cần expose:**
  ```c
  lv_obj_t * ui_image_button_create(lv_obj_t *parent, const void *img_src,
                                   lv_event_cb_t cb, void *user_data);
  void ui_image_button_set_src(lv_obj_t *btn, const void *img_src);
  ```
- **Screen nào sẽ dùng:**
  - Home (icon tiles), Settings (icon actions), Navigation (quick actions), AC (fan/mode icons)
- **Rủi ro khi trích:**
  - Thấp. Chỉ cần đảm bảo không thay đổi style mặc định của music.

---

## 2) `UiCheckableImageButton`

- **Component name đề xuất:** `UiCheckableImageButton`
- **Hiện đang nằm ở:**
  - `components/sx_ui/screens/screen_music_player.c` (play/pause button dùng `lv_imagebutton_create`)
- **Đề xuất vị trí mới:**
  - `components/sx_ui/ui_components/ui_checkable_image_button.c/.h`
- **API tối thiểu cần expose:**
  ```c
  lv_obj_t * ui_checkable_image_button_create(lv_obj_t *parent,
      const void *released_src,
      const void *checked_src,
      lv_event_cb_t cb, void *user_data);

  void ui_checkable_image_button_set_checked(lv_obj_t *btn, bool checked);
  ```
- **Screen nào sẽ dùng:**
  - Settings (toggles dạng icon), AC (power on/off), Navigation (mute/unmute), Chatbot (mic on/off)
- **Rủi ro khi trích:**
  - Thấp–trung bình. Dễ phát sinh “style break” nếu áp dụng style global. Nên expose API style tối thiểu.

---

## 3) `UiGradientSlider`

- **Component name đề xuất:** `UiGradientSlider`
- **Hiện đang nằm ở:**
  - `components/sx_ui/screens/screen_music_player.c` (progress slider, volume slider)
- **Đề xuất vị trí mới:**
  - `components/sx_ui/ui_components/ui_gradient_slider.c/.h`
- **API tối thiểu cần expose:**
  ```c
  typedef struct {
      lv_color_t bg;
      lv_color_t grad_start;
      lv_color_t grad_end;
      lv_color_t knob;
      uint16_t height;
  } ui_slider_style_t;

  lv_obj_t * ui_gradient_slider_create(lv_obj_t *parent, const ui_slider_style_t *style,
                                      int32_t min, int32_t max);
  ```
- **Screen nào sẽ dùng:**
  - Settings (brightness), AC (temperature), Navigation (zoom), Audio effects (EQ bands)
- **Rủi ro khi trích:**
  - Trung bình. Cần thống nhất style để tránh mỗi screen tự set style.

---

## 4) `UiScrollableList`

- **Component name đề xuất:** `UiScrollableList`
- **Hiện đang nằm ở:**
  - `components/sx_ui/screens/screen_music_player_list.c` (`create_playlist_view` + scrollbar style)
- **Đề xuất vị trí mới:**
  - `components/sx_ui/ui_components/ui_scrollable_list.c/.h`
- **API tối thiểu cần expose:**
  ```c
  lv_obj_t * ui_scrollable_list_create(lv_obj_t *parent);
  void ui_scrollable_list_set_scrollbar_style(lv_obj_t *list, const lv_style_t *style);
  ```
- **Screen nào sẽ dùng:**
  - Settings (menu list), Chatbot (history list), Navigation (route steps), Home (notifications)
- **Rủi ro khi trích:**
  - Thấp. Chủ yếu là style và layout.

---

## 5) `UiListItemTwoLine`

- **Component name đề xuất:** `UiListItemTwoLine`
- **Hiện đang nằm ở:**
  - `components/sx_ui/screens/screen_music_player_list.c` (`add_list_button`)
- **Đề xuất vị trí mới:**
  - `components/sx_ui/ui_components/ui_list_item_two_line.c/.h`
- **API tối thiểu cần expose:**
  ```c
  typedef struct {
      const char *title;
      const char *subtitle;
      const char *right_text;  // optional
      const char *left_icon;   // optional
  } ui_list_item_data_t;

  lv_obj_t * ui_list_item_two_line_create(lv_obj_t *parent, const ui_list_item_data_t *data,
                                         lv_event_cb_t cb, void *user_data);
  ```
- **Screen nào sẽ dùng:**
  - Settings (WiFi SSID + status), Navigation (instruction + distance), Chatbot (message + timestamp)
- **Rủi ro khi trích:**
  - Trung bình. Layout grid cần linh hoạt, dễ phát sinh coupling với asset/icon.

---

## 6) `UiIntroAnimator` (Pattern-based helper)

- **Component name đề xuất:** `UiIntroAnimator`
- **Hiện đang nằm ở:**
  - `components/sx_ui/screens/screen_music_player.c` (staggered fade-in)
- **Đề xuất vị trí mới:**
  - `components/sx_ui/ui_helpers/ui_intro_animator.c/.h`
- **API tối thiểu cần expose:**
  ```c
  void ui_intro_fade_in(lv_obj_t *obj, uint32_t duration_ms, uint32_t delay_ms);
  void ui_intro_staggered_fade_in(lv_obj_t **objs, size_t count,
                                 uint32_t base_delay_ms, uint32_t step_ms);
  ```
- **Screen nào sẽ dùng:**
  - Home, Settings, Navigation (đặc biệt khi vào screen mới)
- **Rủi ro khi trích:**
  - Thấp. Chỉ là wrapper gọi LVGL anim.

---

## 7) `UiThemeTokens` (Style/Theme extraction)

- **Component name đề xuất:** `UiThemeTokens`
- **Hiện đang nằm ở:**
  - Các literal trong `screen_music_player.c` (màu sắc, font sizes)
- **Đề xuất vị trí mới:**
  - `components/sx_ui/include/ui_theme_tokens.h`
- **API tối thiểu cần expose:**
  ```c
  typedef struct {
      lv_color_t bg;
      lv_color_t text_primary;
      lv_color_t text_secondary;
      lv_color_t accent_primary;
      lv_color_t accent_secondary;
  } ui_color_tokens_t;

  const ui_color_tokens_t * ui_theme_get_colors(void);
  const lv_font_t * ui_theme_get_font_small(void);
  const lv_font_t * ui_theme_get_font_medium(void);
  const lv_font_t * ui_theme_get_font_large(void);
  ```
- **Screen nào sẽ dùng:**
  - Toàn bộ OS
- **Rủi ro khi trích:**
  - Trung bình–cao. Nếu thay đổi token ảnh hưởng music screen. Cần làm theo hướng “thêm mới”, không đổi music.

---

## Kết luận nhanh

- **Ưu tiên trích ngay (ít rủi ro):** UiImageButton, UiCheckableImageButton, UiIntroAnimator
- **Trích sau (cần thống nhất style):** UiGradientSlider, UiScrollableList, UiListItemTwoLine
- **Chỉ nên chuẩn hóa dần:** UiThemeTokens







