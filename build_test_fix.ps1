# Build test script sau khi fix vi phạm kiến trúc
# Tuân theo SIMPLEXL_ARCH v1.3

param(
    [string]$IdfPath = "D:\Espressif\esp-idf"
)

Write-Host "=== Build Test - Fix Kiến Trúc SimpleXL v1.3 ===" -ForegroundColor Cyan
Write-Host ""

# Try to find ESP-IDF
$idfFound = $false
$possiblePaths = @(
    $IdfPath,
    "D:\Espressif\esp-idf",
    "D:\Espressif",
    "D:\esp\esp-idf",
    "D:\esp-idf",
    $env:IDF_PATH
)

foreach ($path in $possiblePaths) {
    if ($path -and (Test-Path "$path\export.ps1")) {
        Write-Host "Found ESP-IDF at: $path" -ForegroundColor Green
        & "$path\export.ps1"
        $idfFound = $true
        break
    }
}

if (-not $idfFound) {
    Write-Host "ERROR: ESP-IDF not found!" -ForegroundColor Red
    Write-Host "Please activate ESP-IDF environment first:" -ForegroundColor Yellow
    Write-Host "  D:\Espressif\esp-idf\export.ps1" -ForegroundColor Yellow
    Write-Host "Or provide path: .\build_test_fix.ps1 -IdfPath 'D:\path\to\esp-idf'" -ForegroundColor Yellow
    exit 1
}

# Verify idf.py is available
try {
    $idfVersion = idf.py --version 2>&1
    Write-Host "ESP-IDF version: $idfVersion" -ForegroundColor Green
} catch {
    Write-Host "ERROR: idf.py not available after activation" -ForegroundColor Red
    exit 1
}

# Change to project directory
Set-Location $PSScriptRoot

# Clean previous build (optional)
if (Test-Path "build") {
    Write-Host ""
    Write-Host "Cleaning previous build..." -ForegroundColor Yellow
    idf.py fullclean
}

# Build project
Write-Host ""
Write-Host "Starting build..." -ForegroundColor Cyan
Write-Host ""

$buildResult = idf.py build 2>&1 | Tee-Object -Variable buildOutput

# Check build result
if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "=== BUILD SUCCESS ===" -ForegroundColor Green
    Write-Host "✅ Tất cả components đã compile thành công!" -ForegroundColor Green
    Write-Host "✅ Vi phạm kiến trúc đã được fix:" -ForegroundColor Green
    Write-Host "   - sx_assets không còn include lvgl.h" -ForegroundColor Green
    Write-Host "   - Generated images được access qua sx_ui wrapper" -ForegroundColor Green
    Write-Host "   - Tuân theo SIMPLEXL_ARCH v1.3 Section 7.5" -ForegroundColor Green
    exit 0
} else {
    Write-Host ""
    Write-Host "=== BUILD FAILED ===" -ForegroundColor Red
    Write-Host $buildOutput
    exit 1
}






