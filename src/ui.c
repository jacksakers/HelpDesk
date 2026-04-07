// Project  : HelpDesk
// File     : ui.c
// Purpose  : UI lifecycle — initialise theme and launch the Launcher screen
// Depends  : ui.h (LVGL 9.x)

#include "ui.h"
#include "ui_helpers.h"

///////////////////// SETTINGS VALIDATION ////////////////////
#if LV_COLOR_DEPTH != 16
    #error "LV_COLOR_DEPTH should be 16bit to match SquareLine Studio's settings"
#endif

///////////////////// VARIABLES ////////////////////

/* Shared SquareLine initial-actions object (kept for compatibility) */
lv_obj_t * ui____initial_actions0 = NULL;

///////////////////// LIFECYCLE ////////////////////

void ui_init(void)
{
    lv_display_t * dispp = lv_display_get_default();

    /* Dark theme keeps default widget colours consistent with dark bg */
    lv_theme_t * theme = lv_theme_default_init(
        dispp,
        lv_palette_main(LV_PALETTE_BLUE),
        lv_palette_main(LV_PALETTE_RED),
        true,             /* dark mode */
        LV_FONT_DEFAULT);
    lv_display_set_theme(dispp, theme);

    /* Build and load the Launcher as the first screen */
    ui_Screen1_screen_init();
    ui____initial_actions0 = lv_obj_create(NULL);
    lv_screen_load(ui_Screen1);
}

void ui_destroy(void)
{
    /* All other screens self-destruct via scr_unloaded_delete_cb.
       Only the currently active screen needs an explicit teardown. */
    ui_Screen1_screen_destroy();
    ui_Screen2_screen_destroy();
    ui_Screen3_screen_destroy();
    ui_Screen4_screen_destroy();
    ui_Screen5_screen_destroy();
    ui_Screen6_screen_destroy();
    ui_Screen7_screen_destroy();
    ui_Screen8_screen_destroy();
}
