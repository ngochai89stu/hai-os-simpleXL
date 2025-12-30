#!/usr/bin/env python3
"""
Convert PNG image to LVGL RGB565 C array format.

Input: assets/boot/bootscreen_320x480.png (320x480 pixels)
Output: components/sx_assets/generated/bootscreen_img.c/.h
"""

import sys
import os
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("ERROR: PIL (Pillow) is required. Install with: pip install Pillow")
    sys.exit(1)

# Paths
SCRIPT_DIR = Path(__file__).parent.absolute()
PROJECT_ROOT = SCRIPT_DIR.parent
ASSETS_DIR = PROJECT_ROOT / "assets" / "boot"
GENERATED_DIR = PROJECT_ROOT / "components" / "sx_assets" / "generated"

INPUT_PNG = ASSETS_DIR / "bootscreen_320x480.png"
OUTPUT_C = GENERATED_DIR / "bootscreen_img.c"
OUTPUT_H = GENERATED_DIR / "bootscreen_img.h"

# Expected image size
EXPECTED_WIDTH = 320
EXPECTED_HEIGHT = 480

def rgb888_to_bgr565(r, g, b):
    """Convert RGB888 to BGR565 (little-endian) for ST7796 BGR display.
    
    ST7796 expects BGR format, so we swap R and B channels in the pixel data.
    """
    # Swap R and B for BGR format
    r5 = (b >> 3) & 0x1F  # Blue in R position
    g6 = (g >> 2) & 0x3F  # Green stays the same
    b5 = (r >> 3) & 0x1F  # Red in B position
    bgr565 = (r5 << 11) | (g6 << 5) | b5
    # Return as little-endian bytes
    return bytes([bgr565 & 0xFF, (bgr565 >> 8) & 0xFF])

def convert_png_to_rgb565(png_path):
    """Convert PNG to BGR565 byte array (for ST7796 BGR display)."""
    if not png_path.exists():
        print(f"ERROR: Input file not found: {png_path}")
        sys.exit(1)
    
    img = Image.open(png_path)
    
    # Validate size
    if img.size != (EXPECTED_WIDTH, EXPECTED_HEIGHT):
        print(f"ERROR: Image size is {img.size}, expected ({EXPECTED_WIDTH}, {EXPECTED_HEIGHT})")
        sys.exit(1)
    
    # Convert to RGB if needed
    if img.mode != 'RGB':
        img = img.convert('RGB')
    
    # Convert to BGR565 (swap R and B for ST7796 BGR display)
    bgr565_data = bytearray()
    for y in range(EXPECTED_HEIGHT):
        for x in range(EXPECTED_WIDTH):
            r, g, b = img.getpixel((x, y))
            bgr565_data.extend(rgb888_to_bgr565(r, g, b))
    
    return bytes(bgr565_data)

def generate_c_file(data):
    """Generate bootscreen_img.c file."""
    # Calculate size
    data_size = len(data)
    
    # Generate C array
    c_content = f"""/*
 * Auto-generated file - DO NOT EDIT
 * Generated from: assets/boot/bootscreen_320x480.png
 * Image size: {EXPECTED_WIDTH}x{EXPECTED_HEIGHT} (BGR565 for ST7796)
 * Data size: {data_size} bytes
 * Note: R and B channels are swapped in pixel data for BGR display
 */

#include "bootscreen_img.h"
#include "lvgl.h"

// BGR565 image data (little-endian, R and B swapped)
LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST
static const uint8_t bootscreen_img_map[] = {{
"""
    
    # Write data as hex bytes (16 bytes per line)
    for i in range(0, data_size, 16):
        line_data = data[i:i+16]
        hex_bytes = ', '.join(f'0x{b:02X}' for b in line_data)
        c_content += f"    {hex_bytes}"
        if i + 16 < data_size:
            c_content += ","
        c_content += "\n"
    
    c_content += f"""}};

const lv_img_dsc_t bootscreen_img = {{
    .header = {{
        .w = {EXPECTED_WIDTH},
        .h = {EXPECTED_HEIGHT},
        .cf = LV_COLOR_FORMAT_RGB565,
    }},
    .data_size = {data_size},
    .data = bootscreen_img_map,
}};
"""
    
    return c_content

def generate_h_file():
    """Generate bootscreen_img.h file."""
    h_content = f"""/*
 * Auto-generated file - DO NOT EDIT
 * Generated from: assets/boot/bootscreen_320x480.png
 */

#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {{
#endif

// Boot screen image (320x480 BGR565 for ST7796)
extern const lv_img_dsc_t bootscreen_img;

#ifdef __cplusplus
}}
#endif
"""
    return h_content

def main():
    """Main conversion function."""
    print(f"Converting {INPUT_PNG} to LVGL RGB565 format...")
    
    # Ensure output directory exists
    GENERATED_DIR.mkdir(parents=True, exist_ok=True)
    
    # Convert PNG to BGR565 (swap R and B for ST7796 BGR display)
    print("  - Loading PNG image...")
    bgr565_data = convert_png_to_rgb565(INPUT_PNG)
    print(f"  - Converted to BGR565: {len(bgr565_data)} bytes")
    
    # Generate C file
    print("  - Generating C file...")
    c_content = generate_c_file(bgr565_data)
    OUTPUT_C.write_text(c_content, encoding='utf-8')
    print(f"  - Written: {OUTPUT_C}")
    
    # Generate H file
    print("  - Generating header file...")
    h_content = generate_h_file()
    OUTPUT_H.write_text(h_content, encoding='utf-8')
    print(f"  - Written: {OUTPUT_H}")
    
    print("Conversion complete!")

if __name__ == "__main__":
    main()

