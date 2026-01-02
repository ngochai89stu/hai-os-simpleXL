# Build Test Script for SimpleXL OS
# This script activates ESP-IDF and builds the project

param(
    [string]$IdfPath = "D:\esp\esp-idf"
)

Write-Host "=== SimpleXL OS Build Test ===" -ForegroundColor Cyan
Write-Host ""

# Try to find ESP-IDF
$idfFound = $false
$possiblePaths = @(
    $IdfPath,
    "D:\esp-idf",
    "D:\esp\esp-idf",
    "D:\tools\esp-idf",
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
    Write-Host "Please set IDF_PATH or provide path: .\build_test.ps1 -IdfPath 'D:\path\to\esp-idf'" -ForegroundColor Yellow
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

$buildResult = idf.py build 2>&1

# Check build result
if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "=== BUILD SUCCESS ===" -ForegroundColor Green
    Write-Host "All components compiled successfully!" -ForegroundColor Green
    exit 0
} else {
    Write-Host ""
    Write-Host "=== BUILD FAILED ===" -ForegroundColor Red
    Write-Host $buildResult
    exit 1
}






