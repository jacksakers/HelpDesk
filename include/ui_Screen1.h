// Project  : HelpDesk
// File     : ui_Screen1.h
// Purpose  : Launcher screen — public interface
// Depends  : ui.h (LVGL 8.3.11)

#ifndef UI_SCREEN1_H
#define UI_SCREEN1_H

#ifdef __cplusplus
extern "C" {
#endif

/* SCREEN: ui_Screen1 — App launcher / home screen */
void ui_Screen1_screen_init(void);
void ui_Screen1_screen_destroy(void);
extern lv_obj_t * ui_Screen1;

/* Notification tile widget handles — updated by notifications.cpp.
   Both are NULL when Screen1 is not loaded. */
extern lv_obj_t * ui_NotifTile;
extern lv_obj_t * ui_NotifBadge;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif

