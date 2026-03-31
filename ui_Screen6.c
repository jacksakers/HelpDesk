// Project  : HelpDesk
// File     : ui_Screen6.c
// Purpose  : ZenFrame screen — digital photo frame placeholder layout
// Depends  : ui.h (LVGL 9.x)

#include "ui.h"

/* ── Colours ───────────────────────────────────────────────── */
#define CLR_BG      0x1A1A2E
#define CLR_HDR     0x16213E
#define CLR_ACCENT  0x009688   /* Teal — matches launcher tile  */
#define CLR_PANEL   0x0D1821
#define CLR_SUBTLE  0xAAAAAA

/* ── Dimensions ────────────────────────────────────────────── */
#define SCREEN_W    320
#define SCREEN_H    240
#define HDR_H        30
/* Frame area fills most of the screen */
#define FRAME_X      10
#define FRAME_Y      38
#define FRAME_W     300
#define FRAME_H     160

/* ── Public objects ────────────────────────────────────────── */
lv_obj_t * ui_Screen6  = NULL;
lv_obj_t * ui_ZenImage = NULL;

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
    lv_label_set_text(title, "ZenFrame");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
}

static void build_body(lv_obj_t * scr)
{
    /* Image frame border */
    lv_obj_t * frame = lv_obj_create(scr);
    lv_obj_set_size(frame, FRAME_W, FRAME_H);
    lv_obj_set_pos(frame, FRAME_X, FRAME_Y);
    lv_obj_remove_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(frame, lv_color_hex(CLR_PANEL), 0);
    lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(frame, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_border_width(frame, 2, 0);
    lv_obj_set_style_radius(frame, 6, 0);
    lv_obj_set_style_pad_all(frame, 0, 0);

    /* Image widget — updated by zen_frame.cpp once implemented */
    lv_obj_t * img = lv_image_create(frame);
    lv_obj_center(img);
    ui_ZenImage = img;

    /* Placeholder icon + hint */
    lv_obj_t * placeholder = lv_label_create(frame);
    lv_label_set_text(placeholder,
                      LV_SYMBOL_IMAGE "\n\nPlace .bin images in\n/images on SD card");
    lv_obj_set_style_text_color(placeholder, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_text_align(placeholder, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(placeholder, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(placeholder, FRAME_W - 20);
    lv_obj_center(placeholder);

    /* Interval hint below frame */
    lv_obj_t * hint = lv_label_create(scr);
    lv_label_set_text(hint, "Cycles every 5 min  |  Tap to skip");
    lv_obj_set_style_text_color(hint, lv_color_hex(CLR_SUBTLE), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
}

/* ── Public lifecycle ──────────────────────────────────────── */
void ui_Screen6_screen_init(void)
{
    ui_Screen6 = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_Screen6, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen6, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(ui_Screen6, LV_OPA_COVER, 0);

    lv_obj_add_event_cb(ui_Screen6, scr_unloaded_delete_cb,
                        LV_EVENT_SCREEN_UNLOADED, ui_Screen6_screen_destroy);

    build_header(ui_Screen6);
    build_body(ui_Screen6);
}

void ui_Screen6_screen_destroy(void)
{
    ui_ZenImage = NULL;
    if(ui_Screen6) lv_obj_delete(ui_Screen6);
    ui_Screen6 = NULL;
}
