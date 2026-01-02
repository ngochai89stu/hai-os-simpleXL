#include "ui_animations.h"

#include <esp_log.h>
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

static const char *TAG = "ui_anim";

esp_err_t ui_anim_apply(lv_obj_t *obj, const ui_anim_config_t *config) {
    if (obj == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_time(&a, config->duration_ms);
    lv_anim_set_delay(&a, config->delay_ms);
    
    if (config->path_cb != NULL) {
        lv_anim_set_path_cb(&a, config->path_cb);
    } else {
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    }
    
    switch (config->type) {
        case UI_ANIM_FADE_IN:
            lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
#pragma GCC diagnostic pop
            break;
            
        case UI_ANIM_FADE_OUT:
            lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
#pragma GCC diagnostic pop
            break;
            
        case UI_ANIM_SLIDE_IN_LEFT: {
            int32_t start_x = -lv_obj_get_width(obj);
            lv_anim_set_values(&a, start_x, 0);
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
            break;
        }
        
        case UI_ANIM_SLIDE_IN_RIGHT: {
            int32_t start_x = lv_obj_get_width(lv_obj_get_parent(obj));
            lv_anim_set_values(&a, start_x, 0);
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
            break;
        }
        
        case UI_ANIM_SLIDE_IN_TOP: {
            int32_t start_y = -lv_obj_get_height(obj);
            lv_anim_set_values(&a, start_y, 0);
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
            break;
        }
        
        case UI_ANIM_SLIDE_IN_BOTTOM: {
            int32_t start_y = lv_obj_get_height(lv_obj_get_parent(obj));
            lv_anim_set_values(&a, start_y, 0);
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
            break;
        }
        
        case UI_ANIM_SCALE_IN:
            // LVGL zoom: 256 = 100% (no zoom), 0 = 0%
            lv_anim_set_values(&a, 0, 256);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_transform_zoom);
#pragma GCC diagnostic pop
            break;
            
        case UI_ANIM_SCALE_OUT:
            // LVGL zoom: 256 = 100% (no zoom), 0 = 0%
            lv_anim_set_values(&a, 256, 0);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_transform_zoom);
#pragma GCC diagnostic pop
            break;
            
        default:
            ESP_LOGW(TAG, "Unsupported animation type: %d", config->type);
            return ESP_ERR_NOT_SUPPORTED;
    }
    
    lv_anim_start(&a);
    return ESP_OK;
}

esp_err_t ui_anim_fade_in(lv_obj_t *obj, uint32_t duration_ms) {
    ui_anim_config_t config = {
        .type = UI_ANIM_FADE_IN,
        .duration_ms = duration_ms,
        .delay_ms = 0,
        .path_cb = lv_anim_path_ease_in_out,
    };
    return ui_anim_apply(obj, &config);
}

esp_err_t ui_anim_fade_out(lv_obj_t *obj, uint32_t duration_ms) {
    ui_anim_config_t config = {
        .type = UI_ANIM_FADE_OUT,
        .duration_ms = duration_ms,
        .delay_ms = 0,
        .path_cb = lv_anim_path_ease_in_out,
    };
    return ui_anim_apply(obj, &config);
}

esp_err_t ui_anim_slide_in_left(lv_obj_t *obj, uint32_t duration_ms) {
    ui_anim_config_t config = {
        .type = UI_ANIM_SLIDE_IN_LEFT,
        .duration_ms = duration_ms,
        .delay_ms = 0,
        .path_cb = lv_anim_path_ease_in_out,
    };
    return ui_anim_apply(obj, &config);
}

esp_err_t ui_anim_slide_in_right(lv_obj_t *obj, uint32_t duration_ms) {
    ui_anim_config_t config = {
        .type = UI_ANIM_SLIDE_IN_RIGHT,
        .duration_ms = duration_ms,
        .delay_ms = 0,
        .path_cb = lv_anim_path_ease_in_out,
    };
    return ui_anim_apply(obj, &config);
}

esp_err_t ui_anim_scale_in(lv_obj_t *obj, uint32_t duration_ms) {
    ui_anim_config_t config = {
        .type = UI_ANIM_SCALE_IN,
        .duration_ms = duration_ms,
        .delay_ms = 0,
        .path_cb = lv_anim_path_ease_in_out,
    };
    return ui_anim_apply(obj, &config);
}

esp_err_t ui_anim_screen_transition(lv_obj_t *old_screen, lv_obj_t *new_screen, ui_anim_type_t transition_type) {
    if (new_screen == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Apply transition animation to new screen
    ui_anim_config_t config = {
        .type = transition_type,
        .duration_ms = 300,
        .delay_ms = 0,
        .path_cb = lv_anim_path_ease_in_out,
    };
    
    esp_err_t ret = ui_anim_apply(new_screen, &config);
    
    // Fade out old screen if provided
    if (old_screen != NULL && ret == ESP_OK) {
        ui_anim_fade_out(old_screen, 300);
    }
    
    return ret;
}



