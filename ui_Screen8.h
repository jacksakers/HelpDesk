// Project  : HelpDesk
// File     : ui_Screen8.h
// Purpose  : GameBreak screen — public interface
// Depends  : ui.h (LVGL 8.3.11)

#ifndef UI_SCREEN8_H
#define UI_SCREEN8_H

#ifdef __cplusplus
extern "C" {
#endif

/* SCREEN: ui_Screen8 — GameBreak (mini game selector) */
void ui_Screen8_screen_init(void);
void ui_Screen8_screen_destroy(void);
extern lv_obj_t * ui_Screen8;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
