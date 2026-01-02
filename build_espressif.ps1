# Build script for D:\Espressif environment
# SimpleXL OS Build Test

param(
    [string]$IdfPath = "D:\Espressif\esp-idf"
)

Write-Host "=== SimpleXL OS Build Test (D:\Espressif) ===" -ForegroundColor Cyan
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
    Write-Host "Please install ESP-IDF at D:\Espressif\esp-idf or set IDF_PATH" -ForegroundColor Yellow
    Write-Host "Or provide path: .\build_espressif.ps1 -IdfPath 'D:\path\to\esp-idf'" -ForegroundColor Yellow
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
    Write-Host "All components compiled successfully!" -ForegroundColor Green
    exit 0
} else {
    Write-Host ""
    Write-Host "=== BUILD FAILED ===" -ForegroundColor Red
    Write-Host $buildOutput
    exit 1
}






