# OTA / Activation & Sounds

## 1) OTA / Activation flow

- Khi **WiFi connected** (`SX_EVT_WIFI_CONNECTED`) → hệ thống tự gọi **OTA check**.
- Server có thể trả về:
  - **activation**: thiết bị cần kích hoạt (mã 6 số) → phát `SX_EVT_ACTIVATION_REQUIRED`.
  - **mqtt / websocket config**: được lưu vào settings (NVS).
  - **firmware url** (tuỳ server): có thể dùng để upgrade.

Sau khi:
- **Activation xong** (`SX_EVT_ACTIVATION_DONE`) → hệ thống sẽ tự **start protocol** (ưu tiên WebSocket, fallback MQTT).
- **Không cần activation** (`SX_EVT_OTA_FINISHED`) → hệ thống cũng tự start protocol.

## 2) Activation sounds (OGG)

Khi nhận `SX_EVT_ACTIVATION_REQUIRED`, UI sẽ:
- Hiển thị alert "Activation" và mã 6 số.
- Phát âm thanh **activation** trước.
- Phát âm thanh theo từng chữ số của mã.

### File paths được hỗ trợ
Code hiện tại thử theo thứ tự:

1. SD card:
- `/sdcard/sounds/activation.ogg`
- `/sdcard/sounds/digit_0.ogg` .. `/sdcard/sounds/digit_9.ogg`

2. SPIFFS:
- `/spiffs/sounds/activation.ogg`
- `/spiffs/sounds/digit_0.ogg` .. `/spiffs/sounds/digit_9.ogg`

## 3) Build SPIFFS image

Dự án đã được cấu hình để build SPIFFS image từ thư mục **`./spiffs`** ở root.

Hãy tạo thư mục và copy các file âm thanh:

```
spiffs/
  sounds/
    activation.ogg
    digit_0.ogg
    digit_1.ogg
    ...
    digit_9.ogg
```

Sau đó build như bình thường:

```bash
idf.py build
```

SPIFFS image sẽ được đóng gói vào firmware (FLASH_IN_PROJECT) và sẽ có thể truy cập qua đường dẫn `/spiffs/...`.

## 4) Notes

- Playback hiện tại dùng delay ngắn để giảm overlap; nếu muốn phát tuần tự "chuẩn" hơn, nên implement queue/playlist dành riêng cho UI alert sounds.
- Nếu bạn không dùng SD card, chỉ cần SPIFFS là đủ.







