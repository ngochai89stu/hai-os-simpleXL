#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Script giữ ESP32 ở boot mode bằng cách giữ GPIO0 ở mức thấp
Sử dụng khi phương pháp tự động không hoạt động
"""

import serial
import time
import sys
import os

if sys.platform == 'win32':
    os.system('chcp 65001 >nul 2>&1')

def giu_boot_mode(port='COM11', baud=115200):
    """
    Giữ ESP32 ở boot mode bằng cách:
    1. Reset ESP32
    2. Giữ GPIO0 ở mức thấp liên tục
    """
    try:
        print(f"Dang ket noi den {port}...")
        ser = serial.Serial(port, baud, timeout=1)
        
        print("\n" + "="*60)
        print("DANG GIU ESP32 O BOOT MODE")
        print("="*60)
        print()
        print("Buoc 1: Reset ESP32...")
        ser.setRTS(False)  # Reset
        time.sleep(0.2)
        ser.setRTS(True)   # Thả reset
        
        print("Buoc 2: Giu GPIO0 o muc thap...")
        ser.setDTR(False)  # GPIO0 = LOW
        time.sleep(0.1)
        
        print("\n" + "="*60)
        print("ESP32 DANG O BOOT MODE")
        print("="*60)
        print()
        print("Script dang giu GPIO0 o muc thap.")
        print("Hay mo terminal KHAC va chay lenh flash:")
        print()
        print("  idf.py -p COM11 flash")
        print()
        print("HOAC:")
        print()
        print("  python -m esptool --port COM11 write_flash ...")
        print()
        print("="*60)
        print("Nhan Ctrl+C sau khi flash xong de dong ket noi")
        print("="*60)
        print()
        
        # Giữ GPIO0 ở mức thấp liên tục
        try:
            while True:
                ser.setDTR(False)  # Đảm bảo GPIO0 vẫn thấp
                time.sleep(0.1)
        except KeyboardInterrupt:
            print("\n\nDang dong ket noi...")
            ser.setDTR(True)  # Thả GPIO0
            time.sleep(0.1)
            ser.close()
            print("Da dong ket noi.")
            return True
            
    except serial.SerialException as e:
        print(f"\nLoi: Khong the mo cong serial {port}")
        print(f"Chi tiet: {e}")
        return False
    except Exception as e:
        print(f"\nLoi: {e}")
        return False

if __name__ == "__main__":
    port = 'COM11'
    if len(sys.argv) > 1:
        port = sys.argv[1]
    
    print("=" * 60)
    print("Script Giu ESP32 O Boot Mode")
    print("=" * 60)
    print()
    
    giu_boot_mode(port)






