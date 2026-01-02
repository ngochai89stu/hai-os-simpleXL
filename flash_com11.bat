@echo off
chcp 65001 >nul 2>&1
REM Script tự động: Giữ boot mode và flash

echo ========================================
echo Flash ESP32-S3 (COM11)
echo ========================================
echo.
echo LUU Y: Dong tat ca chuong trinh dang dung COM11!
echo.
pause

echo.
echo Buoc 1: Dang dua ESP32 vao boot mode...
echo.

REM Thử reset ESP32
python -c "import serial, time; s=serial.Serial('COM11', 115200); s.setDTR(False); s.setRTS(False); time.sleep(0.2); s.setRTS(True); time.sleep(0.1); s.close()" 2>nul

if %ERRORLEVEL% NEQ 0 (
    echo Canh bao: Khong the reset tu dong
    echo.
    echo Vui long thu cong:
    echo 1. Nhan va giu nut BOOT
    echo 2. Nhan nut RESET roi tha
    echo 3. Giu BOOT them 2 giay roi tha
    echo.
    timeout /t 5
)

echo.
echo Buoc 2: Dang chay lenh flash...
echo (idf.py se tu dong xu ly boot mode)
echo.

idf.py -p COM11 -b 115200 flash

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ========================================
    echo Flash that bai!
    echo ========================================
    echo.
    echo Thu cach khac:
    echo 1. Chay: python giu_boot_mode_com11.py (trong terminal 1)
    echo 2. Chay: idf.py -p COM11 flash (trong terminal 2)
    echo.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================
echo Flash thanh cong!
echo ========================================
pause






