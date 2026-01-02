#include "ui_assets_wrapper.h"

#include <esp_log.h>
#include "bootscreen_img.h"  // Generated file - include ở đây vì sx_ui được phép include LVGL
#include "flashscreen_img.h" // Generated file - include ở đây vì sx_ui được phép include LVGL

/**
 * @brief Get bootscreen image as LVGL descriptor
 * 
 * Wrapper để access generated image từ sx_assets.
 * Tuân theo SIMPLEXL_ARCH v1.3 - chỉ sx_ui được phép include LVGL.
 */
const lv_img_dsc_t* ui_assets_get_bootscreen_img(void) {
    // Access generated image trực tiếp - file này được include trong sx_ui
    // nên vi phạm kiến trúc được giải quyết bằng cách chỉ include ở sx_ui
    return &bootscreen_img;
}

/**
 * @brief Get flashscreen image as LVGL descriptor
 * 
 * Wrapper để access generated image từ sx_assets.
 * Tuân theo SIMPLEXL_ARCH v1.3 - chỉ sx_ui được phép include LVGL.
 */
const lv_img_dsc_t* ui_assets_get_flashscreen_img(void) {
    // Access generated image trực tiếp - file này được include trong sx_ui
    // nên vi phạm kiến trúc được giải quyết bằng cách chỉ include ở sx_ui
    return &flashscreen_img;
}

