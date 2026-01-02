# NHỮNG THỨ KHÔNG NÊN TÁI SỬ DỤNG (MUSIC-SPECIFIC)

> **Mục tiêu:** Chỉ rõ những phần trong Music Demo chỉ phù hợp cho music player, không nên dùng lại cho các screen khác để tránh “demo vá víu” và coupling sai.

---

## 1) Logic Audio Playback / Playlist / Metadata

- **Vì sao không tái dùng:**
  - Đây là logic nghiệp vụ music (domain-specific).
  - Screen khác (Settings, Navigation, Chatbot) không nên phụ thuộc vào playlist hoặc audio decoder.
- **Ví dụ trong code:**
  - `components/sx_services/sx_audio_service.c`
  - `components/sx_services/sx_playlist_manager.c`
  - `components/sx_services/sx_media_metadata.c`
- **Nếu screen khác cần behavior tương tự:**
  - Tạo service riêng theo domain (ví dụ: `sx_navigation_service`, `sx_chat_service`).
  - UI chỉ subscribe state snapshot / events từ service đó.

---

## 2) Seekbar gắn với thời gian audio

- **Vì sao không tái dùng:**
  - Slider trong music mang semantics “seek position”.
  - Screen khác nếu copy sẽ kéo theo concept duration/position, dễ sai UX.
- **Ví dụ trong code:**
  - `components/sx_ui/screens/screen_music_player.c` -> `progress_slider_cb()`, `time_timer_cb()`
- **Thay thế nên làm:**
  - Reuse **UI primitive** slider (GradientSlider) nhưng đổi callback/semantics.
  - Ví dụ: Settings dùng slider cho brightness, AC dùng slider cho temp.

---

## 3) Spectrum visualization (circular bars)

- **Vì sao không tái dùng trực tiếp:**
  - Đây là visualization cực kỳ đặc thù cho audio.
  - Render cost cao, nhiều math/triangles.
- **Ví dụ trong code:**
  - `components/sx_ui/screens/screen_music_player_spectrum.c` -> `spectrum_draw_event_cb()`
- **Nếu screen khác cần visualization:**
  - Chỉ reuse **pattern**: custom draw event callback + invalidate theo timer.
  - Mỗi domain cần visualization riêng (navigation: compass, AC: fan speed).

---

## 4) Assets đặc thù music (nặng / style riêng)

- **Vì sao không tái dùng:**
  - Nhiều asset là icon/playback/waves mang branding music demo.
  - Dung lượng flash lớn; dùng sai sẽ làm UI OS không nhất quán.
- **Ví dụ trong code/assets:**
  - `components/sx_ui/assets/img_lv_demo_music_*`
- **Thay thế nên làm:**
  - Tách thành bộ assets/theme của OS: `sx_ui/assets/os_*`
  - Music demo vẫn dùng bộ assets riêng.

---

## 5) Text formatting đặc thù music

- **Vì sao không tái dùng:**
  - Format time “MM:SS” và prefix “/ total” là đặc thù playback.
- **Ví dụ trong code:**
  - `components/sx_ui/screens/screen_music_player.c` -> `time_timer_cb()`
- **Thay thế nên làm:**
  - Tạo helper chung `ui_format_time_mmss()` nếu nhiều screen cần.
  - Nhưng không nên hardcode prefix kiểu music.

---

## 6) Toggle playlist view bằng hide/show nhiều widget

- **Vì sao không tái dùng nguyên:**
  - Cách hide/show hàng loạt widget phù hợp demo nhỏ, nhưng nếu copy sẽ tạo coupling.
- **Ví dụ trong code:**
  - `components/sx_ui/screens/screen_music_player.c` -> `toggle_list_cb()`
- **Thay thế nên làm:**
  - Reuse **pattern**: screen có 2 mode (main/list) và switch bằng state.
  - Nên chuẩn hóa thành “ViewStack” hoặc “SubView container” nếu nhiều screen cần.

---

## Kết luận

### Những thứ chỉ nên reuse ở mức pattern:
- Spectrum draw callback
- Toggle view bằng hide/show
- Time formatting logic (tách helper nếu cần)

### Những thứ không nên reuse:
- Playlist/audio domain logic
- Assets branding music
- Seek semantics







