// Project  : HelpDesk
// File     : calendar.cpp
// Purpose  : Calendar — event storage, SD persistence, and query functions
// Depends  : calendar.h, sd_card.h, <ArduinoJson.h>, <SD.h>

#include "calendar.h"
#include "sd_card.h"
#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>

/* ── Storage paths ───────────────────────────────────────────────────────── */
#define CALENDAR_DIR   "/calendar"
#define CALENDAR_FILE  "/calendar/events.json"

/* ── In-memory event store ───────────────────────────────────────────────── */
static event_t   s_events[EVENT_MAX];
static int       s_event_count = 0;
static uint32_t  s_next_id     = 1;

/* ── SD: save ────────────────────────────────────────────────────────────── */
static void save_events(void)
{
    if (!sdCardMounted()) return;

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < s_event_count; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["id"]         = s_events[i].id;
        o["title"]      = s_events[i].title;
        o["date"]       = s_events[i].date;
        o["start_time"] = s_events[i].start_time;
        o["end_time"]   = s_events[i].end_time;
        o["all_day"]    = s_events[i].all_day;
    }
    /* Persist next_id so IDs remain unique across reboots */
    doc["_next_id"] = s_next_id;

    File f = SD.open(CALENDAR_FILE, FILE_WRITE);
    if (!f) { Serial.println("[Cal] save: open failed"); return; }
    serializeJson(doc, f);
    f.close();
}

/* ── SD: load ────────────────────────────────────────────────────────────── */
static void load_events(void)
{
    s_event_count = 0;
    if (!sdCardMounted() || !SD.exists(CALENDAR_FILE)) return;

    File f = SD.open(CALENDAR_FILE, FILE_READ);
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) { Serial.println("[Cal] load: JSON parse error"); return; }

    s_next_id = doc["_next_id"] | 1u;

    for (JsonObject o : doc.as<JsonArray>()) {
        if (s_event_count >= EVENT_MAX) break;
        event_t &e = s_events[s_event_count++];
        e.id      = o["id"]      | 0u;
        e.all_day = o["all_day"] | false;

        auto copy_str = [](const char *src, char *dst, size_t sz) {
            if (!src) { dst[0] = '\0'; return; }
            strncpy(dst, src, sz - 1);
            dst[sz - 1] = '\0';
        };
        copy_str(o["title"]      | "", e.title,      EVENT_TITLE_MAX);
        copy_str(o["date"]       | "", e.date,        EVENT_DATE_LEN);
        copy_str(o["start_time"] | "", e.start_time,  EVENT_TIME_LEN);
        copy_str(o["end_time"]   | "", e.end_time,    EVENT_TIME_LEN);
    }
    Serial.printf("[Cal] Loaded %d events.\n", s_event_count);
}

/* ── Comparator for sorting by date then start_time ─────────────────────── */
static int event_cmp(const void *a, const void *b)
{
    const event_t *ea = (const event_t *)a;
    const event_t *eb = (const event_t *)b;
    int dc = strcmp(ea->date, eb->date);
    if (dc != 0) return dc;
    /* All-day events sort before timed events */
    if (ea->all_day && !eb->all_day) return -1;
    if (!ea->all_day && eb->all_day) return 1;
    return strcmp(ea->start_time, eb->start_time);
}

/* ── Public lifecycle ────────────────────────────────────────────────────── */
void calendarInit(void)
{
    if (sdCardMounted() && !SD.exists(CALENDAR_DIR)) {
        SD.mkdir(CALENDAR_DIR);
    }
    load_events();
    /* Sort on load so queries are always ordered */
    if (s_event_count > 1) {
        qsort(s_events, s_event_count, sizeof(event_t), event_cmp);
    }
}

/* ── Public CRUD ─────────────────────────────────────────────────────────── */
bool calendarAddEvent(const char *title, const char *date,
                      const char *start_time, const char *end_time,
                      bool all_day)
{
    if (!title || title[0] == '\0') return false;
    if (!date  || date[0]  == '\0') return false;
    if (s_event_count >= EVENT_MAX) return false;

    event_t &e = s_events[s_event_count++];
    e.id      = s_next_id++;
    e.all_day = all_day;
    strncpy(e.title, title, EVENT_TITLE_MAX - 1);
    e.title[EVENT_TITLE_MAX - 1] = '\0';
    strncpy(e.date, date, EVENT_DATE_LEN - 1);
    e.date[EVENT_DATE_LEN - 1] = '\0';

    if (start_time && start_time[0]) {
        strncpy(e.start_time, start_time, EVENT_TIME_LEN - 1);
        e.start_time[EVENT_TIME_LEN - 1] = '\0';
    } else {
        e.start_time[0] = '\0';
    }
    if (end_time && end_time[0]) {
        strncpy(e.end_time, end_time, EVENT_TIME_LEN - 1);
        e.end_time[EVENT_TIME_LEN - 1] = '\0';
    } else {
        e.end_time[0] = '\0';
    }

    /* Re-sort so new event lands in date+time order */
    if (s_event_count > 1) {
        qsort(s_events, s_event_count, sizeof(event_t), event_cmp);
    }
    save_events();
    Serial.printf("[Cal] Added: \"%s\" on %s (id=%u)\n", e.title, e.date, e.id);
    return true;
}

bool calendarDeleteEvent(uint32_t id)
{
    for (int i = 0; i < s_event_count; i++) {
        if (s_events[i].id == id) {
            for (int j = i; j < s_event_count - 1; j++) {
                s_events[j] = s_events[j + 1];
            }
            s_event_count--;
            save_events();
            return true;
        }
    }
    return false;
}

/* ── Public reads ────────────────────────────────────────────────────────── */
int             calendarGetCount(void) { return s_event_count; }
const event_t * calendarGet(int idx)
{
    return (idx >= 0 && idx < s_event_count) ? &s_events[idx] : nullptr;
}

int calendarGetForDate(const char *date_str,
                       const event_t **out, int max_count)
{
    int found = 0;
    for (int i = 0; i < s_event_count && found < max_count; i++) {
        if (strcmp(s_events[i].date, date_str) == 0) {
            out[found++] = &s_events[i];
        }
    }
    return found;
}

int calendarGetUpcoming(const char *today_str,
                        const event_t **out, int max_count)
{
    int found = 0;
    for (int i = 0; i < s_event_count && found < max_count; i++) {
        if (strcmp(s_events[i].date, today_str) >= 0) {
            out[found++] = &s_events[i];
        }
    }
    return found;
}
