// Project  : HelpDesk
// File     : ui_Screen11.h
// Purpose  : Calendar screen — public interface
// Depends  : ui.h (LVGL 9.x)

#ifndef UI_SCREEN11_H
#define UI_SCREEN11_H

#ifdef __cplusplus
extern "C" {
#endif

void ui_Screen11_screen_init(void);
void ui_Screen11_screen_destroy(void);
extern lv_obj_t * ui_Screen11;

/* Called by calendar.cpp after adding/deleting an event so the
   day view refreshes if Screen11 is currently visible. */
void calendarScreenRefreshDay(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
