// Project  : HelpDesk
// File     : ui_Screen9.c
// Purpose  : Settings screen — sound, tone style, and DeskChat settings
// Depends  : ui.h (LVGL 9.x), buzzer.h, deskchat.h

#include "ui.h"
#include "buzzer.h"
#include "settings.h"
#include "deskchat.h"

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

/* Username keyboard overlay */
static lv_obj_t * s_uname_overlay    = NULL;
static lv_obj_t * s_uname_kbd        = NULL;
static lv_obj_t * s_uname_input      = NULL;
static lv_obj_t * s_uname_display    = NULL;   /* label showing current name */

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
    settingsSave();   /* Persist the new muted state */
}

static void tone_btn_ev(lv_event_t * e)
{
    buzz_tone_t tone = (buzz_tone_t)(uintptr_t)lv_event_get_user_data(e);
    buzzerSetClickTone(tone);
    refresh_tone_selection();
    buzzerPlay(tone);   /* Preview the selected tone immediately */
    settingsSave();     /* Persist the new tone choice */
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

/* ── DeskChat section callbacks ────────────────────────────── */

static void uname_kbd_confirm_ev(lv_event_t * e)
{
    (void)e;
    if (!s_uname_input) return;
    const char * text = lv_textarea_get_text(s_uname_input);
    if (text && text[0] != '\0') {
        settingsSetChatUsername(text);
        settingsSave();
        if (s_uname_display) {
            char buf[48];
            snprintf(buf, sizeof(buf), "Username: %s", text);
            lv_label_set_text(s_uname_display, buf);
        }
    }
    if (s_uname_overlay) lv_obj_add_flag(s_uname_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void uname_kbd_cancel_ev(lv_event_t * e)
{
    (void)e;
    if (s_uname_overlay) lv_obj_add_flag(s_uname_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void uname_kbd_ready_ev(lv_event_t * e)
{
    (void)e;
    uname_kbd_confirm_ev(NULL);
}

static void edit_uname_ev(lv_event_t * e)
{
    (void)e;
    if (!s_uname_overlay) return;
    lv_obj_remove_flag(s_uname_overlay, LV_OBJ_FLAG_HIDDEN);
    if (s_uname_input) {
        lv_textarea_set_text(s_uname_input, settingsGetChatUsername());
        if (s_uname_kbd) lv_keyboard_set_textarea(s_uname_kbd, s_uname_input);
    }
}

static void clear_history_ev(lv_event_t * e)
{
    (void)e;
    deskChatHistoryClear();
}

static void build_deskchat_section(lv_obj_t * scr)
{
    /* Username display */
    s_uname_display = lv_label_create(scr);
    const char * uname = settingsGetChatUsername();
    char ubuf[48];
    snprintf(ubuf, sizeof(ubuf), "Username: %s",
             uname[0] ? uname : "Anonymous");
    lv_label_set_text(s_uname_display, ubuf);
    lv_obj_set_pos(s_uname_display, 20, 250);
    lv_obj_set_style_text_color(s_uname_display, lv_color_white(), 0);

    /* Edit button */
    lv_obj_t * edit_btn = lv_button_create(scr);
    lv_obj_set_size(edit_btn, 80, 26);
    lv_obj_set_pos(edit_btn, SCREEN_W - 100, 244);
    lv_obj_set_style_bg_color(edit_btn, lv_color_hex(0x1565C0), 0);
    lv_obj_set_style_radius(edit_btn, 6, 0);
    lv_obj_set_style_shadow_width(edit_btn, 0, 0);
    lv_obj_set_style_border_width(edit_btn, 0, 0);
    lv_obj_add_event_cb(edit_btn, edit_uname_ev, LV_EVENT_CLICKED, NULL);
    lv_obj_t * edit_lbl = lv_label_create(edit_btn);
    lv_label_set_text(edit_lbl, LV_SYMBOL_EDIT "  Edit");
    lv_obj_set_style_text_color(edit_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(edit_lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(edit_lbl);

    /* Clear chat history button */
    lv_obj_t * clr_btn = lv_button_create(scr);
    lv_obj_set_size(clr_btn, 210, 28);
    lv_obj_set_pos(clr_btn, 20, 284);
    lv_obj_set_style_bg_color(clr_btn, lv_color_hex(0x5D1515), 0);
    lv_obj_set_style_bg_color(clr_btn, lv_color_hex(0x880000),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(clr_btn, 6, 0);
    lv_obj_set_style_shadow_width(clr_btn, 0, 0);
    lv_obj_set_style_border_width(clr_btn, 0, 0);
    lv_obj_add_event_cb(clr_btn, clear_history_ev, LV_EVENT_CLICKED, NULL);
    lv_obj_t * clr_lbl = lv_label_create(clr_btn);
    lv_label_set_text(clr_lbl, LV_SYMBOL_TRASH "  Clear Chat History");
    lv_obj_set_style_text_color(clr_lbl, lv_color_hex(0xFF8888), 0);
    lv_obj_set_style_text_font(clr_lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(clr_lbl);
}

static void build_uname_overlay(lv_obj_t * scr)
{
    s_uname_overlay = lv_obj_create(scr);
    lv_obj_set_size(s_uname_overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(s_uname_overlay, 0, 0);
    lv_obj_remove_flag(s_uname_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_uname_overlay, lv_color_hex(0x040810), 0);
    lv_obj_set_style_bg_opa(s_uname_overlay, LV_OPA_90, 0);
    lv_obj_set_style_border_width(s_uname_overlay, 0, 0);
    lv_obj_set_style_radius(s_uname_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_uname_overlay, 0, 0);
    lv_obj_add_flag(s_uname_overlay, LV_OBJ_FLAG_HIDDEN);

    s_uname_kbd = lv_keyboard_create(s_uname_overlay);
    lv_obj_set_size(s_uname_kbd, SCREEN_W, 200);
    lv_obj_align(s_uname_kbd, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_top(s_uname_kbd, 0, 0);
    lv_obj_set_style_pad_bottom(s_uname_kbd, 0, 0);
    lv_obj_set_style_border_width(s_uname_kbd, 0, 0);
    lv_obj_set_style_shadow_width(s_uname_kbd, 0, 0);
    lv_obj_add_event_cb(s_uname_kbd, uname_kbd_ready_ev,  LV_EVENT_READY,  NULL);
    lv_obj_add_event_cb(s_uname_kbd, uname_kbd_cancel_ev, LV_EVENT_CANCEL, NULL);

    s_uname_input = lv_textarea_create(s_uname_overlay);
    lv_obj_set_size(s_uname_input, SCREEN_W - 60, 36);
    lv_obj_set_pos(s_uname_input, 4, SCREEN_H - 200 - 40);
    lv_textarea_set_one_line(s_uname_input, true);
    lv_textarea_set_max_length(s_uname_input, 31);
    lv_textarea_set_placeholder_text(s_uname_input, "Display name (max 31 chars)");
    lv_obj_set_style_bg_color(s_uname_input, lv_color_hex(0x0E1E2D), 0);
    lv_obj_set_style_text_color(s_uname_input, lv_color_white(), 0);
    lv_obj_set_style_border_color(s_uname_input, lv_color_hex(0x1565C0), 0);
    lv_obj_set_style_border_width(s_uname_input, 1, 0);
    lv_obj_set_style_radius(s_uname_input, 8, 0);
    lv_obj_set_style_pad_hor(s_uname_input, 8, 0);
    lv_obj_set_style_pad_ver(s_uname_input, 4, 0);
    lv_obj_set_style_shadow_width(s_uname_input, 0, 0);
    lv_keyboard_set_textarea(s_uname_kbd, s_uname_input);

    lv_obj_t * save_btn = lv_button_create(s_uname_overlay);
    lv_obj_set_size(save_btn, 52, 36);
    lv_obj_set_pos(save_btn, SCREEN_W - 56, SCREEN_H - 200 - 40);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x0D3B0D), 0);
    lv_obj_set_style_border_color(save_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_border_width(save_btn, 1, 0);
    lv_obj_set_style_radius(save_btn, 8, 0);
    lv_obj_set_style_shadow_width(save_btn, 0, 0);
    lv_obj_add_event_cb(save_btn, uname_kbd_confirm_ev, LV_EVENT_CLICKED, NULL);
    lv_obj_t * save_ico = lv_label_create(save_btn);
    lv_label_set_text(save_ico, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(save_ico, lv_color_hex(0x4CAF50), 0);
    lv_obj_center(save_ico);

    lv_obj_t * close_btn = lv_button_create(s_uname_overlay);
    lv_obj_set_size(close_btn, 44, 32);
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, -4, 4);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x220A0A), 0);
    lv_obj_set_style_border_width(close_btn, 0, 0);
    lv_obj_set_style_radius(close_btn, 8, 0);
    lv_obj_set_style_shadow_width(close_btn, 0, 0);
    lv_obj_add_event_cb(close_btn, uname_kbd_cancel_ev, LV_EVENT_CLICKED, NULL);
    lv_obj_t * close_ico = lv_label_create(close_btn);
    lv_label_set_text(close_ico, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(close_ico, lv_color_hex(0xFF6666), 0);
    lv_obj_center(close_ico);
}

/* ── Public lifecycle ──────────────────────────────────────── */

void ui_Screen9_screen_init(void)
{
    /* Clear stale button references from a previous init */
    int i;
    for(i = 0; i < (int)BUZZ_TONE_COUNT; i++) s_tone_btns[i] = NULL;
    s_uname_overlay = NULL;
    s_uname_kbd     = NULL;
    s_uname_input   = NULL;
    s_uname_display = NULL;

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
    build_section_label(ui_Screen9, "DESKCHAT", 232);
    build_deskchat_section(ui_Screen9);
    build_uname_overlay(ui_Screen9);
}

void ui_Screen9_screen_destroy(void)
{
    if(ui_Screen9) lv_obj_delete(ui_Screen9);
    ui_Screen9 = NULL;
    int i;
    for(i = 0; i < (int)BUZZ_TONE_COUNT; i++) s_tone_btns[i] = NULL;
    s_uname_overlay = NULL;
    s_uname_kbd     = NULL;
    s_uname_input   = NULL;
    s_uname_display = NULL;
}
