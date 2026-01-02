# Simple build script for unit tests
Write-Host "=== BUILD UNIT TESTS ===" -ForegroundColor Cyan

# Check if idf.py is available
Write-Host "`nChecking ESP-IDF environment..."
$idfAvailable = $false
try {
    $null = Get-Command idf.py -ErrorAction Stop
    $idfAvailable = $true
    Write-Host "✓ idf.py is available" -ForegroundColor Green
} catch {
    Write-Host "✗ idf.py not found" -ForegroundColor Red
    Write-Host "`nPlease export ESP-IDF first:" -ForegroundColor Yellow
    Write-Host "  `$env:IDF_PATH = 'D:\Espressif\frameworks\esp-idf-v5.1'" -ForegroundColor Yellow
    Write-Host "  . `"`$env:IDF_PATH\export.ps1`"" -ForegroundColor Yellow
    exit 1
}

# Check project structure
Write-Host "`nChecking project structure..."
if (-not (Test-Path "CMakeLists.txt")) {
    Write-Host "✗ CMakeLists.txt not found" -ForegroundColor Red
    exit 1
}
Write-Host "✓ CMakeLists.txt found" -ForegroundColor Green

if (-not (Test-Path "main\CMakeLists.txt")) {
    Write-Host "✗ main/CMakeLists.txt not found" -ForegroundColor Red
    exit 1
}
Write-Host "✓ main/CMakeLists.txt found" -ForegroundColor Green

# List test files
Write-Host "`nTest files:" -ForegroundColor Cyan
Get-ChildItem -Filter "test_*.c" | ForEach-Object {
    Write-Host "  ✓ $($_.Name)" -ForegroundColor Green
}

# Build
Write-Host "`n=== BUILDING ===" -ForegroundColor Cyan
idf.py build

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n=== BUILD SUCCESS ===" -ForegroundColor Green
    Write-Host "Test binary created successfully!" -ForegroundColor Green
} else {
    Write-Host "`n=== BUILD FAILED ===" -ForegroundColor Red
    Write-Host "Exit code: $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}









