// Project  : HelpDesk
// File     : ui_Screen5.h
// Purpose  : TaskMaster screen — public interface
// Depends  : ui.h (LVGL 8.3.11)

#ifndef UI_SCREEN5_H
#define UI_SCREEN5_H

#ifdef __cplusplus
extern "C" {
#endif

/* SCREEN: ui_Screen5 — TaskMaster (to-do list) */
void ui_Screen5_screen_init(void);
void ui_Screen5_screen_destroy(void);
extern lv_obj_t * ui_Screen5;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
