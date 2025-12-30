# TEST_PLAN — Kế hoạch test/soak/latency/memory cho `hai-os-simplexl`

> Mục tiêu: chứng minh hệ thống đạt tiêu chí **ổn định**, **không rò rỉ tài nguyên**, **audio/UI không jitter**, **network/chatbot không drop event nghiêm trọng**.

---

## 1) Nguyên tắc chung

- Test theo 4 luồng chính:
  1. Boot
  2. UI
  3. Audio
  4. Network/Chatbot
- Mọi test đều có **pass/fail** rõ.
- Ưu tiên test P0 trước (lock/lifecycle/event drop).

---

## 2) Thiết lập đo lường (metrics)

### 2.1 Heap/fragmentation
- Thu thập:
  - `heap_caps_get_free_size(MALLOC_CAP_8BIT)`
  - `heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)`
- Pass: sau soak 30–60 phút, free heap không tụt dần bất thường; largest block không suy giảm mạnh.

### 2.2 Event drop
- Nếu chưa có metric trong dispatcher, tạm thời bật log khi `sx_dispatcher_post_event()` trả false.
- Pass: trong stress test, drop rate < 0.1% cho event critical.

### 2.3 UI frame timing
- Dựa trên loop 16ms trong `sx_ui_task`:
  - đo jitter (min/max) bằng timestamp log debug mỗi 1–5s.
- Pass: không có freeze > 200ms trong thao tác UI bình thường.

### 2.4 Audio underrun
- Log i2s write result/underrun nếu có.
- Pass: không nghe nổ lách tách, không drop frame audible.

---

## 3) Test cases chi tiết

### 3.1 Boot/Init

**TC-B01: Cold boot 20 lần**
- Steps: reset nguồn 20 lần.
- Pass: 20/20 boot lên Boot screen → Flash screen; không reboot loop.

**TC-B02: Touch absent**
- Steps: tháo touch hoặc cấu hình sai INT.
- Pass: hệ thống vẫn boot, UI vẫn chạy; log touch init fail nhưng không panic.

**TC-B03: SD card absent**
- Steps: tháo SD.
- Pass: boot bình thường; UI không crash; assets embedded vẫn hiển thị.

### 3.2 UI (LVGL thread-safety/lifecycle)

**TC-U01: Navigate stress**
- Steps: liên tục chuyển màn (home/settings/chat/music/radio) 200 lần.
- Pass: không crash; không leak LVGL objects (heap ổn).

**TC-U02: Boot timer + gesture race**
- Steps: trong lúc Boot timer sắp chuyển sang Flash, spam touch.
- Pass: không deadlock; không kẹt UI; không log lock fail.

**TC-U03: Screensaver background load**
- Steps: set background = Embedded/Gradient/Image1/Custom (nếu có SD).
- Pass: không crash; đổi background không leak (heap ổn).

### 3.3 Audio

**TC-A01: Play WAV/MP3/FLAC dài 30 phút**
- Steps: phát file dài; đổi volume mỗi 10s.
- Pass: audio không drop/jitter; heap không tụt dần.

**TC-A02: Play/Pause/Stop stress**
- Steps: play/pause/resume/stop 200 lần.
- Pass: không treo; task handle không bị leak; không crash do double delete.

**TC-A03: Record + STT active**
- Steps: start recording + start STT session; chạy 10 phút.
- Pass: không WDT; không heap leak nhanh; STT chunk send không fail liên tục.

### 3.4 Network/Chatbot

**TC-N01: WS connect/disconnect loop**
- Steps: bật tắt WiFi/router 50 lần (hoặc chặn mạng).
- Pass: không crash; state/event xử lý đúng; audio bridge enable/disable hợp lý.

**TC-N02: MQTT hello UDP channel**
- Steps: mô phỏng server gửi hello transport=udp nhiều lần.
- Pass: không mở nhiều kênh UDP chồng; không leak.

**TC-N03: Spam user messages**
- Steps: gửi 100 messages nhanh qua UI.
- Pass: không queue overflow liên tục; nếu drop phải có log/metric; hệ thống không degrade.

---

## 4) Soak tests

**Soak-01: 2 giờ idle ở Flash screen**
- Pass: heap ổn, không crash.

**Soak-02: 2 giờ play nhạc + UI thao tác**
- Pass: không jitter, không leak.

**Soak-03: 2 giờ network flapping**
- Pass: không crash, không leak.

---

## 5) Tiêu chí release (gate)

- Không còn P0 trong `RISKS_P0_P1.md`.
- Pass toàn bộ TC-B01/B02/B03, TC-U01/U02, TC-A01/A02, TC-N01.
- Soak-01 đạt.


