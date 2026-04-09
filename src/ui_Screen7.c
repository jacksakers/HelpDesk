// Project  : HelpDesk
// File     : ui_Screen7.c
// Purpose  : PCMonitor screen — UART-driven PC metrics + serial log viewer
// Depends  : ui.h (LVGL 9.x), handshake.h

#include "ui.h"
#include "handshake.h"

/* ── Colours ───────────────────────────────────────────────── */
#define CLR_BG       0x1A1A2E
#define CLR_HDR      0x16213E
#define CLR_ACCENT   0xFF9800   /* Orange — matches launcher tile */
#define CLR_BAR_BG   0x2A2A3E
#define CLR_CPU      0xFF9800   /* Orange bars for CPU            */
#define CLR_RAM      0x42A5F5   /* Blue bars for RAM              */
#define CLR_GPU      0x66BB6A   /* Green bars for GPU             */
#define CLR_SUBTLE   0xAAAAAA
#define CLR_LOG_BG   0x0D1321   /* Dark navy terminal background  */
#define CLR_LOG_TXT  0x4CAF50   /* Green terminal text            */

/* ── Dimensions ────────────────────────────────────────────── */
#define SCREEN_W     480
#define SCREEN_H     320
#define HDR_H         30
#define ROW_H         26   /* Height of each metric row      */
#define ROW_GAP        8
#define BAR_W        160
#define LABEL_W       48   /* Left-side metric name width    */
#define VAL_W         48   /* Right-side value width         */
#define FIRST_ROW_Y   42
#define LEFT_PAD       8

/* Log panel — occupies the space below the metric rows */
#define LOG_SECTION_Y  (FIRST_ROW_Y + 3 * (ROW_H + ROW_GAP))  /* y=144 */
#define LOG_Y          (LOG_SECTION_Y + 14)                     /* y=158 */
#define LOG_W          (SCREEN_W - 2 * LEFT_PAD)                /* 464px */
#define LOG_H          (SCREEN_H - LOG_Y - 2)                   /* 160px */
#define LOG_MAX_CHARS  1800   /* Trim oldest content above this threshold */

/* ── Public objects ────────────────────────────────────────── */
lv_obj_t * ui_Screen7     = NULL;
lv_obj_t * ui_CpuBar      = NULL;
lv_obj_t * ui_CpuLabel    = NULL;
lv_obj_t * ui_RamBar      = NULL;
lv_obj_t * ui_RamLabel    = NULL;
lv_obj_t * ui_GpuBar        = NULL;
lv_obj_t * ui_GpuLabel      = NULL;
lv_obj_t * ui_PcStatusLabel = NULL;
lv_obj_t * ui_SerialLogArea = NULL;

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
    lv_label_set_text(title, "PCMonitor");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    /* Compact USB status indicator — right side of header */
    lv_obj_t * status_lbl = lv_label_create(hdr);
    lv_label_set_text(status_lbl, LV_SYMBOL_USB " --");
    lv_obj_set_style_text_color(status_lbl, lv_color_hex(CLR_SUBTLE), 0);
    lv_obj_set_style_text_font(status_lbl, &lv_font_montserrat_10, 0);
    lv_obj_align(status_lbl, LV_ALIGN_RIGHT_MID, -6, 0);
    ui_PcStatusLabel = status_lbl;
}

/* Builds one metric row: [name]  [bar]  [value %] */
static void build_metric_row(lv_obj_t * scr,
                              const char * name,
                              uint32_t bar_color,
                              int y,
                              lv_obj_t ** out_bar,
                              lv_obj_t ** out_val_lbl)
{
    /* Name label */
    lv_obj_t * name_lbl = lv_label_create(scr);
    lv_label_set_text(name_lbl, name);
    lv_obj_set_style_text_color(name_lbl, lv_color_hex(CLR_SUBTLE), 0);
    lv_obj_set_pos(name_lbl, LEFT_PAD, y + (ROW_H - 14) / 2);

    /* Progress bar */
    lv_obj_t * bar = lv_bar_create(scr);
    lv_obj_set_size(bar, BAR_W, ROW_H - 8);
    lv_obj_set_pos(bar, LEFT_PAD + LABEL_W + 4, y + 4);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_hex(CLR_BAR_BG),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(bar, lv_color_hex(bar_color),
                              LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(bar, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    *out_bar = bar;

    /* Value label */
    lv_obj_t * val_lbl = lv_label_create(scr);
    lv_label_set_text(val_lbl, "--%");
    lv_obj_set_style_text_color(val_lbl, lv_color_hex(bar_color), 0);
    lv_obj_set_pos(val_lbl, LEFT_PAD + LABEL_W + 4 + BAR_W + 6, y + (ROW_H - 14) / 2);
    *out_val_lbl = val_lbl;
}

static void build_body(lv_obj_t * scr)
{
    build_metric_row(scr, "CPU", CLR_CPU,
                     FIRST_ROW_Y + 0 * (ROW_H + ROW_GAP),
                     &ui_CpuBar, &ui_CpuLabel);

    build_metric_row(scr, "RAM", CLR_RAM,
                     FIRST_ROW_Y + 1 * (ROW_H + ROW_GAP),
                     &ui_RamBar, &ui_RamLabel);

    build_metric_row(scr, "GPU", CLR_GPU,
                     FIRST_ROW_Y + 2 * (ROW_H + ROW_GAP),
                     &ui_GpuBar, &ui_GpuLabel);
    /* Status label is now embedded in the header — see build_header(). */
}

static void build_log_panel(lv_obj_t * scr)
{
    /* Section heading */
    lv_obj_t * heading = lv_label_create(scr);
    lv_label_set_text(heading, "SERIAL LOG");
    lv_obj_set_style_text_color(heading, lv_color_hex(CLR_SUBTLE), 0);
    lv_obj_set_style_text_font(heading, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(heading, LEFT_PAD, LOG_SECTION_Y + 1);

    /* Read-only terminal textarea */
    lv_obj_t * ta = lv_textarea_create(scr);
    lv_obj_set_size(ta, LOG_W, LOG_H);
    lv_obj_set_pos(ta, LEFT_PAD, LOG_Y);
    lv_textarea_set_text(ta, "");
    /* Prevent touch from opening the on-screen keyboard */
    lv_obj_remove_flag(ta, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    /* Dark terminal styling */
    lv_obj_set_style_bg_color(ta, lv_color_hex(CLR_LOG_BG), 0);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ta, lv_color_hex(CLR_BAR_BG), 0);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_radius(ta, 4, 0);
    lv_obj_set_style_text_color(ta, lv_color_hex(CLR_LOG_TXT), 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_10, 0);
    lv_obj_set_style_pad_all(ta, 4, 0);
    /* Hide the blinking cursor — this is display-only */
    lv_obj_set_style_opa(ta, LV_OPA_TRANSP, LV_PART_CURSOR | LV_STATE_DEFAULT);
    lv_obj_set_style_opa(ta, LV_OPA_TRANSP, LV_PART_CURSOR | LV_STATE_FOCUSED);

    ui_SerialLogArea = ta;
}

/* ── Public lifecycle ──────────────────────────────────────── */
void ui_Screen7_screen_init(void)
{
    handshakeSetScreen("pc_monitor");
    ui_Screen7 = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_Screen7, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen7, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(ui_Screen7, LV_OPA_COVER, 0);

    lv_obj_add_event_cb(ui_Screen7, scr_unloaded_delete_cb,
                        LV_EVENT_SCREEN_UNLOADED, ui_Screen7_screen_destroy);

    build_header(ui_Screen7);
    build_body(ui_Screen7);
    build_log_panel(ui_Screen7);
}

void ui_Screen7_screen_destroy(void)
{
    ui_CpuBar        = NULL;
    ui_CpuLabel      = NULL;
    ui_RamBar        = NULL;
    ui_RamLabel      = NULL;
    ui_GpuBar        = NULL;
    ui_GpuLabel      = NULL;
    ui_PcStatusLabel = NULL;
    ui_SerialLogArea = NULL;

    if(ui_Screen7) lv_obj_delete(ui_Screen7);
    ui_Screen7 = NULL;
}

void pcMonitorLogLine(const char * line)
{
    if (!ui_SerialLogArea) return;

    /* Trim the oldest content when approaching the character cap */
    const char * cur = lv_textarea_get_text(ui_SerialLogArea);
    if (cur && strlen(cur) > LOG_MAX_CHARS) {
        const char * trim = cur + LOG_MAX_CHARS / 2;
        const char * nl   = strchr(trim, '\n');
        lv_textarea_set_text(ui_SerialLogArea, nl ? nl + 1 : "");
    }

    lv_textarea_add_text(ui_SerialLogArea, line);
    lv_textarea_add_text(ui_SerialLogArea, "\n");
    /* Scroll to the latest line */
    lv_obj_scroll_to_y(ui_SerialLogArea, INT32_MAX, LV_ANIM_OFF);
}
