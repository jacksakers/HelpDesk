// Project  : HelpDesk
// File     : ui.h
// Purpose  : Top-level UI header — includes LVGL, helpers, and all screen headers
// Depends  : lvgl.h, ui_helpers.h, ui_events.h

#ifndef _HELPDESK_UI_H
#define _HELPDESK_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined __has_include
#  if __has_include("lvgl.h")
#    include "lvgl.h"
#  elif __has_include("lvgl/lvgl.h")
#    include "lvgl/lvgl.h"
#  else
#    include "lvgl.h"
#  endif
#else
#  include "lvgl.h"
#endif

#include "ui_helpers.h"
#include "ui_events.h"

/* ── All screen headers ────────────────────────────────────── */
#include "ui_Screen1.h"   /* Launcher         */
#include "ui_Screen2.h"   /* DeskDash         */
#include "ui_Screen3.h"   /* Tomatimer        */
#include "ui_Screen4.h"   /* JukeDesk         */
#include "ui_Screen5.h"   /* TaskMaster       */
#include "ui_Screen6.h"   /* ZenFrame         */
#include "ui_Screen7.h"   /* PCMonitor        */
#include "ui_Screen8.h"   /* GameBreak        */
#include "ui_Screen9.h"   /* Settings         */

/* ── Shared LVGL event object (SquareLine pattern) ─────────── */
extern lv_obj_t * ui____initial_actions0;

/* ── UI lifecycle ──────────────────────────────────────────── */
void ui_init(void);
void ui_destroy(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
