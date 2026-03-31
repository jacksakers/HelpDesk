// Project  : HelpDesk
// File     : ui_Screen8.c
// Purpose  : GameBreak screen — mini game selector placeholder layout
// Depends  : ui.h (LVGL 8.3.11)

#include "ui.h"

/* ── Colours ───────────────────────────────────────────────── */
#define CLR_BG          0x1A1A2E
#define CLR_HDR         0x16213E
#define CLR_ACCENT      0xE91E63   /* Pink — matches launcher tile  */
#define CLR_TTT         0xD81B60   /* Tic-Tac-Toe button colour     */
#define CLR_SLIDE       0x8E24AA   /* Slide Puzzle button colour    */
#define CLR_SIMON       0x1565C0   /* Simon Says button colour      */

/* ── Dimensions ────────────────────────────────────────────── */
#define SCREEN_W        320
#define HDR_H            30
#define GAME_BTN_W      280
#define GAME_BTN_H       46
#define GAME_BTN_GAP     12
#define FIRST_BTN_Y      48

/* ── Public object ─────────────────────────────────────────── */
lv_obj_t * ui_Screen8 = NULL;

/* ── Event callbacks ───────────────────────────────────────── */
static void back_to_launcher_ev(lv_event_t * e)
{
    _ui_screen_change(&ui_Screen1, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0,
                      ui_Screen1_screen_init);
}

/* ── Private helpers ───────────────────────────────────────── */
static void build_header(lv_obj_t * scr)
{
    lv_obj_t * hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, SCREEN_W, HDR_H);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(CLR_HDR), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);

    lv_obj_t * back_btn = lv_btn_create(hdr);
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
    lv_label_set_text(title, "GameBreak");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
}

/* Builds a game selection button with a "Coming Soon" badge */
static void build_game_btn(lv_obj_t * scr, const char * label,
                            uint32_t color, int y)
{
    int x = (SCREEN_W - GAME_BTN_W) / 2;

    lv_obj_t * btn = lv_obj_create(scr);
    lv_obj_set_size(btn, GAME_BTN_W, GAME_BTN_H);
    lv_obj_set_pos(btn, x, y);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, lv_color_darken(lv_color_hex(color), LV_OPA_20),
                              LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t * name_lbl = lv_label_create(btn);
    lv_label_set_text(name_lbl, label);
    lv_obj_set_style_text_color(name_lbl, lv_color_white(), 0);
    lv_obj_align(name_lbl, LV_ALIGN_LEFT_MID, 12, 0);

    lv_obj_t * soon_lbl = lv_label_create(btn);
    lv_label_set_text(soon_lbl, "Coming Soon");
    lv_obj_set_style_text_color(soon_lbl, lv_color_hex(0xFFFFFF80), 0);
    lv_obj_align(soon_lbl, LV_ALIGN_RIGHT_MID, -12, 0);
}

static void build_body(lv_obj_t * scr)
{
    build_game_btn(scr, LV_SYMBOL_SHUFFLE "  Tic-Tac-Toe",  CLR_TTT,
                   FIRST_BTN_Y + 0 * (GAME_BTN_H + GAME_BTN_GAP));
    build_game_btn(scr, LV_SYMBOL_LOOP    "  Slide Puzzle", CLR_SLIDE,
                   FIRST_BTN_Y + 1 * (GAME_BTN_H + GAME_BTN_GAP));
    build_game_btn(scr, LV_SYMBOL_BELL    "  Simon Says",   CLR_SIMON,
                   FIRST_BTN_Y + 2 * (GAME_BTN_H + GAME_BTN_GAP));

    lv_obj_t * hint = lv_label_create(scr);
    lv_label_set_text(hint, "Take a 2-minute mental break.");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x606080), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
}

/* ── Public lifecycle ──────────────────────────────────────── */
void ui_Screen8_screen_init(void)
{
    ui_Screen8 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen8, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen8, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(ui_Screen8, LV_OPA_COVER, 0);

    lv_obj_add_event_cb(ui_Screen8, scr_unloaded_delete_cb,
                        LV_EVENT_SCREEN_UNLOADED, ui_Screen8_screen_destroy);

    build_header(ui_Screen8);
    build_body(ui_Screen8);
}

void ui_Screen8_screen_destroy(void)
{
    if(ui_Screen8) lv_obj_del(ui_Screen8);
    ui_Screen8 = NULL;
}
