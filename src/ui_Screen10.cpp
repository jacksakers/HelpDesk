// Project  : HelpDesk
// File     : ui_Screen10.cpp
// Purpose  : DeskDrive — touch-optimised SD card file manager (LVGL 9.x)
// Depends  : ui.h, ui_Screen10.h, sd_card.h, handshake.h, <SD.h>, <WiFi.h>

#include "ui.h"
#include "sd_card.h"
#include "handshake.h"
#include <Arduino.h>
#include <SD.h>
#include <WiFi.h>

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

// ── Forward declarations ──────────────────────────────────────────────────────
static void navigate_to(const char * path);
static void close_viewer(void);

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

// ── Text viewer overlay ───────────────────────────────────────────────────────

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
}

static void open_text_file(const char * path)
{
    if (!sdCardMounted()) return;

    File f = SD.open(path, FILE_READ);
    if (!f) return;

    /* Cap at 8 KB to stay within LVGL's heap */
    size_t len = f.size();
    if (len > 8192) len = 8192;

    char * buf = (char *)malloc(len + 1);
    if (!buf) { f.close(); return; }
    f.read((uint8_t *)buf, len);
    buf[len] = '\0';
    f.close();

    /* Fullscreen overlay on top of current screen */
    s_viewer_overlay = lv_obj_create(ui_Screen10);
    lv_obj_set_size(s_viewer_overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(s_viewer_overlay, 0, 0);
    lv_obj_remove_flag(s_viewer_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_viewer_overlay, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(s_viewer_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_viewer_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_viewer_overlay, 0, 0);

    /* Tap header bar to close */
    lv_obj_t * close_btn = lv_button_create(s_viewer_overlay);
    lv_obj_set_size(close_btn, SCREEN_W, HDR_H);
    lv_obj_set_pos(close_btn, 0, 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(CLR_HDR), 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(CLR_PRESSED),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(close_btn, 0, 0);
    lv_obj_set_style_radius(close_btn, 0, 0);
    lv_obj_set_style_pad_all(close_btn, 6, 0);
    lv_obj_add_event_cb(close_btn, viewer_close_ev, LV_EVENT_CLICKED, NULL);

    const char * fname = strrchr(path, '/');
    fname = fname ? fname + 1 : path;

    lv_obj_t * hdr_lbl = lv_label_create(close_btn);
    lv_label_set_text_fmt(hdr_lbl, LV_SYMBOL_CLOSE "  %s", fname);
    lv_label_set_long_mode(hdr_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(hdr_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(hdr_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(hdr_lbl);

    /* Scrollable read-only text area */
    lv_obj_t * ta = lv_textarea_create(s_viewer_overlay);
    lv_obj_set_size(ta, SCREEN_W, SCREEN_H - HDR_H);
    lv_obj_set_pos(ta, 0, HDR_H);
    lv_textarea_set_text(ta, buf);
    lv_textarea_set_cursor_pos(ta, 0);               /* cursor at top */
    lv_obj_add_state(ta, LV_STATE_DISABLED);          /* read-only */
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x0D1117), LV_STATE_DISABLED);
    lv_obj_set_style_text_color(ta, lv_color_hex(0xC9D1D9), 0);
    lv_obj_set_style_text_color(ta, lv_color_hex(0xC9D1D9), LV_STATE_DISABLED);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_12, 0);
    lv_obj_set_style_border_width(ta, 0, 0);
    lv_obj_set_style_pad_all(ta, 8, 0);

    free(buf);
}

// ── List item event ───────────────────────────────────────────────────────────

static void file_item_ev(lv_event_t * e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= s_entry_count) return;

    /* Copy before navigate_to() may rebuild s_entries */
    bool is_dir = s_entries[idx].is_dir;
    char path_copy[MAX_PATH_LEN];
    strncpy(path_copy, s_entries[idx].path, MAX_PATH_LEN - 1);
    path_copy[MAX_PATH_LEN - 1] = '\0';

    if (is_dir) {
        navigate_to(path_copy);
    } else {
        if (get_file_type(path_copy, false) == FTYPE_TEXT) {
            open_text_file(path_copy);
        }
        /* Future: image viewer, audio launcher */
    }
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
    s_entry_count    = 0;
    if (ui_Screen10) lv_obj_delete(ui_Screen10);
    ui_Screen10 = NULL;
}
