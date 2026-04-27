// Project  : HelpDesk
// File     : ui_Screen12.cpp
// Purpose  : WiFi settings screen — toggle, SSID, password, connection status
// Depends  : ui.h (LVGL 9.x), wifi_connect.h, wifi_status.h, settings.h, get_time.h, WiFi.h

#include "ui.h"
#include "wifi_connect.h"
#include "wifi_status.h"
#include "settings.h"
#include "get_time.h"
#include <WiFi.h>

/* ── Colours ────────────────────────────────────────────────── */
#define CLR_BG          0x1A1A2E
#define CLR_HDR         0x16213E
#define CLR_CARD        0x0D1321
#define CLR_INPUT       0x0A0F1C
#define CLR_ACCENT      0x2196F3   /* Blue — WiFi identity colour  */
#define CLR_GREEN       0x4CAF50
#define CLR_ORANGE      0xFF9800
#define CLR_RED         0xF44336
#define CLR_YELLOW      0xFFC107
#define CLR_MUTED       0x546E7A

/* ── Dimensions ──────────────────────────────────────────────── */
#define SCREEN_W    480
#define HDR_H        36
#define KBD_H       200

/* y-positions for the form sections */
#define Y_STATUS_CARD  44
#define STATUS_CARD_H  62
#define Y_SSID        114    /* label at Y_SSID, textarea at Y_SSID+18 */
#define Y_PASS        168    /* label at Y_PASS, textarea at Y_PASS+18  */
#define Y_CONNECT     228
#define Y_WIFI_TOGGLE 276

/* Textarea dimensions */
#define TA_H          34
#define TA_FULL_W     (SCREEN_W - 24)          /* SSID field — full width */
#define TA_PASS_W     (SCREEN_W - 24 - 68)     /* Password — room for Show/Hide btn */
#define SHOW_BTN_W    60

/* ── Private state ───────────────────────────────────────────── */
lv_obj_t * ui_Screen12     = NULL;

static lv_obj_t  * s_status_icon  = NULL;
static lv_obj_t  * s_status_lbl   = NULL;
static lv_obj_t  * s_detail_lbl   = NULL;
static lv_obj_t  * s_ssid_ta      = NULL;
static lv_obj_t  * s_pass_ta      = NULL;
static lv_obj_t  * s_kbd          = NULL;
static lv_obj_t  * s_wifi_sw      = NULL;
static lv_timer_t * s_timer       = NULL;
static bool         s_pass_shown  = false;

/* ── Status update ───────────────────────────────────────────── */

static void update_status(void)
{
    if (!s_status_lbl) return;

    const char * status_text;
    lv_color_t   icon_color;

    if (WiFi.getMode() == WIFI_MODE_NULL) {
        status_text = "WiFi Off";
        icon_color  = lv_color_hex(CLR_MUTED);
    } else {
        switch (WiFi.status()) {
            case WL_CONNECTED:
                status_text = "Connected";
                icon_color  = lv_color_hex(CLR_GREEN);
                break;
            case WL_IDLE_STATUS:
                status_text = "Connecting\xe2\x80\xa6";
                icon_color  = lv_color_hex(CLR_YELLOW);
                break;
            case WL_NO_SSID_AVAIL:
                status_text = "Network not found";
                icon_color  = lv_color_hex(CLR_ORANGE);
                break;
            case WL_CONNECT_FAILED:
                status_text = "Auth failed";
                icon_color  = lv_color_hex(CLR_RED);
                break;
            case WL_CONNECTION_LOST:
                status_text = "Connection lost";
                icon_color  = lv_color_hex(CLR_ORANGE);
                break;
            default:
                status_text = "Offline";
                icon_color  = lv_color_hex(CLR_MUTED);
                break;
        }
    }

    lv_obj_set_style_text_color(s_status_icon, icon_color, 0);
    lv_label_set_text(s_status_lbl, status_text);

    if (WiFi.status() == WL_CONNECTED) {
        char detail[80];
        int  rssi    = WiFi.RSSI();
        const char * quality =
            (rssi >= -50) ? "Excellent" :
            (rssi >= -60) ? "Good"      :
            (rssi >= -70) ? "Fair"      : "Weak";
        snprintf(detail, sizeof(detail),
                 "IP: %s    Signal: %s (%d dBm)",
                 WiFi.localIP().toString().c_str(), quality, rssi);
        lv_label_set_text(s_detail_lbl, detail);
    } else {
        lv_label_set_text(s_detail_lbl, "Not connected");
    }
}

static void status_timer_cb(lv_timer_t * t)
{
    (void)t;
    update_status();
}

/* ── Event callbacks ─────────────────────────────────────────── */

static void back_btn_ev(lv_event_t * e)
{
    (void)e;
    _ui_screen_change(&ui_Screen1, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, 300, 0,
                      ui_Screen1_screen_init);
}

static void ta_focused_ev(lv_event_t * e)
{
    lv_obj_t * ta = (lv_obj_t *)lv_event_get_target(e);
    lv_keyboard_set_textarea(s_kbd, ta);
    lv_obj_remove_flag(s_kbd, LV_OBJ_FLAG_HIDDEN);
}

static void kbd_hide_ev(lv_event_t * e)
{
    (void)e;
    lv_obj_add_flag(s_kbd, LV_OBJ_FLAG_HIDDEN);
}

static void connect_btn_ev(lv_event_t * e)
{
    (void)e;
    lv_obj_add_flag(s_kbd, LV_OBJ_FLAG_HIDDEN);

    const char * ssid = lv_textarea_get_text(s_ssid_ta);
    const char * pass = lv_textarea_get_text(s_pass_ta);

    settingsSetWifiSSID(ssid);
    settingsSetWifiPassword(pass);
    settingsSave();

    WiFi.disconnect(false);
    connectToWiFi();
    update_status();
}

static void wifi_sw_ev(lv_event_t * e)
{
    lv_obj_t * sw = (lv_obj_t *)lv_event_get_target(e);
    bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);

    if (enabled) {
        WiFi.mode(WIFI_STA);
        connectToWiFi();
    } else {
        WiFi.mode(WIFI_OFF);
    }
    update_status();
}

static void show_pass_ev(lv_event_t * e)
{
    lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);
    s_pass_shown = !s_pass_shown;
    lv_textarea_set_password_mode(s_pass_ta, !s_pass_shown);

    lv_obj_t * lbl = lv_obj_get_child(btn, 0);
    if (lbl) lv_label_set_text(lbl, s_pass_shown ? "Hide" : "Show");
}

/* ── Build helpers ───────────────────────────────────────────── */

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
    lv_obj_add_event_cb(back_btn, back_btn_ev, LV_EVENT_CLICKED, NULL);

    lv_obj_t * back_ico = lv_label_create(back_btn);
    lv_label_set_text(back_ico, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_ico, lv_color_white(), 0);
    lv_obj_center(back_ico);

    lv_obj_t * title = lv_label_create(hdr);
    lv_label_set_text(title, LV_SYMBOL_WIFI "  WiFi Settings");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_center(title);

    uiAddHeaderClock(hdr, -8);
}

static void build_status_card(lv_obj_t * scr)
{
    lv_obj_t * card = lv_obj_create(scr);
    lv_obj_set_size(card, SCREEN_W - 24, STATUS_CARD_H);
    lv_obj_set_pos(card, 12, Y_STATUS_CARD);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(card, lv_color_hex(CLR_CARD), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_30, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 8, 0);

    s_status_icon = lv_label_create(card);
    lv_label_set_text(s_status_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(s_status_icon, lv_color_hex(CLR_MUTED), 0);
    lv_obj_set_pos(s_status_icon, 0, 4);

    s_status_lbl = lv_label_create(card);
    lv_label_set_text(s_status_lbl, "Checking...");
    lv_obj_set_style_text_color(s_status_lbl, lv_color_white(), 0);
    lv_obj_set_pos(s_status_lbl, 28, 2);

    s_detail_lbl = lv_label_create(card);
    lv_label_set_text(s_detail_lbl, "");
    lv_obj_set_style_text_color(s_detail_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(s_detail_lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(s_detail_lbl, 0, 32);
    lv_obj_set_size(s_detail_lbl, SCREEN_W - 40, LV_SIZE_CONTENT);
}

static void build_textarea(lv_obj_t * scr, const char * label_text,
                            lv_obj_t ** ta_out, int y, bool is_password)
{
    lv_obj_t * lbl = lv_label_create(scr);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_pos(lbl, 12, y);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);

    int ta_w = is_password ? TA_PASS_W : TA_FULL_W;

    lv_obj_t * ta = lv_textarea_create(scr);
    lv_obj_set_size(ta, ta_w, TA_H);
    lv_obj_set_pos(ta, 12, y + 18);
    lv_textarea_set_one_line(ta, true);
    lv_obj_set_style_bg_color(ta, lv_color_hex(CLR_INPUT), 0);
    lv_obj_set_style_text_color(ta, lv_color_white(), 0);
    lv_obj_set_style_border_color(ta, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_radius(ta, 6, 0);
    lv_obj_set_style_pad_hor(ta, 8, 0);
    lv_textarea_set_password_mode(ta, is_password);
    lv_obj_add_event_cb(ta, ta_focused_ev, LV_EVENT_FOCUSED, NULL);
    *ta_out = ta;

    if (!is_password) return;

    /* Show / Hide button to the right of the password field */
    lv_obj_t * eye_btn = lv_button_create(scr);
    lv_obj_set_size(eye_btn, SHOW_BTN_W, TA_H);
    lv_obj_set_pos(eye_btn, 12 + ta_w + 8, y + 18);
    lv_obj_set_style_bg_color(eye_btn, lv_color_hex(0x2C3E50), 0);
    lv_obj_set_style_bg_color(eye_btn, lv_color_hex(0x1A252F),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(eye_btn, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_border_width(eye_btn, 1, 0);
    lv_obj_set_style_radius(eye_btn, 6, 0);
    lv_obj_set_style_shadow_width(eye_btn, 0, 0);
    lv_obj_add_event_cb(eye_btn, show_pass_ev, LV_EVENT_CLICKED, NULL);

    lv_obj_t * eye_lbl = lv_label_create(eye_btn);
    lv_label_set_text(eye_lbl, "Show");
    lv_obj_set_style_text_color(eye_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(eye_lbl, &lv_font_montserrat_10, 0);
    lv_obj_center(eye_lbl);
}

static void build_connect_button(lv_obj_t * scr)
{
    lv_obj_t * btn = lv_button_create(scr);
    lv_obj_set_size(btn, 200, 36);
    lv_obj_set_pos(btn, (SCREEN_W - 200) / 2, Y_CONNECT);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A5C1A), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2E7D32),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, connect_btn_ev, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_WIFI "  Connect");
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);
}

static void build_wifi_toggle(lv_obj_t * scr)
{
    lv_obj_t * lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "WiFi Enabled");
    lv_obj_set_pos(lbl, 12, Y_WIFI_TOGGLE + 4);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);

    s_wifi_sw = lv_switch_create(scr);
    lv_obj_set_size(s_wifi_sw, 52, 26);
    lv_obj_set_pos(s_wifi_sw, SCREEN_W - 72, Y_WIFI_TOGGLE);

    /* Default ON unless the radio is completely off */
    if (WiFi.getMode() != WIFI_MODE_NULL) {
        lv_obj_add_state(s_wifi_sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(s_wifi_sw, wifi_sw_ev, LV_EVENT_VALUE_CHANGED, NULL);
}

static void build_keyboard(lv_obj_t * scr)
{
    s_kbd = lv_keyboard_create(scr);
    lv_obj_set_size(s_kbd, SCREEN_W, KBD_H);
    lv_obj_align(s_kbd, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(s_kbd, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_pad_top(s_kbd, 0, 0);
    lv_obj_set_style_pad_bottom(s_kbd, 0, 0);
    lv_obj_set_style_border_width(s_kbd, 0, 0);
    lv_obj_set_style_shadow_width(s_kbd, 0, 0);
    ui_kbd_apply_space_map(s_kbd);
    lv_obj_add_event_cb(s_kbd, kbd_hide_ev, LV_EVENT_READY,  NULL);
    lv_obj_add_event_cb(s_kbd, kbd_hide_ev, LV_EVENT_CANCEL, NULL);
}

/* ── Public lifecycle ────────────────────────────────────────── */

void ui_Screen12_screen_init(void)
{
    s_pass_shown = false;

    ui_Screen12 = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_Screen12, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen12, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(ui_Screen12, LV_OPA_COVER, 0);

    lv_obj_add_event_cb(ui_Screen12, scr_unloaded_delete_cb,
                        LV_EVENT_SCREEN_UNLOADED, (void*)ui_Screen12_screen_destroy);

    build_header(ui_Screen12);
    build_status_card(ui_Screen12);
    build_textarea(ui_Screen12, "Network Name (SSID)", &s_ssid_ta, Y_SSID, false);
    build_textarea(ui_Screen12, "Password",            &s_pass_ta, Y_PASS, true);

    /* Pre-fill fields from saved settings */
    const char * saved_ssid = settingsGetWifiSSID();
    const char * saved_pass = settingsGetWifiPassword();
    lv_textarea_set_text(s_ssid_ta, saved_ssid ? saved_ssid : "");
    lv_textarea_set_text(s_pass_ta, saved_pass ? saved_pass : "");

    build_connect_button(ui_Screen12);
    build_wifi_toggle(ui_Screen12);

    /* Keyboard overlay — must be built last so it renders on top */
    build_keyboard(ui_Screen12);

    s_timer = lv_timer_create(status_timer_cb, 2000, NULL);
    update_status();
}

void ui_Screen12_screen_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }

    s_status_icon = NULL;
    s_status_lbl  = NULL;
    s_detail_lbl  = NULL;
    s_ssid_ta     = NULL;
    s_pass_ta     = NULL;
    s_kbd         = NULL;
    s_wifi_sw     = NULL;

    uiClearHeaderClock(ui_Screen12);
    if (ui_Screen12) lv_obj_delete(ui_Screen12);
    ui_Screen12 = NULL;
}
