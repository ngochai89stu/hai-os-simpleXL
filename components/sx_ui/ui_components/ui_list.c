#include "ui_list.h"
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)
#include <string.h>

static lv_style_t s_style_scrollbar;
static bool s_style_scrollbar_initialized = false;

static void init_scrollbar_style(void) {
    if (s_style_scrollbar_initialized) {
        return;
    }
    
    lv_style_init(&s_style_scrollbar);
    lv_style_set_width(&s_style_scrollbar, 4);
    lv_style_set_bg_opa(&s_style_scrollbar, LV_OPA_COVER);
    lv_style_set_bg_color(&s_style_scrollbar, lv_color_hex3(0xeee));
    lv_style_set_radius(&s_style_scrollbar, LV_RADIUS_CIRCLE);
    lv_style_set_pad_right(&s_style_scrollbar, 4);
    
    s_style_scrollbar_initialized = true;
}

lv_obj_t *ui_scrollable_list_create(lv_obj_t *parent) {
    if (parent == NULL) {
        return NULL;
    }
    
    init_scrollbar_style();
    
    lv_obj_t *list = lv_obj_create(parent);
    if (list == NULL) {
        return NULL;
    }
    
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
    lv_obj_add_style(list, &s_style_scrollbar, LV_PART_SCROLLBAR);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
    
    return list;
}

lv_obj_t *ui_list_item_two_line_create(
    lv_obj_t *parent,
    const void *icon_src,
    const char *title,
    const char *subtitle,
    const char *extra_text,
    lv_event_cb_t cb,
    void *user_data
) {
    if (parent == NULL || title == NULL) {
        return NULL;
    }
    
    // Grid layout: [Icon] [Title/Subtitle] [Extra]
    static const int32_t grid_cols[] = {
        LV_GRID_CONTENT,  // Icon
        LV_GRID_FR(1),    // Title/Subtitle
        LV_GRID_CONTENT,  // Extra text
        LV_GRID_TEMPLATE_LAST
    };
    static const int32_t grid_rows[] = {
        22,  // Title row
        17,  // Subtitle row
        LV_GRID_TEMPLATE_LAST
    };
    
    // Create button (list item)
    lv_obj_t *item = lv_btn_create(parent);
    lv_obj_set_size(item, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_color(item, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(item, lv_color_hex(0x3a3a3a), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(item, lv_color_hex(0x4c4965), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(item, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(item, 12, LV_PART_MAIN);
    lv_obj_set_style_layout(item, LV_LAYOUT_GRID, LV_PART_MAIN);
    lv_obj_set_style_grid_column_dsc_array(item, grid_cols, LV_PART_MAIN);
    lv_obj_set_style_grid_row_dsc_array(item, grid_rows, LV_PART_MAIN);
    lv_obj_set_style_grid_row_align(item, LV_GRID_ALIGN_CENTER, LV_PART_MAIN);
    
    // Add event callback if provided
    if (cb != NULL) {
        lv_obj_add_event_cb(item, cb, LV_EVENT_CLICKED, user_data);
    }
    
    // Icon (optional)
    if (icon_src != NULL) {
        lv_obj_t *icon = lv_img_create(item);
        lv_img_set_src(icon, icon_src);
        lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 2);
    }
    
    // Title
    lv_obj_t *title_label = lv_label_create(item);
    lv_label_set_text(title_label, title);
#if LV_FONT_MONTSERRAT_16
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);
#else
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
#endif
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_grid_cell(title_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    
    // Subtitle (optional)
    if (subtitle != NULL && strlen(subtitle) > 0) {
        lv_obj_t *subtitle_label = lv_label_create(item);
        lv_label_set_text(subtitle_label, subtitle);
#if LV_FONT_MONTSERRAT_12
        lv_obj_set_style_text_font(subtitle_label, &lv_font_montserrat_12, 0);
#else
        lv_obj_set_style_text_font(subtitle_label, &lv_font_montserrat_14, 0);
#endif
        lv_obj_set_style_text_color(subtitle_label, lv_color_hex(0xb1b0be), 0);
        lv_obj_set_grid_cell(subtitle_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    }
    
    // Extra text (optional, right-aligned)
    if (extra_text != NULL && strlen(extra_text) > 0) {
        lv_obj_t *extra_label = lv_label_create(item);
        lv_label_set_text(extra_label, extra_text);
#if LV_FONT_MONTSERRAT_16
        lv_obj_set_style_text_font(extra_label, &lv_font_montserrat_16, 0);
#else
        lv_obj_set_style_text_font(extra_label, &lv_font_montserrat_14, 0);
#endif
        lv_obj_set_style_text_color(extra_label, lv_color_hex(0xffffff), 0);
        lv_obj_set_grid_cell(extra_label, LV_GRID_ALIGN_END, 2, 1, LV_GRID_ALIGN_CENTER, 0, 2);
    }
    
    return item;
}

