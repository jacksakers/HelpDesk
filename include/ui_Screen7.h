// Project  : HelpDesk
// File     : ui_Screen7.h
// Purpose  : PCMonitor screen — public interface
// Depends  : ui.h (LVGL 9.x)

#ifndef UI_SCREEN7_H
#define UI_SCREEN7_H

#ifdef __cplusplus
extern "C" {
#endif

/* SCREEN: ui_Screen7 — PCMonitor (live PC metrics via UART) */
void ui_Screen7_screen_init(void);
void ui_Screen7_screen_destroy(void);
extern lv_obj_t * ui_Screen7;

/* Widget handles updated by the UART serial reader */
extern lv_obj_t * ui_CpuBar;
extern lv_obj_t * ui_CpuLabel;
extern lv_obj_t * ui_RamBar;
extern lv_obj_t * ui_RamLabel;
extern lv_obj_t * ui_GpuBar;
extern lv_obj_t * ui_GpuLabel;
extern lv_obj_t * ui_PcStatusLabel;
extern lv_obj_t * ui_SerialLogArea;

/* Append a line to the on-screen serial log.
   Safe to call with ui_Screen7 not loaded (no-op when NULL). */
void pcMonitorLogLine(const char * line);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
