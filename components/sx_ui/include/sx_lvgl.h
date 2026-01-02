#pragma once

/**
 * @file sx_lvgl.h
 * @brief LVGL include wrapper for sx_ui component
 * 
 * This is the ONLY allowed way to include LVGL headers in the codebase.
 * Section 7.5 of SIMPLEXL_ARCH v1.3 requires this wrapper to enforce
 * compile-time checks that LVGL is only included in sx_ui component.
 * 
 * Usage:
 *   #include "sx_lvgl.h"  // Instead of #include "lvgl.h"
 */

// Compile-time guard: Chỉ cho phép include trong sx_ui component
#if !defined(SX_COMPONENT_SX_UI)
#error "LVGL headers can only be included in sx_ui component. Include sx_lvgl.h only from sx_ui source files. This is enforced by SIMPLEXL_ARCH v1.3 Section 7.5."
#endif

// Include LVGL headers
#include "lvgl.h"
#include "esp_lvgl_port.h"

#ifdef __cplusplus
extern "C" {
#endif

// Include runtime guard (Section 7.3 SIMPLEXL_ARCH v1.3)
#include "sx_lvgl_guard.h"

// Note: Use SX_LVGL_CALL() macro for LVGL calls to enable runtime thread verification

#ifdef __cplusplus
}
#endif

