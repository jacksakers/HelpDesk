// Project  : HelpDesk
// File     : ui_Screen9.c
// Purpose  : Settings screen — sound mute toggle and tone style selector
// Depends  : ui.h (LVGL 9.x), buzzer.h

#include "ui.h"
#include "buzzer.h"

/* ── Colours ───────────────────────────────────────────────── */
#define CLR_BG           0x1A1A2E
#define CLR_HDR          0x16213E
#define CLR_SECTION_LBL  0x546E7A   /* Muted blue-grey section headers */

/* Tone button accent colours — match their launcher tile colours */
#define CLR_T_CLICK      0x2196F3   /* Blue   */
#define CLR_T_NAVIGATE   0x9C27B0   /* Purple */
#define CLR_T_SUCCESS    0x4CAF50   /* Green  */
#define CLR_T_ERROR      0xF44336   /* Red    */

#define SCREEN_W   480
#define HDR_H       36

/* ── Module-private state ──────────────────────────────────── */
lv_obj_t * ui_Screen9 = NULL;

/* Pointers to the 4 tone buttons so we can refresh their selection border */
static lv_obj_t * s_tone_btns[BUZZ_TONE_COUNT];

/* ── Helpers ───────────────────────────────────────────────── */

/* Highlight the currently active click-tone button; clear all others */
static void refresh_tone_selection(void)
{
    buzz_tone_t active = buzzerGetClickTone();
    int i;
    for(i = 0; i < (int)BUZZ_TONE_COUNT; i++) {
        if(!s_tone_btns[i]) continue;
        if(i == (int)active) {
            lv_obj_set_style_border_color(s_tone_btns[i], lv_color_white(), 0);
            lv_obj_set_style_border_width(s_tone_btns[i], 3, 0);
            lv_obj_set_style_border_opa(s_tone_btns[i], LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_border_width(s_tone_btns[i], 0, 0);
        }
    }
}

/* ── Event callbacks ───────────────────────────────────────── */

static void back_btn_ev(lv_event_t * e)
{
    (void)e;
    _ui_screen_change(&ui_Screen1, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, 300, 0,
                      ui_Screen1_screen_init);
}

static void sound_switch_ev(lv_event_t * e)
{
    lv_obj_t * sw = lv_event_get_target(e);
    bool sound_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    buzzerSetMuted(!sound_on);
    /* Play a confirmation ding when the user turns sound back ON */
    if(sound_on) buzzerPlay(BUZZ_TONE_SUCCESS);
}

static void tone_btn_ev(lv_event_t * e)
{
    buzz_tone_t tone = (buzz_tone_t)(uintptr_t)lv_event_get_user_data(e);
    buzzerSetClickTone(tone);
    refresh_tone_selection();
    buzzerPlay(tone);   /* Preview the selected tone immediately */
}

/* ── Build helpers ─────────────────────────────────────────── */

static void build_section_label(lv_obj_t * scr, const char * text, int y)
{
    lv_obj_t * lbl = lv_label_create(scr);
    lv_label_set_text(lbl, text);
    lv_obj_set_pos(lbl, 20, y);
    lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_SECTION_LBL), 0);

    /* Hairline separator below the label */
    lv_obj_t * sep = lv_obj_create(scr);
    lv_obj_set_size(sep, SCREEN_W - 40, 1);
    lv_obj_set_pos(sep, 20, y + 18);
    lv_obj_set_style_bg_color(sep, lv_color_hex(CLR_SECTION_LBL), 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_remove_flag(sep, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
}

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

    /* Back button (left side) */
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

    lv_obj_t * back_ico = lv_label_create(back_btn);
    lv_label_set_text(back_ico, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_ico, lv_color_white(), 0);
    lv_obj_center(back_ico);

    /* Title (centred) */
    lv_obj_t * title = lv_label_create(hdr);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_center(title);
}

static void build_sound_row(lv_obj_t * scr)
{
    /* "Click Sound" row label */
    lv_obj_t * lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Click Sound");
    lv_obj_set_pos(lbl, 20, 90);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);

    /* ON/OFF switch — right-aligned */
    lv_obj_t * sw = lv_switch_create(scr);
    lv_obj_set_size(sw, 52, 26);
    lv_obj_set_pos(sw, SCREEN_W - 72, 83);

    /* Reflect current muted state: sound ON = switch checked */
    if(!buzzerIsMuted()) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }

    lv_obj_add_event_cb(sw, sound_switch_ev, LV_EVENT_VALUE_CHANGED, NULL);
}

static void build_tone_row(lv_obj_t * scr)
{
    static const char * k_labels[BUZZ_TONE_COUNT] = {
        LV_SYMBOL_AUDIO "\nClick",
        LV_SYMBOL_RIGHT "\nNavigate",
        LV_SYMBOL_OK    "\nSuccess",
        LV_SYMBOL_CLOSE "\nError",
    };
    static const uint32_t k_colors[BUZZ_TONE_COUNT] = {
        CLR_T_CLICK, CLR_T_NAVIGATE, CLR_T_SUCCESS, CLR_T_ERROR
    };

    /* 4 buttons × 95px + 3 gaps × 8px = 404px total; centre in 480px */
    const int BTN_W   = 95;
    const int BTN_H   = 54;
    const int BTN_GAP = 8;
    const int START_X =
        (SCREEN_W - (BTN_W * (int)BUZZ_TONE_COUNT + BTN_GAP * ((int)BUZZ_TONE_COUNT - 1))) / 2;
    const int BTN_Y   = 168;

    int i;
    for(i = 0; i < (int)BUZZ_TONE_COUNT; i++) {
        lv_color_t c = lv_color_hex(k_colors[i]);
        int x = START_X + i * (BTN_W + BTN_GAP);

        lv_obj_t * btn = lv_button_create(scr);
        lv_obj_set_size(btn, BTN_W, BTN_H);
        lv_obj_set_pos(btn, x, BTN_Y);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(btn, c, 0);
        lv_obj_set_style_bg_color(btn,
            lv_color_mix(lv_color_black(), c, LV_OPA_20),
            LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 4, 0);
        lv_obj_add_event_cb(btn, tone_btn_ev, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)i);

        lv_obj_t * lbl = lv_label_create(btn);
        lv_label_set_text(lbl, k_labels[i]);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(lbl);

        s_tone_btns[i] = btn;
    }

    /* Mark the currently active tone */
    refresh_tone_selection();
}

/* ── Public lifecycle ──────────────────────────────────────── */

void ui_Screen9_screen_init(void)
{
    /* Clear stale button references from a previous init */
    int i;
    for(i = 0; i < (int)BUZZ_TONE_COUNT; i++) s_tone_btns[i] = NULL;

    ui_Screen9 = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_Screen9, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen9, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(ui_Screen9, LV_OPA_COVER, 0);

    /* Auto-cleanup when LVGL unloads this screen */
    lv_obj_add_event_cb(ui_Screen9, scr_unloaded_delete_cb,
                        LV_EVENT_SCREEN_UNLOADED, ui_Screen9_screen_destroy);

    build_header(ui_Screen9);
    build_section_label(ui_Screen9, "SOUND", 48);
    build_sound_row(ui_Screen9);
    build_section_label(ui_Screen9, "TONE STYLE  (tap to select & preview)", 132);
    build_tone_row(ui_Screen9);
}

void ui_Screen9_screen_destroy(void)
{
    if(ui_Screen9) lv_obj_delete(ui_Screen9);
    ui_Screen9 = NULL;
    int i;
    for(i = 0; i < (int)BUZZ_TONE_COUNT; i++) s_tone_btns[i] = NULL;
}
