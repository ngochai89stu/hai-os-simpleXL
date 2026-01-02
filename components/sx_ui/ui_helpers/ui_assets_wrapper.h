#pragma once

/**
 * @file ui_assets_wrapper.h
 * @brief Wrapper để convert sx_assets raw data thành LVGL image descriptors
 * 
 * Tuân theo SIMPLEXL_ARCH v1.3:
 * - sx_assets không được include LVGL
 * - Chỉ sx_ui được phép include LVGL và tạo lv_img_dsc_t
 */

#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)
#include "sx_assets.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get bootscreen image as LVGL descriptor
 * 
 * Wrapper để convert raw data từ sx_assets thành lv_img_dsc_t
 * Chỉ được gọi trong UI task (sx_ui component)
 */
const lv_img_dsc_t* ui_assets_get_bootscreen_img(void);

/**
 * @brief Get flashscreen image as LVGL descriptor
 * 
 * Wrapper để convert raw data từ sx_assets thành lv_img_dsc_t
 * Chỉ được gọi trong UI task (sx_ui component)
 */
const lv_img_dsc_t* ui_assets_get_flashscreen_img(void);

#ifdef __cplusplus
}
#endif






