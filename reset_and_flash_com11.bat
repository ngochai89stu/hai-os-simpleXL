@echo off
REM Reset ESP32 và tự động vào boot mode để flash
REM Script này sẽ reset ESP32 và ngay lập tức chạy lệnh flash

echo ========================================
echo Reset ESP32 va vao boot mode de flash
echo ========================================
echo.

REM Reset ESP32 bằng cách điều khiển RTS
echo Dang reset ESP32...
python -c "import serial, time; s=serial.Serial('COM11', 115200); s.setRTS(False); time.sleep(0.1); s.setRTS(True); s.close(); print('ESP32 da duoc reset')"

if %ERRORLEVEL% NEQ 0 (
    echo Canh bao: Khong the reset ESP32 tu dong
    echo Vui long nhan nut RESET tren ESP32 thu cong
    echo.
    timeout /t 3
)

echo.
echo Dang chay lenh flash (se tu dong vao boot mode)...
echo.

REM Chạy lệnh flash - idf.py sẽ tự động xử lý việc vào boot mode
idf.py -p COM11 flash

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ========================================
    echo Flash that bai!
    echo ========================================
    echo.
    echo Thu cach thu cong:
    echo 1. Nhan va giu nut BOOT
    echo 2. Nhan nut RESET roi tha
    echo 3. Giu BOOT them 2 giay roi tha
    echo 4. Chay lai: idf.py -p COM11 flash
    echo.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================
echo Flash thanh cong!
echo ========================================
pause






