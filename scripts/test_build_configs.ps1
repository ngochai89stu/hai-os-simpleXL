# PowerShell script to test build with different LCD configurations
# Usage: .\scripts\test_build_configs.ps1

$ErrorActionPreference = "Stop"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Testing Build with Different LCD Configs" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# Test configurations
$configs = @(
    @{Name="ST7796_320X480"; Config="CONFIG_LCD_ST7796_320X480=y"},
    @{Name="ST7789_240X320"; Config="CONFIG_LCD_ST7789_240X320=y"},
    @{Name="ST7789_240X240"; Config="CONFIG_LCD_ST7789_240X240=y"},
    @{Name="ILI9341_240X320"; Config="CONFIG_LCD_ILI9341_240X320=y"}
)

$passed = 0
$failed = 0

foreach ($config in $configs) {
    Write-Host ""
    Write-Host "Testing: $($config.Name)" -ForegroundColor Yellow
    Write-Host "Config: $($config.Config)"
    
    try {
        # Check if required includes exist
        $platformFile = "components\sx_platform\sx_platform.c"
        if (Test-Path $platformFile) {
            $content = Get-Content $platformFile -Raw
            $found = $false
            
            if ($config.Name -like "*ST7789*") {
                if ($content -match "esp_lcd_new_panel_st7789") {
                    Write-Host "  ✓ ST7789 support found" -ForegroundColor Green
                    $found = $true
                } else {
                    Write-Host "  ✗ ST7789 support missing" -ForegroundColor Red
                    $failed++
                    continue
                }
            }
            if ($config.Name -like "*ILI9341*") {
                if ($content -match "esp_lcd_new_panel_ili9341") {
                    Write-Host "  ✓ ILI9341 support found" -ForegroundColor Green
                    $found = $true
                } else {
                    Write-Host "  ✗ ILI9341 support missing" -ForegroundColor Red
                    $failed++
                    continue
                }
            }
            if ($config.Name -like "*ST7796*") {
                if ($content -match "esp_lcd_new_panel_st7796") {
                    Write-Host "  ✓ ST7796 support found" -ForegroundColor Green
                    $found = $true
                } else {
                    Write-Host "  ✗ ST7796 support missing" -ForegroundColor Red
                    $failed++
                    continue
                }
            }
            
            if ($found) {
                Write-Host "  ✓ Code check passed for $($config.Name)" -ForegroundColor Green
                $passed++
            }
        } else {
            Write-Host "  ✗ Platform file not found" -ForegroundColor Red
            $failed++
        }
    } catch {
        Write-Host "  ✗ Error: $_" -ForegroundColor Red
        $failed++
    }
}

# Summary
Write-Host ""
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Test Summary" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Passed: $passed" -ForegroundColor Green
if ($failed -eq 0) {
    Write-Host "Failed: $failed" -ForegroundColor Green
} else {
    Write-Host "Failed: $failed" -ForegroundColor Red
}
Write-Host "Total: $($passed + $failed)"

if ($failed -eq 0) {
    Write-Host "All code checks passed!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Note: To actually test builds, run:" -ForegroundColor Yellow
    Write-Host "  idf.py menuconfig" -ForegroundColor Yellow
    Write-Host "  (Select different LCD types)" -ForegroundColor Yellow
    Write-Host "  idf.py build" -ForegroundColor Yellow
    exit 0
} else {
    Write-Host "Some checks failed!" -ForegroundColor Red
    exit 1
}
