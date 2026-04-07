// Project  : HelpDesk
// File     : ui_Screen5.c
// Purpose  : TaskMaster screen — to-do list placeholder layout
// Depends  : ui.h (LVGL 9.x)

#include "ui.h"

/* ── Colours ───────────────────────────────────────────────── */
#define CLR_BG      0x1A1A2E
#define CLR_HDR     0x16213E
#define CLR_ACCENT  0x4CAF50   /* Green — matches launcher tile */
#define CLR_ITEM    0x1E2D3D   /* Task row background           */
#define CLR_SUBTLE  0xAAAAAA

/* ── Dimensions ────────────────────────────────────────────── */
#define SCREEN_W    480
#define HDR_H        30

/* ── Public object ─────────────────────────────────────────── */
lv_obj_t * ui_Screen5 = NULL;

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
    lv_label_set_text(title, "TaskMaster");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
}

/* Renders one placeholder task row with a checkbox-style icon */
static void add_placeholder_task(lv_obj_t * scr, const char * text, int y)
{
    lv_obj_t * row = lv_obj_create(scr);
    lv_obj_set_size(row, 290, 30);
    lv_obj_set_pos(row, 15, y);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(row, lv_color_hex(CLR_ITEM), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_all(row, 4, 0);

    lv_obj_t * chk = lv_label_create(row);
    lv_label_set_text(chk, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(chk, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_align(chk, LV_ALIGN_LEFT_MID, 4, 0);

    lv_obj_t * lbl = lv_label_create(row);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_SUBTLE), 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 24, 0);
}

static void build_body(lv_obj_t * scr)
{
    /* Placeholder tasks */
    add_placeholder_task(scr, "Add tasks via web UI (coming soon)", 40);
    add_placeholder_task(scr, "SD card saves list on reboot",       78);
    add_placeholder_task(scr, "Tap items to mark complete",        116);

    /* Empty state guide text */
    lv_obj_t * hint = lv_label_create(scr);
    lv_label_set_text(hint, "Tasks load from /tasks.json on the SD card.");
    lv_obj_set_style_text_color(hint, lv_color_hex(CLR_SUBTLE), 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, 260);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -48);

    /* Add task button (placeholder — no action yet) */
    lv_obj_t * add_btn = lv_button_create(scr);
    lv_obj_set_size(add_btn, 160, 34);
    lv_obj_align(add_btn, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(add_btn, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_border_width(add_btn, 0, 0);
    lv_obj_set_style_radius(add_btn, 8, 0);
    lv_obj_set_style_shadow_width(add_btn, 0, 0);

    lv_obj_t * add_lbl = lv_label_create(add_btn);
    lv_label_set_text(add_lbl, LV_SYMBOL_PLUS "  Add Task");
    lv_obj_set_style_text_color(add_lbl, lv_color_white(), 0);
    lv_obj_center(add_lbl);
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
    build_body(ui_Screen5);
}

void ui_Screen5_screen_destroy(void)
{
    if(ui_Screen5) lv_obj_delete(ui_Screen5);
    ui_Screen5 = NULL;
}
