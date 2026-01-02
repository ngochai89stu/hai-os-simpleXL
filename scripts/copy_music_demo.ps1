# Script to copy LVGL Music Demo 1-1 into project
# Usage: .\scripts\copy_music_demo.ps1

$ErrorActionPreference = "Stop"

# Paths
$ProjectRoot = $PSScriptRoot + "\.."
$DemoSource = "$ProjectRoot\managed_components\lvgl__lvgl\demos\music"
$DemoDest = "$ProjectRoot\components\sx_ui\screens\music_player_demo"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Copying LVGL Music Demo 1-1" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# Create directories
Write-Host "Creating directory structure..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path "$DemoDest\include" | Out-Null
New-Item -ItemType Directory -Force -Path "$DemoDest\src" | Out-Null
New-Item -ItemType Directory -Force -Path "$DemoDest\assets" | Out-Null

# Copy source files
Write-Host "Copying source files..." -ForegroundColor Yellow
Copy-Item "$DemoSource\lv_demo_music.c" "$DemoDest\src\sx_music_player_demo.c"
Copy-Item "$DemoSource\lv_demo_music_main.c" "$DemoDest\src\sx_music_player_main.c"
Copy-Item "$DemoSource\lv_demo_music_list.c" "$DemoDest\src\sx_music_player_list.c"

# Copy headers
Write-Host "Copying header files..." -ForegroundColor Yellow
Copy-Item "$DemoSource\lv_demo_music.h" "$DemoDest\include\sx_music_player_demo.h"
Copy-Item "$DemoSource\lv_demo_music_main.h" "$DemoDest\include\sx_music_player_main.h"
Copy-Item "$DemoSource\lv_demo_music_list.h" "$DemoDest\include\sx_music_player_list.h"

# Copy assets
Write-Host "Copying assets..." -ForegroundColor Yellow
Copy-Item "$DemoSource\assets\*" "$DemoDest\assets\" -Recurse -Force

Write-Host "========================================" -ForegroundColor Green
Write-Host "Files copied successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. Run rename script: .\scripts\rename_music_demo.ps1" -ForegroundColor White
Write-Host "2. Create audio integration layer" -ForegroundColor White
Write-Host "3. Update screen_music_player.c" -ForegroundColor White
Write-Host "4. Update CMakeLists.txt" -ForegroundColor White









