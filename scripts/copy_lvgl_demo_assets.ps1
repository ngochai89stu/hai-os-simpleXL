# Script to copy LVGL Music Demo assets to sx_ui/assets
# Không cần clone repo, copy trực tiếp từ managed_components

$ErrorActionPreference = "Stop"

$SourceDir = "managed_components\lvgl__lvgl\demos\music\assets"
$DestDir = "components\sx_ui\assets"

Write-Host "=== Copy LVGL Music Demo Assets ===" -ForegroundColor Cyan
Write-Host "Source: $SourceDir" -ForegroundColor Yellow
Write-Host "Destination: $DestDir" -ForegroundColor Yellow
Write-Host ""

# Check source exists
if (-not (Test-Path $SourceDir)) {
    Write-Host "ERROR: Source directory not found: $SourceDir" -ForegroundColor Red
    exit 1
}

# Create destination directory
if (-not (Test-Path $DestDir)) {
    New-Item -ItemType Directory -Path $DestDir -Force | Out-Null
    Write-Host "Created directory: $DestDir" -ForegroundColor Green
}

# List of assets to copy
$AssetsToCopy = @(
    # Button images (16 files)
    "img_lv_demo_music_btn_play.c",
    "img_lv_demo_music_btn_play_large.c",
    "img_lv_demo_music_btn_pause.c",
    "img_lv_demo_music_btn_pause_large.c",
    "img_lv_demo_music_btn_prev.c",
    "img_lv_demo_music_btn_prev_large.c",
    "img_lv_demo_music_btn_next.c",
    "img_lv_demo_music_btn_next_large.c",
    "img_lv_demo_music_btn_loop.c",
    "img_lv_demo_music_btn_loop_large.c",
    "img_lv_demo_music_btn_rnd.c",
    "img_lv_demo_music_btn_rnd_large.c",
    "img_lv_demo_music_btn_list_play.c",
    "img_lv_demo_music_btn_list_play_large.c",
    "img_lv_demo_music_btn_list_pause.c",
    "img_lv_demo_music_btn_list_pause_large.c",
    
    # Icon images (8 files)
    "img_lv_demo_music_icon_1.c",
    "img_lv_demo_music_icon_1_large.c",
    "img_lv_demo_music_icon_2.c",
    "img_lv_demo_music_icon_2_large.c",
    "img_lv_demo_music_icon_3.c",
    "img_lv_demo_music_icon_3_large.c",
    "img_lv_demo_music_icon_4.c",
    "img_lv_demo_music_icon_4_large.c",
    
    # Decorative elements (12 files)
    "img_lv_demo_music_wave_top.c",
    "img_lv_demo_music_wave_top_large.c",
    "img_lv_demo_music_wave_bottom.c",
    "img_lv_demo_music_wave_bottom_large.c",
    "img_lv_demo_music_corner_left.c",
    "img_lv_demo_music_corner_left_large.c",
    "img_lv_demo_music_corner_right.c",
    "img_lv_demo_music_corner_right_large.c",
    "img_lv_demo_music_list_border.c",
    "img_lv_demo_music_list_border_large.c",
    "img_lv_demo_music_logo.c",
    "img_lv_demo_music_slider_knob.c",
    "img_lv_demo_music_slider_knob_large.c",
    
    # Album covers (6 files)
    "img_lv_demo_music_cover_1.c",
    "img_lv_demo_music_cover_1_large.c",
    "img_lv_demo_music_cover_2.c",
    "img_lv_demo_music_cover_2_large.c",
    "img_lv_demo_music_cover_3.c",
    "img_lv_demo_music_cover_3_large.c",
    
    # Spectrum data (3 files)
    "spectrum_1.h",
    "spectrum_2.h",
    "spectrum_3.h"
)

$CopiedCount = 0
$SkippedCount = 0
$ErrorCount = 0

foreach ($asset in $AssetsToCopy) {
    $SourcePath = Join-Path $SourceDir $asset
    $DestPath = Join-Path $DestDir $asset
    
    if (Test-Path $SourcePath) {
        try {
            Copy-Item -Path $SourcePath -Destination $DestPath -Force
            Write-Host "  ✓ Copied: $asset" -ForegroundColor Green
            $CopiedCount++
        } catch {
            Write-Host "  ✗ Error copying $asset : $_" -ForegroundColor Red
            $ErrorCount++
        }
    } else {
        Write-Host "  ⚠ Skipped (not found): $asset" -ForegroundColor Yellow
        $SkippedCount++
    }
}

Write-Host ""
Write-Host "=== Summary ===" -ForegroundColor Cyan
Write-Host "  Copied: $CopiedCount files" -ForegroundColor Green
Write-Host "  Skipped: $SkippedCount files" -ForegroundColor Yellow
Write-Host "  Errors: $ErrorCount files" -ForegroundColor $(if ($ErrorCount -eq 0) { "Green" } else { "Red" })
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "  1. Create components/sx_ui/assets/sx_ui_assets.h with LV_IMAGE_DECLARE for all assets"
Write-Host "  2. Update CMakeLists.txt to include assets directory"
Write-Host "  3. Include sx_ui_assets.h in screen_music_player.c"









