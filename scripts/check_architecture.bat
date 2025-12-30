@echo off
REM Architecture boundary checker for SimpleXL (Windows batch)
REM Checks for forbidden includes and LVGL calls

set ERRORS=0
set WARNINGS=0

echo === SimpleXL Architecture Boundary Check ===
echo.

REM Check 1: Services should not include UI headers
echo Checking: Services should not include sx_ui headers...
findstr /S /M /C:"sx_ui" /C:"ui_router" /C:"ui_screen" components\sx_services\*.c components\sx_services\*.h >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [ERROR] VIOLATION: Services include UI headers
    findstr /S /M /C:"sx_ui" /C:"ui_router" /C:"ui_screen" components\sx_services\*.c components\sx_services\*.h
    set /A ERRORS+=1
) else (
    echo [OK] No services include UI headers
)
echo.

REM Check 2: UI should not include service headers
echo Checking: UI should not include service headers...
findstr /S /M /C:"sx_audio_service" /C:"sx_radio_service" /C:"sx_wifi_service" /C:"sx_ir_service" /C:"sx_chatbot_service" /C:"sx_sd_service" /C:"sx_settings_service" /C:"sx_ota_service" /C:"sx_intent_service" components\sx_ui\*.c components\sx_ui\*.h >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [ERROR] VIOLATION: UI includes service headers
    findstr /S /M /C:"sx_audio_service" /C:"sx_radio_service" /C:"sx_wifi_service" /C:"sx_ir_service" /C:"sx_chatbot_service" /C:"sx_sd_service" /C:"sx_settings_service" /C:"sx_ota_service" /C:"sx_intent_service" components\sx_ui\*.c components\sx_ui\*.h
    set /A ERRORS+=1
) else (
    echo [OK] No UI includes service headers
)
echo.

REM Check 3: Platform should not call LVGL
echo Checking: Platform should not call LVGL...
findstr /S /M /I /C:"lv_" /C:"LVGL" /C:"lvgl" components\sx_platform\*.c components\sx_platform\*.h >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [ERROR] VIOLATION: Platform calls LVGL
    findstr /S /M /I /C:"lv_" /C:"LVGL" /C:"lvgl" components\sx_platform\*.c components\sx_platform\*.h
    set /A ERRORS+=1
) else (
    echo [OK] Platform does not call LVGL
)
echo.

REM Check 4: Services should not call LVGL
echo Checking: Services should not call LVGL...
findstr /S /M /I /C:"lv_" /C:"LVGL" /C:"lvgl" components\sx_services\*.c components\sx_services\*.h >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [ERROR] VIOLATION: Services call LVGL
    findstr /S /M /I /C:"lv_" /C:"LVGL" /C:"lvgl" components\sx_services\*.c components\sx_services\*.h
    set /A ERRORS+=1
) else (
    echo [OK] Services do not call LVGL
)
echo.

REM Summary
echo === Summary ===
echo Errors: %ERRORS%
echo Warnings: %WARNINGS%
echo.

if %ERRORS% GTR 0 (
    echo [FAIL] Architecture boundary violations found!
    exit /B 1
) else (
    echo [PASS] All architecture boundaries respected!
    exit /B 0
)














