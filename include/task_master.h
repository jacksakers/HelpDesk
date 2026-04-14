// Project  : HelpDesk
// File     : task_master.h
// Purpose  : TaskMaster — task CRUD, XP / level / streak gamification, SD persistence
// Depends  : sd_card.h, <ArduinoJson.h>, ui_Screen5.h (for taskRefreshScreen impl)

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Limits / constants ───────────────────────────────────────────────────── */
#define TASK_TEXT_MAX       80   /* Max task title chars (incl. NUL)           */
#define TASK_MAX            32   /* Max tasks held in RAM simultaneously        */
#define TASK_XP_VALUE       10   /* XP awarded per completed task               */
#define TASK_DAILY_GOAL_XP  50   /* Progress-bar full = this many XP in one day */
#define TASK_XP_PER_LEVEL  100   /* Total-XP threshold for each level gain      */

/* ── Data model ───────────────────────────────────────────────────────────── */
typedef struct {
    uint32_t id;
    char     text[TASK_TEXT_MAX];
    bool     repeat;        /* Resets daily instead of being purged on complete */
    bool     done_today;    /* True after the task was completed today           */
} task_t;

/* ── Lifecycle ────────────────────────────────────────────────────────────── */

/* Load tasks + stats from SD; detect new calendar day; perform daily reset.
   Call once in setup() after sdCardInit(). */
void taskMasterInit(void);

/* ── CRUD ─────────────────────────────────────────────────────────────────── */

/* Add a new task.  Returns false when TASK_MAX is reached. */
bool taskAdd(const char *text, bool repeat);

/* Mark a task as done-today, award XP, play success tone, refresh screen.
   Returns false if the id is not found or already done today. */
bool taskComplete(uint32_t id);

/* Remove a task permanently (repeat or not). Returns false if not found. */
bool taskDelete(uint32_t id);

/* ── Reads ────────────────────────────────────────────────────────────────── */

/* Total number of tasks currently in the list (active + done). */
int           taskGetCount(void);

/* Returns a pointer to task[idx], or NULL if out of range.
   Pointer is valid until the next taskAdd / taskDelete / taskMasterInit. */
const task_t *taskGet(int idx);

/* Fills the four out-parameters with current gamification stats. */
void          taskGetStats(int *out_daily_xp, int *out_total_xp,
                           int *out_level,    int *out_streak);

/* How many tasks have been completed today. */
int           taskGetDailyDone(void);

/* ── UI hooks ─────────────────────────────────────────────────────────────── */

/* Rebuild the LVGL task rows and update XP widgets.
   Implemented in task_master.cpp; no-ops when Screen5 is not loaded. */
void taskRefreshScreen(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
