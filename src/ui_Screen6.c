// Project  : HelpDesk
// File     : ui_Screen6.c
// Purpose  : ZenFrame screen — digital photo frame (full-screen image cycling)
// Depends  : ui.h (LVGL 9.x), zen_frame.h

#include "ui.h"
#include "zen_frame.h"
#include "get_time.h"

/* ── Colours ───────────────────────────────────────────────── */
#define CLR_BG      0x1A1A2E
#define CLR_HDR     0x16213E
#define CLR_ACCENT  0x009688   /* Teal — matches launcher tile  */
#define CLR_PANEL   0x0D1821
#define CLR_SUBTLE  0xAAAAAA

/* ── Dimensions ────────────────────────────────────────────── */
#define SCREEN_W    480
#define SCREEN_H    320
#define HDR_H        30
/* Image fills the full width below the header */
#define IMG_X        0
#define IMG_Y        HDR_H
#define IMG_W        SCREEN_W
#define IMG_H        (SCREEN_H - HDR_H)

/* ── Public objects ────────────────────────────────────────── */
lv_obj_t * ui_Screen6       = NULL;
lv_obj_t * ui_ZenImage      = NULL;
lv_obj_t * ui_ZenPlaceholder = NULL;

/* ── Event callbacks ───────────────────────────────────────── */
static void back_to_launcher_ev(lv_event_t * e)
{
    _ui_screen_change(&ui_Screen1, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, 300, 0,
                      ui_Screen1_screen_init);
}

static void tap_skip_ev(lv_event_t * e)
{
    (void)e;
    zenFrameNext();   /* Advance to next image on tap */
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

    uiAddHeaderClock(hdr, -8);
}

static void build_body(lv_obj_t * scr)
{
    /* Dark canvas below the header — fills the rest of the screen */
    lv_obj_t * canvas = lv_obj_create(scr);
    lv_obj_set_size(canvas, IMG_W, IMG_H);
    lv_obj_set_pos(canvas, IMG_X, IMG_Y);
    lv_obj_remove_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(canvas, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(canvas, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(canvas, 0, 0);
    lv_obj_set_style_radius(canvas, 0, 0);
    lv_obj_set_style_pad_all(canvas, 0, 0);

    /* Full-screen image widget — zen_frame.cpp updates the source */
    lv_obj_t * img = lv_image_create(canvas);
    lv_obj_set_size(img, IMG_W, IMG_H);
    lv_obj_set_pos(img, 0, 0);
    lv_image_set_inner_align(img, LV_IMAGE_ALIGN_STRETCH);
    lv_obj_add_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(img, tap_skip_ev, LV_EVENT_CLICKED, NULL);
    ui_ZenImage = img;

    /* Placeholder shown when /images is empty or SD not mounted */
    lv_obj_t * placeholder = lv_label_create(canvas);
    ui_ZenPlaceholder = placeholder;
    lv_label_set_text(placeholder,
                      LV_SYMBOL_IMAGE "\n\nAdd LVGL .bin images to\n/images on SD card\n\n"
                      "Convert at lvgl.io/tools/imageconverter\n"
                      "(LVGL v9 · RGB565 · 480\xC3\x97""320)");
    lv_obj_set_style_text_color(placeholder, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_text_align(placeholder, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(placeholder, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(placeholder, IMG_W - 40);
    lv_obj_center(placeholder);
    /* Hidden by default; zen_frame reveals it only when no images exist */
    lv_obj_add_flag(placeholder, LV_OBJ_FLAG_HIDDEN);

    /* Subtle hint at the bottom of the image area */
    lv_obj_t * hint = lv_label_create(scr);
    lv_label_set_text(hint, "Cycles every 5 min  \xE2\x80\xA2  Tap to skip");
    lv_obj_set_style_text_color(hint, lv_color_hex(CLR_SUBTLE), 0);
    lv_obj_set_style_bg_color(hint, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(hint, LV_OPA_70, 0);
    lv_obj_set_style_pad_hor(hint, 8, 0);
    lv_obj_set_style_pad_ver(hint, 2, 0);
    lv_obj_set_style_radius(hint, 4, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -6);
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

    /* Show whichever image is current in the zen_frame playlist */
    zenFrameRefresh();
}

void ui_Screen6_screen_destroy(void)
{
    ui_ZenImage       = NULL;
    ui_ZenPlaceholder = NULL;
    ui_ActiveClockLabel = NULL;
    if(ui_Screen6) lv_obj_delete(ui_Screen6);
    ui_Screen6 = NULL;
}
