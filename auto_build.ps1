# Auto-build script với tự động tìm ESP-IDF
# Tìm ESP-IDF trong D:\ và các vị trí phổ biến

param(
    [string]$IDF_PATH = ""
)

Write-Host "=== AUTO BUILD hai-os-simplexl ===" -ForegroundColor Cyan
Write-Host ""

# Danh sách các vị trí có thể có ESP-IDF
$searchPaths = @(
    "D:\Espressif\frameworks\esp-idf-v5.5.1",
    "D:\Espressif\frameworks\esp-idf-v5.5",
    "D:\Espressif\frameworks\esp-idf-v5.1",
    "D:\Espressif\frameworks\esp-idf-v5.2",
    "D:\esp\esp-idf",
    "D:\Espressif\frameworks\esp-idf",
    "C:\Espressif\frameworks\esp-idf-v5.5.1",
    "C:\Espressif\frameworks\esp-idf-v5.5",
    "$env:IDF_PATH"
)

# Nếu đã có IDF_PATH từ parameter
if ($IDF_PATH -ne "" -and (Test-Path "$IDF_PATH\export.ps1")) {
    Write-Host "Using provided IDF_PATH: $IDF_PATH" -ForegroundColor Green
} else {
    # Tìm ESP-IDF
    Write-Host "Searching for ESP-IDF..." -ForegroundColor Yellow
    $found = $false
    
    foreach ($path in $searchPaths) {
        if ($path -and (Test-Path "$path\export.ps1")) {
            $IDF_PATH = $path
            Write-Host "Found ESP-IDF at: $IDF_PATH" -ForegroundColor Green
            $found = $true
            break
        }
    }
    
    if (-not $found) {
        Write-Host "ERROR: ESP-IDF not found!" -ForegroundColor Red
        Write-Host "Searched in:" -ForegroundColor Yellow
        foreach ($path in $searchPaths) {
            if ($path) { Write-Host "  - $path" }
        }
        Write-Host ""
        Write-Host "Please install ESP-IDF or provide path:" -ForegroundColor Yellow
        Write-Host "  .\auto_build.ps1 -IDF_PATH 'D:\Espressif\frameworks\esp-idf-v5.5.1'" -ForegroundColor Yellow
        exit 1
    }
}

# Export ESP-IDF environment
Write-Host ""
Write-Host "Exporting ESP-IDF environment..." -ForegroundColor Cyan
try {
    . "$IDF_PATH\export.ps1"
    Write-Host "ESP-IDF environment exported successfully" -ForegroundColor Green
} catch {
    Write-Host "ERROR: Failed to export ESP-IDF environment" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}

# Verify idf.py is available
if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: idf.py not found after exporting ESP-IDF" -ForegroundColor Red
    exit 1
}

# Navigate to project root
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $projectRoot
Write-Host ""
Write-Host "Project directory: $projectRoot" -ForegroundColor Cyan

# Clean previous build (optional, comment out if not needed)
# Write-Host "Cleaning previous build..." -ForegroundColor Yellow
# idf.py fullclean

# Build project
Write-Host ""
Write-Host "=== BUILDING PROJECT ===" -ForegroundColor Cyan
Write-Host "Running: idf.py build" -ForegroundColor Yellow
Write-Host ""

$buildStart = Get-Date
try {
    idf.py build 2>&1 | Tee-Object -FilePath "build_output_$(Get-Date -Format 'yyyyMMdd_HHmmss').log"
    $buildExitCode = $LASTEXITCODE
    $buildEnd = Get-Date
    $buildDuration = $buildEnd - $buildStart
    
    Write-Host ""
    Write-Host "Build duration: $($buildDuration.TotalSeconds) seconds" -ForegroundColor Cyan
    
    if ($buildExitCode -eq 0) {
        Write-Host ""
        Write-Host "=== BUILD SUCCESS ===" -ForegroundColor Green
        Write-Host "Binary created: build\hai_os_simplexl.bin" -ForegroundColor Green
        Write-Host ""
        
        # Verify binary exists
        if (Test-Path "build\hai_os_simplexl.bin") {
            $binSize = (Get-Item "build\hai_os_simplexl.bin").Length
            Write-Host "Binary size: $([math]::Round($binSize/1KB, 2)) KB" -ForegroundColor Green
        }
        
        exit 0
    } else {
        Write-Host ""
        Write-Host "=== BUILD FAILED ===" -ForegroundColor Red
        Write-Host "Exit code: $buildExitCode" -ForegroundColor Red
        Write-Host ""
        Write-Host "Check build log for details." -ForegroundColor Yellow
        exit $buildExitCode
    }
} catch {
    Write-Host ""
    Write-Host "=== BUILD ERROR ===" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}

