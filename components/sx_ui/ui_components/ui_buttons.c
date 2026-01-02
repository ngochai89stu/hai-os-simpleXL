#include "ui_buttons.h"
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)
#include <stdbool.h>

lv_obj_t *ui_image_button_create(lv_obj_t *parent, const void *img_src, lv_event_cb_t cb, void *user_data) {
    if (parent == NULL) {
        return NULL;
    }
    
    lv_obj_t *btn = lv_img_create(parent);
    if (btn == NULL) {
        return NULL;
    }
    
    // Set image source
    if (img_src != NULL) {
        lv_img_set_src(btn, img_src);
    }
    
    // Make it clickable
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    
    // Add event callback if provided
    if (cb != NULL) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }
    
    return btn;
}

void ui_image_button_set_src(lv_obj_t *btn, const void *img_src) {
    if (btn == NULL) {
        return;
    }
    
    lv_img_set_src(btn, img_src);
}

lv_obj_t *ui_checkable_image_button_create(lv_obj_t *parent, const void *released_src, const void *checked_src, lv_event_cb_t cb, void *user_data) {
    if (parent == NULL) {
        return NULL;
    }
    
    lv_obj_t *btn = lv_imagebutton_create(parent);
    if (btn == NULL) {
        return NULL;
    }
    
    // Set image sources (LVGL 9.1.0 API: src_left, src_mid, src_right)
    if (released_src != NULL) {
        lv_imagebutton_set_src(btn, LV_IMAGEBUTTON_STATE_RELEASED, NULL, released_src, NULL);
        lv_imagebutton_set_src(btn, LV_IMAGEBUTTON_STATE_PRESSED, NULL, released_src, NULL);
    }
    
    if (checked_src != NULL) {
        lv_imagebutton_set_src(btn, LV_IMAGEBUTTON_STATE_CHECKED_RELEASED, NULL, checked_src, NULL);
        lv_imagebutton_set_src(btn, LV_IMAGEBUTTON_STATE_CHECKED_PRESSED, NULL, checked_src, NULL);
    }
    
    // Make it checkable
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
    
    // Add event callback if provided
    if (cb != NULL) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }
    
    return btn;
}

void ui_checkable_image_button_set_checked(lv_obj_t *btn, bool checked) {
    if (btn == NULL) {
        return;
    }
    
    if (checked) {
        lv_obj_add_state(btn, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(btn, LV_STATE_CHECKED);
    }
}

