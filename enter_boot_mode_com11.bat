@echo off
REM Enter ESP32 flash boot mode (COM11)
REM Uses Python script to control DTR and RTS signals

echo ========================================
echo Entering ESP32 (COM11) flash boot mode...
echo ========================================
echo.

python enter_boot_mode_com11.py

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ========================================
    echo Warning: Failed to enter boot mode automatically
    echo ========================================
    echo.
    echo Please do it manually:
    echo 1. Hold BOOT button (GPIO0 to ground)
    echo 2. Press and release RESET button
    echo 3. Release BOOT button
    echo.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================
echo ESP32 is now in flash boot mode!
echo ========================================
echo.
echo You can now run flash commands
pause

