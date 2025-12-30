@echo off
REM Build and flash firmware to COM23
REM This script assumes ESP-IDF environment is already activated

echo ========================================
echo Building hai-os-simplexl firmware...
echo ========================================

idf.py build

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================
echo Flashing to COM23...
echo ========================================

idf.py -p COM23 flash

if %ERRORLEVEL% NEQ 0 (
    echo Flash failed!
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================
echo Build and flash completed successfully!
echo ========================================
pause









