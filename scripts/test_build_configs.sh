#!/bin/bash
# Script to test build with different LCD configurations
# Usage: ./scripts/test_build_configs.sh

set -e

echo "=========================================="
echo "Testing Build with Different LCD Configs"
echo "=========================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test configurations
CONFIGS=(
    "ST7796_320X480:CONFIG_LCD_ST7796_320X480=y"
    "ST7789_240X320:CONFIG_LCD_ST7789_240X320=y"
    "ST7789_240X240:CONFIG_LCD_ST7789_240X240=y"
    "ILI9341_240X320:CONFIG_LCD_ILI9341_240X320=y"
)

# Function to test build with config
test_build() {
    local config_name=$1
    local config_value=$2
    
    echo ""
    echo -e "${YELLOW}Testing: ${config_name}${NC}"
    echo "Config: ${config_value}"
    
    # Set Kconfig value
    idf.py menuconfig --non-interactive <<EOF
CONFIG_BOARD_TYPE_HAI_OS_SIMPLEXL=y
${config_value}
CONFIG_HAI_TOUCH_ENABLE=y
EOF
    
    # Clean and build
    idf.py fullclean
    if idf.py build 2>&1 | tee /tmp/build_${config_name}.log; then
        echo -e "${GREEN}✓ Build successful for ${config_name}${NC}"
        return 0
    else
        echo -e "${RED}✗ Build failed for ${config_name}${NC}"
        return 1
    fi
}

# Main test loop
PASSED=0
FAILED=0

for config in "${CONFIGS[@]}"; do
    IFS=':' read -r name value <<< "$config"
    if test_build "$name" "$value"; then
        ((PASSED++))
    else
        ((FAILED++))
    fi
done

# Summary
echo ""
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo -e "${GREEN}Passed: ${PASSED}${NC}"
echo -e "${RED}Failed: ${FAILED}${NC}"
echo "Total: $((PASSED + FAILED))"

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi

