#pragma once

#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file ui_theme_tokens.h
 * @brief Design system tokens for SimpleXL OS
 * 
 * This header provides standardized color, font, and spacing tokens
 * to ensure UI consistency across all screens.
 */

// ============================================================================
// COLOR TOKENS
// ============================================================================

// Background colors
#define UI_COLOR_BG_PRIMARY      lv_color_hex(0x1a1a1a)  // Main screen background
#define UI_COLOR_BG_SECONDARY    lv_color_hex(0x2a2a2a)  // Card background, button background (unpressed)
#define UI_COLOR_BG_PRESSED      lv_color_hex(0x3a3a3a)  // Button pressed state, list item pressed
#define UI_COLOR_BG_DISABLED     lv_color_hex(0x1a1a1a)  // Disabled controls (use with opacity 50%)

// Primary/Accent colors
#define UI_COLOR_PRIMARY         lv_color_hex(0x5b7fff)  // Primary button, active slider indicator
#define UI_COLOR_PRIMARY_DARK    lv_color_hex(0x4a6fff)  // Primary button pressed state
#define UI_COLOR_ACCENT          lv_color_hex(0xa666f1)  // Gradient end color, secondary accent

// Text colors
#define UI_COLOR_TEXT_PRIMARY    lv_color_hex(0xFFFFFF)  // Main text, title, important labels
#define UI_COLOR_TEXT_SECONDARY  lv_color_hex(0x888888)  // Secondary text, artist, time total, hint text
#define UI_COLOR_TEXT_ACCENT     lv_color_hex(0x8a86b8)  // Genre, metadata, special info
#define UI_COLOR_TEXT_ERROR      lv_color_hex(0xFF0000)  // Error messages

// Spectrum/Visualization colors (for charts, graphs, spectrum)
#define UI_COLOR_SPECTRUM_LOW    lv_color_hex(0xe9dbfc)  // Low frequency band
#define UI_COLOR_SPECTRUM_MID    lv_color_hex(0x6f8af6)  // Mid frequency band
#define UI_COLOR_SPECTRUM_HIGH   lv_color_hex(0xffffff)  // High frequency band

// ============================================================================
// FONT TOKENS
// ============================================================================

// Font size tokens (with fallback)
#if LV_FONT_MONTSERRAT_12
    #define UI_FONT_SMALL        (&lv_font_montserrat_12)
#else
    #define UI_FONT_SMALL        (&lv_font_montserrat_14)
#endif

#define UI_FONT_MEDIUM           (&lv_font_montserrat_14)  // Default font

#if LV_FONT_MONTSERRAT_16
    #define UI_FONT_LARGE        (&lv_font_montserrat_16)
#else
    #define UI_FONT_LARGE        (&lv_font_montserrat_14)
#endif

#if LV_FONT_MONTSERRAT_22
    #define UI_FONT_XLARGE       (&lv_font_montserrat_22)
#else
    #define UI_FONT_XLARGE       (&lv_font_montserrat_14)
#endif

// ============================================================================
// SPACING TOKENS
// ============================================================================

#define UI_SPACE_XS              4   // Tight spacing, icon padding
#define UI_SPACE_SM              8   // Small gap between related items
#define UI_SPACE_MD              12  // Medium gap, list item padding
#define UI_SPACE_LG              16  // Large gap, section spacing
#define UI_SPACE_XL              20  // Extra large, container padding
#define UI_SPACE_XXL             24  // Very large, major section separation

// ============================================================================
// SIZE TOKENS
// ============================================================================

#define UI_SIZE_BUTTON_HEIGHT    60  // Standard button height
#define UI_SIZE_SLIDER_HEIGHT    6   // Progress slider height
#define UI_SIZE_SLIDER_HEIGHT_THICK 24  // Volume slider height (thicker for easier interaction)
#define UI_SIZE_ALBUM_ART        220 // Large image/icon size (220x220)
#define UI_SIZE_ICON_SMALL       24  // Small icon
#define UI_SIZE_ICON_MEDIUM      48  // Medium icon
#define UI_SIZE_ICON_LARGE       64  // Large icon

// ============================================================================
// ANIMATION TOKENS
// ============================================================================

#define UI_ANIM_DURATION_FAST    200   // Quick feedback (button press)
#define UI_ANIM_DURATION_NORMAL  500   // Standard transition (fade, slide)
#define UI_ANIM_DURATION_SLOW    800   // Intro animation, major transition

#ifdef __cplusplus
}
#endif

