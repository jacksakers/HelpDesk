// Project  : HelpDesk
// File     : ui_Screen5.c
// Purpose  : TaskMaster screen — gamified to-do list with XP, levels, streak,
//            on-device keyboard input, and voice-input button
// Depends  : ui.h, task_master.h, voice_input.h (LVGL 9.x)

#include "ui.h"
#include "task_master.h"
#include "voice_input.h"

/* ── Colours ───────────────────────────────────────────────── */
#define CLR_BG         0x1A1A2E
#define CLR_HDR        0x16213E
#define CLR_ACCENT     0x4CAF50   /* Green  */
#define CLR_XP_GOLD    0xFFD700   /* Gold   */
#define CLR_LEVEL      0x9C6EFF   /* Purple */
#define CLR_STREAK     0xFF7043   /* Ember  */
#define CLR_BAR_BG     0x2A2A3E
#define CLR_SUBTLE     0xAAAAAA
#define CLR_FOOTER     0x0F1525
#define CLR_KBD_BG     0x0A0F1E
#define CLR_INPUT_BG   0x1E2D3D

/* ── Dimensions ────────────────────────────────────────────── */
#define SCREEN_W    480
#define SCREEN_H    320
#define HDR_H        30
#define PROG_H       28
#define FOOTER_H     38
#define LIST_Y       (HDR_H + PROG_H)
#define LIST_H       (SCREEN_H - LIST_Y - FOOTER_H)   /* 224 px */

/* ── Public widget handles (defined here, declared extern in ui_Screen5.h) ── */
lv_obj_t * ui_Screen5        = NULL;
lv_obj_t * ui_TaskList       = NULL;
lv_obj_t * ui_TaskXpBar      = NULL;
lv_obj_t * ui_TaskXpLabel    = NULL;
lv_obj_t * ui_TaskLevelLabel = NULL;
lv_obj_t * ui_TaskStreakLabel = NULL;
lv_obj_t * ui_TaskInputArea  = NULL;

/* ── Private state ─────────────────────────────────────────── */
static lv_obj_t * s_kbd_overlay  = NULL;
static lv_obj_t * s_kbd          = NULL;
static lv_obj_t * s_repeat_btn   = NULL;
static bool       s_repeat_flag  = false;

/* ── Back button ───────────────────────────────────────────── */
static void back_to_launcher_ev(lv_event_t *e)
{
    (void)e;
    _ui_screen_change(&ui_Screen1, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, 300, 0,
                      ui_Screen1_screen_init);
}

/* ── Keyboard overlay helpers ──────────────────────────────── */
static void hide_keyboard(void)
{
    if (!s_kbd_overlay) return;
    lv_obj_add_flag(s_kbd_overlay, LV_OBJ_FLAG_HIDDEN);
    if (ui_TaskInputArea) lv_textarea_set_text(ui_TaskInputArea, "");
    s_repeat_flag = false;
    if (s_repeat_btn) {
        lv_obj_set_style_bg_color(s_repeat_btn, lv_color_hex(0x334455), 0);
        lv_label_set_text(lv_obj_get_child(s_repeat_btn, 0),
                          LV_SYMBOL_REFRESH "  Repeat: OFF");
    }
}

static void confirm_task_ev(lv_event_t *e)
{
    (void)e;
    if (!ui_TaskInputArea) return;
    const char *text = lv_textarea_get_text(ui_TaskInputArea);
    if (text && strlen(text) > 0) {
        taskAdd(text, s_repeat_flag);
    }
    hide_keyboard();
}

static void cancel_kbd_ev(lv_event_t *e)
{
    (void)e;
    hide_keyboard();
}

static void repeat_toggle_ev(lv_event_t *e)
{
    (void)e;
    s_repeat_flag = !s_repeat_flag;
    if (!s_repeat_btn) return;
    if (s_repeat_flag) {
        lv_obj_set_style_bg_color(s_repeat_btn, lv_color_hex(0x1565C0), 0);
        lv_label_set_text(lv_obj_get_child(s_repeat_btn, 0),
                          LV_SYMBOL_REFRESH "  Repeat: ON");
    } else {
        lv_obj_set_style_bg_color(s_repeat_btn, lv_color_hex(0x334455), 0);
        lv_label_set_text(lv_obj_get_child(s_repeat_btn, 0),
                          LV_SYMBOL_REFRESH "  Repeat: OFF");
    }
}

/* LVGL keyboard fires LV_EVENT_READY when the user taps the Enter key */
static void kbd_ready_ev(lv_event_t *e)
{
    (void)e;
    confirm_task_ev(NULL);
}

static void show_keyboard(void)
{
    if (!s_kbd_overlay) return;
    lv_obj_remove_flag(s_kbd_overlay, LV_OBJ_FLAG_HIDDEN);
    if (ui_TaskInputArea) lv_textarea_set_text(ui_TaskInputArea, "");
    if (s_kbd) lv_keyboard_set_textarea(s_kbd, ui_TaskInputArea);
}

/* Called by voice_input when transcription finishes — fills the textarea. */
static void on_voice_result(const char *text)
{
    if (ui_TaskInputArea && text) {
        lv_textarea_set_text(ui_TaskInputArea, text);
    }
}

/* ── Voice button ──────────────────────────────────────────── */
static void voice_btn_ev(lv_event_t *e)
{
    (void)e;
    if (voiceInputIsActive()) return;
    show_keyboard();   /* Open overlay so the result can populate the field */
    voiceInputStart();
}

/* ── Footer buttons ────────────────────────────────────────── */
static void add_task_btn_ev(lv_event_t *e)
{
    (void)e;
    show_keyboard();
}

/* ── XP popup animation ─────────────────────────────────────── */
static void xp_popup_done_cb(lv_anim_t *a)
{
    lv_obj_t *obj = (lv_obj_t *)a->var;
    if (obj) lv_obj_delete(obj);
}

/* Wrapper so lv_anim's two-arg exec_cb can set opa (selector is always 0) */
static void set_opa_cb(void *obj, int32_t val)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)val, 0);
}

void ui_Screen5_show_xp_popup(int xp)
{
    if (!ui_Screen5) return;

    char buf[16];
    snprintf(buf, sizeof(buf), "+%d XP!", xp);

    lv_obj_t *lbl = lv_label_create(ui_Screen5);
    lv_label_set_text(lbl, buf);
    lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_XP_GOLD), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl, SCREEN_W / 2 - 30, SCREEN_H / 2);

    /* Float upward — delete the label when animation finishes */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, lbl);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a, SCREEN_H / 2, SCREEN_H / 2 - 80);
    lv_anim_set_duration(&a, 900);
    lv_anim_set_completed_cb(&a, xp_popup_done_cb);
    lv_anim_start(&a);

    /* Fade out */
    lv_anim_t a2;
    lv_anim_init(&a2);
    lv_anim_set_var(&a2, lbl);
    lv_anim_set_exec_cb(&a2, set_opa_cb);
    lv_anim_set_values(&a2, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&a2, 900);
    lv_anim_start(&a2);
}

/* ── Section builders ──────────────────────────────────────── */

static void build_header(lv_obj_t *scr)
{
    lv_obj_t *hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, SCREEN_W, HDR_H);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_remove_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(CLR_HDR), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);

    lv_obj_t *back_btn = lv_button_create(hdr);
    lv_obj_set_size(back_btn, 40, HDR_H - 2);
    lv_obj_set_pos(back_btn, 2, 1);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(CLR_HDR), 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x0D1321),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_shadow_width(back_btn, 0, 0);
    lv_obj_set_style_pad_all(back_btn, 0, 0);
    lv_obj_add_event_cb(back_btn, back_to_launcher_ev, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_ico = lv_label_create(back_btn);
    lv_label_set_text(back_ico, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_ico, lv_color_white(), 0);
    lv_obj_center(back_ico);

    lv_obj_t *title = lv_label_create(hdr);
    lv_label_set_text(title, "TaskMaster");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    ui_TaskLevelLabel = lv_label_create(hdr);
    lv_label_set_text(ui_TaskLevelLabel, "Lv.0");
    lv_obj_set_style_text_color(ui_TaskLevelLabel, lv_color_hex(CLR_LEVEL), 0);
    lv_obj_set_style_text_font(ui_TaskLevelLabel, &lv_font_montserrat_12, 0);
    lv_obj_align(ui_TaskLevelLabel, LV_ALIGN_RIGHT_MID, -72, 0);

    ui_TaskStreakLabel = lv_label_create(hdr);
    lv_label_set_text(ui_TaskStreakLabel, "0d streak");
    lv_obj_set_style_text_color(ui_TaskStreakLabel, lv_color_hex(CLR_STREAK), 0);
    lv_obj_set_style_text_font(ui_TaskStreakLabel, &lv_font_montserrat_10, 0);
    lv_obj_align(ui_TaskStreakLabel, LV_ALIGN_RIGHT_MID, -4, 0);
}

static void build_progress_row(lv_obj_t *scr)
{
    lv_obj_t *row = lv_obj_create(scr);
    lv_obj_set_pos(row, 0, HDR_H);
    lv_obj_set_size(row, SCREEN_W, PROG_H);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(row, lv_color_hex(CLR_BAR_BG), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);

    ui_TaskXpBar = lv_bar_create(row);
    lv_obj_set_size(ui_TaskXpBar, 180, 10);
    lv_obj_set_pos(ui_TaskXpBar, 6, PROG_H / 2 - 5);
    lv_bar_set_range(ui_TaskXpBar, 0, TASK_DAILY_GOAL_XP);
    lv_bar_set_value(ui_TaskXpBar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui_TaskXpBar, lv_color_hex(0x1A1A2E), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_TaskXpBar, lv_color_hex(CLR_ACCENT),
                               LV_PART_INDICATOR);
    lv_obj_set_style_border_color(ui_TaskXpBar, lv_color_hex(0x334455), 0);
    lv_obj_set_style_border_width(ui_TaskXpBar, 1, 0);
    lv_obj_set_style_radius(ui_TaskXpBar, 4, 0);
    lv_obj_set_style_radius(ui_TaskXpBar, 4, LV_PART_INDICATOR);

    ui_TaskXpLabel = lv_label_create(row);
    lv_label_set_text(ui_TaskXpLabel, "0/50 XP  |  0 done today  |  0 left");
    lv_obj_set_style_text_color(ui_TaskXpLabel, lv_color_hex(CLR_SUBTLE), 0);
    lv_obj_set_style_text_font(ui_TaskXpLabel, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(ui_TaskXpLabel, 194, PROG_H / 2 - 5);
}

static void build_task_list(lv_obj_t *scr)
{
    ui_TaskList = lv_obj_create(scr);
    lv_obj_set_pos(ui_TaskList, 0, LIST_Y);
    lv_obj_set_size(ui_TaskList, SCREEN_W, LIST_H);
    lv_obj_set_style_bg_color(ui_TaskList, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(ui_TaskList, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui_TaskList, 0, 0);
    lv_obj_set_style_radius(ui_TaskList, 0, 0);
    lv_obj_set_style_pad_all(ui_TaskList, 4, 0);
    lv_obj_set_style_pad_row(ui_TaskList, 3, 0);
    lv_obj_set_flex_flow(ui_TaskList, LV_FLEX_FLOW_COLUMN);
}

static void build_footer(lv_obj_t *scr)
{
    lv_obj_t *footer = lv_obj_create(scr);
    lv_obj_set_pos(footer, 0, SCREEN_H - FOOTER_H);
    lv_obj_set_size(footer, SCREEN_W, FOOTER_H);
    lv_obj_remove_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(footer, lv_color_hex(CLR_FOOTER), 0);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_set_style_radius(footer, 0, 0);
    lv_obj_set_style_pad_all(footer, 4, 0);

    lv_obj_t *add_btn = lv_button_create(footer);
    lv_obj_set_size(add_btn, 170, 30);
    lv_obj_set_pos(add_btn, 6, 4);
    lv_obj_set_style_bg_color(add_btn, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_bg_color(add_btn, lv_color_hex(0x388E3C),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(add_btn, 0, 0);
    lv_obj_set_style_radius(add_btn, 8, 0);
    lv_obj_set_style_shadow_width(add_btn, 0, 0);
    lv_obj_add_event_cb(add_btn, add_task_btn_ev, LV_EVENT_CLICKED, NULL);

    lv_obj_t *add_lbl = lv_label_create(add_btn);
    lv_label_set_text(add_lbl, LV_SYMBOL_PLUS "  Add Task");
    lv_obj_set_style_text_color(add_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(add_lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(add_lbl);

    lv_obj_t *mic_btn = lv_button_create(footer);
    lv_obj_set_size(mic_btn, 130, 30);
    lv_obj_set_pos(mic_btn, 182, 4);
    lv_obj_set_style_bg_color(mic_btn, lv_color_hex(0x1565C0), 0);
    lv_obj_set_style_bg_color(mic_btn, lv_color_hex(0x0D47A1),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(mic_btn, 0, 0);
    lv_obj_set_style_radius(mic_btn, 8, 0);
    lv_obj_set_style_shadow_width(mic_btn, 0, 0);
    lv_obj_add_event_cb(mic_btn, voice_btn_ev, LV_EVENT_CLICKED, NULL);

    lv_obj_t *mic_lbl = lv_label_create(mic_btn);
    lv_label_set_text(mic_lbl, LV_SYMBOL_AUDIO "  Voice Add");
    lv_obj_set_style_text_color(mic_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(mic_lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(mic_lbl);

    lv_obj_t *hint_lbl = lv_label_create(footer);
    lv_label_set_text(hint_lbl, "Long-press row to delete");
    lv_obj_set_style_text_color(hint_lbl, lv_color_hex(0x445566), 0);
    lv_obj_set_style_text_font(hint_lbl, &lv_font_montserrat_8, 0);
    lv_obj_align(hint_lbl, LV_ALIGN_RIGHT_MID, -4, 0);
}

static void build_keyboard_overlay(lv_obj_t *scr)
{
    s_kbd_overlay = lv_obj_create(scr);
    lv_obj_set_size(s_kbd_overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(s_kbd_overlay, 0, 0);
    lv_obj_remove_flag(s_kbd_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_kbd_overlay, lv_color_hex(CLR_KBD_BG), 0);
    lv_obj_set_style_bg_opa(s_kbd_overlay, LV_OPA_90, 0);
    lv_obj_set_style_border_width(s_kbd_overlay, 0, 0);
    lv_obj_set_style_radius(s_kbd_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_kbd_overlay, 0, 0);
    lv_obj_add_flag(s_kbd_overlay, LV_OBJ_FLAG_HIDDEN);

    ui_TaskInputArea = lv_textarea_create(s_kbd_overlay);
    lv_obj_set_size(ui_TaskInputArea, 330, 30);
    lv_obj_set_pos(ui_TaskInputArea, 4, 4);
    lv_textarea_set_one_line(ui_TaskInputArea, true);
    lv_textarea_set_placeholder_text(ui_TaskInputArea, "What needs doing?");
    lv_obj_set_style_bg_color(ui_TaskInputArea, lv_color_hex(CLR_INPUT_BG), 0);
    lv_obj_set_style_text_color(ui_TaskInputArea, lv_color_white(), 0);
    lv_obj_set_style_border_color(ui_TaskInputArea, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_border_width(ui_TaskInputArea, 1, 0);
    lv_obj_set_style_radius(ui_TaskInputArea, 6, 0);

    lv_obj_t *cancel_btn = lv_button_create(s_kbd_overlay);
    lv_obj_set_size(cancel_btn, 64, 30);
    lv_obj_set_pos(cancel_btn, 338, 4);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x442222), 0);
    lv_obj_set_style_border_width(cancel_btn, 0, 0);
    lv_obj_set_style_radius(cancel_btn, 6, 0);
    lv_obj_set_style_shadow_width(cancel_btn, 0, 0);
    lv_obj_add_event_cb(cancel_btn, cancel_kbd_ev, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cancel_lbl = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_lbl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(cancel_lbl, lv_color_hex(0xFF6666), 0);
    lv_obj_center(cancel_lbl);

    lv_obj_t *confirm_btn = lv_button_create(s_kbd_overlay);
    lv_obj_set_size(confirm_btn, 64, 30);
    lv_obj_set_pos(confirm_btn, 406, 4);
    lv_obj_set_style_bg_color(confirm_btn, lv_color_hex(0x224422), 0);
    lv_obj_set_style_border_width(confirm_btn, 0, 0);
    lv_obj_set_style_radius(confirm_btn, 6, 0);
    lv_obj_set_style_shadow_width(confirm_btn, 0, 0);
    lv_obj_add_event_cb(confirm_btn, confirm_task_ev, LV_EVENT_CLICKED, NULL);
    lv_obj_t *confirm_lbl = lv_label_create(confirm_btn);
    lv_label_set_text(confirm_lbl, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(confirm_lbl, lv_color_hex(0x66FF66), 0);
    lv_obj_center(confirm_lbl);

    s_repeat_btn = lv_button_create(s_kbd_overlay);
    lv_obj_set_size(s_repeat_btn, 160, 22);
    lv_obj_set_pos(s_repeat_btn, 4, 38);
    lv_obj_set_style_bg_color(s_repeat_btn, lv_color_hex(0x334455), 0);
    lv_obj_set_style_border_width(s_repeat_btn, 0, 0);
    lv_obj_set_style_radius(s_repeat_btn, 6, 0);
    lv_obj_set_style_shadow_width(s_repeat_btn, 0, 0);
    lv_obj_add_event_cb(s_repeat_btn, repeat_toggle_ev, LV_EVENT_CLICKED, NULL);
    lv_obj_t *rep_lbl = lv_label_create(s_repeat_btn);
    lv_label_set_text(rep_lbl, LV_SYMBOL_REFRESH "  Repeat: OFF");
    lv_obj_set_style_text_color(rep_lbl, lv_color_hex(CLR_SUBTLE), 0);
    lv_obj_set_style_text_font(rep_lbl, &lv_font_montserrat_10, 0);
    lv_obj_center(rep_lbl);

    s_kbd = lv_keyboard_create(s_kbd_overlay);
    lv_obj_set_pos(s_kbd, 0, 62);
    lv_obj_set_size(s_kbd, SCREEN_W, SCREEN_H - 64);
    /* Strip default theme padding/border so the button matrix gets the full
       height to distribute across all 4 rows (including the spacebar row).
       Without this the container padding squeezes row 4 to 0 px in landscape. */
    lv_obj_set_style_pad_top(s_kbd, 0, 0);
    lv_obj_set_style_pad_bottom(s_kbd, 0, 0);
    lv_obj_set_style_border_width(s_kbd, 0, 0);
    lv_obj_set_style_shadow_width(s_kbd, 0, 0);
    lv_keyboard_set_textarea(s_kbd, ui_TaskInputArea);
    lv_obj_add_event_cb(s_kbd, kbd_ready_ev, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_kbd, cancel_kbd_ev, LV_EVENT_CANCEL, NULL);
}

/* ── Public lifecycle ──────────────────────────────────────── */
void ui_Screen5_screen_init(void)
{
    ui_Screen5 = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_Screen5, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen5, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(ui_Screen5, LV_OPA_COVER, 0);

    lv_obj_add_event_cb(ui_Screen5, scr_unloaded_delete_cb,
                        LV_EVENT_SCREEN_UNLOADED, ui_Screen5_screen_destroy);

    build_header(ui_Screen5);
    build_progress_row(ui_Screen5);
    build_task_list(ui_Screen5);
    build_footer(ui_Screen5);
    build_keyboard_overlay(ui_Screen5);

    voiceInputSetResultCallback(on_voice_result);  /* wire result → textarea */
    taskRefreshScreen();   /* Populate from live task model */
}

void ui_Screen5_screen_destroy(void)
{
    /* Null all handles before deleting the root to prevent dangling-pointer
       access if task_master fires a refresh after teardown begins. */
    ui_TaskList       = NULL;
    ui_TaskXpBar      = NULL;
    ui_TaskXpLabel    = NULL;
    ui_TaskLevelLabel = NULL;
    ui_TaskStreakLabel = NULL;
    ui_TaskInputArea  = NULL;
    s_kbd_overlay     = NULL;
    s_kbd             = NULL;
    s_repeat_btn      = NULL;
    s_repeat_flag     = false;

    voiceInputSetResultCallback(NULL);  /* prevent callback to dead widget */

    if (ui_Screen5) lv_obj_delete(ui_Screen5);
    ui_Screen5 = NULL;
}
