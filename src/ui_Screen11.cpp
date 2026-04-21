// Project  : HelpDesk
// File     : ui_Screen11.cpp
// Purpose  : Calendar screen — week selector, day event list, add-event overlay
// Depends  : ui.h, calendar.h, get_time.h (LVGL 9.x)

#define _GNU_SOURCE   /* strptime */
#include "ui.h"
#include "calendar.h"
#include "get_time.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

/* ── Colours ───────────────────────────────────────────────── */
#define CLR_BG          0x1A1A2E
#define CLR_HDR         0x16213E
#define CLR_ACCENT      0xAB47BC   /* Purple — Calendar identity */
#define CLR_DAY_ACTIVE  0xAB47BC
#define CLR_DAY_NORMAL  0x1E2D3E
#define CLR_DAY_TODAY   0x2E3D4E
#define CLR_PANEL       0x16213E
#define CLR_ROW         0x1E2A3E
#define CLR_SUBTLE      0xAAAAAA
#define CLR_EVENT_ICO   0xAB47BC
#define CLR_ALL_DAY     0x4CAF50
#define CLR_OVERLAY     0x0A0F1E

/* ── Dimensions ────────────────────────────────────────────── */
#define SCREEN_W     480
#define SCREEN_H     320
#define HDR_H         30
#define WEEK_H        40
#define DATE_LBL_H    22
#define LIST_Y        (HDR_H + WEEK_H + DATE_LBL_H)
#define LIST_H        (SCREEN_H - LIST_Y - 34)   /* 34 px for Add button */
#define ADD_BTN_H     30

/* ── Days-of-week names ────────────────────────────────────── */
static const char * const k_day_names[7] = {
    "MON","TUE","WED","THU","FRI","SAT","SUN"
};

/* ── Private state ─────────────────────────────────────────── */
static int      s_selected_dow = 0;     /* 0=Mon … 6=Sun of current week */
static char     s_week_dates[7][11];    /* "YYYY-MM-DD" for each day      */
static char     s_today_str[11];

/* Widget references */
lv_obj_t * ui_Screen11       = NULL;
static lv_obj_t * s_day_btns[7];
static lv_obj_t * s_date_label    = NULL;
static lv_obj_t * s_event_list    = NULL;
static lv_obj_t * s_kbd_overlay   = NULL;
static lv_obj_t * s_title_ta      = NULL;
static lv_obj_t * s_kbd           = NULL;
static lv_obj_t * s_hour_roller   = NULL;
static lv_obj_t * s_min_roller    = NULL;

/* ── Helper: compute week dates for today ──────────────────── */
static void compute_week(void)
{
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    strftime(s_today_str, sizeof(s_today_str), "%Y-%m-%d", &t);

    /* Move back to Monday (tm_wday: 0=Sun, 1=Mon … 6=Sat) */
    int offset = (t.tm_wday == 0) ? 6 : (t.tm_wday - 1);
    time_t mon = now - (time_t)offset * 86400;

    for (int i = 0; i < 7; i++) {
        time_t day_t = mon + (time_t)i * 86400;
        struct tm day_tm;
        localtime_r(&day_t, &day_tm);
        strftime(s_week_dates[i], sizeof(s_week_dates[0]), "%Y-%m-%d", &day_tm);
    }

    /* Select today's column */
    s_selected_dow = offset;
}

/* ── Helper: style a day button as selected / normal / today ─ */
static void style_day_btn(int dow)
{
    if (!s_day_btns[dow]) return;
    bool is_selected = (dow == s_selected_dow);
    bool is_today    = (strcmp(s_week_dates[dow], s_today_str) == 0);
    uint32_t col = is_selected ? CLR_DAY_ACTIVE
                               : (is_today ? CLR_DAY_TODAY : CLR_DAY_NORMAL);
    lv_obj_set_style_bg_color(s_day_btns[dow], lv_color_hex(col), 0);
}

/* ── Refresh event list for selected day ───────────────────── */
void calendarScreenRefreshDay(void)
{
    if (!s_event_list) return;
    lv_obj_clean(s_event_list);

    const event_t * evs[32];
    int count = calendarGetForDate(s_week_dates[s_selected_dow], evs, 32);

    /* Update date label */
    if (s_date_label) {
        char buf[32];
        /* Parse back the date string to a nice display form */
        struct tm t = {0};
        strptime(s_week_dates[s_selected_dow], "%Y-%m-%d", &t);
        strftime(buf, sizeof(buf), "%A, %b %d", &t);
        lv_label_set_text(s_date_label, buf);
    }

    if (count == 0) {
        lv_obj_t * empty = lv_label_create(s_event_list);
        lv_label_set_text(empty, "No events.\nTap \"+\" to add one.");
        lv_obj_set_style_text_color(empty, lv_color_hex(CLR_SUBTLE), 0);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(empty, lv_pct(90));
        lv_obj_center(empty);
        return;
    }

    for (int i = 0; i < count; i++) {
        const event_t * ev = evs[i];

        lv_obj_t * row = lv_obj_create(s_event_list);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 38);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(row, lv_color_hex(CLR_ROW), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_pad_all(row, 6, 0);
        lv_obj_set_style_margin_bottom(row, 3, 0);

        lv_obj_t * icon = lv_label_create(row);
        lv_label_set_text(icon, ev->all_day ? LV_SYMBOL_LOOP : LV_SYMBOL_BELL);
        lv_obj_set_style_text_color(icon,
            ev->all_day ? lv_color_hex(CLR_ALL_DAY) : lv_color_hex(CLR_EVENT_ICO), 0);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_12, 0);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t * title = lv_label_create(row);
        lv_obj_set_width(title, 340);
        lv_label_set_text(title, ev->title);
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
        lv_obj_align(title, LV_ALIGN_LEFT_MID, 20, 0);

        if (!ev->all_day && ev->start_time[0]) {
            char time_buf[14];
            if (ev->end_time[0])
                snprintf(time_buf, sizeof(time_buf), "%s-%s", ev->start_time, ev->end_time);
            else
                snprintf(time_buf, sizeof(time_buf), "%s", ev->start_time);
            lv_obj_t * tlbl = lv_label_create(row);
            lv_label_set_text(tlbl, time_buf);
            lv_obj_set_style_text_color(tlbl, lv_color_hex(CLR_EVENT_ICO), 0);
            lv_obj_set_style_text_font(tlbl, &lv_font_montserrat_10, 0);
            lv_obj_align(tlbl, LV_ALIGN_RIGHT_MID, -4, 0);
        } else if (ev->all_day) {
            lv_obj_t * tlbl = lv_label_create(row);
            lv_label_set_text(tlbl, "All day");
            lv_obj_set_style_text_color(tlbl, lv_color_hex(CLR_ALL_DAY), 0);
            lv_obj_set_style_text_font(tlbl, &lv_font_montserrat_10, 0);
            lv_obj_align(tlbl, LV_ALIGN_RIGHT_MID, -4, 0);
        }

        /* Long-press to delete */
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, [](lv_event_t *e){
            uint32_t id = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
            calendarDeleteEvent(id);
            calendarScreenRefreshDay();
        }, LV_EVENT_LONG_PRESSED, (void *)(uintptr_t)ev->id);
    }
}

/* ── Event callbacks ───────────────────────────────────────── */
static void back_btn_ev(lv_event_t * e)
{
    (void)e;
    _ui_screen_change(&ui_Screen1, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, 300, 0,
                      ui_Screen1_screen_init);
}

static void day_btn_ev(lv_event_t * e)
{
    int dow = (int)(intptr_t)lv_event_get_user_data(e);
    style_day_btn(s_selected_dow);   /* de-select old */
    s_selected_dow = dow;
    style_day_btn(s_selected_dow);   /* select new    */
    calendarScreenRefreshDay();
}

static void hide_kbd_overlay(void)
{
    if (s_kbd_overlay) lv_obj_add_flag(s_kbd_overlay, LV_OBJ_FLAG_HIDDEN);
    if (s_title_ta)    lv_textarea_set_text(s_title_ta, "");
}

static void confirm_add_ev(lv_event_t * e)
{
    (void)e;
    if (!s_title_ta) return;
    const char *title = lv_textarea_get_text(s_title_ta);
    if (!title || strlen(title) == 0) { hide_kbd_overlay(); return; }

    char start_buf[6] = "";
    if (s_hour_roller && s_min_roller) {
        uint16_t h = lv_roller_get_selected(s_hour_roller);
        uint16_t m = lv_roller_get_selected(s_min_roller) * 5;   /* 0,5,10... */
        snprintf(start_buf, sizeof(start_buf), "%02u:%02u", h, m);
    }

    calendarAddEvent(title, s_week_dates[s_selected_dow], start_buf, "", false);
    calendarScreenRefreshDay();
    hide_kbd_overlay();
}

static void cancel_add_ev(lv_event_t * e) { (void)e; hide_kbd_overlay(); }

static void add_btn_ev(lv_event_t * e)
{
    (void)e;
    if (!s_kbd_overlay) return;
    lv_obj_remove_flag(s_kbd_overlay, LV_OBJ_FLAG_HIDDEN);
    if (s_title_ta) {
        lv_textarea_set_text(s_title_ta, "");
        if (s_kbd) lv_keyboard_set_textarea(s_kbd, s_title_ta);
    }
}

/* ── Build helpers ─────────────────────────────────────────── */
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
    lv_obj_add_event_cb(back_btn, back_btn_ev, LV_EVENT_CLICKED, NULL);

    lv_obj_t * ico = lv_label_create(back_btn);
    lv_label_set_text(ico, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(ico, lv_color_white(), 0);
    lv_obj_center(ico);

    lv_obj_t * title = lv_label_create(hdr);
    lv_label_set_text(title, "Calendar");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    uiAddHeaderClock(hdr, -8);
}

static void build_week_bar(lv_obj_t * scr)
{
    /* 7 equal-width day buttons spanning the full screen width */
    const int BTN_W = SCREEN_W / 7;

    for (int i = 0; i < 7; i++) {
        lv_obj_t * btn = lv_obj_create(scr);
        lv_obj_set_size(btn, BTN_W, WEEK_H);
        lv_obj_set_pos(btn, i * BTN_W, HDR_H);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(btn, lv_color_hex(CLR_DAY_NORMAL), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 2, 0);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(btn, day_btn_ev, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
        s_day_btns[i] = btn;

        /* Day abbreviation */
        lv_obj_t * day_lbl = lv_label_create(btn);
        lv_label_set_text(day_lbl, k_day_names[i]);
        lv_obj_set_style_text_color(day_lbl, lv_color_white(), 0);
        lv_obj_set_style_text_font(day_lbl, &lv_font_montserrat_10, 0);
        lv_obj_align(day_lbl, LV_ALIGN_TOP_MID, 0, 2);

        /* Day number (extract from date string "YYYY-MM-DD" → DD) */
        char dd[3] = "??";
        if (s_week_dates[i][8]) {
            dd[0] = s_week_dates[i][8];
            dd[1] = s_week_dates[i][9];
            dd[2] = '\0';
        }
        lv_obj_t * num_lbl = lv_label_create(btn);
        lv_label_set_text(num_lbl, dd);
        lv_obj_set_style_text_color(num_lbl, lv_color_white(), 0);
        lv_obj_set_style_text_font(num_lbl, &lv_font_montserrat_14, 0);
        lv_obj_align(num_lbl, LV_ALIGN_BOTTOM_MID, 0, -2);

        style_day_btn(i);   /* apply correct colour for today / selected */
    }
}

static void build_day_view(lv_obj_t * scr)
{
    /* Selected date label */
    s_date_label = lv_label_create(scr);
    lv_label_set_text(s_date_label, "");
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(CLR_SUBTLE), 0);
    lv_obj_set_style_text_font(s_date_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(s_date_label, 8, HDR_H + WEEK_H + 4);

    /* Scrollable event list */
    s_event_list = lv_obj_create(scr);
    lv_obj_set_size(s_event_list, SCREEN_W - 10, LIST_H);
    lv_obj_set_pos(s_event_list, 5, LIST_Y);
    lv_obj_set_style_bg_color(s_event_list, lv_color_hex(CLR_PANEL), 0);
    lv_obj_set_style_bg_opa(s_event_list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_event_list, 0, 0);
    lv_obj_set_style_radius(s_event_list, 6, 0);
    lv_obj_set_style_pad_all(s_event_list, 4, 0);
    lv_obj_set_flex_flow(s_event_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_event_list, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
}

static void build_add_button(lv_obj_t * scr)
{
    lv_obj_t * btn = lv_button_create(scr);
    lv_obj_set_size(btn, 140, ADD_BTN_H);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -5, -2);
    lv_obj_set_style_bg_color(btn, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x7B1FA2),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, add_btn_ev, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_PLUS " Add Event");
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);
}

static void build_add_overlay(lv_obj_t * scr)
{
    /* Full-screen dark overlay — hidden by default */
    lv_obj_t * overlay = lv_obj_create(scr);
    lv_obj_set_size(overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(CLR_OVERLAY), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_90, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    s_kbd_overlay = overlay;

    /* Title textarea */
    lv_obj_t * ta = lv_textarea_create(overlay);
    lv_obj_set_size(ta, SCREEN_W - 20, 36);
    lv_obj_set_pos(ta, 10, 8);
    lv_textarea_set_placeholder_text(ta, "Event title...");
    lv_textarea_set_one_line(ta, true);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x1E2D3D), 0);
    lv_obj_set_style_text_color(ta, lv_color_white(), 0);
    s_title_ta = ta;

    /* Hour roller */
    lv_obj_t * hour_r = lv_roller_create(overlay);
    lv_roller_set_options(hour_r,
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11"
        "\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23",
        LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(hour_r, 2);
    lv_obj_set_size(hour_r, 54, 60);
    lv_obj_set_pos(hour_r, 10, 52);
    lv_obj_set_style_bg_color(hour_r, lv_color_hex(0x1E2D3D), 0);
    lv_obj_set_style_text_color(hour_r, lv_color_white(), 0);
    s_hour_roller = hour_r;

    lv_obj_t * colon = lv_label_create(overlay);
    lv_label_set_text(colon, ":");
    lv_obj_set_style_text_color(colon, lv_color_white(), 0);
    lv_obj_set_pos(colon, 66, 72);

    /* Minute roller (steps of 5) */
    lv_obj_t * min_r = lv_roller_create(overlay);
    lv_roller_set_options(min_r,
        "00\n05\n10\n15\n20\n25\n30\n35\n40\n45\n50\n55",
        LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(min_r, 2);
    lv_obj_set_size(min_r, 54, 60);
    lv_obj_set_pos(min_r, 74, 52);
    lv_obj_set_style_bg_color(min_r, lv_color_hex(0x1E2D3D), 0);
    lv_obj_set_style_text_color(min_r, lv_color_white(), 0);
    s_min_roller = min_r;

    lv_obj_t * time_hint = lv_label_create(overlay);
    lv_label_set_text(time_hint, "Time (optional)");
    lv_obj_set_style_text_color(time_hint, lv_color_hex(CLR_SUBTLE), 0);
    lv_obj_set_style_text_font(time_hint, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(time_hint, 10, 116);

    /* Confirm / Cancel buttons */
    lv_obj_t * confirm = lv_button_create(overlay);
    lv_obj_set_size(confirm, 100, 28);
    lv_obj_set_pos(confirm, SCREEN_W - 215, 118);
    lv_obj_set_style_bg_color(confirm, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_border_width(confirm, 0, 0);
    lv_obj_set_style_radius(confirm, 6, 0);
    lv_obj_add_event_cb(confirm, confirm_add_ev, LV_EVENT_CLICKED, NULL);
    lv_obj_t * c_lbl = lv_label_create(confirm);
    lv_label_set_text(c_lbl, LV_SYMBOL_OK " Add");
    lv_obj_set_style_text_color(c_lbl, lv_color_white(), 0);
    lv_obj_center(c_lbl);

    lv_obj_t * cancel = lv_button_create(overlay);
    lv_obj_set_size(cancel, 100, 28);
    lv_obj_set_pos(cancel, SCREEN_W - 110, 118);
    lv_obj_set_style_bg_color(cancel, lv_color_hex(0x444455), 0);
    lv_obj_set_style_border_width(cancel, 0, 0);
    lv_obj_set_style_radius(cancel, 6, 0);
    lv_obj_add_event_cb(cancel, cancel_add_ev, LV_EVENT_CLICKED, NULL);
    lv_obj_t * x_lbl = lv_label_create(cancel);
    lv_label_set_text(x_lbl, LV_SYMBOL_CLOSE " Cancel");
    lv_obj_set_style_text_color(x_lbl, lv_color_white(), 0);
    lv_obj_center(x_lbl);

    /* On-screen keyboard */
    lv_obj_t * kbd = lv_keyboard_create(overlay);
    lv_obj_set_size(kbd, SCREEN_W, 152);
    lv_obj_align(kbd, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kbd, ta);
    lv_obj_add_event_cb(kbd, [](lv_event_t *e){ confirm_add_ev(NULL); },
                        LV_EVENT_READY, NULL);
    s_kbd = kbd;
}

/* ── Public lifecycle ──────────────────────────────────────── */
void ui_Screen11_screen_init(void)
{
    compute_week();

    for (int i = 0; i < 7; i++) s_day_btns[i] = NULL;
    s_date_label  = NULL;
    s_event_list  = NULL;
    s_kbd_overlay = NULL;
    s_title_ta    = NULL;
    s_kbd         = NULL;
    s_hour_roller = NULL;
    s_min_roller  = NULL;

    ui_Screen11 = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_Screen11, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen11, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(ui_Screen11, LV_OPA_COVER, 0);

    lv_obj_add_event_cb(ui_Screen11, scr_unloaded_delete_cb,
                        LV_EVENT_SCREEN_UNLOADED, (void*)ui_Screen11_screen_destroy);

    build_header(ui_Screen11);
    build_week_bar(ui_Screen11);
    build_day_view(ui_Screen11);
    build_add_button(ui_Screen11);
    build_add_overlay(ui_Screen11);

    calendarScreenRefreshDay();
}

void ui_Screen11_screen_destroy(void)
{
    for (int i = 0; i < 7; i++) s_day_btns[i] = NULL;
    s_date_label  = NULL;
    s_event_list  = NULL;
    s_kbd_overlay = NULL;
    s_title_ta    = NULL;
    s_kbd         = NULL;
    s_hour_roller = NULL;
    s_min_roller  = NULL;
    ui_ActiveClockLabel = NULL;

    if (ui_Screen11) lv_obj_delete(ui_Screen11);
    ui_Screen11 = NULL;
}
