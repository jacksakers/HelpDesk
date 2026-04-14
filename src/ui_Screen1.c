// Project  : HelpDesk
// File     : ui_Screen1.c
// Purpose  : Launcher screen — app-tile grid and navigation
// Depends  : ui.h (LVGL 9.x), handshake.h

#include "ui.h"
#include "handshake.h"
#include "notifications.h"

/* ── Layout constants ──────────────────────────────────────── */
#define SCREEN_W       480
#define SCREEN_H       320
#define HDR_H           36
#define TILE_W         100
#define TILE_H         120
#define TILE_GAP         8
#define TILE_MARGIN     12
#define TILES_PER_ROW    4
#define COL_STRIDE      (TILE_W + TILE_GAP)
#define ROW_STRIDE      (TILE_H + TILE_GAP)
#define ROW0_Y          (HDR_H + TILE_MARGIN)

/* ── Colours (24-bit RGB; passed to lv_color_hex at runtime) ─ */
#define CLR_BG      0x1A1A2E
#define CLR_HDR     0x16213E

/* ── App descriptor ────────────────────────────────────────── */
typedef struct {
    const char  * label;       /* symbol + "\n" + name          */
    uint32_t      color_hex;   /* tile background colour        */
    lv_obj_t    **screen;      /* pointer to the screen global  */
    void        (*init)(void); /* screen init function          */
} app_tile_t;

static const app_tile_t k_apps[] = {
    { LV_SYMBOL_HOME    "\nDeskDash",   0x2196F3, &ui_Screen2,  ui_Screen2_screen_init  },
    { LV_SYMBOL_PAUSE   "\nTomatimer",  0xF44336, &ui_Screen3,  ui_Screen3_screen_init  },
    { LV_SYMBOL_BELL    "\nNotifs",     0xFF5722, &ui_Screen4,  ui_Screen4_screen_init  },
    { LV_SYMBOL_LIST    "\nTaskMaster", 0x4CAF50, &ui_Screen5,  ui_Screen5_screen_init  },
    { LV_SYMBOL_IMAGE   "\nZenFrame",   0x009688, &ui_Screen6,  ui_Screen6_screen_init  },
    { LV_SYMBOL_USB     "\nPCMonitor",  0xFF9800, &ui_Screen7,  ui_Screen7_screen_init  },
    { LV_SYMBOL_SHUFFLE "\nGameBreak",  0xE91E63, &ui_Screen8,  ui_Screen8_screen_init  },
    { LV_SYMBOL_DRIVE   "\nDeskDrive",  0x0077B6, &ui_Screen10, ui_Screen10_screen_init },
};
#define APP_COUNT  (sizeof(k_apps) / sizeof(k_apps[0]))

/* ── Public screen object ──────────────────────────────────── */
lv_obj_t * ui_Screen1    = NULL;
lv_obj_t * ui_NotifTile  = NULL;   /* Notifications tile — flashed by notifications.cpp */
lv_obj_t * ui_NotifBadge = NULL;   /* Unread count badge on that tile */

/* ── Event callbacks ───────────────────────────────────────── */
static void tile_clicked_ev(lv_event_t * e)
{
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    const app_tile_t * app = &k_apps[idx];
    _ui_screen_change(app->screen, LV_SCREEN_LOAD_ANIM_MOVE_LEFT, 300, 0, app->init);
}

static void settings_btn_ev(lv_event_t * e)
{
    (void)e;
    _ui_screen_change(&ui_Screen9, LV_SCREEN_LOAD_ANIM_MOVE_LEFT, 300, 0,
                      ui_Screen9_screen_init);
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

    lv_obj_t * title = lv_label_create(hdr);
    lv_label_set_text(title, "HelpDesk");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_center(title);

    /* Settings gear button — upper-right corner */
    lv_obj_t * gear_btn = lv_button_create(hdr);
    lv_obj_set_size(gear_btn, 38, HDR_H - 2);
    lv_obj_set_pos(gear_btn, SCREEN_W - 40, 1);
    lv_obj_set_style_bg_color(gear_btn, lv_color_hex(CLR_HDR), 0);
    lv_obj_set_style_bg_color(gear_btn, lv_color_hex(0x0D1321),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(gear_btn, 0, 0);
    lv_obj_set_style_shadow_width(gear_btn, 0, 0);
    lv_obj_set_style_pad_all(gear_btn, 0, 0);
    lv_obj_add_event_cb(gear_btn, settings_btn_ev, LV_EVENT_CLICKED, NULL);

    lv_obj_t * gear_ico = lv_label_create(gear_btn);
    lv_label_set_text(gear_ico, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(gear_ico, lv_color_white(), 0);
    lv_obj_center(gear_ico);
}

/* The index of the Notifications tile in k_apps (used to attach the badge). */
#define NOTIF_TILE_IDX  2

static lv_obj_t * build_one_tile(lv_obj_t * scr, uint8_t idx)
{
    const app_tile_t * app = &k_apps[idx];
    int col = idx % TILES_PER_ROW;
    int row = idx / TILES_PER_ROW;
    int x   = TILE_MARGIN + col * COL_STRIDE;
    int y   = ROW0_Y      + row * ROW_STRIDE;

    lv_color_t color = lv_color_hex(app->color_hex);

    lv_obj_t * tile = lv_obj_create(scr);
    lv_obj_set_size(tile, TILE_W, TILE_H);
    lv_obj_set_pos(tile, x, y);
    lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(tile, color, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_radius(tile, 10, 0);
    lv_obj_set_style_shadow_width(tile, 0, 0);
    lv_obj_set_style_pad_all(tile, 4, 0);
    /* Darker shade when pressed for tactile feedback */
    lv_obj_set_style_bg_color(tile, lv_color_mix(lv_color_black(), color, LV_OPA_20),
                              LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t * lbl = lv_label_create(tile);
    lv_label_set_text(lbl, app->label);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(tile, tile_clicked_ev, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)idx);
    return tile;
}

static void build_tile_grid(lv_obj_t * scr)
{
    for(uint8_t i = 0; i < APP_COUNT; i++) {
        lv_obj_t * tile = build_one_tile(scr, i);

        if (i == NOTIF_TILE_IDX) {
            /* Store the tile so notifications.cpp can flash it */
            ui_NotifTile = tile;

            /* Small badge: red circle in the top-right corner showing unread count */
            lv_obj_t * badge = lv_label_create(tile);
            lv_label_set_text(badge, "0");
            lv_obj_set_style_bg_color(badge, lv_color_hex(0xD32F2F), 0);
            lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(badge, lv_color_white(), 0);
            lv_obj_set_style_text_font(badge, &lv_font_montserrat_10, 0);
            lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_pad_hor(badge, 4, 0);
            lv_obj_set_style_pad_ver(badge, 2, 0);
            lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, -2, 2);
            lv_obj_add_flag(badge, LV_OBJ_FLAG_HIDDEN);  /* hidden until unread > 0 */
            ui_NotifBadge = badge;
        }
    }
}

/* ── Public lifecycle ──────────────────────────────────────── */
void ui_Screen1_screen_init(void)
{
    handshakeSetScreen("launcher");
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen1, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(ui_Screen1, LV_OPA_COVER, 0);

    /* Auto-cleanup when this screen is unloaded */
    lv_obj_add_event_cb(ui_Screen1, scr_unloaded_delete_cb,
                        LV_EVENT_SCREEN_UNLOADED, ui_Screen1_screen_destroy);

    build_header(ui_Screen1);
    build_tile_grid(ui_Screen1);
}

void ui_Screen1_screen_destroy(void)
{
    ui_NotifTile  = NULL;
    ui_NotifBadge = NULL;
    if(ui_Screen1) lv_obj_delete(ui_Screen1);
    ui_Screen1 = NULL;
}
