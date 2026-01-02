#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ESP32 Flash Boot Mode Entry Script
Controls DTR and RTS signals to enter boot mode
"""

import serial
import time
import sys
import os

# Fix Windows console encoding
if sys.platform == 'win32':
    os.system('chcp 65001 >nul 2>&1')

def enter_boot_mode(port='COM11', baud=115200):
    """
    Đưa ESP32 vào chế độ flash boot
    
    Phương pháp:
    1. Đặt DTR = LOW (GPIO0 xuống thấp)
    2. Đặt RTS = LOW (Reset ESP32)
    3. Đợi một chút
    4. Đặt RTS = HIGH (Thả reset, ESP32 khởi động)
    5. Giữ DTR = LOW (GPIO0 vẫn thấp) để ESP32 vào boot mode
    """
    try:
        print(f"Dang ket noi den {port}...")
        # Mở cổng serial với các tùy chọn phù hợp
        ser = serial.Serial(
            port=port,
            baudrate=baud,
            timeout=1,
            write_timeout=1
        )
        
        # Đảm bảo cổng đã mở
        if not ser.is_open:
            print(f"Loi: Khong the mo cong serial {port}")
            return False
        
        print("Dang dua ESP32 vao boot mode...")
        print("(Phuong phap: Giu GPIO0 thap TRONG KHI reset)")
        print()
        
        # Phương pháp 1: Giữ GPIO0 thấp TRƯỚC KHI reset
        print("Buoc 1: Keo GPIO0 xuong thap (DTR = LOW)...")
        ser.setDTR(False)  # GPIO0 = LOW
        time.sleep(0.1)  # Đợi để GPIO0 ổn định
        
        print("Buoc 2: Reset ESP32 (RTS = LOW)...")
        ser.setRTS(False)  # EN = LOW (Reset)
        time.sleep(0.2)  # Giữ reset lâu hơn (200ms)
        
        print("Buoc 3: Tha reset nhung VAN GIU GPIO0 thap (RTS = HIGH, DTR = LOW)...")
        ser.setRTS(True)   # Thả EN (ESP32 khởi động)
        # QUAN TRỌNG: Giữ DTR ở mức thấp trong khi ESP32 khởi động
        ser.setDTR(False)  # Đảm bảo GPIO0 vẫn thấp
        time.sleep(0.3)  # Đợi ESP32 khởi động với GPIO0 thấp
        
        print("Buoc 4: Giu GPIO0 thap them de dam bao vao boot mode...")
        ser.setDTR(False)  # Tiếp tục giữ GPIO0 thấp
        time.sleep(0.5)  # Giữ thêm 500ms
        
        print("\n" + "="*50)
        print("ESP32 da duoc reset voi GPIO0 o muc thap")
        print("="*50)
        
        # Kiểm tra xem có vào boot mode không bằng cách thử đọc chip ID
        print("\nDang kiem tra xem ESP32 co vao boot mode khong...")
        ser.close()  # Đóng để esptool có thể dùng
        
        # Thử kết nối với esptool để kiểm tra
        time.sleep(0.5)
        import subprocess
        result = subprocess.run(
            ['python', '-m', 'esptool', '--port', port, 'chip-id'],
            capture_output=True,
            text=True,
            timeout=3
        )
        
        if result.returncode == 0:
            print("✓ THANH CONG! ESP32 da vao boot mode!")
            print("\nBan co the chay lenh flash ngay:")
            print("  idf.py -p COM11 flash")
            return True
        else:
            print("✗ ESP32 chua vao boot mode hoac da thoat khoi boot mode.")
            print("\nThu phuong phap 2: Giu ket noi mo de duy tri boot mode...")
            
            # Phương pháp 2: Giữ kết nối mở và GPIO0 ở mức thấp
            try:
                ser2 = serial.Serial(port, baud, timeout=1)
                ser2.setDTR(False)  # Giữ GPIO0 thấp
                print("\n" + "="*50)
                print("Dang giu GPIO0 o muc thap (ket noi mo)...")
                print("="*50)
                print("\nQUAN TRONG: Script dang giu GPIO0 o muc thap.")
                print("Hay mo terminal KHAC va chay lenh flash:")
                print("  idf.py -p COM11 flash")
                print("\nNhan Ctrl+C sau khi flash xong de dong ket noi...")
                print()
                
                while True:
                    time.sleep(1)
            except KeyboardInterrupt:
                print("\n\nDang dong ket noi...")
                ser2.setDTR(True)
                ser2.close()
                print("Da dong ket noi.")
                return True
            except Exception as e2:
                print(f"\nLoi khi giu ket noi: {e2}")
                return False
            
    except serial.SerialException as e:
        print(f"\nLoi: Khong the mo cong serial {port}")
        print(f"Chi tiet: {e}")
        print("\nVui long kiem tra:")
        print("1. So cong COM co dung khong (COM11)")
        print("2. ESP32 da ket noi chua")
        print("3. Cong serial co bi chuong trinh khac dung khong")
        return False
    except Exception as e:
        print(f"\nDa xay ra loi: {e}")
        import traceback
        traceback.print_exc()
        return False

def verify_boot_mode(port='COM11'):
    """Kiểm tra xem ESP32 có ở boot mode không"""
    try:
        import subprocess
        result = subprocess.run(
            ['python', '-m', 'esptool', '--port', port, 'chip-id'],
            capture_output=True,
            text=True,
            timeout=5
        )
        if result.returncode == 0:
            print("\n✓ Xac nhan: ESP32 da vao boot mode thanh cong!")
            return True
        else:
            print("\n✗ Khong the xac nhan boot mode. Co the ESP32 chua vao boot mode.")
            return False
    except Exception as e:
        print(f"\nKhong the kiem tra boot mode: {e}")
        return None

if __name__ == "__main__":
    port = 'COM11'
    if len(sys.argv) > 1:
        port = sys.argv[1]
    
    print("=" * 50)
    print("Cong cu dua ESP32 vao Flash Boot Mode")
    print("=" * 50)
    print()
    
    success = enter_boot_mode(port)
    
    if success:
        print("\n" + "="*50)
        print("Hoan thanh!")
        print("="*50)

