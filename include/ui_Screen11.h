// Project  : HelpDesk
// File     : ui_Screen11.h
// Purpose  : DeskChat screen — LoRa group chat (public API)
// Depends  : ui.h (LVGL 9.x), deskchat.h

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/* ── Public widget handles ──────────────────────────────────── */
extern lv_obj_t * ui_Screen11;          /* Screen root                       */
extern lv_obj_t * ui_ChatList;          /* Scrollable message log            */
extern lv_obj_t * ui_ChatStatusLabel;   /* RSSI / radio status in header     */

/* ── Lifecycle ──────────────────────────────────────────────── */
void ui_Screen11_screen_init(void);
void ui_Screen11_screen_destroy(void);

/* ── Called by deskchat module when a message arrives ───────── */
void ui_Screen11_push_message(const char *user, const char *id,
                               const char *msg, int rssi);

#ifdef __cplusplus
} /* extern "C" */
#endif
