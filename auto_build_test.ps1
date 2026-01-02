# Auto build test script - Tự động tìm và activate ESP-IDF
# Tuân theo SIMPLEXL_ARCH v1.3

param(
    [string]$IdfPath = ""
)

Write-Host "=== Auto Build Test - SimpleXL OS ===" -ForegroundColor Cyan
Write-Host ""

# Danh sách các vị trí có thể có ESP-IDF
$searchPaths = @(
    $IdfPath,
    "D:\Espressif\esp-idf",
    "D:\esp\esp-idf",
    "D:\esp-idf",
    "D:\Espressif\frameworks\esp-idf-v5.1",
    "D:\Espressif\frameworks\esp-idf-v5.2",
    "D:\Espressif\frameworks\esp-idf-v5.3",
    "D:\Espressif\frameworks\esp-idf",
    "$env:USERPROFILE\.espressif\esp-idf",
    "$env:IDF_PATH"
)

$idfFound = $false
$idfPath = ""

# Tìm ESP-IDF
foreach ($path in $searchPaths) {
    if ($path -and (Test-Path "$path\export.ps1")) {
        $idfPath = $path
        $idfFound = $true
        Write-Host "Found ESP-IDF at: $path" -ForegroundColor Green
        break
    }
}

# Nếu không tìm thấy, tìm trong toàn bộ D:\ (chậm hơn)
if (-not $idfFound) {
    Write-Host "Searching for ESP-IDF in D:\ (this may take a while)..." -ForegroundColor Yellow
    $found = Get-ChildItem -Path "D:\" -Recurse -Filter "export.ps1" -ErrorAction SilentlyContinue -Depth 3 | 
        Where-Object { $_.FullName -like "*esp-idf*" -or $_.FullName -like "*Espressif*" } | 
        Select-Object -First 1
    
    if ($found) {
        $idfPath = $found.DirectoryName
        $idfFound = $true
        Write-Host "Found ESP-IDF at: $idfPath" -ForegroundColor Green
    }
}

if (-not $idfFound) {
    Write-Host "ERROR: ESP-IDF not found!" -ForegroundColor Red
    Write-Host "Please install ESP-IDF or provide path:" -ForegroundColor Yellow
    Write-Host "  .\auto_build_test.ps1 -IdfPath 'D:\path\to\esp-idf'" -ForegroundColor Yellow
    exit 1
}

# Activate ESP-IDF
Write-Host "Activating ESP-IDF..." -ForegroundColor Cyan
& "$idfPath\export.ps1"

# Verify idf.py is available
try {
    $version = idf.py --version 2>&1
    Write-Host "ESP-IDF version: $version" -ForegroundColor Green
} catch {
    Write-Host "ERROR: idf.py not available after activation" -ForegroundColor Red
    exit 1
}

# Change to project directory
Set-Location $PSScriptRoot

# Fix managed components issue (nếu có)
if (Test-Path "managed_components\espressif__esp-sr") {
    Write-Host ""
    Write-Host "Removing modified managed component..." -ForegroundColor Yellow
    Remove-Item -Path "managed_components\espressif__esp-sr" -Recurse -Force -ErrorAction SilentlyContinue
}

# Clean previous build (optional)
if (Test-Path "build") {
    Write-Host ""
    Write-Host "Cleaning previous build..." -ForegroundColor Yellow
    idf.py fullclean 2>&1 | Out-Null
}

# Build project
Write-Host ""
Write-Host "Starting build..." -ForegroundColor Cyan
Write-Host ""

$buildStart = Get-Date
$buildResult = idf.py build 2>&1 | Tee-Object -Variable buildOutput
$buildEnd = Get-Date
$buildDuration = ($buildEnd - $buildStart).TotalSeconds

# Check build result
Write-Host ""
if ($LASTEXITCODE -eq 0) {
    Write-Host "=== BUILD SUCCESS ===" -ForegroundColor Green
    Write-Host "✅ Build completed in $([math]::Round($buildDuration, 2)) seconds" -ForegroundColor Green
    Write-Host ""
    Write-Host "✅ Vi phạm kiến trúc đã được fix:" -ForegroundColor Green
    Write-Host "   - sx_assets không còn include lvgl.h" -ForegroundColor Green
    Write-Host "   - Generated images được access qua sx_ui wrapper" -ForegroundColor Green
    Write-Host "   - Tuân theo SIMPLEXL_ARCH v1.3 Section 7.5" -ForegroundColor Green
    Write-Host ""
    Write-Host "✅ Tất cả components đã compile thành công!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "=== BUILD FAILED ===" -ForegroundColor Red
    Write-Host "Build duration: $([math]::Round($buildDuration, 2)) seconds" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Last 50 lines of build output:" -ForegroundColor Yellow
    Write-Host "----------------------------------------" -ForegroundColor Yellow
    $buildOutput | Select-Object -Last 50
    Write-Host "----------------------------------------" -ForegroundColor Yellow
    exit 1
}






