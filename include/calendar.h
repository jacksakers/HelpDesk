// Project  : HelpDesk
// File     : calendar.h
// Purpose  : Calendar — event data model, SD persistence, and query API
// Depends  : sd_card.h, <ArduinoJson.h>

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Limits ───────────────────────────────────────────────────────────────── */
#define EVENT_TITLE_MAX   80
#define EVENT_DATE_LEN    11   /* "YYYY-MM-DD" + NUL */
#define EVENT_TIME_LEN     6   /* "HH:MM" + NUL      */
#define EVENT_MAX         64

/* ── Data model ───────────────────────────────────────────────────────────── */
typedef struct {
    uint32_t id;
    char     title[EVENT_TITLE_MAX];
    char     date[EVENT_DATE_LEN];        /* "YYYY-MM-DD"                  */
    char     start_time[EVENT_TIME_LEN];  /* "HH:MM" or "" if all-day      */
    char     end_time[EVENT_TIME_LEN];    /* "HH:MM" or "" if open-ended   */
    bool     all_day;
} event_t;

/* ── Lifecycle ────────────────────────────────────────────────────────────── */
void calendarInit(void);

/* ── CRUD ─────────────────────────────────────────────────────────────────── */

/* Add a new event.  Returns false when EVENT_MAX is reached or title is empty.
   start_time / end_time may be NULL or "" for all-day events. */
bool calendarAddEvent(const char *title, const char *date,
                      const char *start_time, const char *end_time,
                      bool all_day);

/* Delete event by id.  Returns false if not found. */
bool calendarDeleteEvent(uint32_t id);

/* ── Reads ────────────────────────────────────────────────────────────────── */
int             calendarGetCount(void);
const event_t * calendarGet(int idx);

/* Fills out[] with pointers to events on date_str ("YYYY-MM-DD"),
   sorted by start_time ascending.  Returns number of entries written. */
int calendarGetForDate(const char *date_str,
                       const event_t **out, int max_count);

/* Returns up to max_count events from today onward (sorted date+time). */
int calendarGetUpcoming(const char *today_str,
                        const event_t **out, int max_count);

#ifdef __cplusplus
} /* extern "C" */
#endif
