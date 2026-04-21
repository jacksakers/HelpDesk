// Project  : HelpDesk
// File     : ui_Screen2.c
// Purpose  : DeskDash screen — clock, weather, and upcoming tasks/events
// Depends  : ui.h (LVGL 9.x), weather.h, task_master.h, calendar.h

#include "ui.h"
#include "weather.h"
#include "task_master.h"
#include "calendar.h"
#include "get_time.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

/* ── Colours ───────────────────────────────────────────────── */
#define CLR_BG          0x1A1A2E
#define CLR_HDR         0x16213E
#define CLR_ACCENT      0x2196F3   /* Blue */
#define CLR_SUBTLE      0xAAAAAA
#define CLR_PANEL       0x16213E
#define CLR_PANEL_ROW   0x1E2A3E
#define CLR_EVENT       0x2196F3   /* Blue — calendar events  */
#define CLR_TASK_DUE    0xFF7043   /* Orange — tasks due today */
#define CLR_EMPTY       0x556677

/* ── Screen dimensions ─────────────────────────────────────── */
#define SCREEN_W     480
#define HDR_H         30

/* ── Public objects ────────────────────────────────────────── */
lv_obj_t * ui_Screen2          = NULL;
lv_obj_t * ui_TimeLabel        = NULL;
lv_obj_t * ui_DateLabel        = NULL;
lv_obj_t * ui_WeatherLabel     = NULL;
lv_obj_t * ui_WeatherCondLabel = NULL;
lv_obj_t * ui_DeskDashPanel    = NULL;

/* ── Event callbacks ───────────────────────────────────────── */
static void back_to_launcher_ev(lv_event_t * e)
{
    _ui_screen_change(&ui_Screen1, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, 300, 0,
                      ui_Screen1_screen_init);
}

/* ── Private helpers ───────────────────────────────────────── */
static void build_header(lv_obj_t * scr)
{
    lv_obj_t * hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, SCREEN_W, HDR_H);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_remove_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(CLR_HDR), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);

    lv_obj_t * back_btn = lv_button_create(hdr);
    lv_obj_set_size(back_btn, 40, HDR_H - 2);
    lv_obj_set_pos(back_btn, 2, 1);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(CLR_HDR), 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x0D1321),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_shadow_width(back_btn, 0, 0);
    lv_obj_set_style_pad_all(back_btn, 0, 0);
    lv_obj_add_event_cb(back_btn, back_to_launcher_ev, LV_EVENT_CLICKED, NULL);

    lv_obj_t * ico = lv_label_create(back_btn);
    lv_label_set_text(ico, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(ico, lv_color_white(), 0);
    lv_obj_center(ico);

    lv_obj_t * title = lv_label_create(hdr);
    lv_label_set_text(title, "DeskDash");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
}

static void build_clock_section(lv_obj_t * scr)
{
    lv_obj_t * time_lbl = lv_label_create(scr);
    lv_label_set_text(time_lbl, "--:-- --");
    lv_obj_set_style_text_color(time_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_48, 0);
    lv_obj_align(time_lbl, LV_ALIGN_TOP_MID, 0, 34);
    ui_TimeLabel = time_lbl;

    lv_obj_t * date_lbl = lv_label_create(scr);
    lv_label_set_text(date_lbl, "----, --- -- ----");
    lv_obj_set_style_text_color(date_lbl, lv_color_hex(CLR_SUBTLE), 0);
    lv_obj_set_style_text_font(date_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(date_lbl, LV_ALIGN_TOP_MID, 0, 92);
    ui_DateLabel = date_lbl;
}

static void build_weather_row(lv_obj_t * scr)
{
    lv_obj_t * wth = lv_obj_create(scr);
    lv_obj_set_size(wth, SCREEN_W - 20, 38);
    lv_obj_align(wth, LV_ALIGN_TOP_MID, 0, 112);
    lv_obj_remove_flag(wth, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(wth, lv_color_hex(CLR_PANEL), 0);
    lv_obj_set_style_bg_opa(wth, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(wth, 0, 0);
    lv_obj_set_style_radius(wth, 8, 0);
    lv_obj_set_style_pad_hor(wth, 10, 0);
    lv_obj_set_style_pad_ver(wth, 4, 0);

    lv_obj_t * temp_lbl = lv_label_create(wth);
    lv_label_set_text(temp_lbl, LV_SYMBOL_TINT "  --");
    lv_obj_set_style_text_color(temp_lbl, lv_color_hex(0x90CAF9), 0);
    lv_obj_set_style_text_font(temp_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(temp_lbl, LV_ALIGN_LEFT_MID, 0, 0);
    ui_WeatherLabel = temp_lbl;

    lv_obj_t * cond_lbl = lv_label_create(wth);
    lv_label_set_text(cond_lbl, "Connecting...");
    lv_obj_set_style_text_color(cond_lbl, lv_color_hex(CLR_SUBTLE), 0);
    lv_obj_set_style_text_font(cond_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(cond_lbl, LV_ALIGN_RIGHT_MID, 0, 0);
    ui_WeatherCondLabel = cond_lbl;
}

static void build_upcoming_section(lv_obj_t * scr)
{
    lv_obj_t * hdr_lbl = lv_label_create(scr);
    lv_label_set_text(hdr_lbl, LV_SYMBOL_LOOP "  UPCOMING");
    lv_obj_set_style_text_color(hdr_lbl, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_text_font(hdr_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(hdr_lbl, LV_ALIGN_TOP_LEFT, 10, 158);

    lv_obj_t * panel = lv_obj_create(scr);
    lv_obj_set_size(panel, SCREEN_W - 20, 148);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 174);
    lv_obj_set_style_bg_color(panel, lv_color_hex(CLR_PANEL), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_pad_all(panel, 4, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    ui_DeskDashPanel = panel;
}

/* ── deskDashRefreshUpcoming ─────────────────────────────────────────────── */
void deskDashRefreshUpcoming(void)
{
    if (!ui_DeskDashPanel) return;
    lv_obj_clean(ui_DeskDashPanel);

    char today[11];
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    strftime(today, sizeof(today), "%Y-%m-%d", &t);

    int total = 0;

    /* ── Calendar events today ── */
    const event_t *events[4];
    int ev_count = calendarGetForDate(today, events, 4);
    for (int i = 0; i < ev_count && total < 4; i++, total++) {
        lv_obj_t * row = lv_obj_create(ui_DeskDashPanel);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 34);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(row, lv_color_hex(CLR_PANEL_ROW), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_pad_all(row, 6, 0);
        lv_obj_set_style_margin_bottom(row, 2, 0);

        lv_obj_t * icon = lv_label_create(row);
        lv_label_set_text(icon, LV_SYMBOL_LOOP);
        lv_obj_set_style_text_color(icon, lv_color_hex(CLR_EVENT), 0);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_12, 0);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t * lbl = lv_label_create(row);
        lv_obj_set_width(lbl, 330);
        lv_label_set_text(lbl, events[i]->title);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 20, 0);

        if (!events[i]->all_day && events[i]->start_time[0]) {
            lv_obj_t * tlbl = lv_label_create(row);
            lv_label_set_text(tlbl, events[i]->start_time);
            lv_obj_set_style_text_color(tlbl, lv_color_hex(CLR_EVENT), 0);
            lv_obj_set_style_text_font(tlbl, &lv_font_montserrat_10, 0);
            lv_obj_align(tlbl, LV_ALIGN_RIGHT_MID, -4, 0);
        }
    }

    /* ── Tasks due today or overdue ── */
    const task_t *tasks[4];
    int task_count = taskGetDueSoon(tasks, 4 - ev_count, today);
    for (int i = 0; i < task_count && total < 4; i++, total++) {
        lv_obj_t * row = lv_obj_create(ui_DeskDashPanel);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 34);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(row, lv_color_hex(CLR_PANEL_ROW), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_pad_all(row, 6, 0);
        lv_obj_set_style_margin_bottom(row, 2, 0);

        lv_obj_t * icon = lv_label_create(row);
        lv_label_set_text(icon, LV_SYMBOL_LIST);
        lv_obj_set_style_text_color(icon, lv_color_hex(CLR_TASK_DUE), 0);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_12, 0);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t * lbl = lv_label_create(row);
        lv_obj_set_width(lbl, 330);
        lv_label_set_text(lbl, tasks[i]->text);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 20, 0);

        lv_obj_t * due = lv_label_create(row);
        lv_label_set_text(due, "DUE");
        lv_obj_set_style_text_color(due, lv_color_hex(CLR_TASK_DUE), 0);
        lv_obj_set_style_text_font(due, &lv_font_montserrat_10, 0);
        lv_obj_align(due, LV_ALIGN_RIGHT_MID, -4, 0);
    }

    /* ── Empty state ── */
    if (total == 0) {
        lv_obj_t * empty = lv_label_create(ui_DeskDashPanel);
        lv_label_set_text(empty,
            LV_SYMBOL_OK "  All clear for today!\n"
            "Add a meeting from the Calendar app.");
        lv_obj_set_style_text_color(empty, lv_color_hex(CLR_EMPTY), 0);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(empty, lv_pct(90));
        lv_obj_center(empty);
    }
}

/* ── Public lifecycle ──────────────────────────────────────── */
void ui_Screen2_screen_init(void)
{
    ui_Screen2 = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_Screen2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen2, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(ui_Screen2, LV_OPA_COVER, 0);

    lv_obj_add_event_cb(ui_Screen2, scr_unloaded_delete_cb,
                        LV_EVENT_SCREEN_UNLOADED, ui_Screen2_screen_destroy);

    build_header(ui_Screen2);
    build_clock_section(ui_Screen2);
    build_weather_row(ui_Screen2);
    build_upcoming_section(ui_Screen2);

    /* Apply cached weather immediately — don't wait for next fetch */
    weatherApplyToScreen();
    /* Populate upcoming panel with current data */
    deskDashRefreshUpcoming();
}

void ui_Screen2_screen_destroy(void)
{
    ui_TimeLabel        = NULL;
    ui_DateLabel        = NULL;
    ui_WeatherLabel     = NULL;
    ui_WeatherCondLabel = NULL;
    ui_DeskDashPanel    = NULL;

    if(ui_Screen2) lv_obj_delete(ui_Screen2);
    ui_Screen2 = NULL;
}
