// Project  : HelpDesk
// File     : display_Screen.cpp
// Purpose  : Initialise LovyanGFX driver, DMA, touch, and LVGL (LVGL v9 API)
// Depends  : LovyanGFX_Driver.h, lvgl.h

#include <lvgl.h>
#include "display_Screen.h"
#include "LovyanGFX_Driver.h"
#include <Arduino.h>

// ─── Hardware instance ──────────────────────────────────────────────────────
static LGFX gfx;

// ─── LVGL draw buffer ────────────────────────────────────────────────────────
// Use a static SRAM buffer — same approach as the working Desktop_Assistant
// example.  ps_malloc fails silently when PSRAM is not enabled in the IDE,
// which was causing a completely black screen.
#define DRAW_BUF_SIZE (320 * 240 / 10)
static lv_color_t    draw_buf_arr[DRAW_BUF_SIZE];
static lv_display_t * disp = nullptr;

// ─── LVGL tick source ────────────────────────────────────────────────────────
static uint32_t my_tick(void)
{
    return millis();
}

// ─── LVGL callbacks (v9 signatures) ──────────────────────────────────────────

static void my_disp_flush(lv_display_t * d, const lv_area_t * area, uint8_t * px_map)
{
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    gfx.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t *)px_map);
    lv_display_flush_ready(d);
}

static void my_touchpad_read(lv_indev_t * indev, lv_indev_data_t * data)
{
    (void)indev;
    if(gfx.getTouch(&data->point.x, &data->point.y)) {
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ─── LVGL init (v9 API) ──────────────────────────────────────────────────────

static void initLVGL()
{
    lv_init();
    lv_tick_set_cb(my_tick);
    Serial.println("[LVGL] lv_init() done");

    // Create and configure the display
    disp = lv_display_create(320, 240);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf_arr, NULL,
                           sizeof(draw_buf_arr),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    Serial.println("[LVGL] display driver registered (320x240)");

    // Create and configure the input device
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
    Serial.println("[LVGL] touch input driver registered");
}

// ─── Public ─────────────────────────────────────────────────────────────────

void initDisplay()
{
    Serial.println("[Display] Starting initDisplay...");

    // LovyanGFX handles I2C for the touch controller internally
    // (pins configured in LovyanGFX_Driver.h). No Wire.begin() needed.

    gfx.init();
    Serial.println("[Display] gfx.init() done");

    gfx.initDMA();
    gfx.startWrite();
    gfx.fillScreen(TFT_BLACK);
    Serial.println("[Display] DMA + fillScreen done");

    initLVGL();

    // Backlight on GPIO 38
    pinMode(38, OUTPUT);
    digitalWrite(38, HIGH);
    Serial.println("[Display] Backlight ON (GPIO 38)");

    gfx.fillScreen(TFT_BLACK);
    Serial.println("[Display] initDisplay complete");
}
