// Project  : HelpDesk
// File     : ui_Screen3.h
// Purpose  : Tomatimer screen — public interface
// Depends  : ui.h (LVGL 8.3.11)

#ifndef UI_SCREEN3_H
#define UI_SCREEN3_H

#ifdef __cplusplus
extern "C" {
#endif

/* SCREEN: ui_Screen3 — Tomatimer (Pomodoro productivity timer) */
void ui_Screen3_screen_init(void);
void ui_Screen3_screen_destroy(void);
extern lv_obj_t * ui_Screen3;

/* Widget handles for the timer logic module */
extern lv_obj_t * ui_PomoArc;
extern lv_obj_t * ui_PomoTimeLabel;
extern lv_obj_t * ui_PomoPhaseLabel;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
