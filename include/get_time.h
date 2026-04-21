// Project  : HelpDesk
// File     : get_time.h
// Purpose  : NTP time sync — public interface
// Depends  : ESP32 Arduino built-in SNTP (no external library)

#pragma once

#include "ui.h"   /* lv_obj_t */

#ifdef __cplusplus
extern "C" {
#endif

// ─── Configuration ────────────────────────────────────────────────────────────
// Set your POSIX TZ string before flashing.
// Examples:
//   US Eastern  : "EST5EDT,M3.2.0,M11.1.0"
//   US Central  : "CST6CDT,M3.2.0,M11.1.0"
//   US Mountain : "MST7MDT,M3.2.0,M11.1.0"
//   US Pacific  : "PST8PDT,M3.2.0,M11.1.0"
//   UTC         : "UTC0"
//   Beijing     : "CST-8"
//   London      : "GMT0BST,M3.5.0/1,M10.5.0"
#ifndef HELPDESK_TZ
  #define HELPDESK_TZ "EST5EDT,M3.2.0,M11.1.0"
#endif

// ─── API ─────────────────────────────────────────────────────────────────────
// Call once in setup(), after WiFi connects.
void initNTP(void);

// Call every loop() iteration. Updates ui_TimeLabel / ui_DateLabel
// (on Screen2) and ui_ActiveClockLabel (header mini-clock on any other screen).
void handleTimeUpdate(unsigned long now_ms);

// ─── Mini clock shared across screens ────────────────────────────────────────
// Pointer to the currently visible header clock label (NULL when no screen
// with a mini-clock is active).  Set by uiAddHeaderClock().
extern lv_obj_t * ui_ActiveClockLabel;

// Creates a small time label inside `header`, right-aligned with the given
// pixel offset from the right edge (pass a negative value, e.g. -8).
// Also sets ui_ActiveClockLabel to the new label.
void uiAddHeaderClock(lv_obj_t * header, int right_offset);

#ifdef __cplusplus
} /* extern "C" */
#endif
