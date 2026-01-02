/**
 * @file screen_music_player_spectrum.c
 * @brief Spectrum visualization for music player screen
 * Phase 2: Core Features - Spectrum Visualization
 */

#include "screen_music_player_spectrum.h"
#include "sx_audio_service.h"
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)
#include <esp_log.h>

// Spectrum constants (from LVGL Demo)
#define BAR_CNT             20
#define DEG_STEP            (180/BAR_CNT)
#define BAND_CNT            4
#define BAR_PER_BAND_CNT    (BAR_CNT / BAND_CNT)
#define BAR_COLOR1          lv_color_hex(0xe9dbfc)
#define BAR_COLOR2          lv_color_hex(0x6f8af6)
#define BAR_COLOR3          lv_color_hex(0xffffff)
#define BAR_COLOR1_STOP     80
#define BAR_COLOR2_STOP     100
#define BAR_COLOR3_STOP     (2 * LV_HOR_RES / 3)

// Static variables
static uint32_t s_spectrum_i = 0;
static uint32_t s_bar_ofs = 0;
static uint32_t s_spectrum_lane_ofs_start = 0;
static uint32_t s_bar_rot = 0;
static bool s_start_anim = false;
static int32_t s_start_anim_values[40];
static const uint16_t s_rnd_array[30] = {994, 285, 553, 11, 792, 707, 966, 641, 852, 827, 44, 352, 146, 581, 490, 80, 729, 58, 695, 940, 724, 561, 124, 653, 27, 292, 557, 506, 382, 199};

// External reference to album art (set by music player screen)
extern lv_obj_t *g_album_art_obj;

// Helper functions
static int32_t get_cos(int32_t deg, int32_t a) {
    int32_t r = (lv_trigo_cos(deg) * a);
    r += LV_TRIGO_SIN_MAX / 2;
    return r >> LV_TRIGO_SHIFT;
}

static int32_t get_sin(int32_t deg, int32_t a) {
    int32_t r = lv_trigo_sin(deg) * a;
    return (r + LV_TRIGO_SIN_MAX / 2) >> LV_TRIGO_SHIFT;
}

// Spectrum draw event callback
void spectrum_draw_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
        lv_event_set_ext_draw_size(e, LV_VER_RES);
    }
    else if(code == LV_EVENT_COVER_CHECK) {
        lv_event_set_cover_res(e, LV_COVER_RES_NOT_COVER);
    }
    else if(code == LV_EVENT_DRAW_MAIN_BEGIN) {
        lv_obj_t *obj = lv_event_get_target(e);
        lv_layer_t *layer = lv_event_get_layer(e);

        lv_opa_t opa = lv_obj_get_style_opa_recursive(obj, LV_PART_MAIN);
        if(opa < LV_OPA_MIN) return;

        lv_point_t center;
        center.x = obj->coords.x1 + lv_obj_get_width(obj) / 2;
        center.y = obj->coords.y1 + lv_obj_get_height(obj) / 2;

        lv_draw_triangle_dsc_t draw_dsc;
        lv_draw_triangle_dsc_init(&draw_dsc);
        draw_dsc.bg_opa = LV_OPA_COVER;

        uint16_t r[64];
        uint32_t i;

        int32_t min_a = 5;
        int32_t r_in = 1;
        if(g_album_art_obj != NULL) {
            // Get scale from album art (if it's an image)
            // For now, use fixed value
            r_in = 80;
        }
        for(i = 0; i < BAR_CNT; i++) r[i] = r_in + min_a + 77;

        // Get spectrum data from audio service
        uint16_t bands[4] = {0, 0, 0, 0};
        esp_err_t ret = sx_audio_get_spectrum(bands, 4);
        if(ret != ESP_OK) {
            // Use default values if spectrum not available
            bands[0] = 10;
            bands[1] = 8;
            bands[2] = 6;
            bands[3] = 4;
        }

        uint32_t s;
        for(s = 0; s < 4; s++) {
            uint32_t f;
            uint32_t band_w = 0;
            switch(s) {
                case 0: band_w = 20; break;
                case 1: band_w = 8; break;
                case 2: band_w = 4; break;
                case 3: band_w = 2; break;
            }

            // Add "side bars" with cosine characteristic
            for(f = 0; f < band_w; f++) {
                uint32_t ampl_main = bands[s];  // Use audio service data
                int32_t ampl_mod = get_cos(f * 360 / band_w + 180, 180) + 180;
                int32_t t = BAR_PER_BAND_CNT * s - band_w / 2 + f;
                if(t < 0) t = BAR_CNT + t;
                if(t >= BAR_CNT) t = t - BAR_CNT;
                r[t] += (ampl_main * ampl_mod) >> 9;
            }
        }

        const int32_t amax = 20;
        int32_t animv = s_spectrum_i - s_spectrum_lane_ofs_start;
        if(animv > amax) animv = amax;
        for(i = 0; i < BAR_CNT; i++) {
            uint32_t deg_space = 1;
            uint32_t deg = i * DEG_STEP + 90;
            uint32_t j = (i + s_bar_rot + s_rnd_array[s_bar_ofs % 10]) % BAR_CNT;
            uint32_t k = (i + s_bar_rot + s_rnd_array[(s_bar_ofs + 1) % 10]) % BAR_CNT;

            uint32_t v = (r[k] * animv + r[j] * (amax - animv)) / amax;
            if(s_start_anim) {
                v = r_in + s_start_anim_values[i];
                deg_space = v >> 7;
                if(deg_space < 1) deg_space = 1;
            }

            if(v < BAR_COLOR1_STOP) draw_dsc.bg_color = BAR_COLOR1;
            else if(v > (uint32_t)BAR_COLOR3_STOP) draw_dsc.bg_color = BAR_COLOR3;
            else if(v > BAR_COLOR2_STOP) draw_dsc.bg_color = lv_color_mix(BAR_COLOR3, BAR_COLOR2,
                                                                              ((v - BAR_COLOR2_STOP) * 255) / (BAR_COLOR3_STOP - BAR_COLOR2_STOP));
            else draw_dsc.bg_color = lv_color_mix(BAR_COLOR2, BAR_COLOR1,
                                                      ((v - BAR_COLOR1_STOP) * 255) / (BAR_COLOR2_STOP - BAR_COLOR1_STOP));

            uint32_t di = deg + deg_space;

            int32_t x1_out = get_cos(di, v);
            draw_dsc.p[0].x = center.x + x1_out;
            draw_dsc.p[0].y = center.y + get_sin(di, v);

            di += DEG_STEP - deg_space * 2;

            int32_t x2_out = get_cos(di, v);
            draw_dsc.p[1].x = center.x + x2_out;
            draw_dsc.p[1].y = center.y + get_sin(di, v);

            int32_t x2_in = get_cos(di, r_in);
            draw_dsc.p[2].x = center.x + x2_in;
            draw_dsc.p[2].y = center.y + get_sin(di, r_in);

            lv_draw_triangle(layer, &draw_dsc);

            draw_dsc.p[0].x = center.x - x1_out;
            draw_dsc.p[1].x = center.x - x2_out;
            draw_dsc.p[2].x = center.x - x2_in;
            lv_draw_triangle(layer, &draw_dsc);
        }
    }
}

// Spectrum animation callback
void spectrum_anim_cb(void *a, int32_t v) {
    lv_obj_t *obj = (lv_obj_t *)a;
    if(s_start_anim) {
        lv_obj_invalidate(obj);
        return;
    }

    s_spectrum_i = v;
    lv_obj_invalidate(obj);

    static uint32_t bass_cnt = 0;
    static int32_t last_bass = -1000;
    static int32_t dir = 1;
    
    // Get spectrum data for bass detection
    uint16_t bands[4] = {0, 0, 0, 0};
    sx_audio_get_spectrum(bands, 4);
    
    if(bands[0] > 12) {  // Bass threshold
        if(s_spectrum_i - last_bass > 5) {
            bass_cnt++;
            last_bass = s_spectrum_i;
            if(bass_cnt >= 2) {
                bass_cnt = 0;
                s_spectrum_lane_ofs_start = s_spectrum_i;
                s_bar_ofs++;
            }
        }
    }
    if(bands[0] < 4) s_bar_rot += dir;

    // Phase 3: Sync album art scale with bass
    // Note: g_album_art_obj is an object container, not an image
    // Scale sync is handled in the main screen if album art is an image
    // For now, we'll skip this as album art is currently an object with icon
}

// Reset spectrum state
void spectrum_reset(void) {
    s_spectrum_i = 0;
    s_bar_ofs = 0;
    s_spectrum_lane_ofs_start = 0;
    s_bar_rot = 0;
    s_start_anim = false;
}

