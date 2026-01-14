# Hướng Dẫn Flash ESP32-S3 - COM23

## Vấn Đề Hiện Tại

ESP32-S3 không phản hồi khi flash. Lỗi: `No serial data received`

## Nguyên Nhân

ESP32-S3 cần được đưa vào **Download Mode** trước khi flash.

## Giải Pháp

### Cách 1: Dùng Script Tự Động (Khuyến Nghị)

1. **Mở PowerShell trong thư mục dự án:**
   ```powershell
   cd D:\NEWESP32\hai-os-simplexl
   ```

2. **Chạy script:**
   ```powershell
   .\flash_with_download_mode.ps1
   ```

3. **Làm theo hướng dẫn trên màn hình:**
   - Nhấn và GIỮ nút **BOOT**
   - Nhấn nút **RESET** một lần
   - Thả nút **RESET** trước
   - Thả nút **BOOT** sau

4. **Script sẽ tự động flash sau 5 giây**

---

### Cách 2: Flash Thủ Công

#### Bước 1: Đưa ESP32-S3 vào Download Mode

**Thứ tự quan trọng:**
1. ✅ Nhấn và **GIỮ** nút **BOOT** (hoặc **IO0**)
2. ✅ Nhấn nút **RESET** (hoặc **EN**) **một lần**
3. ✅ **Thả nút RESET trước**
4. ✅ **Thả nút BOOT sau**

**Lưu ý:** Nếu thả nút BOOT trước RESET, ESP32-S3 sẽ không vào download mode!

#### Bước 2: Flash Ngay Lập Tức

**Sau khi thả nút BOOT, flash ngay:**

```powershell
cd D:\NEWESP32\hai-os-simplexl
D:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe D:\Espressif\frameworks\esp-idf-v5.5.1\tools\idf.py -p COM23 -b 115200 flash
```

**Hoặc nếu đã activate ESP-IDF:**

```powershell
idf.py -p COM23 -b 115200 flash
```

---

### Cách 3: Flash với Auto-Reset (Nếu Board Hỗ Trợ)

**Một số board ESP32-S3 có auto-reset, thử:**

```powershell
idf.py -p COM23 -b 115200 --before default_reset --after hard_reset flash
```

---

## Kiểm Tra Kết Nối

**Trước khi flash, kiểm tra:**

1. ✅ **COM23 hiển thị trong Device Manager**
2. ✅ **Cable USB đã kết nối**
3. ✅ **ESP32-S3 đã được cấp nguồn (LED sáng)**
4. ✅ **Không có chương trình khác đang dùng COM23**

**Kiểm tra COM port:**

```powershell
# Xem COM ports
Get-PnpDevice -Class Ports | Where-Object {$_.Status -eq 'OK'}
```

---

## Troubleshooting

### Nếu vẫn lỗi "No serial data received":

1. **Thử lại download mode:**
   - Đảm bảo nhấn đúng thứ tự: BOOT → RESET → Thả RESET → Thả BOOT
   - Thử nhiều lần nếu cần

2. **Kiểm tra nút BOOT:**
   - Nút BOOT có thể là **IO0** hoặc **GPIO0**
   - Xem schematic board để xác định

3. **Thử baud rate khác:**
   ```powershell
   idf.py -p COM23 -b 9600 flash
   ```

4. **Thử flash thủ công với esptool:**
   ```powershell
   D:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe D:\Espressif\frameworks\esp-idf-v5.5.1\components\esptool_py\esptool\esptool.py --chip esp32s3 -p COM23 -b 115200 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB 0x0 build\bootloader\bootloader.bin 0x8000 build\partition_table\partition-table.bin 0x10000 build\hai-os-simplexl.bin
   ```

5. **Kiểm tra driver:**
   - Đảm bảo driver USB-to-Serial đã được cài đặt
   - Thử rút và cắm lại cable USB

---

## Lưu Ý Quan Trọng

⚠️ **ESP32-S3 chỉ ở download mode trong vài giây sau khi thả nút BOOT**

⚠️ **Phải flash NGAY sau khi thả nút BOOT**

⚠️ **Nếu quá lâu, ESP32-S3 sẽ thoát download mode và cần làm lại**

---

## Thành Công

**Khi flash thành công, bạn sẽ thấy:**

```
Writing at 0x00010000... (100 %)
Wrote 2734208 bytes (1746244 compressed) at 0x00010000 in 15.4 seconds (effective 1420.0 kbit/s)...
Hash of data verified.

Leaving...
Hard resetting via RTS pin...
```

**Sau đó ESP32-S3 sẽ tự động reset và chạy firmware mới!**

---

## Liên Hệ

Nếu vẫn không được, vui lòng cung cấp:
1. Model board ESP32-S3 (ví dụ: ESP32-S3-DevKitC-1)
2. Vị trí nút BOOT và RESET trên board
3. Output của `idf.py -p COM23 -b 115200 -v flash`



