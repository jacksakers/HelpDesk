// Project  : HelpDesk
// File     : task_master.cpp
// Purpose  : TaskMaster data model — SD persist, XP/level/streak gamification,
//            and LVGL row builder for ui_Screen5.
// Depends  : task_master.h, ui_Screen5.h, sd_card.h, buzzer.h, <ArduinoJson.h>

#include "task_master.h"
#include "sd_card.h"
#include "buzzer.h"
#include "ui.h"
#include "ui_Screen5.h"
#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <time.h>

// ── Storage paths ─────────────────────────────────────────────────────────────
#define TASKS_DIR        "/tasks"
#define TASKS_FILE       "/tasks/tasks.json"
#define STATS_FILE       "/tasks/stats.json"

// ── In-memory task store ──────────────────────────────────────────────────────
static task_t    s_tasks[TASK_MAX];
static int       s_task_count = 0;
static uint32_t  s_next_id   = 1;

// ── In-memory XP / gamification state ────────────────────────────────────────
static int  s_daily_xp  = 0;
static int  s_total_xp  = 0;
static int  s_level     = 0;
static int  s_streak    = 0;
static int  s_daily_done = 0;
static char s_last_date[12] = "";   /* "YYYY-MM-DD" of last day with completions */

// ── Colours used by row builder ───────────────────────────────────────────────
#define CLR_ROW_ACTIVE  0x1E2D3D
#define CLR_ROW_DONE    0x0D1F0D
#define CLR_ACCENT      0x4CAF50
#define CLR_SUBTLE      0xAAAAAA
#define CLR_XP_BADGE    0xFFD700
#define CLR_REPEAT_BADGE 0x29B6F6
#define CLR_WHITE       0xFFFFFF
#define CLR_DONE_TXT    0x667766

// ── Helper: today's date string ───────────────────────────────────────────────
static void get_today(char *buf, size_t sz)
{
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    strftime(buf, sz, "%Y-%m-%d", &t);
}

// ── SD: save tasks ─────────────────────────────────────────────────────────────
static void save_tasks(void)
{
    if (!sdCardMounted()) return;

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < s_task_count; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["id"]     = s_tasks[i].id;
        o["text"]   = s_tasks[i].text;
        o["repeat"] = s_tasks[i].repeat;
        o["done"]   = s_tasks[i].done_today;
    }

    File f = SD.open(TASKS_FILE, FILE_WRITE);
    if (!f) { Serial.println("[Tasks] save_tasks: open failed"); return; }
    serializeJson(doc, f);
    f.close();
}

// ── SD: save stats ─────────────────────────────────────────────────────────────
static void save_stats(void)
{
    if (!sdCardMounted()) return;

    JsonDocument doc;
    doc["total_xp"]  = s_total_xp;
    doc["daily_xp"]  = s_daily_xp;
    doc["streak"]    = s_streak;
    doc["last_date"] = s_last_date;
    doc["next_id"]   = s_next_id;

    File f = SD.open(STATS_FILE, FILE_WRITE);
    if (!f) { Serial.println("[Tasks] save_stats: open failed"); return; }
    serializeJson(doc, f);
    f.close();
}

// ── SD: load tasks ─────────────────────────────────────────────────────────────
static void load_tasks(void)
{
    s_task_count = 0;
    if (!sdCardMounted() || !SD.exists(TASKS_FILE)) return;

    File f = SD.open(TASKS_FILE, FILE_READ);
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) { Serial.println("[Tasks] load_tasks: JSON parse error"); return; }

    for (JsonObject o : doc.as<JsonArray>()) {
        if (s_task_count >= TASK_MAX) break;
        task_t &t = s_tasks[s_task_count++];
        t.id         = o["id"]     | 0u;
        t.repeat     = o["repeat"] | false;
        t.done_today = o["done"]   | false;
        const char *txt = o["text"] | "";
        strncpy(t.text, txt, TASK_TEXT_MAX - 1);
        t.text[TASK_TEXT_MAX - 1] = '\0';
    }
    Serial.printf("[Tasks] Loaded %d tasks from SD.\n", s_task_count);
}

// ── SD: load stats ─────────────────────────────────────────────────────────────
static void load_stats(void)
{
    s_total_xp  = 0;
    s_daily_xp  = 0;
    s_streak    = 0;
    s_next_id   = 1;
    s_last_date[0] = '\0';

    if (!sdCardMounted() || !SD.exists(STATS_FILE)) return;

    File f = SD.open(STATS_FILE, FILE_READ);
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) { Serial.println("[Tasks] load_stats: JSON parse error"); return; }

    s_total_xp = doc["total_xp"] | 0;
    s_daily_xp = doc["daily_xp"] | 0;
    s_streak   = doc["streak"]   | 0;
    s_next_id  = doc["next_id"]  | 1u;
    const char *d = doc["last_date"] | "";
    strncpy(s_last_date, d, sizeof(s_last_date) - 1);
    s_last_date[sizeof(s_last_date) - 1] = '\0';
}

// ── Daily reset logic ─────────────────────────────────────────────────────────
static void check_day_rollover(void)
{
    char today[12];
    get_today(today, sizeof(today));

    /* Same day — nothing to do */
    if (strcmp(today, s_last_date) == 0) return;

    Serial.printf("[Tasks] New day: %s (was %s). Performing daily reset.\n",
                  today, s_last_date);

    /* Update streak: was there a completion yesterday? */
    if (s_daily_done > 0) {
        /* We completed tasks on the day stored in last_date; check if that
           was yesterday (streak) or earlier (broken). */
        /* Simple check: if streak was already active and last_date != "today"
           we assume any previous day with done>0 increments the streak.
           A more rigorous check would parse date arithmetic. */
        s_streak++;
    } else if (strlen(s_last_date) > 0) {
        /* Had a previous tracked day but did nothing — streak breaks */
        s_streak = 0;
    }

    /* Reset daily counters */
    s_daily_xp  = 0;
    s_daily_done = 0;
    strncpy(s_last_date, today, sizeof(s_last_date) - 1);
    s_last_date[sizeof(s_last_date) - 1] = '\0';

    /* Repeating tasks: clear done_today so they appear active again.
       Non-repeating done tasks: remove them from the list. */
    int write_idx = 0;
    for (int i = 0; i < s_task_count; i++) {
        if (s_tasks[i].done_today && !s_tasks[i].repeat) {
            /* Purge completed non-repeating tasks */
            continue;
        }
        if (s_tasks[i].done_today && s_tasks[i].repeat) {
            s_tasks[i].done_today = false;
        }
        if (write_idx != i) s_tasks[write_idx] = s_tasks[i];
        write_idx++;
    }
    s_task_count = write_idx;

    save_tasks();
    save_stats();
}

// ── Helpers: recalc derived state ─────────────────────────────────────────────
static void recalc_daily_done(void)
{
    s_daily_done = 0;
    for (int i = 0; i < s_task_count; i++) {
        if (s_tasks[i].done_today) s_daily_done++;
    }
}

static void recalc_level(void)
{
    s_level = s_total_xp / TASK_XP_PER_LEVEL;
}

// ── Public lifecycle ──────────────────────────────────────────────────────────
void taskMasterInit(void)
{
    /* Ensure /tasks directory exists */
    if (sdCardMounted() && !SD.exists(TASKS_DIR)) {
        SD.mkdir(TASKS_DIR);
    }

    load_stats();
    load_tasks();
    recalc_daily_done();
    recalc_level();
    check_day_rollover();   /* May reset daily_xp / remove stale tasks */

    Serial.printf("[Tasks] Init: %d tasks, XP=%d, Lv=%d, streak=%d\n",
                  s_task_count, s_total_xp, s_level, s_streak);
}

// ── Public CRUD ───────────────────────────────────────────────────────────────
bool taskAdd(const char *text, bool repeat)
{
    if (!text || strlen(text) == 0) return false;
    if (s_task_count >= TASK_MAX) return false;

    task_t &t   = s_tasks[s_task_count++];
    t.id         = s_next_id++;
    t.repeat     = repeat;
    t.done_today = false;
    strncpy(t.text, text, TASK_TEXT_MAX - 1);
    t.text[TASK_TEXT_MAX - 1] = '\0';

    save_tasks();
    save_stats();
    taskRefreshScreen();

    Serial.printf("[Tasks] Added: \"%s\" (id=%u, repeat=%d)\n",
                  t.text, t.id, (int)t.repeat);
    return true;
}

bool taskComplete(uint32_t id)
{
    for (int i = 0; i < s_task_count; i++) {
        if (s_tasks[i].id == id) {
            if (s_tasks[i].done_today) return false; /* already done */

            s_tasks[i].done_today = true;
            s_daily_xp  += TASK_XP_VALUE;
            s_total_xp  += TASK_XP_VALUE;
            s_daily_done++;
            recalc_level();

            /* First completion of the day: init last_date if blank */
            if (strlen(s_last_date) == 0) {
                get_today(s_last_date, sizeof(s_last_date));
            }

            save_tasks();
            save_stats();

            buzzerPlay(BUZZ_TONE_SUCCESS);
            taskRefreshScreen();
            ui_Screen5_show_xp_popup(TASK_XP_VALUE);

            Serial.printf("[Tasks] Complete: id=%u  daily_xp=%d  total_xp=%d  lv=%d\n",
                          id, s_daily_xp, s_total_xp, s_level);
            return true;
        }
    }
    return false;
}

bool taskDelete(uint32_t id)
{
    for (int i = 0; i < s_task_count; i++) {
        if (s_tasks[i].id == id) {
            /* Shift remaining tasks left */
            for (int j = i; j < s_task_count - 1; j++) {
                s_tasks[j] = s_tasks[j + 1];
            }
            s_task_count--;
            recalc_daily_done();
            save_tasks();
            taskRefreshScreen();
            return true;
        }
    }
    return false;
}

// ── Public reads ──────────────────────────────────────────────────────────────
int taskGetCount(void)             { return s_task_count; }
const task_t *taskGet(int idx)     { return (idx >= 0 && idx < s_task_count) ? &s_tasks[idx] : nullptr; }
int taskGetDailyDone(void)         { return s_daily_done; }

void taskGetStats(int *out_daily_xp, int *out_total_xp,
                  int *out_level,    int *out_streak)
{
    if (out_daily_xp) *out_daily_xp = s_daily_xp;
    if (out_total_xp) *out_total_xp = s_total_xp;
    if (out_level)    *out_level    = s_level;
    if (out_streak)   *out_streak   = s_streak;
}

// ── taskRefreshScreen — rebuilds LVGL widgets on ui_Screen5 ──────────────────

/* Forward-declared in task_master.h; implemented here.
   Accesses ui_TaskList, ui_TaskXpBar, ui_TaskXpLabel, ui_TaskLevelLabel,
   ui_TaskStreakLabel — all defined in ui_Screen5.c, exposed via ui_Screen5.h. */

/* Event: tapping an active task row completes it. */
static void task_row_complete_ev(lv_event_t *e)
{
    uint32_t id = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    taskComplete(id);
}

/* Event: long-pressing a task row deletes it. */
static void task_row_delete_ev(lv_event_t *e)
{
    uint32_t id = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    taskDelete(id);
}

static void build_task_row(lv_obj_t *parent, const task_t *t)
{
    const bool done = t->done_today;

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 38);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(row, lv_color_hex(done ? CLR_ROW_DONE : CLR_ROW_ACTIVE), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_all(row, 6, 0);
    lv_obj_set_style_margin_bottom(row, 3, 0);

    /* Checkbox icon */
    lv_obj_t *ico = lv_label_create(row);
    lv_label_set_text(ico, done ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(ico, done ? lv_color_hex(CLR_ACCENT) : lv_color_hex(0x556677), 0);
    lv_obj_align(ico, LV_ALIGN_LEFT_MID, 2, 0);

    /* Task text */
    lv_obj_t *lbl = lv_label_create(row);
    /* Cap width so badges on the right don't overlap */
    lv_obj_set_width(lbl, 310);
    lv_label_set_text(lbl, t->text);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(lbl,
        done ? lv_color_hex(CLR_DONE_TXT) : lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 24, 0);

    /* Repeat badge */
    if (t->repeat) {
        lv_obj_t *rep = lv_label_create(row);
        lv_label_set_text(rep, LV_SYMBOL_REFRESH);
        lv_obj_set_style_text_color(rep, lv_color_hex(CLR_REPEAT_BADGE), 0);
        lv_obj_set_style_text_font(rep, &lv_font_montserrat_10, 0);
        lv_obj_align(rep, LV_ALIGN_RIGHT_MID, -32, 0);
    }

    /* XP badge */
    lv_obj_t *xp = lv_label_create(row);
    {
        char xp_buf[8];
        snprintf(xp_buf, sizeof(xp_buf), "+%d", TASK_XP_VALUE);
        lv_label_set_text(xp, xp_buf);
    }
    lv_obj_set_style_text_color(xp, lv_color_hex(done ? 0x445544 : CLR_XP_BADGE), 0);
    lv_obj_set_style_text_font(xp, &lv_font_montserrat_10, 0);
    lv_obj_align(xp, LV_ALIGN_RIGHT_MID, -4, 0);

    /* Interaction: tap to complete (only if not already done) */
    if (!done) {
        void *user_data = (void *)(uintptr_t)t->id;
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x2A3F50),
                                   LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_add_event_cb(row, task_row_complete_ev,
                            LV_EVENT_CLICKED, user_data);
        /* Long-press to delete */
        lv_obj_add_event_cb(row, task_row_delete_ev,
                            LV_EVENT_LONG_PRESSED, user_data);
    }
}

void taskRefreshScreen(void)
{
    if (!ui_TaskList) return;   /* Screen5 not loaded */

    /* Rebuild all task rows */
    lv_obj_clean(ui_TaskList);

    /* Active tasks first */
    for (int i = 0; i < s_task_count; i++) {
        if (!s_tasks[i].done_today) {
            build_task_row(ui_TaskList, &s_tasks[i]);
        }
    }

    /* Completed tasks below with a subtle divider */
    bool has_done = false;
    for (int i = 0; i < s_task_count; i++) {
        if (s_tasks[i].done_today) { has_done = true; break; }
    }
    if (has_done) {
        lv_obj_t *div_lbl = lv_label_create(ui_TaskList);
        lv_label_set_text(div_lbl, "-- completed today --");
        lv_obj_set_style_text_color(div_lbl, lv_color_hex(0x445566), 0);
        lv_obj_set_style_text_font(div_lbl, &lv_font_montserrat_10, 0);
        lv_obj_set_width(div_lbl, lv_pct(100));
        lv_obj_set_style_text_align(div_lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_top(div_lbl, 4, 0);

        for (int i = 0; i < s_task_count; i++) {
            if (s_tasks[i].done_today) {
                build_task_row(ui_TaskList, &s_tasks[i]);
            }
        }
    }

    /* Update XP progress bar */
    if (ui_TaskXpBar) {
        lv_bar_set_value(ui_TaskXpBar,
                         (s_daily_xp > TASK_DAILY_GOAL_XP) ? TASK_DAILY_GOAL_XP : s_daily_xp,
                         LV_ANIM_ON);
    }

    /* XP label: "40 / 50 XP  |  3 done today" */
    if (ui_TaskXpLabel) {
        char buf[48];
        int active = s_task_count - s_daily_done;
        snprintf(buf, sizeof(buf), "%d/%d XP  |  %d done today  |  %d left",
                 s_daily_xp, TASK_DAILY_GOAL_XP, s_daily_done, active);
        lv_label_set_text(ui_TaskXpLabel, buf);
    }

    /* Level label */
    if (ui_TaskLevelLabel) {
        char buf[16];
        snprintf(buf, sizeof(buf), "Lv.%d", s_level);
        lv_label_set_text(ui_TaskLevelLabel, buf);
    }

    /* Streak label */
    if (ui_TaskStreakLabel) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%dd streak", s_streak);
        lv_label_set_text(ui_TaskStreakLabel, buf);
    }
}
