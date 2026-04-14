// Project  : HelpDesk
// File     : ui_Screen10.cpp
// Purpose  : DeskDrive — touch-optimised SD card file manager (LVGL 9.x)
// Depends  : ui.h, ui_Screen10.h, sd_card.h, handshake.h, settings.h, <SD.h>, <WiFi.h>, <HTTPClient.h>

#include "ui.h"
#include "sd_card.h"
#include "handshake.h"
#include "settings.h"
#include <Arduino.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ── Layout ────────────────────────────────────────────────────────────────────
#define SCREEN_W      480
#define SCREEN_H      320
#define HDR_H          36
#define INFO_BAR_H     20
#define LIST_Y        (HDR_H + INFO_BAR_H)
#define LIST_H        (SCREEN_H - LIST_Y)

// ── Colours ───────────────────────────────────────────────────────────────────
#define CLR_BG        0x0F172A
#define CLR_HDR       0x0D3B4F
#define CLR_ACCENT    0x00B4D8   /* cyan — DeskDrive brand */
#define CLR_INFO_BG   0x0A1929
#define CLR_ITEM_BG   0x1E293B
#define CLR_PRESSED   0x334155
#define CLR_FOLDER    0xFFB703
#define CLR_TXT       0x4CC9F0
#define CLR_IMG       0x80B918
#define CLR_AUDIO     0xF77F00
#define CLR_OTHER     0x8D99AE
#define CLR_MUTED     0x64748B

// ── Editor overlay dimensions ─────────────────────────────────────────────────
#define EDITOR_TA_H   110                              /* textarea height          */
#define EDITOR_KBD_Y  (HDR_H + EDITOR_TA_H)           /* y where keyboard starts  */
#define EDITOR_KBD_H  (SCREEN_H - EDITOR_KBD_Y)       /* keyboard height = 174 px */

// ── Per-entry storage (static, no heap per item) ──────────────────────────────
#define MAX_ENTRIES   64
#define MAX_PATH_LEN  128

typedef enum { FTYPE_FOLDER, FTYPE_TEXT, FTYPE_IMAGE, FTYPE_AUDIO, FTYPE_OTHER } file_type_t;

static struct {
    char path[MAX_PATH_LEN];
    bool is_dir;
} s_entries[MAX_ENTRIES];

static int s_entry_count = 0;

// ── Module state ──────────────────────────────────────────────────────────────
lv_obj_t * ui_Screen10    = NULL;
static char       s_cwd[MAX_PATH_LEN] = "/";
static lv_obj_t * s_list              = NULL;
static lv_obj_t * s_path_label        = NULL;
static lv_obj_t * s_viewer_overlay    = NULL;
/* Text editor */
static lv_obj_t * s_editor_ta         = NULL;
static lv_obj_t * s_editor_kbd        = NULL;
static char       s_edit_path[MAX_PATH_LEN] = "";
/* Image conversion */
static lv_obj_t * s_convert_label     = NULL;
static char       s_convert_path[MAX_PATH_LEN] = "";
static bool       s_convert_busy      = false;
/* PSRAM buffer for binary image viewer */
static uint8_t *       s_view_img_buf = NULL;
static lv_image_dsc_t  s_view_img_dsc;

// ── Forward declarations ──────────────────────────────────────────────────────
static void navigate_to(const char * path);
static void close_viewer(void);
static void open_text_editor(const char * path);
static void open_bin_viewer(const char * path);
static void open_convert_overlay(const char * path);

// ── File type helpers ─────────────────────────────────────────────────────────

static file_type_t get_file_type(const char * name, bool is_dir)
{
    if (is_dir) return FTYPE_FOLDER;
    const char * dot = strrchr(name, '.');
    if (!dot) return FTYPE_OTHER;
    char ext[8] = {};
    int i = 0;
    const char * p = dot + 1;
    while (*p && i < 7) ext[i++] = (char)tolower((unsigned char)*p++);
    if (!strcmp(ext,"txt") || !strcmp(ext,"json") || !strcmp(ext,"md"))  return FTYPE_TEXT;
    if (!strcmp(ext,"bmp") || !strcmp(ext,"jpg")  || !strcmp(ext,"jpeg") ||
        !strcmp(ext,"png") || !strcmp(ext,"bin"))                         return FTYPE_IMAGE;
    if (!strcmp(ext,"mp3"))                                                return FTYPE_AUDIO;
    return FTYPE_OTHER;
}

/* Local unload handler — avoids C++ prohibition on function-pointer → void* cast */
static void screen_unload_ev(lv_event_t * e)
{
    (void)e;
    ui_Screen10_screen_destroy();
}

static const char * icon_for_type(file_type_t t)
{
    switch (t) {
        case FTYPE_FOLDER: return LV_SYMBOL_DIRECTORY;
        case FTYPE_TEXT:   return LV_SYMBOL_FILE;
        case FTYPE_IMAGE:  return LV_SYMBOL_IMAGE;
        case FTYPE_AUDIO:  return LV_SYMBOL_AUDIO;
        default:           return LV_SYMBOL_FILE;
    }
}

static uint32_t color_for_type(file_type_t t)
{
    switch (t) {
        case FTYPE_FOLDER: return CLR_FOLDER;
        case FTYPE_TEXT:   return CLR_TXT;
        case FTYPE_IMAGE:  return CLR_IMG;
        case FTYPE_AUDIO:  return CLR_AUDIO;
        default:           return CLR_OTHER;
    }
}

// ── Overlay shared helpers ────────────────────────────────────────────────────

static void viewer_close_ev(lv_event_t * e)
{
    (void)e;
    close_viewer();
}

static void close_viewer(void)
{
    if (s_viewer_overlay) {
        lv_obj_delete(s_viewer_overlay);
        s_viewer_overlay = NULL;
    }
    if (s_view_img_buf) { free(s_view_img_buf); s_view_img_buf = NULL; }
    s_editor_ta     = NULL;
    s_editor_kbd    = NULL;
    s_convert_label = NULL;
    s_edit_path[0]  = '\0';
    /* s_convert_busy is cleared by convert_done_cb, NOT here, to prevent
       a second conversion launching while the BG task is still in flight. */
}

// ── Text editor overlay ───────────────────────────────────────────────────────

static void editor_save_ev(lv_event_t * e)
{
    (void)e;
    if (!s_editor_ta || s_edit_path[0] == '\0') { close_viewer(); return; }
    const char * text = lv_textarea_get_text(s_editor_ta);
    if (!text) { close_viewer(); return; }
    if (SD.exists(s_edit_path)) SD.remove(s_edit_path);
    File f = SD.open(s_edit_path, FILE_WRITE);
    if (f) { f.print(text); f.close(); }
    close_viewer();
}

static void open_text_editor(const char * path)
{
    if (!sdCardMounted()) return;
    File f = SD.open(path, FILE_READ);
    if (!f) return;

    size_t len = f.size();
    if (len > 8192) len = 8192;
    char * buf = (char *)malloc(len + 1);
    if (!buf) { f.close(); return; }
    f.read((uint8_t *)buf, len);
    buf[len] = '\0';
    f.close();

    strncpy(s_edit_path, path, MAX_PATH_LEN - 1);
    s_edit_path[MAX_PATH_LEN - 1] = '\0';

    s_viewer_overlay = lv_obj_create(ui_Screen10);
    lv_obj_set_size(s_viewer_overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(s_viewer_overlay, 0, 0);
    lv_obj_remove_flag(s_viewer_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_viewer_overlay, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(s_viewer_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_viewer_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_viewer_overlay, 0, 0);

    /* Header bar */
    lv_obj_t * hdr = lv_obj_create(s_viewer_overlay);
    lv_obj_set_size(hdr, SCREEN_W, HDR_H);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_remove_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(CLR_HDR), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);

    lv_obj_t * close_btn = lv_button_create(hdr);
    lv_obj_set_size(close_btn, 44, HDR_H - 4);
    lv_obj_set_pos(close_btn, 2, 2);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(CLR_HDR), 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x0D1321),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(close_btn, 0, 0);
    lv_obj_set_style_shadow_width(close_btn, 0, 0);
    lv_obj_set_style_pad_all(close_btn, 0, 0);
    lv_obj_add_event_cb(close_btn, viewer_close_ev, LV_EVENT_CLICKED, NULL);
    lv_obj_t * close_ico = lv_label_create(close_btn);
    lv_label_set_text(close_ico, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(close_ico, lv_color_white(), 0);
    lv_obj_center(close_ico);

    const char * fname = strrchr(path, '/');
    fname = fname ? fname + 1 : path;
    lv_obj_t * name_lbl = lv_label_create(hdr);
    lv_label_set_text(name_lbl, fname);
    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(name_lbl, SCREEN_W - 120, LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(name_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(name_lbl, LV_ALIGN_CENTER, -25, 0);

    lv_obj_t * save_btn = lv_button_create(hdr);
    lv_obj_set_size(save_btn, 62, HDR_H - 4);
    lv_obj_set_pos(save_btn, SCREEN_W - 64, 2);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x1A5C1A), 0);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x2E7D32),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(save_btn, 0, 0);
    lv_obj_set_style_shadow_width(save_btn, 0, 0);
    lv_obj_set_style_radius(save_btn, 4, 0);
    lv_obj_add_event_cb(save_btn, editor_save_ev, LV_EVENT_CLICKED, NULL);
    lv_obj_t * save_lbl = lv_label_create(save_btn);
    lv_label_set_text(save_lbl, "Save");
    lv_obj_set_style_text_color(save_lbl, lv_color_hex(0x88FF99), 0);
    lv_obj_set_style_text_font(save_lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(save_lbl);

    /* Editable textarea */
    s_editor_ta = lv_textarea_create(s_viewer_overlay);
    lv_obj_set_size(s_editor_ta, SCREEN_W, EDITOR_TA_H);
    lv_obj_set_pos(s_editor_ta, 0, HDR_H);
    lv_textarea_set_text(s_editor_ta, buf);
    lv_textarea_set_cursor_pos(s_editor_ta, 0);
    lv_obj_set_style_bg_color(s_editor_ta, lv_color_hex(0x161B22), 0);
    lv_obj_set_style_text_color(s_editor_ta, lv_color_hex(0xC9D1D9), 0);
    lv_obj_set_style_text_font(s_editor_ta, &lv_font_montserrat_12, 0);
    lv_obj_set_style_border_width(s_editor_ta, 0, 0);
    lv_obj_set_style_pad_all(s_editor_ta, 8, 0);

    /* Keyboard */
    s_editor_kbd = lv_keyboard_create(s_viewer_overlay);
    lv_obj_set_pos(s_editor_kbd, 0, EDITOR_KBD_Y);
    lv_obj_set_size(s_editor_kbd, SCREEN_W, EDITOR_KBD_H);
    lv_obj_set_style_border_width(s_editor_kbd, 0, 0);
    lv_obj_set_style_shadow_width(s_editor_kbd, 0, 0);
    lv_keyboard_set_textarea(s_editor_kbd, s_editor_ta);
    ui_kbd_apply_space_map(s_editor_kbd);

    free(buf);
}

// ── Binary image viewer overlay ───────────────────────────────────────────────

static void open_bin_viewer(const char * path)
{
    if (!sdCardMounted()) return;
    File f = SD.open(path, FILE_READ);
    if (!f) return;

    size_t file_size = f.size();
    if (file_size <= sizeof(lv_image_header_t)) { f.close(); return; }

    if (s_view_img_buf) { free(s_view_img_buf); s_view_img_buf = NULL; }
    s_view_img_buf = (uint8_t *)ps_malloc(file_size);
    if (!s_view_img_buf) { f.close(); return; }
    f.read(s_view_img_buf, file_size);
    f.close();

    memset(&s_view_img_dsc, 0, sizeof(s_view_img_dsc));
    memcpy(&s_view_img_dsc.header, s_view_img_buf, sizeof(lv_image_header_t));
    s_view_img_dsc.data      = s_view_img_buf + sizeof(lv_image_header_t);
    s_view_img_dsc.data_size = (uint32_t)(file_size - sizeof(lv_image_header_t));

    s_viewer_overlay = lv_obj_create(ui_Screen10);
    lv_obj_set_size(s_viewer_overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(s_viewer_overlay, 0, 0);
    lv_obj_remove_flag(s_viewer_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_viewer_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_viewer_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_viewer_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_viewer_overlay, 0, 0);

    lv_obj_t * img = lv_image_create(s_viewer_overlay);
    lv_image_set_src(img, &s_view_img_dsc);
    lv_obj_center(img);

    lv_obj_t * close_btn = lv_button_create(s_viewer_overlay);
    lv_obj_set_size(close_btn, 44, 32);
    lv_obj_set_pos(close_btn, 4, 4);
    lv_obj_set_style_bg_color(close_btn, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(close_btn, LV_OPA_60, 0);
    lv_obj_set_style_border_width(close_btn, 0, 0);
    lv_obj_set_style_shadow_width(close_btn, 0, 0);
    lv_obj_set_style_radius(close_btn, 6, 0);
    lv_obj_add_event_cb(close_btn, viewer_close_ev, LV_EVENT_CLICKED, NULL);
    lv_obj_t * close_ico = lv_label_create(close_btn);
    lv_label_set_text(close_ico, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(close_ico, lv_color_white(), 0);
    lv_obj_center(close_ico);
}

// ── JPG/PNG → companion-app conversion ───────────────────────────────────────

/* Called from LVGL task via lv_async_call after the BG task finishes. */
static void convert_done_cb(void * arg)
{
    bool ok = (bool)(intptr_t)arg;
    s_convert_busy = false;
    if (!ui_Screen10) return;   /* screen was destroyed while task ran */
    if (ok) {
        close_viewer();
        navigate_to(s_cwd);     /* refresh list to show the new .bin */
    } else if (s_convert_label) {
        lv_label_set_text(s_convert_label,
                          LV_SYMBOL_WARNING "  Conversion failed. Check companion app.");
        lv_obj_set_style_text_color(s_convert_label, lv_color_hex(0xFF5555), 0);
    }
}

static void do_convert_task(void * param)
{
    (void)param;
    const char * companion_ip = settingsGetCompanionIP();
    if (!companion_ip || companion_ip[0] == '\0') {
        lv_async_call(convert_done_cb, (void *)(intptr_t)0);
        vTaskDelete(NULL);
        return;
    }

    char url[128];
    snprintf(url, sizeof(url), "http://%s:8080/api/drive/convert", companion_ip);

    String device_ip_str = WiFi.localIP().toString();
    char body[256];
    snprintf(body, sizeof(body),
             "{\"path\":\"%s\",\"device_ip\":\"%s\"}",
             s_convert_path, device_ip_str.c_str());

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(30000);
    int code = http.POST(body);
    http.end();

    lv_async_call(convert_done_cb, (void *)(intptr_t)(code == 200 ? 1 : 0));
    vTaskDelete(NULL);
}

static void open_convert_overlay(const char * path)
{
    if (s_convert_busy) return;
    s_convert_busy = true;
    strncpy(s_convert_path, path, MAX_PATH_LEN - 1);
    s_convert_path[MAX_PATH_LEN - 1] = '\0';

    s_viewer_overlay = lv_obj_create(ui_Screen10);
    lv_obj_set_size(s_viewer_overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(s_viewer_overlay, 0, 0);
    lv_obj_remove_flag(s_viewer_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_viewer_overlay, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(s_viewer_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_viewer_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_viewer_overlay, 0, 0);

    s_convert_label = lv_label_create(s_viewer_overlay);
    lv_label_set_text(s_convert_label,
                      LV_SYMBOL_UPLOAD "  Sending to companion app for conversion...");
    lv_label_set_long_mode(s_convert_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_convert_label, SCREEN_W - 40);
    lv_obj_set_style_text_color(s_convert_label, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_text_font(s_convert_label, &lv_font_montserrat_14, 0);
    lv_obj_center(s_convert_label);

    lv_obj_t * close_btn = lv_button_create(s_viewer_overlay);
    lv_obj_set_size(close_btn, 44, 32);
    lv_obj_set_pos(close_btn, 4, 4);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(CLR_HDR), 0);
    lv_obj_set_style_border_width(close_btn, 0, 0);
    lv_obj_set_style_shadow_width(close_btn, 0, 0);
    lv_obj_add_event_cb(close_btn, viewer_close_ev, LV_EVENT_CLICKED, NULL);
    lv_obj_t * close_ico = lv_label_create(close_btn);
    lv_label_set_text(close_ico, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(close_ico, lv_color_white(), 0);
    lv_obj_center(close_ico);

    xTaskCreate(do_convert_task, "cvt", 8192, NULL, 1, NULL);
}

// ── List item event ───────────────────────────────────────────────────────────

static void file_item_ev(lv_event_t * e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= s_entry_count) return;

    bool is_dir = s_entries[idx].is_dir;
    char path_copy[MAX_PATH_LEN];
    strncpy(path_copy, s_entries[idx].path, MAX_PATH_LEN - 1);
    path_copy[MAX_PATH_LEN - 1] = '\0';

    if (is_dir) { navigate_to(path_copy); return; }

    file_type_t t = get_file_type(path_copy, false);

    if (t == FTYPE_TEXT) {
        open_text_editor(path_copy);
        return;
    }

    if (t == FTYPE_IMAGE) {
        const char * dot = strrchr(path_copy, '.');
        if (!dot) return;
        char ext[8] = {};
        int  i = 0;
        const char * p = dot + 1;
        while (*p && i < 7) ext[i++] = (char)tolower((unsigned char)*p++);
        if (strcmp(ext, "bin") == 0) {
            open_bin_viewer(path_copy);
        } else {
            open_convert_overlay(path_copy);    /* jpg / jpeg / png / bmp */
        }
    }
    /* FTYPE_AUDIO: future — pass to JukeDesk */
}

// ── Up / Back button ──────────────────────────────────────────────────────────

static void up_btn_ev(lv_event_t * e)
{
    (void)e;

    /* If a text viewer is open, close it first */
    if (s_viewer_overlay) {
        close_viewer();
        return;
    }

    if (strcmp(s_cwd, "/") == 0) {
        /* Already at SD root — return to launcher */
        _ui_screen_change(&ui_Screen1, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, 300, 0,
                          ui_Screen1_screen_init);
        return;
    }

    /* Strip the last path component */
    char parent[MAX_PATH_LEN];
    strncpy(parent, s_cwd, sizeof(parent) - 1);
    parent[sizeof(parent) - 1] = '\0';
    char * last_slash = strrchr(parent, '/');
    if (last_slash == parent) {
        parent[1] = '\0';    /* e.g. "/images" → "/" */
    } else if (last_slash) {
        *last_slash = '\0';  /* e.g. "/a/b" → "/a" */
    }
    navigate_to(parent);
}

// ── Directory listing ─────────────────────────────────────────────────────────

static void navigate_to(const char * path)
{
    strncpy(s_cwd, path, MAX_PATH_LEN - 1);
    s_cwd[MAX_PATH_LEN - 1] = '\0';

    if (s_path_label) lv_label_set_text(s_path_label, s_cwd);

    /* Clear old list */
    if (s_list) lv_obj_clean(s_list);
    s_entry_count = 0;

    if (!sdCardMounted()) {
        lv_obj_t * lbl = lv_label_create(s_list);
        lv_label_set_text(lbl, "SD card not mounted");
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFF5555), 0);
        return;
    }

    File dir = SD.open(s_cwd);
    if (!dir || !dir.isDirectory()) {
        lv_obj_t * lbl = lv_label_create(s_list);
        lv_label_set_text(lbl, "Cannot open directory");
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFF5555), 0);
        return;
    }

    dir.rewindDirectory();
    while (s_entry_count < MAX_ENTRIES) {
        File entry = dir.openNextFile();
        if (!entry) break;

        const char * name = entry.name();
        bool          is_d = entry.isDirectory();
        entry.close();

        if (!name || name[0] == '\0' || name[0] == '.') continue;

        /* Build full path */
        char full_path[MAX_PATH_LEN];
        if (strcmp(s_cwd, "/") == 0) {
            snprintf(full_path, sizeof(full_path), "/%s", name);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", s_cwd, name);
        }

        strncpy(s_entries[s_entry_count].path, full_path, MAX_PATH_LEN - 1);
        s_entries[s_entry_count].path[MAX_PATH_LEN - 1] = '\0';
        s_entries[s_entry_count].is_dir = is_d;

        file_type_t t = get_file_type(name, is_d);

        /* Build list button */
        lv_obj_t * btn = lv_list_add_button(s_list, icon_for_type(t), name);
        lv_obj_set_style_bg_color(btn, lv_color_hex(CLR_ITEM_BG), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(CLR_PRESSED),
                                  LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x1E293B), 0);
        lv_obj_set_style_radius(btn, 0, 0);
        lv_obj_set_style_pad_hor(btn, 10, 0);
        lv_obj_set_style_pad_ver(btn, 6, 0);

        /* Colour the icon by file type */
        lv_obj_t * icon_obj = lv_obj_get_child(btn, 0);
        if (icon_obj) {
            lv_obj_set_style_text_color(icon_obj, lv_color_hex(color_for_type(t)), 0);
        }

        /* Text label white */
        lv_obj_t * text_obj = lv_obj_get_child(btn, 1);
        if (text_obj) {
            lv_obj_set_style_text_color(text_obj, lv_color_white(), 0);
            lv_obj_set_style_text_font(text_obj, &lv_font_montserrat_14, 0);
        }

        lv_obj_add_event_cb(btn, file_item_ev, LV_EVENT_CLICKED,
                            (void *)(intptr_t)s_entry_count);
        s_entry_count++;
    }
    dir.close();

    if (s_entry_count == 0) {
        lv_obj_t * lbl = lv_label_create(s_list);
        lv_label_set_text(lbl, "(empty folder)");
        lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_MUTED), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_pad_all(lbl, 12, 0);
    }
}

// ── Build header ──────────────────────────────────────────────────────────────

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

    /* Up / back button */
    lv_obj_t * up_btn = lv_button_create(hdr);
    lv_obj_set_size(up_btn, 44, HDR_H - 4);
    lv_obj_set_pos(up_btn, 2, 2);
    lv_obj_set_style_bg_color(up_btn, lv_color_hex(CLR_HDR), 0);
    lv_obj_set_style_bg_color(up_btn, lv_color_hex(0x0D1321),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(up_btn, 0, 0);
    lv_obj_set_style_shadow_width(up_btn, 0, 0);
    lv_obj_set_style_pad_all(up_btn, 0, 0);
    lv_obj_add_event_cb(up_btn, up_btn_ev, LV_EVENT_CLICKED, NULL);

    lv_obj_t * up_ico = lv_label_create(up_btn);
    lv_label_set_text(up_ico, LV_SYMBOL_UP);
    lv_obj_set_style_text_color(up_ico, lv_color_white(), 0);
    lv_obj_center(up_ico);

    /* Screen title */
    lv_obj_t * title = lv_label_create(hdr);
    lv_label_set_text(title, LV_SYMBOL_DRIVE "  DeskDrive");
    lv_obj_set_style_text_color(title, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, -6);

    /* Path sub-label below title */
    s_path_label = lv_label_create(hdr);
    lv_label_set_text(s_path_label, s_cwd);
    lv_label_set_long_mode(s_path_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(s_path_label, SCREEN_W - 55);
    lv_obj_set_style_text_color(s_path_label, lv_color_hex(CLR_MUTED), 0);
    lv_obj_set_style_text_font(s_path_label, &lv_font_montserrat_10, 0);
    lv_obj_align(s_path_label, LV_ALIGN_BOTTOM_LEFT, 50, -2);
}

// ── Build info bar ────────────────────────────────────────────────────────────

static void build_info_bar(lv_obj_t * scr)
{
    lv_obj_t * bar = lv_obj_create(scr);
    lv_obj_set_size(bar, SCREEN_W, INFO_BAR_H);
    lv_obj_set_pos(bar, 0, HDR_H);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(bar, lv_color_hex(CLR_INFO_BG), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_hor(bar, 8, 0);
    lv_obj_set_style_pad_ver(bar, 2, 0);

    lv_obj_t * info = lv_label_create(bar);
    char buf[96];
    if (sdCardMounted()) {
        int used_mb  = (int)(SD.usedBytes()  / (1024ULL * 1024ULL));
        int total_mb = (int)(SD.totalBytes() / (1024ULL * 1024ULL));
        snprintf(buf, sizeof(buf),
                 LV_SYMBOL_WIFI "  %s   " LV_SYMBOL_DRIVE "  %d / %d MB",
                 WiFi.localIP().toString().c_str(), used_mb, total_mb);
    } else {
        snprintf(buf, sizeof(buf),
                 LV_SYMBOL_WIFI "  %s   SD: not mounted",
                 WiFi.localIP().toString().c_str());
    }
    lv_label_set_text(info, buf);
    lv_obj_set_style_text_color(info, lv_color_hex(CLR_MUTED), 0);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_10, 0);
    lv_obj_align(info, LV_ALIGN_LEFT_MID, 0, 0);
}

// ── Public lifecycle ──────────────────────────────────────────────────────────

void ui_Screen10_screen_init(void)
{
    handshakeSetScreen("deskdrive");
    s_viewer_overlay = NULL;
    s_entry_count    = 0;
    strncpy(s_cwd, "/", sizeof(s_cwd) - 1);

    ui_Screen10 = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_Screen10, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen10, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(ui_Screen10, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(ui_Screen10, screen_unload_ev,
                        LV_EVENT_SCREEN_UNLOADED, NULL);

    build_header(ui_Screen10);
    build_info_bar(ui_Screen10);

    /* File list fills the remainder of the screen */
    s_list = lv_list_create(ui_Screen10);
    lv_obj_set_size(s_list, SCREEN_W, LIST_H);
    lv_obj_set_pos(s_list, 0, LIST_Y);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_radius(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_set_style_pad_row(s_list, 1, 0);

    navigate_to("/");
}

void ui_Screen10_screen_destroy(void)
{
    s_list           = NULL;
    s_path_label     = NULL;
    s_viewer_overlay = NULL;
    s_editor_ta      = NULL;
    s_editor_kbd     = NULL;
    s_convert_label  = NULL;
    s_entry_count    = 0;
    s_edit_path[0]   = '\0';
    s_convert_busy   = false;
    if (s_view_img_buf) { free(s_view_img_buf); s_view_img_buf = NULL; }
    if (ui_Screen10) lv_obj_delete(ui_Screen10);
    ui_Screen10 = NULL;
}
