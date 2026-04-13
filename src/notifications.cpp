// Project  : HelpDesk
// File     : notifications.cpp
// Purpose  : Notification store — groups incoming notifications by sender, drives
//            the launcher tile flash badge, and refreshes the Screen4 list widget
// Depends  : notifications.h, ui_Screen1.h, ui_Screen4.h, buzzer.h

#include "notifications.h"
#include "ui.h"
#include "ui_Screen1.h"
#include "ui_Screen4.h"
#include "buzzer.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

// ── Constants ────────────────────────────────────────────────────────────────
#define GROUP_MAX            12     // Max distinct (title, app) senders tracked
#define FLASH_INTERVAL_MS   500UL   // Tile flicker period
#define CLR_NOTIF_NORMAL   0xFF5722 // Deep-orange (matches launcher tile normal)
#define CLR_NOTIF_FLASH    0xE53935 // Bright red (flash state)

// ── Data ─────────────────────────────────────────────────────────────────────
typedef struct {
    char title[64];
    char app[32];
    int  count;
} notif_group_t;

static notif_group_t s_groups[GROUP_MAX];
static int           s_group_count  = 0;
static int           s_unread_count = 0;

// ── Flash state ───────────────────────────────────────────────────────────────
static unsigned long s_flash_ms = 0;
static bool          s_flash_hi = false;

// ── Private helpers ───────────────────────────────────────────────────────────

/* Find the group index for (title, app). Returns -1 if not found. */
static int find_group(const char * title, const char * app)
{
    for (int i = 0; i < s_group_count; i++) {
        if (strncmp(s_groups[i].title, title, sizeof(s_groups[i].title) - 1) == 0 &&
            strncmp(s_groups[i].app,   app,   sizeof(s_groups[i].app)   - 1) == 0) {
            return i;
        }
    }
    return -1;
}

/* Update (or hide) the red badge label on the launcher tile. */
static void update_launcher_badge(void)
{
    if (!ui_NotifBadge) return;
    if (s_unread_count == 0) {
        lv_obj_add_flag(ui_NotifBadge, LV_OBJ_FLAG_HIDDEN);
        /* Reset tile to normal colour when all read */
        if (ui_NotifTile) {
            lv_obj_set_style_bg_color(ui_NotifTile,
                                      lv_color_hex(CLR_NOTIF_NORMAL), 0);
        }
    } else {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", s_unread_count);
        lv_label_set_text(ui_NotifBadge, buf);
        lv_obj_remove_flag(ui_NotifBadge, LV_OBJ_FLAG_HIDDEN);
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void notifInit(void)
{
    s_group_count  = 0;
    s_unread_count = 0;
    s_flash_ms     = 0;
    s_flash_hi     = false;
}

void notifAdd(const char * app, const char * title, const char * body)
{
    (void)body;  /* Body stored only in companion; ESP32 just displays grouped senders */

    int idx = find_group(title, app);
    if (idx >= 0) {
        s_groups[idx].count++;
    } else if (s_group_count < GROUP_MAX) {
        idx = s_group_count++;
        strncpy(s_groups[idx].title, title, sizeof(s_groups[idx].title) - 1);
        s_groups[idx].title[sizeof(s_groups[idx].title) - 1] = '\0';
        strncpy(s_groups[idx].app, app, sizeof(s_groups[idx].app) - 1);
        s_groups[idx].app[sizeof(s_groups[idx].app) - 1] = '\0';
        s_groups[idx].count = 1;
    } else {
        /* Group buffer full — add to the last slot's count as a catch-all */
        s_groups[GROUP_MAX - 1].count++;
    }

    s_unread_count++;
    update_launcher_badge();
    notifRefreshScreen();

    /* Short rising tone to signal a new notification */
    buzzerPlay(BUZZ_TONE_NAVIGATE);
}

int notifGetUnreadCount(void)
{
    return s_unread_count;
}

int notifGetGroupCount(void)
{
    return s_group_count;
}

void notifMarkAllRead(void)
{
    s_unread_count = 0;
    update_launcher_badge();
}

void notifClearAll(void)
{
    s_group_count  = 0;
    s_unread_count = 0;
    update_launcher_badge();
    notifRefreshScreen();
}

void notifRefreshScreen(void)
{
    if (!ui_NotifList) return;

    /* Wipe existing rows */
    lv_obj_clean(ui_NotifList);

    if (s_group_count == 0) {
        lv_obj_t * empty = lv_label_create(ui_NotifList);
        lv_label_set_text(empty, "No notifications yet.");
        lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
        lv_obj_center(empty);
        return;
    }

    for (int i = 0; i < s_group_count; i++) {
        char rowbuf[96];
        snprintf(rowbuf, sizeof(rowbuf), "%s  x%d  \u2022  %s",
                 s_groups[i].title,
                 s_groups[i].count,
                 s_groups[i].app);
        lv_obj_t * btn = lv_list_add_button(ui_NotifList,
                                             LV_SYMBOL_BELL, rowbuf);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E1E38), 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_text_color(
            lv_obj_get_child(btn, 1),   /* text label inside the list button */
            lv_color_white(), 0);
    }
}

void handleNotifications(unsigned long now_ms)
{
    if (s_unread_count == 0 || !ui_NotifTile) return;

    if (now_ms - s_flash_ms >= FLASH_INTERVAL_MS) {
        s_flash_ms = now_ms;
        s_flash_hi = !s_flash_hi;
        lv_color_t c = s_flash_hi
                       ? lv_color_hex(CLR_NOTIF_FLASH)
                       : lv_color_hex(CLR_NOTIF_NORMAL);
        lv_obj_set_style_bg_color(ui_NotifTile, c, 0);
    }
}
