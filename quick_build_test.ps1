# Quick Build Test - Tự động tìm ESP-IDF và build
Write-Host "=== SimpleXL OS Quick Build Test ===" -ForegroundColor Cyan

# Tìm ESP-IDF ở D:\
$idfPaths = @(
    "D:\esp\esp-idf",
    "D:\esp-idf", 
    "D:\tools\esp-idf",
    "D:\Program Files\esp-idf",
    "D:\Espressif\frameworks\esp-idf-v*"
)

$idfPath = $null
foreach ($path in $idfPaths) {
    if (Test-Path "$path\export.ps1") {
        $idfPath = $path
        break
    }
}

# Nếu không tìm thấy, thử tìm tất cả thư mục có export.ps1
if (-not $idfPath) {
    Write-Host "Searching for ESP-IDF in D:\..." -ForegroundColor Yellow
    $found = Get-ChildItem D:\ -Recurse -Filter "export.ps1" -ErrorAction SilentlyContinue -Depth 3 | Select-Object -First 1
    if ($found) {
        $idfPath = $found.DirectoryName
    }
}

if ($idfPath) {
    Write-Host "Found ESP-IDF at: $idfPath" -ForegroundColor Green
    & "$idfPath\export.ps1"
    Write-Host "ESP-IDF activated" -ForegroundColor Green
} else {
    Write-Host "ESP-IDF not found. Please activate manually:" -ForegroundColor Yellow
    Write-Host "  D:\path\to\esp-idf\export.ps1" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Or set IDF_PATH environment variable" -ForegroundColor Yellow
    exit 1
}

# Build
Write-Host ""
Write-Host "Building..." -ForegroundColor Cyan
idf.py build

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "=== BUILD SUCCESS ===" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "=== BUILD FAILED ===" -ForegroundColor Red
    exit 1
}






