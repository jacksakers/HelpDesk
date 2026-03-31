// Project  : HelpDesk
// File     : ui_Screen4.c
// Purpose  : JukeDesk screen — MP3 player placeholder layout
// Depends  : ui.h (LVGL 8.3.11)

#include "ui.h"

/* ── Colours ───────────────────────────────────────────────── */
#define CLR_BG          0x1A1A2E
#define CLR_HDR         0x16213E
#define CLR_ACCENT      0x9C27B0   /* Purple — matches launcher tile */
#define CLR_PANEL       0x12122A
#define CLR_BTN         0x6A1B9A   /* Darker purple for buttons      */
#define CLR_SUBTLE      0xAAAAAA

/* ── Dimensions ────────────────────────────────────────────── */
#define SCREEN_W        320
#define HDR_H            30
#define ART_SIZE         90   /* Album art placeholder square  */
#define ART_X           ((SCREEN_W - ART_SIZE) / 2)
#define ART_Y            38
#define CTRL_BTN_W       70
#define CTRL_BTN_H       34
#define CTRL_BTN_Y      188

/* ── Public objects ────────────────────────────────────────── */
lv_obj_t * ui_Screen4         = NULL;
lv_obj_t * ui_SongLabel       = NULL;
lv_obj_t * ui_PlayPauseLabel  = NULL;
lv_obj_t * ui_MusicStatusLabel = NULL;

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
    lv_label_set_text(title, "JukeDesk");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
}

static void build_body(lv_obj_t * scr)
{
    /* Album art placeholder */
    lv_obj_t * art = lv_obj_create(scr);
    lv_obj_set_size(art, ART_SIZE, ART_SIZE);
    lv_obj_set_pos(art, ART_X, ART_Y);
    lv_obj_clear_flag(art, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(art, lv_color_hex(CLR_PANEL), 0);
    lv_obj_set_style_bg_opa(art, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(art, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_border_width(art, 2, 0);
    lv_obj_set_style_radius(art, 8, 0);

    lv_obj_t * art_ico = lv_label_create(art);
    lv_label_set_text(art_ico, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_color(art_ico, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_center(art_ico);

    /* Song title */
    lv_obj_t * song_lbl = lv_label_create(scr);
    lv_label_set_text(song_lbl, "No track loaded");
    lv_obj_set_style_text_color(song_lbl, lv_color_white(), 0);
    lv_obj_align(song_lbl, LV_ALIGN_TOP_MID, 0, ART_Y + ART_SIZE + 8);
    lv_label_set_long_mode(song_lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(song_lbl, 260);
    ui_SongLabel = song_lbl;

    /* Status line */
    lv_obj_t * status_lbl = lv_label_create(scr);
    lv_label_set_text(status_lbl, "Insert SD card with /mp3 folder");
    lv_obj_set_style_text_color(status_lbl, lv_color_hex(CLR_SUBTLE), 0);
    lv_obj_align(status_lbl, LV_ALIGN_TOP_MID, 0, ART_Y + ART_SIZE + 26);
    lv_label_set_long_mode(status_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(status_lbl, 280);
    lv_obj_set_style_text_align(status_lbl, LV_TEXT_ALIGN_CENTER, 0);
    ui_MusicStatusLabel = status_lbl;

    /* Playback controls: [|<]  [Play/Pause]  [>|] */
    int ctrl_gap  = 20;
    int total_w   = 3 * CTRL_BTN_W + 2 * ctrl_gap;
    int ctrl_x0   = (SCREEN_W - total_w) / 2;

    /* Previous */
    lv_obj_t * prev_btn = lv_btn_create(scr);
    lv_obj_set_size(prev_btn, CTRL_BTN_W, CTRL_BTN_H);
    lv_obj_set_pos(prev_btn, ctrl_x0, CTRL_BTN_Y);
    lv_obj_set_style_bg_color(prev_btn, lv_color_hex(CLR_BTN), 0);
    lv_obj_set_style_border_width(prev_btn, 0, 0);
    lv_obj_set_style_radius(prev_btn, 8, 0);
    lv_obj_set_style_shadow_width(prev_btn, 0, 0);
    lv_obj_t * prev_ico = lv_label_create(prev_btn);
    lv_label_set_text(prev_ico, LV_SYMBOL_PREV);
    lv_obj_set_style_text_color(prev_ico, lv_color_white(), 0);
    lv_obj_center(prev_ico);

    /* Play / Pause */
    lv_obj_t * pp_btn = lv_btn_create(scr);
    lv_obj_set_size(pp_btn, CTRL_BTN_W, CTRL_BTN_H);
    lv_obj_set_pos(pp_btn, ctrl_x0 + CTRL_BTN_W + ctrl_gap, CTRL_BTN_Y);
    lv_obj_set_style_bg_color(pp_btn, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_border_width(pp_btn, 0, 0);
    lv_obj_set_style_radius(pp_btn, 8, 0);
    lv_obj_set_style_shadow_width(pp_btn, 0, 0);
    lv_obj_t * pp_lbl = lv_label_create(pp_btn);
    lv_label_set_text(pp_lbl, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(pp_lbl, lv_color_white(), 0);
    lv_obj_center(pp_lbl);
    ui_PlayPauseLabel = pp_lbl;

    /* Next */
    lv_obj_t * next_btn = lv_btn_create(scr);
    lv_obj_set_size(next_btn, CTRL_BTN_W, CTRL_BTN_H);
    lv_obj_set_pos(next_btn, ctrl_x0 + 2 * (CTRL_BTN_W + ctrl_gap), CTRL_BTN_Y);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(CLR_BTN), 0);
    lv_obj_set_style_border_width(next_btn, 0, 0);
    lv_obj_set_style_radius(next_btn, 8, 0);
    lv_obj_set_style_shadow_width(next_btn, 0, 0);
    lv_obj_t * next_ico = lv_label_create(next_btn);
    lv_label_set_text(next_ico, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_color(next_ico, lv_color_white(), 0);
    lv_obj_center(next_ico);
}

/* ── Public lifecycle ──────────────────────────────────────── */
void ui_Screen4_screen_init(void)
{
    ui_Screen4 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen4, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen4, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(ui_Screen4, LV_OPA_COVER, 0);

    lv_obj_add_event_cb(ui_Screen4, scr_unloaded_delete_cb,
                        LV_EVENT_SCREEN_UNLOADED, ui_Screen4_screen_destroy);

    build_header(ui_Screen4);
    build_body(ui_Screen4);
}

void ui_Screen4_screen_destroy(void)
{
    ui_SongLabel        = NULL;
    ui_PlayPauseLabel   = NULL;
    ui_MusicStatusLabel = NULL;

    if(ui_Screen4) lv_obj_del(ui_Screen4);
    ui_Screen4 = NULL;
}
