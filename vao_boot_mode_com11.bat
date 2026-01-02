@echo off
chcp 65001 >nul 2>&1
REM Script đơn giản để đưa ESP32 vào boot mode

echo ========================================
echo Dua ESP32 (COM11) vao Boot Mode
echo ========================================
echo.
echo LUU Y: Dong tat ca chuong trinh dang dung COM11 truoc!
echo (Arduino IDE, Serial Monitor, Python scripts, etc.)
echo.
pause

echo.
echo Dang thu phuong phap 1: Reset bang esptool...
python -m esptool --port COM11 --baud 115200 --before default-reset --after hard-reset chip-id

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo THANH CONG! ESP32 da vao boot mode
    echo ========================================
    echo.
    echo Ban co the chay lenh flash ngay:
    echo   idf.py -p COM11 flash
    echo.
    pause
    exit /b 0
)

echo.
echo Phuong phap 1 khong thanh cong.
echo.
echo ========================================
echo HUONG DAN THU CONG:
echo ========================================
echo.
echo 1. Nhan va GIU nut BOOT tren ESP32
echo 2. Nhan nut RESET roi THA ngay
echo 3. Tiep tuc GIU nut BOOT them 2 giay
echo 4. THA nut BOOT
echo.
echo Sau do chay lenh flash:
echo   idf.py -p COM11 flash
echo.
pause






