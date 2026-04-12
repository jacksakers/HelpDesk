// Project  : HelpDesk
// File     : ui_Screen4.h
// Purpose  : Notifications screen — public interface
// Depends  : ui.h (LVGL 9.x)

#ifndef UI_SCREEN4_H
#define UI_SCREEN4_H

#ifdef __cplusplus
extern "C" {
#endif

/* SCREEN: ui_Screen4 — Notifications (grouped sender list from companion app) */
void ui_Screen4_screen_init(void);
void ui_Screen4_screen_destroy(void);
extern lv_obj_t * ui_Screen4;

/* List widget — rebuilt by notifications.cpp whenever the store changes.
   NULL when Screen4 is not loaded. */
extern lv_obj_t * ui_NotifList;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
