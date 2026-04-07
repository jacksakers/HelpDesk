// Project  : HelpDesk
// File     : ui_Screen4.h
// Purpose  : JukeDesk screen — public interface
// Depends  : ui.h (LVGL 8.3.11)

#ifndef UI_SCREEN4_H
#define UI_SCREEN4_H

#ifdef __cplusplus
extern "C" {
#endif

/* SCREEN: ui_Screen4 — JukeDesk (local MP3 player) */
void ui_Screen4_screen_init(void);
void ui_Screen4_screen_destroy(void);
extern lv_obj_t * ui_Screen4;

/* Widget handles for the music player module */
extern lv_obj_t * ui_SongLabel;
extern lv_obj_t * ui_PlayPauseLabel;
extern lv_obj_t * ui_MusicStatusLabel;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
