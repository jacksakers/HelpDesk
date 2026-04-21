// Project  : HelpDesk
// File     : ui_Screen2.h
// Purpose  : DeskDash screen — public interface
// Depends  : ui.h (LVGL 8.3.11)

#ifndef UI_SCREEN2_H
#define UI_SCREEN2_H

#ifdef __cplusplus
extern "C" {
#endif

/* SCREEN: ui_Screen2 — DeskDash (clock + weather ambient display) */
void ui_Screen2_screen_init(void);
void ui_Screen2_screen_destroy(void);
extern lv_obj_t * ui_Screen2;

/* Widget handles accessed by get_Time.cpp and weather.cpp */
extern lv_obj_t * ui_TimeLabel;
extern lv_obj_t * ui_DateLabel;
extern lv_obj_t * ui_WeatherLabel;
extern lv_obj_t * ui_WeatherCondLabel;

/* Upcoming-events panel (calendar events + tasks due today).
   Container is owned by Screen2; individual rows are rebuilt by
   deskDashRefreshUpcoming() each minute and on screen open. */
extern lv_obj_t * ui_DeskDashPanel;

/* Rebuild the upcoming-events rows inside ui_DeskDashPanel.
   Safe to call when Screen2 is not active (ui_DeskDashPanel will be NULL). */
void deskDashRefreshUpcoming(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
