#!/bin/bash
# Architecture boundary checker for SimpleXL
# Checks for forbidden includes and LVGL calls

set -e

ERRORS=0
WARNINGS=0

echo "=== SimpleXL Architecture Boundary Check ==="
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







