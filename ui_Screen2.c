// Project  : HelpDesk
// File     : ui_Screen2.c
// Purpose  : DeskDash screen — clock, date, and weather placeholder layout
// Depends  : ui.h (LVGL 8.3.11)

#include "ui.h"

/* ── Colours ───────────────────────────────────────────────── */
#define CLR_BG       0x1A1A2E
#define CLR_HDR      0x16213E
#define CLR_ACCENT   0x2196F3   /* Blue — matches launcher tile */
#define CLR_SUBTLE   0xAAAAAA   /* Muted text                  */
#define CLR_PANEL    0x16213E

/* ── Screen dimensions ─────────────────────────────────────── */
#define SCREEN_W     320
#define HDR_H         30

/* ── Public objects ────────────────────────────────────────── */
lv_obj_t * ui_Screen2      = NULL;
lv_obj_t * ui_TimeLabel    = NULL;
lv_obj_t * ui_DateLabel    = NULL;
lv_obj_t * ui_WeatherLabel = NULL;
lv_obj_t * ui_WeatherCondLabel = NULL;

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
    lv_label_set_text(title, "DeskDash");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
}

static void build_body(lv_obj_t * scr)
{
    /* ---- Time label (centred, upper body) ---- */
    /* TODO: enable lv_font_montserrat_28 in lv_conf.h and set font here */
    lv_obj_t * time_lbl = lv_label_create(scr);
    lv_label_set_text(time_lbl, "--:--:--");
    lv_obj_set_style_text_color(time_lbl, lv_color_white(), 0);
    lv_obj_align(time_lbl, LV_ALIGN_TOP_MID, 0, 55);
    ui_TimeLabel = time_lbl;

    /* ---- Date label ---- */
    lv_obj_t * date_lbl = lv_label_create(scr);
    lv_label_set_text(date_lbl, "----, --- -- ----");
    lv_obj_set_style_text_color(date_lbl, lv_color_hex(CLR_SUBTLE), 0);
    lv_obj_align(date_lbl, LV_ALIGN_TOP_MID, 0, 80);
    ui_DateLabel = date_lbl;

    /* ---- Weather panel (bottom strip) ---- */
    lv_obj_t * wth = lv_obj_create(scr);
    lv_obj_set_size(wth, 300, 65);
    lv_obj_align(wth, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_clear_flag(wth, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(wth, lv_color_hex(CLR_PANEL), 0);
    lv_obj_set_style_bg_opa(wth, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(wth, 0, 0);
    lv_obj_set_style_radius(wth, 8, 0);
    lv_obj_set_style_pad_all(wth, 8, 0);

    lv_obj_t * temp_lbl = lv_label_create(wth);
    lv_label_set_text(temp_lbl, LV_SYMBOL_TINT "  --°C");
    lv_obj_set_style_text_color(temp_lbl, lv_color_hex(0x90CAF9), 0);
    lv_obj_align(temp_lbl, LV_ALIGN_LEFT_MID, 0, 0);
    ui_WeatherLabel = temp_lbl;

    lv_obj_t * cond_lbl = lv_label_create(wth);
    lv_label_set_text(cond_lbl, "Connecting...");
    lv_obj_set_style_text_color(cond_lbl, lv_color_hex(CLR_SUBTLE), 0);
    lv_obj_align(cond_lbl, LV_ALIGN_RIGHT_MID, 0, 0);
    ui_WeatherCondLabel = cond_lbl;
}

/* ── Public lifecycle ──────────────────────────────────────── */
void ui_Screen2_screen_init(void)
{
    ui_Screen2 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen2, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(ui_Screen2, LV_OPA_COVER, 0);

    lv_obj_add_event_cb(ui_Screen2, scr_unloaded_delete_cb,
                        LV_EVENT_SCREEN_UNLOADED, ui_Screen2_screen_destroy);

    build_header(ui_Screen2);
    build_body(ui_Screen2);
}

void ui_Screen2_screen_destroy(void)
{
    /* Null widget handles so callers can detect an inactive screen. */
    ui_TimeLabel       = NULL;
    ui_DateLabel       = NULL;
    ui_WeatherLabel    = NULL;
    ui_WeatherCondLabel = NULL;

    if(ui_Screen2) lv_obj_del(ui_Screen2);
    ui_Screen2 = NULL;
}
