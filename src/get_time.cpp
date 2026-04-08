// Project  : HelpDesk
// File     : get_time.cpp
// Purpose  : NTP time sync — updates DeskDash clock labels each second
// Depends  : get_time.h, ui_Screen2.h, Arduino ESP32 built-in SNTP

#include "get_time.h"
#include "ui_Screen2.h"
#include <Arduino.h>
#include <time.h>

static unsigned long s_last_update_ms = 0;

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
    if (!ui_TimeLabel && !ui_DateLabel) return;

    time_t raw = time(nullptr);
    if (raw < 100000L) {
        // SNTP hasn't delivered a valid time yet
        if (ui_TimeLabel) lv_label_set_text(ui_TimeLabel, "--:--:--");
        if (ui_DateLabel) lv_label_set_text(ui_DateLabel, "Syncing...");
        return;
    }

    struct tm t;
    localtime_r(&raw, &t);

    char time_buf[9];   // HH:MM:SS + NUL
    char date_buf[20];  // Weekday, Mon DD YYYY + NUL
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &t);
    strftime(date_buf, sizeof(date_buf), "%a, %b %d %Y", &t);

    lv_label_set_text(ui_TimeLabel, time_buf);
    lv_label_set_text(ui_DateLabel, date_buf);
}
