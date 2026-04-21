// Project  : HelpDesk
// File     : get_time.cpp
// Purpose  : NTP time sync — updates DeskDash clock labels each second
// Depends  : get_time.h, ui_Screen2.h, Arduino ESP32 built-in SNTP

#include "get_time.h"
#include "ui.h"
#include <Arduino.h>
#include <time.h>
#include <lvgl.h>

static unsigned long s_last_update_ms = 0;

// ─── Mini-clock label ────────────────────────────────────────────────────────
lv_obj_t * ui_ActiveClockLabel = NULL;
static lv_obj_t * s_clock_owner = NULL;  /* screen that owns the active clock */

void uiAddHeaderClock(lv_obj_t * header, int right_offset)
{
    lv_obj_t * clk = lv_label_create(header);

    /* Show current time immediately — no waiting for next 1-s tick */
    time_t raw = time(nullptr);
    if (raw > 100000L) {
        struct tm t;
        localtime_r(&raw, &t);
        char buf[12];
        strftime(buf, sizeof(buf), "%I:%M %p", &t);
        lv_label_set_text(clk, buf);
    } else {
        lv_label_set_text(clk, "--:-- --");
    }

    lv_obj_set_style_text_color(clk, lv_color_hex(0xBBBBCC), 0);
    lv_obj_set_style_text_font(clk, &lv_font_montserrat_12, 0);
    lv_obj_align(clk, LV_ALIGN_RIGHT_MID, right_offset, 0);

    /* header is a direct child of the screen object */
    s_clock_owner      = lv_obj_get_parent(header);
    ui_ActiveClockLabel = clk;
}

void uiClearHeaderClock(lv_obj_t * screen)
{
    /* Only clear if this screen is still the owner.
       If the new screen already called uiAddHeaderClock() first, s_clock_owner
       will already belong to the new screen, so we leave it alone. */
    if (s_clock_owner == screen) {
        ui_ActiveClockLabel = NULL;
        s_clock_owner       = NULL;
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────

void initNTP(void)
{
    // configTzTime sets the POSIX TZ string and starts the SNTP client.
    // Change HELPDESK_TZ in get_time.h (or add -DHELPDESK_TZ=... in build_flags).
    configTzTime(HELPDESK_TZ, "pool.ntp.org", "time.nist.gov");
    Serial.printf("[NTP] SNTP started. TZ = %s\n", HELPDESK_TZ);
}

void handleTimeUpdate(unsigned long now_ms)
{
    // Throttle to once per second
    if (now_ms - s_last_update_ms < 1000UL) return;
    s_last_update_ms = now_ms;

    // Screen2 widgets are null when DeskDash isn't active — skip silently
    if (!ui_TimeLabel && !ui_DateLabel && !ui_ActiveClockLabel) return;

    time_t raw = time(nullptr);
    if (raw < 100000L) {
        // SNTP hasn't delivered a valid time yet
        if (ui_TimeLabel)         lv_label_set_text(ui_TimeLabel,        "--:--");
        if (ui_DateLabel)         lv_label_set_text(ui_DateLabel,        "Syncing...");
        if (ui_ActiveClockLabel)  lv_label_set_text(ui_ActiveClockLabel, "--:-- --");
        return;
    }

    struct tm t;
    localtime_r(&raw, &t);

    char time_buf[12];  // "12:59 PM" + NUL
    char date_buf[20];  // Weekday, Mon DD YYYY + NUL
    strftime(time_buf, sizeof(time_buf), "%I:%M %p", &t);
    strftime(date_buf, sizeof(date_buf), "%a, %b %d %Y", &t);

    if (ui_TimeLabel)        lv_label_set_text(ui_TimeLabel, time_buf);
    if (ui_DateLabel)        lv_label_set_text(ui_DateLabel, date_buf);
    if (ui_ActiveClockLabel) lv_label_set_text(ui_ActiveClockLabel, time_buf);

    /* Every 60 s, refresh DeskDash upcoming-events panel */
    static unsigned long s_last_upcoming_ms = 0;
    if (ui_DeskDashPanel &&
        (now_ms - s_last_upcoming_ms >= 60000UL || s_last_upcoming_ms == 0)) {
        s_last_upcoming_ms = now_ms;
        deskDashRefreshUpcoming();
    }
}
