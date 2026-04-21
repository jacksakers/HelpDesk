// Project  : HelpDesk
// File     : ui_Screen4.c
// Purpose  : Notifications screen — shows grouped sender list from companion app
// Depends  : ui.h (LVGL 9.x), handshake.h, notifications.h

#include "ui.h"
#include "handshake.h"
#include "notifications.h"
#include "get_time.h"

/* ── Colours ───────────────────────────────────────────────── */
#define CLR_BG          0x1A1A2E
#define CLR_HDR         0x16213E
#define CLR_ACCENT      0xFF5722   /* Deep orange — matches launcher tile */
#define CLR_LIST_BG     0x12122A
#define CLR_BTN_CLEAR   0xB71C1C   /* Dark red for the clear button */
#define CLR_SUBTLE      0xAAAAAA

/* ── Dimensions ────────────────────────────────────────────── */
#define SCREEN_W        480
#define SCREEN_H        320
#define HDR_H            30
#define LIST_Y          (HDR_H + 4)
#define LIST_H          (SCREEN_H - LIST_Y - 4)

/* ── Public objects ────────────────────────────────────────── */
lv_obj_t * ui_Screen4   = NULL;
lv_obj_t * ui_NotifList = NULL;

/* ── Event callbacks ───────────────────────────────────────── */
static void back_to_launcher_ev(lv_event_t * e)
{
    (void)e;
    _ui_screen_change(&ui_Screen1, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, 300, 0,
                      ui_Screen1_screen_init);
}

static void clear_all_ev(lv_event_t * e)
{
    (void)e;
    notifClearAll();
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

    /* Back button — left side */
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

    lv_obj_t * back_ico = lv_label_create(back_btn);
    lv_label_set_text(back_ico, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_ico, lv_color_white(), 0);
    lv_obj_center(back_ico);

    /* Screen title */
    lv_obj_t * title = lv_label_create(hdr);
    lv_label_set_text(title, LV_SYMBOL_BELL "  Notifications");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    /* Clear-all button — right side */
    lv_obj_t * clr_btn = lv_button_create(hdr);
    lv_obj_set_size(clr_btn, 52, HDR_H - 4);
    lv_obj_set_pos(clr_btn, SCREEN_W - 56, 2);
    lv_obj_set_style_bg_color(clr_btn, lv_color_hex(CLR_BTN_CLEAR), 0);
    lv_obj_set_style_bg_color(clr_btn, lv_color_hex(0x7F0000),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(clr_btn, 0, 0);
    lv_obj_set_style_shadow_width(clr_btn, 0, 0);
    lv_obj_set_style_radius(clr_btn, 4, 0);
    lv_obj_set_style_pad_all(clr_btn, 0, 0);
    lv_obj_add_event_cb(clr_btn, clear_all_ev, LV_EVENT_CLICKED, NULL);

    lv_obj_t * clr_lbl = lv_label_create(clr_btn);
    lv_label_set_text(clr_lbl, "Clear");
    lv_obj_set_style_text_color(clr_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(clr_lbl, &lv_font_montserrat_10, 0);
    lv_obj_center(clr_lbl);
}

static void build_list(lv_obj_t * scr)
{
    lv_obj_t * list = lv_list_create(scr);
    lv_obj_set_size(list, SCREEN_W, LIST_H);
    lv_obj_set_pos(list, 0, LIST_Y);
    lv_obj_set_style_bg_color(list, lv_color_hex(CLR_LIST_BG), 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_row(list, 2, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    ui_NotifList = list;

    /* Populate from current notification store */
    notifRefreshScreen();
}

/* ── Public lifecycle ──────────────────────────────────────── */
void ui_Screen4_screen_init(void)
{
    handshakeSetScreen("notifications");

    ui_Screen4 = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_Screen4, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen4, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(ui_Screen4, LV_OPA_COVER, 0);

    lv_obj_add_event_cb(ui_Screen4, scr_unloaded_delete_cb,
                        LV_EVENT_SCREEN_UNLOADED, ui_Screen4_screen_destroy);

    build_header(ui_Screen4);
    build_list(ui_Screen4);

    /* Opening the screen counts as reading all current notifications */
    notifMarkAllRead();
}

void ui_Screen4_screen_destroy(void)
{
    ui_NotifList = NULL;
    uiClearHeaderClock(ui_Screen4);

    if(ui_Screen4) lv_obj_delete(ui_Screen4);
    ui_Screen4 = NULL;
}
