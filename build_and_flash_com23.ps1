# Build and flash firmware to COM23
# Usage: .\build_and_flash_com23.ps1
# Note: ESP-IDF environment must be activated first (run get_idf.ps1)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Building hai-os-simplexl firmware..." -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if idf.py is available
if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: idf.py not found!" -ForegroundColor Red
    Write-Host "Please activate ESP-IDF environment first:" -ForegroundColor Yellow
    Write-Host "  - Run get_idf.ps1, or" -ForegroundColor Yellow
    Write-Host "  - Source export.ps1 from ESP-IDF directory" -ForegroundColor Yellow
    exit 1
}

# Build
idf.py build

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "Build failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Flashing to COM23..." -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Flash to COM23
idf.py -p COM23 flash

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "Flash failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "Build and flash completed successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green


