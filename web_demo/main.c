/*
 * Thin wrapper to expose LVGL screen functions to JS.
 * We will include existing LVGL project files containing
 * boot screen, home screen, music player, settings, etc.
 * Each ui_show_* function will ensure the screen is loaded.
 */

#include "lvgl.h"

extern void simplexl_boot_screen_create(void);
extern void simplexl_home_screen_create(void);
extern void simplexl_music_screen_create(void);
extern void simplexl_settings_screen_create(void);

static lv_obj_t *current_screen = NULL;

static void load_screen(lv_obj_t *scr)
{
    if(!scr) return;
    lv_scr_load(scr);
    current_screen = scr;
}

// Exposed functions
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define EXPORTED EMSCRIPTEN_KEEPALIVE
#else
#define EXPORTED
#endif

lv_obj_t *boot_scr = NULL;
EXPORTED void ui_show_boot(void)
{
    if(!boot_scr) {
        boot_scr = lv_obj_create(NULL); // placeholder, replace by real create
        simplexl_boot_screen_create();
    }
    load_screen(boot_scr);
}

lv_obj_t *home_scr = NULL;
EXPORTED void ui_show_home(void)
{
    if(!home_scr) {
        home_scr = lv_obj_create(NULL);
        simplexl_home_screen_create();
    }
    load_screen(home_scr);
}

lv_obj_t *music_scr = NULL;
EXPORTED void ui_show_music(void)
{
    if(!music_scr) {
        music_scr = lv_obj_create(NULL);
        simplexl_music_screen_create();
    }
    load_screen(music_scr);
}

lv_obj_t *settings_scr = NULL;
EXPORTED void ui_show_settings(void)
{
    if(!settings_scr) {
        settings_scr = lv_obj_create(NULL);
        simplexl_settings_screen_create();
    }
    load_screen(settings_scr);
}

// Main
int main(void)
{
    lv_init();
    /* Emscripten specific: init display and input drivers provided by LVGL port for SDL or similar */
    /* TODO: call lv_port_init(); stub for now */

    ui_show_boot();

    while (1) {
        lv_timer_handler();
        /* In WASM, we can yield to browser */
    }
    return 0;
}
