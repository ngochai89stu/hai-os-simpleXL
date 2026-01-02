@echo off
REM Đưa ESP32 vào boot mode bằng esptool
REM Phương pháp này sử dụng esptool để reset và vào boot mode

echo ========================================
echo Dua ESP32 (COM11) vao boot mode...
echo ========================================
echo.

REM Phương pháp 1: Sử dụng esptool với --before và --after để điều khiển DTR/RTS
echo Phuong phap 1: Su dung esptool de reset va vao boot mode...
python -m esptool --port COM11 --baud 115200 --before default_reset --after hard_reset chip-id

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Thanh cong! ESP32 da vao boot mode
    echo ========================================
    echo.
    echo Ban co the chay lenh flash ngay bay gio:
    echo   idf.py -p COM11 flash
    echo.
    pause
    exit /b 0
)

echo.
echo Phuong phap 1 khong thanh cong, thu phuong phap 2...
echo.

REM Phương pháp 2: Chỉ reset ESP32
python -m esptool --port COM11 --baud 115200 --before no_reset --after hard_reset chip-id

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Thanh cong! ESP32 da vao boot mode
    echo ========================================
    pause
    exit /b 0
)

echo.
echo ========================================
echo Canh bao: Khong the tu dong vao boot mode
echo ========================================
echo.
echo Vui long thu cach thu cong:
echo 1. Nhan va giu nut BOOT tren ESP32
echo 2. Nhan nut RESET roi tha ra
echo 3. Tiep tuc giu nut BOOT trong 2 giay
echo 4. Tha nut BOOT
echo.
echo Sau do chay lenh flash:
echo   idf.py -p COM11 flash
echo.
pause






