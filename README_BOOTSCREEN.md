# Boot Screen Image Setup

## Quick Start

1. **Place your bootscreen image:**
   - File: `assets/boot/bootscreen_320x480.png`
   - Size: 320x480 pixels (exact)
   - Format: PNG (RGB)

2. **Convert to LVGL format:**
   ```bash
   python tools/convert_png_to_lvgl_rgb565.py
   ```

3. **Build:**
   ```bash
   idf.py build
   ```

## Files Generated

After running the conversion script:
- `components/sx_assets/generated/bootscreen_img.c` - Image data (RGB565)
- `components/sx_assets/generated/bootscreen_img.h` - Header file

## Requirements

- Python 3.x
- Pillow library: `pip install Pillow`

## Notes

- The image is embedded at compile time (no SD card needed)
- Image format: RGB565 little-endian (matches LVGL TRUE_COLOR 16-bit)
- Display resolution: 320x480 (portrait)
- Boot screen will show the image at position (0,0) with no scaling














