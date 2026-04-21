// Project  : HelpDesk
// File     : ui_Screen3.c
// Purpose  : Tomatimer screen — Pomodoro timer placeholder layout
// Depends  : ui.h (LVGL 9.x)

#include "ui.h"
#include "get_time.h"
#include "pomo_timer.h"

/* ── Colours ───────────────────────────────────────────────── */
#define CLR_BG          0x1A1A2E
#define CLR_HDR         0x16213E
#define CLR_ACCENT      0xF44336   /* Red — matches launcher tile */
#define CLR_ARC_BG      0x2A2A3E   /* Dim ring background         */
#define CLR_BTN_START   0x388E3C   /* Green start button          */
#define CLR_BTN_STOP    0xD32F2F   /* Red stop button             */
#define CLR_BTN_RESET   0x455A64   /* Grey reset button           */
#define CLR_WORK_MODE   0xF44336   /* Work mode indicator         */
#define CLR_BREAK_MODE  0x1565C0   /* Break mode indicator        */

/* ── Dimensions ────────────────────────────────────────────── */
#define SCREEN_W        480
#define HDR_H            30
#define ARC_SIZE        120
#define ARC_THICK        12
/* Arc positioned so controls fit below it within 240px height */
#define ARC_Y            42   /* top of arc from screen top    */
#define ARC_CX          240   /* horizontal centre             */
#define ARC_CY          (ARC_Y + ARC_SIZE / 2)

#define MODE_BTN_Y      (ARC_Y + ARC_SIZE + 8)
#define MODE_BTN_W      110
#define MODE_BTN_H       26
#define ACT_BTN_Y       (MODE_BTN_Y + MODE_BTN_H + 8)
#define ACT_BTN_W        85
#define ACT_BTN_H        28

/* ── Public objects ────────────────────────────────────────── */
lv_obj_t * ui_Screen3      = NULL;
lv_obj_t * ui_PomoArc      = NULL;
lv_obj_t * ui_PomoTimeLabel = NULL;
lv_obj_t * ui_PomoPhaseLabel = NULL;

/* ── Event callbacks ───────────────────────────────────────── */
static void back_to_launcher_ev(lv_event_t * e)
{
    pomoStop();   /* pause timer when leaving the screen */
    _ui_screen_change(&ui_Screen1, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, 300, 0,
                      ui_Screen1_screen_init);
}

static void pomo_start_ev(lv_event_t * e) { (void)e; pomoStart();        }
static void pomo_stop_ev (lv_event_t * e) { (void)e; pomoStop();         }
static void pomo_reset_ev(lv_event_t * e) { (void)e; pomoReset();        }
static void pomo_work_ev (lv_event_t * e) { (void)e; pomoSetWorkMode();  }
static void pomo_break_ev(lv_event_t * e) { (void)e; pomoSetBreakMode(); }

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
    lv_label_set_text(title, "Tomatimer");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    uiAddHeaderClock(hdr, -8);
}

static lv_obj_t * make_btn(lv_obj_t * scr, const char * label,
                            uint32_t color, int x, int y, int w, int h)
{
    lv_obj_t * btn = lv_button_create(scr);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(btn, lv_color_mix(lv_color_black(), lv_color_hex(color), LV_OPA_30),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);

    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);
    return btn;
}

static void build_arc(lv_obj_t * scr)
{
    lv_obj_t * arc = lv_arc_create(scr);
    lv_obj_set_size(arc, ARC_SIZE, ARC_SIZE);
    lv_obj_set_pos(arc, ARC_CX - ARC_SIZE / 2, ARC_Y);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 100);
    /* 270° sweep: starts bottom-left, ends bottom-right, gap at bottom */
    lv_arc_set_bg_angles(arc, 135, 45);
    lv_obj_set_style_arc_color(arc, lv_color_hex(CLR_ARC_BG),
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(arc, lv_color_hex(CLR_ACCENT),
                               LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(arc, ARC_THICK, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(arc, ARC_THICK, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    /* Hide knob so it acts as a display-only indicator */
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    ui_PomoArc = arc;

    /* Time remaining — centred inside the arc */
    lv_obj_t * time_lbl = lv_label_create(arc);
    lv_label_set_text(time_lbl, "25:00");
    lv_obj_set_style_text_color(time_lbl, lv_color_white(), 0);
    lv_obj_align(time_lbl, LV_ALIGN_CENTER, 0, -6);
    ui_PomoTimeLabel = time_lbl;

    /* Phase sub-label inside arc */
    lv_obj_t * phase_lbl = lv_label_create(arc);
    lv_label_set_text(phase_lbl, "WORK");
    lv_obj_set_style_text_color(phase_lbl, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_align(phase_lbl, LV_ALIGN_CENTER, 0, 10);
    ui_PomoPhaseLabel = phase_lbl;
}

static void build_controls(lv_obj_t * scr)
{
    /* Mode toggle — Work / Break */
    int mode_x0 = (SCREEN_W - (2 * MODE_BTN_W + 10)) / 2;
    lv_obj_t * work_btn  = make_btn(scr, "WORK",  CLR_WORK_MODE,  mode_x0,                    MODE_BTN_Y, MODE_BTN_W, MODE_BTN_H);
    lv_obj_t * break_btn = make_btn(scr, "BREAK", CLR_BREAK_MODE, mode_x0 + MODE_BTN_W + 10, MODE_BTN_Y, MODE_BTN_W, MODE_BTN_H);
    lv_obj_add_event_cb(work_btn,  pomo_work_ev,  LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(break_btn, pomo_break_ev, LV_EVENT_CLICKED, NULL);

    /* Action row — Start / Stop / Reset */
    int act_x0 = (SCREEN_W - (3 * ACT_BTN_W + 2 * 10)) / 2;
    lv_obj_t * start_btn = make_btn(scr, LV_SYMBOL_PLAY  " Start", CLR_BTN_START, act_x0,                       ACT_BTN_Y, ACT_BTN_W, ACT_BTN_H);
    lv_obj_t * stop_btn  = make_btn(scr, LV_SYMBOL_PAUSE " Stop",  CLR_BTN_STOP,  act_x0 +     ACT_BTN_W + 10, ACT_BTN_Y, ACT_BTN_W, ACT_BTN_H);
    lv_obj_t * reset_btn = make_btn(scr, LV_SYMBOL_STOP  " Reset", CLR_BTN_RESET, act_x0 + 2 * ACT_BTN_W + 20, ACT_BTN_Y, ACT_BTN_W, ACT_BTN_H);
    lv_obj_add_event_cb(start_btn, pomo_start_ev, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(stop_btn,  pomo_stop_ev,  LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(reset_btn, pomo_reset_ev, LV_EVENT_CLICKED, NULL);
}

/* ── Public lifecycle ──────────────────────────────────────── */
void ui_Screen3_screen_init(void)
{
    ui_Screen3 = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_Screen3, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen3, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(ui_Screen3, LV_OPA_COVER, 0);

    lv_obj_add_event_cb(ui_Screen3, scr_unloaded_delete_cb,
                        LV_EVENT_SCREEN_UNLOADED, ui_Screen3_screen_destroy);

    build_header(ui_Screen3);
    build_arc(ui_Screen3);
    build_controls(ui_Screen3);
}

void ui_Screen3_screen_destroy(void)
{
    ui_PomoArc       = NULL;
    ui_PomoTimeLabel = NULL;
    ui_PomoPhaseLabel = NULL;
    uiClearHeaderClock(ui_Screen3);

    if(ui_Screen3) lv_obj_delete(ui_Screen3);
    ui_Screen3 = NULL;
}
