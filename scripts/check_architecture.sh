#!/bin/bash
# Architecture boundary checker for SimpleXL
# Checks for forbidden includes and LVGL calls
# P2.1: Enhanced with sx_lvgl.h and compile-time guard checks (Section 7.2 SIMPLEXL_ARCH v1.3)

set -e

ERRORS=0
WARNINGS=0

echo "=== SimpleXL Architecture Boundary Check (Enhanced P2.1) ==="
echo ""

# Check 1: Services should not include UI headers
echo "Checking: Services should not include sx_ui headers..."
SERVICE_UI_VIOLATIONS=$(find components/sx_services -name "*.c" -o -name "*.h" | xargs grep -l "sx_ui\|ui_router\|ui_screen" 2>/dev/null || true)
if [ -n "$SERVICE_UI_VIOLATIONS" ]; then
    echo "❌ VIOLATION: Services include UI headers:"
    echo "$SERVICE_UI_VIOLATIONS"
    ERRORS=$((ERRORS + 1))
else
    echo "✅ OK: No services include UI headers"
fi
echo ""

# Check 2: UI should not include service headers (except through core)
echo "Checking: UI should not include service headers..."
UI_SERVICE_VIOLATIONS=$(find components/sx_ui -name "*.c" -o -name "*.h" | xargs grep -l "sx_audio_service\|sx_radio_service\|sx_wifi_service\|sx_ir_service\|sx_chatbot_service\|sx_sd_service\|sx_settings_service\|sx_ota_service\|sx_intent_service" 2>/dev/null || true)
if [ -n "$UI_SERVICE_VIOLATIONS" ]; then
    echo "❌ VIOLATION: UI includes service headers:"
    echo "$UI_SERVICE_VIOLATIONS"
    ERRORS=$((ERRORS + 1))
else
    echo "✅ OK: No UI includes service headers"
fi
echo ""

# Check 3: Platform should not call LVGL
echo "Checking: Platform should not call LVGL..."
PLATFORM_LVGL_VIOLATIONS=$(find components/sx_platform -name "*.c" -o -name "*.h" | xargs grep -l "lv_\|LVGL\|lvgl" 2>/dev/null || true)
if [ -n "$PLATFORM_LVGL_VIOLATIONS" ]; then
    echo "❌ VIOLATION: Platform calls LVGL:"
    echo "$PLATFORM_LVGL_VIOLATIONS"
    ERRORS=$((ERRORS + 1))
else
    echo "✅ OK: Platform does not call LVGL"
fi
echo ""

# Check 4: Services should not call LVGL
echo "Checking: Services should not call LVGL..."
SERVICE_LVGL_VIOLATIONS=$(find components/sx_services -name "*.c" -o -name "*.h" | xargs grep -l "lv_\|LVGL\|lvgl" 2>/dev/null || true)
if [ -n "$SERVICE_LVGL_VIOLATIONS" ]; then
    echo "❌ VIOLATION: Services call LVGL:"
    echo "$SERVICE_LVGL_VIOLATIONS"
    ERRORS=$((ERRORS + 1))
else
    echo "✅ OK: Services do not call LVGL"
fi
echo ""

# Check 5: Core should not call LVGL (except bootstrap which is allowed)
echo "Checking: Core should not call LVGL (bootstrap exception)..."
CORE_LVGL_VIOLATIONS=$(find components/sx_core -name "*.c" -o -name "*.h" | grep -v "sx_bootstrap.c" | xargs grep -l "lv_\|LVGL\|lvgl" 2>/dev/null || true)
if [ -n "$CORE_LVGL_VIOLATIONS" ]; then
    echo "⚠️  WARNING: Core files (except bootstrap) call LVGL:"
    echo "$CORE_LVGL_VIOLATIONS"
    WARNINGS=$((WARNINGS + 1))
else
    echo "✅ OK: Core does not call LVGL (bootstrap exception allowed)"
fi
echo ""

# P2.1: Check 6: Direct lvgl.h includes (should use sx_lvgl.h instead)
echo "Checking: Direct lvgl.h includes (should use sx_lvgl.h)..."
DIRECT_LVGL_INCLUDES=$(find components/sx_ui -name "*.c" -o -name "*.h" | xargs grep -l '^#include\s*[<"]lvgl\.h[>"]' 2>/dev/null | grep -v "sx_lvgl.h" | grep -v "sx_lvgl_guard.h" || true)
if [ -n "$DIRECT_LVGL_INCLUDES" ]; then
    echo "❌ VIOLATION: Direct lvgl.h includes found (should use sx_lvgl.h):"
    echo "$DIRECT_LVGL_INCLUDES"
    ERRORS=$((ERRORS + 1))
else
    echo "✅ OK: No direct lvgl.h includes (using sx_lvgl.h wrapper)"
fi
echo ""

# P2.1: Check 7: sx_lvgl.h includes outside sx_ui (should fail)
echo "Checking: sx_lvgl.h includes outside sx_ui component..."
SX_LVGL_OUTSIDE_UI=$(find components -name "*.c" -o -name "*.h" | grep -v "components/sx_ui" | xargs grep -l 'sx_lvgl\.h' 2>/dev/null || true)
if [ -n "$SX_LVGL_OUTSIDE_UI" ]; then
    echo "❌ VIOLATION: sx_lvgl.h included outside sx_ui component:"
    echo "$SX_LVGL_OUTSIDE_UI"
    ERRORS=$((ERRORS + 1))
else
    echo "✅ OK: sx_lvgl.h only included in sx_ui component"
fi
echo ""

# P2.1: Check 8: esp_lvgl_port.h includes (should be through sx_lvgl.h)
echo "Checking: Direct esp_lvgl_port.h includes in sx_ui..."
ESP_LVGL_PORT_DIRECT=$(find components/sx_ui -name "*.c" -o -name "*.h" | xargs grep -l '^#include\s*[<"]esp_lvgl_port\.h[>"]' 2>/dev/null | grep -v "sx_lvgl.h" | grep -v "sx_lvgl_guard.h" || true)
if [ -n "$ESP_LVGL_PORT_DIRECT" ]; then
    echo "⚠️  WARNING: Direct esp_lvgl_port.h includes (should be through sx_lvgl.h):"
    echo "$ESP_LVGL_PORT_DIRECT"
    WARNINGS=$((WARNINGS + 1))
else
    echo "✅ OK: esp_lvgl_port.h included through sx_lvgl.h wrapper"
fi
echo ""

# P2.1: Check 9: Verify sx_lvgl.h exists and has compile-time guard
echo "Checking: sx_lvgl.h exists and has compile-time guard..."
if [ ! -f "components/sx_ui/include/sx_lvgl.h" ]; then
    echo "❌ VIOLATION: sx_lvgl.h does not exist (Section 7.5 requirement)"
    ERRORS=$((ERRORS + 1))
else
    if grep -q "SX_COMPONENT_SX_UI" components/sx_ui/include/sx_lvgl.h; then
        echo "✅ OK: sx_lvgl.h exists and has compile-time guard"
    else
        echo "❌ VIOLATION: sx_lvgl.h missing compile-time guard (SX_COMPONENT_SX_UI)"
        ERRORS=$((ERRORS + 1))
    fi
fi
echo ""

# P2.1: Check 10: Verify CMakeLists.txt has compile-time guard definition
echo "Checking: CMakeLists.txt has SX_COMPONENT_SX_UI definition..."
if grep -q "SX_COMPONENT_SX_UI" components/sx_ui/CMakeLists.txt; then
    echo "✅ OK: CMakeLists.txt defines SX_COMPONENT_SX_UI"
else
    echo "❌ VIOLATION: CMakeLists.txt missing SX_COMPONENT_SX_UI definition"
    ERRORS=$((ERRORS + 1))
fi
echo ""

# Summary
echo "=== Summary ==="
echo "Errors: $ERRORS"
echo "Warnings: $WARNINGS"
echo ""

if [ $ERRORS -gt 0 ]; then
    echo "❌ Architecture boundary violations found!"
    exit 1
elif [ $WARNINGS -gt 0 ]; then
    echo "⚠️  Warnings found, but no critical violations"
    exit 0
else
    echo "✅ All architecture boundaries respected!"
    exit 0
fi

















