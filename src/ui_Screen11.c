// Project  : HelpDesk
// File     : ui_Screen11.c
// Purpose  : DeskChat — LoRa group-chat screen (LVGL 9.x)
// Depends  : ui.h, deskchat.h, settings.h

#include "ui.h"
#include "deskchat.h"
#include "settings.h"
#include "handshake.h"
#include <string.h>
#include <stdio.h>

/* ── Colours ──────────────────────────────────────────────────────────────── */
#define CLR_BG          0x0A0F1A   /* Near-black background                  */
#define CLR_HDR         0x0D2137   /* Header bar                             */
#define CLR_ACCENT      0x00E676   /* Bright green — off-grid terminal feel  */
#define CLR_ACCENT_DIM  0x114422   /* Dimmed green for borders               */
#define CLR_OWN_MSG     0x1A3A1A   /* Own message bubble background          */
#define CLR_PEER_MSG    0x111B2E   /* Peer message bubble background         */
#define CLR_SYS_MSG     0x1A1A0A   /* System / observe packet background     */
#define CLR_USERNAME    0x00E676   /* Own username                           */
#define CLR_PEER_NAME   0x40C4FF   /* Peer username                          */
#define CLR_SYS_TEXT    0x888844   /* System / raw packet text               */
#define CLR_MUTED       0x445566   /* De-emphasised text                     */
#define CLR_FOOTER      0x060D18   /* Compose bar background                 */
#define CLR_INPUT_BG    0x0E1E2D   /* Text-input area background             */
#define CLR_KBD_BG      0x040810   /* Keyboard overlay background            */
#define CLR_WARN        0xFF5722   /* Radio-offline warning                  */

/* ── Layout ───────────────────────────────────────────────────────────────── */
#define SCREEN_W        480
#define SCREEN_H        320
#define HDR_H            34
#define FOOTER_H         42
#define LIST_Y           HDR_H
#define LIST_H          (SCREEN_H - HDR_H - FOOTER_H)   /* 244 px           */
#define KBD_H           200
/* When keyboard is open the input bar floats just above it                   */
#define INPUT_Y_KBD     (SCREEN_H - KBD_H - 40)

/* Max characters the user may type in one message                            */
#define MSG_MAX         160

/* Max message bubbles kept in the scrollable list before the oldest          */
/* is removed to cap RAM usage                                                */
#define BUBBLE_MAX       80

/* ── Public widget handles ────────────────────────────────────────────────── */
lv_obj_t * ui_Screen11       = NULL;
lv_obj_t * ui_ChatList       = NULL;
lv_obj_t * ui_ChatStatusLabel = NULL;

/* ── Private state ────────────────────────────────────────────────────────── */
static lv_obj_t * s_kbd_overlay  = NULL;
static lv_obj_t * s_kbd          = NULL;
static lv_obj_t * s_input_area   = NULL;   /* textarea in keyboard overlay   */
static lv_obj_t * s_footer_hint  = NULL;   /* "Tap to type..." in footer bar */
static lv_obj_t * s_observe_btn  = NULL;
static int        s_bubble_count = 0;
static lv_timer_t * s_rssi_timer  = NULL;

/* ── Helper: keyboard overlay show / hide ────────────────────────────────── */
static void show_keyboard(void)
{
    if (!s_kbd_overlay) return;
    lv_obj_remove_flag(s_kbd_overlay, LV_OBJ_FLAG_HIDDEN);
    if (s_kbd) lv_keyboard_set_textarea(s_kbd, s_input_area);
    if (s_input_area) {
        lv_textarea_set_text(s_input_area, "");
        lv_obj_add_state(s_input_area, LV_STATE_FOCUSED);
    }
}

static void hide_keyboard(void)
{
    if (!s_kbd_overlay) return;
    lv_obj_add_flag(s_kbd_overlay, LV_OBJ_FLAG_HIDDEN);
    if (s_input_area) lv_textarea_set_text(s_input_area, "");
}

/* ── Helper: trim bubble list ────────────────────────────────────────────── */
static void trim_bubbles(void)
{
    if (!ui_ChatList) return;
    while (s_bubble_count > BUBBLE_MAX) {
        lv_obj_t *first = lv_obj_get_child(ui_ChatList, 0);
        if (!first) break;
        lv_obj_delete(first);
        s_bubble_count--;
    }
}

/* ── Message bubble builder ──────────────────────────────────────────────── */
static void add_bubble(const char *user, const char *id,
                        const char *msg, int rssi, bool is_own, bool is_raw)
{
    if (!ui_ChatList) return;

    /* Outer bubble container */
    lv_obj_t *row = lv_obj_create(ui_ChatList);
    lv_obj_set_width(row, SCREEN_W - 8);
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_shadow_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 4, 0);

    uint32_t bg_col = is_own  ? CLR_OWN_MSG  :
                      is_raw  ? CLR_SYS_MSG  : CLR_PEER_MSG;
    lv_obj_set_style_bg_color(row, lv_color_hex(bg_col), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);

    /* Sender label */
    lv_obj_t *name_lbl = lv_label_create(row);
    lv_obj_set_width(name_lbl, SCREEN_W - 24);

    if (is_raw) {
        lv_label_set_text(name_lbl, LV_SYMBOL_REFRESH "  [observe]");
        lv_obj_set_style_text_color(name_lbl, lv_color_hex(CLR_SYS_TEXT), 0);
    } else if (is_own) {
        char buf[48];
        snprintf(buf, sizeof(buf), LV_SYMBOL_RIGHT "  %s",
                 user[0] ? user : "You");
        lv_label_set_text(name_lbl, buf);
        lv_obj_set_style_text_color(name_lbl, lv_color_hex(CLR_USERNAME), 0);
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s  [%s]",
                 user[0] ? user : "Anon",
                 id[0]   ? id   : "?");
        lv_label_set_text(name_lbl, buf);
        lv_obj_set_style_text_color(name_lbl, lv_color_hex(CLR_PEER_NAME), 0);
    }
    lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(name_lbl, 4, 0);

    /* Message body */
    lv_obj_t *msg_lbl = lv_label_create(row);
    lv_label_set_text(msg_lbl, msg);
    lv_obj_set_width(msg_lbl, SCREEN_W - 24);
    lv_label_set_long_mode(msg_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(msg_lbl, 4, 14);

    if (is_raw) {
        lv_obj_set_style_text_color(msg_lbl, lv_color_hex(CLR_SYS_TEXT), 0);
        lv_obj_set_style_text_font(msg_lbl, &lv_font_montserrat_10, 0);
    } else {
        lv_obj_set_style_text_color(msg_lbl, lv_color_white(), 0);
        lv_obj_set_style_text_font(msg_lbl, &lv_font_montserrat_12, 0);
    }

    /* RSSI badge (not shown for own TX or rssi=0) */
    if (rssi != 0 && !is_own) {
        lv_obj_t *rssi_lbl = lv_label_create(row);
        char rbuf[12];
        snprintf(rbuf, sizeof(rbuf), "%d dBm", rssi);
        lv_label_set_text(rssi_lbl, rbuf);
        lv_obj_set_style_text_color(rssi_lbl, lv_color_hex(CLR_MUTED), 0);
        lv_obj_set_style_text_font(rssi_lbl, &lv_font_montserrat_8, 0);
        lv_obj_align(rssi_lbl, LV_ALIGN_TOP_RIGHT, -4, 0);
    }

    /* Auto-scroll to bottom */
    lv_obj_scroll_to_y(ui_ChatList, LV_COORD_MAX, LV_ANIM_ON);

    s_bubble_count++;
    trim_bubbles();
}

/* ── DeskChat callback (called from deskchat module) ─────────────────────── */
static void on_message(const char *user, const char *id,
                        const char *msg, int rssi)
{
    if (!ui_Screen11) return;   /* Screen not mounted — skip */

    bool is_own = false;
    if (id && id[0] != '\0') {
        char my_id[9];
        deskChatGetDeviceID(my_id, sizeof(my_id));
        is_own = (strcmp(id, my_id) == 0);
    }
    bool is_raw = (!user && !id);   /* observe-mode raw packet */

    add_bubble(user ? user : "?",
               id   ? id   : "?",
               msg, rssi, is_own, is_raw);
}

/* ── Event callbacks ─────────────────────────────────────────────────────── */
static void back_ev(lv_event_t *e)
{
    (void)e;
    _ui_screen_change(&ui_Screen1, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, 300, 0,
                      ui_Screen1_screen_init);
}

static void send_ev(lv_event_t *e)
{
    (void)e;
    if (!s_input_area) return;
    const char *text = lv_textarea_get_text(s_input_area);
    if (!text || text[0] == '\0') return;

    if (!deskChatReady()) {
        /* Radio offline — show a system bubble warning */
        add_bubble("", "", "Radio not ready. Check LoRa hardware.", 0, false, true);
        hide_keyboard();
        return;
    }
    deskChatSend(text);
    hide_keyboard();
}

static void kbd_ready_ev(lv_event_t *e)
{
    (void)e;
    send_ev(NULL);
}

static void kbd_cancel_ev(lv_event_t *e)
{
    (void)e;
    hide_keyboard();
}

/* Tapping the "tap to type" footer area opens the keyboard */
static void footer_tap_ev(lv_event_t *e)
{
    (void)e;
    show_keyboard();
}

/* Tapping the input textarea inside the overlay keeps focus */
static void input_clicked_ev(lv_event_t *e)
{
    (void)e;
    if (s_kbd_overlay && lv_obj_has_flag(s_kbd_overlay, LV_OBJ_FLAG_HIDDEN)) {
        show_keyboard();
    }
}

static void observe_btn_ev(lv_event_t *e)
{
    (void)e;
    bool now = !deskChatGetObserveMode();
    deskChatSetObserveMode(now);
    if (!s_observe_btn) return;
    if (now) {
        lv_obj_set_style_bg_color(s_observe_btn, lv_color_hex(0x1A3A0A), 0);
        lv_label_set_text(lv_obj_get_child(s_observe_btn, 0),
                          LV_SYMBOL_EYE_OPEN "  Listen: ON");
    } else {
        lv_obj_set_style_bg_color(s_observe_btn, lv_color_hex(0x1A1A2E), 0);
        lv_label_set_text(lv_obj_get_child(s_observe_btn, 0),
                          LV_SYMBOL_EYE_OPEN "  Listen: OFF");
    }
}

/* ── Periodic RSSI / status label refresh ────────────────────────────────── */
static void rssi_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (!ui_ChatStatusLabel) return;
    char buf[48];
    if (!deskChatReady()) {
        lv_label_set_text(ui_ChatStatusLabel, LV_SYMBOL_WARNING "  No radio");
        lv_obj_set_style_text_color(ui_ChatStatusLabel,
                                    lv_color_hex(CLR_WARN), 0);
        return;
    }
    int rssi = deskChatLastRSSI();
    if (rssi == 0) {
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI "  Listening...");
    } else {
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI "  %d dBm", rssi);
    }
    lv_label_set_text(ui_ChatStatusLabel, buf);
    lv_obj_set_style_text_color(ui_ChatStatusLabel,
                                lv_color_hex(CLR_ACCENT), 0);
}

/* ── Section builders ────────────────────────────────────────────────────── */
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

    /* Back button */
    lv_obj_t *back = lv_button_create(hdr);
    lv_obj_set_size(back, 40, HDR_H - 2);
    lv_obj_set_pos(back, 2, 1);
    lv_obj_set_style_bg_color(back, lv_color_hex(CLR_HDR), 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x020A10),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_set_style_shadow_width(back, 0, 0);
    lv_obj_set_style_pad_all(back, 0, 0);
    lv_obj_add_event_cb(back, back_ev, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_ico = lv_label_create(back);
    lv_label_set_text(back_ico, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_ico, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_center(back_ico);

    /* Title */
    lv_obj_t *title = lv_label_create(hdr);
    lv_label_set_text(title, LV_SYMBOL_WIFI "  DeskChat");
    lv_obj_set_style_text_color(title, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    /* RSSI / status (right side) */
    ui_ChatStatusLabel = lv_label_create(hdr);
    lv_label_set_text(ui_ChatStatusLabel, "...");
    lv_obj_set_style_text_color(ui_ChatStatusLabel, lv_color_hex(CLR_MUTED), 0);
    lv_obj_set_style_text_font(ui_ChatStatusLabel, &lv_font_montserrat_10, 0);
    lv_obj_align(ui_ChatStatusLabel, LV_ALIGN_RIGHT_MID, -6, 0);
}

static void build_message_list(lv_obj_t *scr)
{
    ui_ChatList = lv_obj_create(scr);
    lv_obj_set_pos(ui_ChatList, 0, LIST_Y);
    lv_obj_set_size(ui_ChatList, SCREEN_W, LIST_H);
    lv_obj_set_style_bg_color(ui_ChatList, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(ui_ChatList, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui_ChatList, 0, 0);
    lv_obj_set_style_radius(ui_ChatList, 0, 0);
    lv_obj_set_style_pad_all(ui_ChatList, 4, 0);
    lv_obj_set_style_pad_row(ui_ChatList, 3, 0);
    lv_obj_set_flex_flow(ui_ChatList, LV_FLEX_FLOW_COLUMN);
}

static void build_footer(lv_obj_t *scr)
{
    lv_obj_t *footer = lv_obj_create(scr);
    lv_obj_set_pos(footer, 0, SCREEN_H - FOOTER_H);
    lv_obj_set_size(footer, SCREEN_W, FOOTER_H);
    lv_obj_remove_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(footer, lv_color_hex(CLR_FOOTER), 0);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(footer, lv_color_hex(CLR_ACCENT_DIM), 0);
    lv_obj_set_style_border_width(footer, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(footer, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_radius(footer, 0, 0);
    lv_obj_set_style_pad_all(footer, 4, 0);

    /* "Tap to type..." hint — tapping opens keyboard */
    s_footer_hint = lv_label_create(footer);
    const char *uname = settingsGetChatUsername();
    char hint[48];
    snprintf(hint, sizeof(hint), "%s  |  tap to type...",
             uname[0] ? uname : "Anon");
    lv_label_set_text(s_footer_hint, hint);
    lv_obj_set_style_text_color(s_footer_hint, lv_color_hex(CLR_MUTED), 0);
    lv_obj_set_style_text_font(s_footer_hint, &lv_font_montserrat_12, 0);
    lv_obj_align(s_footer_hint, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_add_flag(footer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(footer, footer_tap_ev, LV_EVENT_CLICKED, NULL);

    /* Observe / listen mode toggle */
    s_observe_btn = lv_button_create(footer);
    lv_obj_set_size(s_observe_btn, 110, 30);
    lv_obj_align(s_observe_btn, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_color(s_observe_btn, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_color(s_observe_btn, lv_color_hex(CLR_ACCENT_DIM), 0);
    lv_obj_set_style_border_width(s_observe_btn, 1, 0);
    lv_obj_set_style_radius(s_observe_btn, 6, 0);
    lv_obj_set_style_shadow_width(s_observe_btn, 0, 0);
    lv_obj_add_event_cb(s_observe_btn, observe_btn_ev, LV_EVENT_CLICKED, NULL);
    lv_obj_t *obs_lbl = lv_label_create(s_observe_btn);
    lv_label_set_text(obs_lbl, LV_SYMBOL_EYE_OPEN "  Listen: OFF");
    lv_obj_set_style_text_color(obs_lbl, lv_color_hex(CLR_MUTED), 0);
    lv_obj_set_style_text_font(obs_lbl, &lv_font_montserrat_10, 0);
    lv_obj_center(obs_lbl);
}

/* Full-screen keyboard overlay:
   - Semi-opaque dark layer covers chat list (user can still read context)
   - Input bar sits just above the keyboard — always visible while typing
   - Close button dismisses without sending                                  */
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

    /* ── Keyboard (200 px, anchored to bottom) ── */
    s_kbd = lv_keyboard_create(s_kbd_overlay);
    lv_obj_set_size(s_kbd, SCREEN_W, KBD_H);
    lv_obj_align(s_kbd, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_top(s_kbd, 0, 0);
    lv_obj_set_style_pad_bottom(s_kbd, 0, 0);
    lv_obj_set_style_border_width(s_kbd, 0, 0);
    lv_obj_set_style_shadow_width(s_kbd, 0, 0);
    lv_obj_add_event_cb(s_kbd, kbd_ready_ev,  LV_EVENT_READY,  NULL);
    lv_obj_add_event_cb(s_kbd, kbd_cancel_ev, LV_EVENT_CANCEL, NULL);

    /* ── Input textarea — sits just above the keyboard so it's ALWAYS ──
       visible while typing.  Position: SCREEN_H - KBD_H - 40 = 80 px   */
    s_input_area = lv_textarea_create(s_kbd_overlay);
    lv_obj_set_size(s_input_area, SCREEN_W - 60, 36);
    lv_obj_set_pos(s_input_area, 4, SCREEN_H - KBD_H - 40);
    lv_textarea_set_one_line(s_input_area, true);
    lv_textarea_set_max_length(s_input_area, MSG_MAX);
    lv_textarea_set_placeholder_text(s_input_area, "Message...");
    lv_obj_set_style_bg_color(s_input_area, lv_color_hex(CLR_INPUT_BG), 0);
    lv_obj_set_style_text_color(s_input_area, lv_color_white(), 0);
    lv_obj_set_style_border_color(s_input_area, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_border_width(s_input_area, 1, 0);
    lv_obj_set_style_radius(s_input_area, 8, 0);
    lv_obj_set_style_pad_hor(s_input_area, 8, 0);
    lv_obj_set_style_pad_ver(s_input_area, 4, 0);
    lv_obj_set_style_shadow_width(s_input_area, 0, 0);
    lv_obj_add_event_cb(s_input_area, input_clicked_ev, LV_EVENT_CLICKED, NULL);
    lv_keyboard_set_textarea(s_kbd, s_input_area);

    /* ── Send button — right of input ── */
    lv_obj_t *send_btn = lv_button_create(s_kbd_overlay);
    lv_obj_set_size(send_btn, 52, 36);
    lv_obj_set_pos(send_btn, SCREEN_W - 56, SCREEN_H - KBD_H - 40);
    lv_obj_set_style_bg_color(send_btn, lv_color_hex(CLR_ACCENT_DIM), 0);
    lv_obj_set_style_bg_color(send_btn, lv_color_hex(0x003311),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(send_btn, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_border_width(send_btn, 1, 0);
    lv_obj_set_style_radius(send_btn, 8, 0);
    lv_obj_set_style_shadow_width(send_btn, 0, 0);
    lv_obj_add_event_cb(send_btn, send_ev, LV_EVENT_CLICKED, NULL);
    lv_obj_t *send_ico = lv_label_create(send_btn);
    lv_label_set_text(send_ico, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(send_ico, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_center(send_ico);

    /* ── Close button — top-right corner of overlay ── */
    lv_obj_t *close_btn = lv_button_create(s_kbd_overlay);
    lv_obj_set_size(close_btn, 44, 32);
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, -4, 4);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x220A0A), 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x440808),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(close_btn, 0, 0);
    lv_obj_set_style_radius(close_btn, 8, 0);
    lv_obj_set_style_shadow_width(close_btn, 0, 0);
    lv_obj_add_event_cb(close_btn, kbd_cancel_ev, LV_EVENT_CLICKED, NULL);
    lv_obj_t *close_ico = lv_label_create(close_btn);
    lv_label_set_text(close_ico, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(close_ico, lv_color_hex(0xFF6666), 0);
    lv_obj_center(close_ico);
}

/* ── Welcome / status system message ────────────────────────────────────── */
static void push_system_message(const char *text)
{
    add_bubble("", "", text, 0, false, true);
}

/* ── Public lifecycle ────────────────────────────────────────────────────── */
void ui_Screen11_screen_init(void)
{
    handshakeSetScreen("deskchat");
    s_bubble_count = 0;
    s_kbd_overlay  = NULL;
    s_kbd          = NULL;
    s_input_area   = NULL;
    s_footer_hint  = NULL;
    s_observe_btn  = NULL;
    s_rssi_timer   = NULL;

    ui_Screen11 = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_Screen11, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen11, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(ui_Screen11, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(ui_Screen11, scr_unloaded_delete_cb,
                        LV_EVENT_SCREEN_UNLOADED, ui_Screen11_screen_destroy);

    build_header(ui_Screen11);
    build_message_list(ui_Screen11);
    build_footer(ui_Screen11);
    build_keyboard_overlay(ui_Screen11);

    /* Register LoRa message callback */
    deskChatSetCallback(on_message);

    /* Replay saved chat history */
    deskChatHistoryLoad(on_message);

    /* Show radio status */
    if (!deskChatReady()) {
        push_system_message(
            "LoRa radio not detected.\n"
            "Check pin definitions in deskchat.cpp and\n"
            "verify your SX1262 module is connected.");
    } else {
        char id[9];
        deskChatGetDeviceID(id, sizeof(id));
        char welcome[64];
        snprintf(welcome, sizeof(welcome),
                 "DeskChat ready.  Device ID: %s", id);
        push_system_message(welcome);
    }

    /* Periodic header status refresh */
    s_rssi_timer = lv_timer_create(rssi_timer_cb, 3000, NULL);
    rssi_timer_cb(NULL);
}

void ui_Screen11_screen_destroy(void)
{
    deskChatSetCallback(NULL);
    ui_ChatStatusLabel = NULL;
    ui_ChatList        = NULL;
    s_kbd_overlay      = NULL;
    s_kbd              = NULL;
    s_input_area       = NULL;
    s_footer_hint      = NULL;
    s_observe_btn      = NULL;
    s_bubble_count     = 0;

    if (s_rssi_timer) {
        lv_timer_delete(s_rssi_timer);
        s_rssi_timer = NULL;
    }
    if (ui_Screen11) {
        lv_obj_delete(ui_Screen11);
        ui_Screen11 = NULL;
    }
}

/* ── External push (callable from deskchat module) ───────────────────────── */
void ui_Screen11_push_message(const char *user, const char *id,
                               const char *msg, int rssi)
{
    on_message(user, id, msg, rssi);
}
