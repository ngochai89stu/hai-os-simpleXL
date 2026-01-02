# Script để fix lỗi linking - rebuild clean
# Các hàm undefined reference đều có implementation, vấn đề có thể do build cache

Write-Host "=== Fix Linking Errors - Rebuild Clean ===" -ForegroundColor Cyan
Write-Host ""

# Activate ESP-IDF nếu chưa activate
if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    $idfPaths = @(
        "D:\Espressif\esp-idf",
        "D:\Espressif\frameworks\esp-idf-v5.1",
        "D:\Espressif\frameworks\esp-idf-v5.2",
        "D:\Espressif\frameworks\esp-idf-v5.3"
    )
    
    foreach ($path in $idfPaths) {
        if (Test-Path "$path\export.ps1") {
            Write-Host "Activating ESP-IDF: $path" -ForegroundColor Green
            & "$path\export.ps1"
            break
        }
    }
    
    if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
        Write-Host "ERROR: ESP-IDF not found. Please activate manually." -ForegroundColor Red
        exit 1
    }
}

# Change to project directory
Set-Location $PSScriptRoot

# Full clean
Write-Host "Cleaning build..." -ForegroundColor Yellow
idf.py fullclean 2>&1 | Out-Null

# Remove build directory completely
if (Test-Path "build") {
    Write-Host "Removing build directory..." -ForegroundColor Yellow
    Remove-Item -Path "build" -Recurse -Force -ErrorAction SilentlyContinue
}

# Build again
Write-Host ""
Write-Host "Building project..." -ForegroundColor Cyan
Write-Host ""

$buildResult = idf.py build 2>&1 | Tee-Object -Variable buildOutput

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "=== BUILD SUCCESS ===" -ForegroundColor Green
    Write-Host "✅ Linking errors đã được fix!" -ForegroundColor Green
    exit 0
} else {
    Write-Host ""
    Write-Host "=== BUILD FAILED ===" -ForegroundColor Red
    Write-Host "Last 50 lines:" -ForegroundColor Yellow
    $buildOutput | Select-Object -Last 50
    exit 1
}






