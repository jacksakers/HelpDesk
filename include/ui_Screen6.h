// Project  : HelpDesk
// File     : ui_Screen6.h
// Purpose  : ZenFrame screen — public interface
// Depends  : ui.h (LVGL 8.3.11)

#ifndef UI_SCREEN6_H
#define UI_SCREEN6_H

#ifdef __cplusplus
extern "C" {
#endif

/* SCREEN: ui_Screen6 — ZenFrame (digital photo frame) */
void ui_Screen6_screen_init(void);
void ui_Screen6_screen_destroy(void);
extern lv_obj_t * ui_Screen6;

/* Widget handles for the image and the no-image placeholder */
extern lv_obj_t * ui_ZenImage;
extern lv_obj_t * ui_ZenPlaceholder;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
