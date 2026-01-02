# Script để chạy smoke test trên phần cứng
# Yêu cầu: ESP-IDF đã được cấu hình và device đã được flash firmware

param(
    [string]$Port = "COM11",
    [int]$Timeout = 30
)

Write-Host "=== SimpleXL Smoke Test Runner ===" -ForegroundColor Cyan
Write-Host ""

# Kiểm tra port
if (-not $Port) {
    Write-Host "ERROR: Port không được chỉ định" -ForegroundColor Red
    Write-Host "Usage: .\run_smoke_test.ps1 -Port COM11" -ForegroundColor Yellow
    exit 1
}

Write-Host "Port: $Port" -ForegroundColor Green
Write-Host "Timeout: $Timeout seconds" -ForegroundColor Green
Write-Host ""

# Tìm ESP-IDF
$idfFound = $false
$possiblePaths = @(
    "D:\Espressif\frameworks\esp-idf-v5.5.1",
    "D:\Espressif\frameworks\esp-idf-v5.5",
    "D:\Espressif\esp-idf",
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
    Write-Host "Please install ESP-IDF or set IDF_PATH" -ForegroundColor Yellow
    exit 1
}

# Chạy monitor và tìm smoke test results
Write-Host "Starting monitor to capture smoke test results..." -ForegroundColor Yellow
Write-Host "Press Ctrl+] to exit monitor" -ForegroundColor Yellow
Write-Host ""

# Chạy monitor với filter để chỉ hiển thị smoke test logs
try {
    idf.py -p $Port monitor --print-filter="sx_selftest:*"
} catch {
    Write-Host "ERROR: Monitor failed" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Smoke test monitoring completed" -ForegroundColor Green


