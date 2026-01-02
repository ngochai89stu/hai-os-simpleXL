#include "ui_slider.h"
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

lv_obj_t *ui_gradient_slider_create(lv_obj_t *parent, lv_event_cb_t cb, void *user_data) {
    if (parent == NULL) {
        return NULL;
    }
    
    lv_obj_t *slider = lv_slider_create(parent);
    if (slider == NULL) {
        return NULL;
    }
    
    // Animation duration
    lv_obj_set_style_anim_duration(slider, 100, 0);
    
    // Slider background
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 3, LV_PART_MAIN);
    
    // Slider indicator (gradient)
    lv_obj_set_style_bg_grad_dir(slider, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x569af8), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(slider, lv_color_hex(0xa666f1), LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, 3, LV_PART_INDICATOR);
    
    // Slider knob
    lv_obj_set_style_pad_all(slider, 20, LV_PART_KNOB);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x5b7fff), LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    
    // Add event callback if provided
    if (cb != NULL) {
        lv_obj_add_event_cb(slider, cb, LV_EVENT_VALUE_CHANGED, user_data);
    }
    
    return slider;
}

