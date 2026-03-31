// Project  : HelpDesk
// File     : display_Screen.cpp
// Purpose  : Initialise LovyanGFX driver, DMA, touch, and LVGL (LVGL v8.3.x API)
// Depends  : LovyanGFX_Driver.h, lvgl.h

#include <lvgl.h>
#include "display_Screen.h"
#include "LovyanGFX_Driver.h"
#include <Arduino.h>
#include <Wire.h>
#include <esp_timer.h>

// ─── Hardware instance ──────────────────────────────────────────────────────
static LGFX gfx;

// ─── LVGL draw buffer (v8 uses lv_disp_draw_buf_t) ──────────────────────────
#define DRAW_BUF_SIZE (320 * 240 / 10)
static lv_color_t        draw_buf_arr[DRAW_BUF_SIZE];
static lv_disp_draw_buf_t draw_buf;

// ─── Tick source: esp_timer fires every 2 ms ────────────────────────────────
#define LV_TICK_PERIOD_MS 2

static void lv_tick_cb(void * arg)
{
    (void)arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

// ─── LVGL callbacks (v8 signatures) ─────────────────────────────────────────

static void my_disp_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_p)
{
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    gfx.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t *)color_p);
    lv_disp_flush_ready(drv);
}

static void my_touchpad_read(lv_indev_drv_t * drv, lv_indev_data_t * data)
{
    (void)drv;
    if(gfx.getTouch(&data->point.x, &data->point.y)) {
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ─── LVGL init (v8 driver-struct pattern) ───────────────────────────────────

static void initLVGL()
{
    lv_init();

    // Tick source via esp_timer (replaces lv_tick_set_cb which is v9-only)
    const esp_timer_create_args_t timer_args = {
        .callback = lv_tick_cb,
        .name     = "lv_tick"
    };
    esp_timer_handle_t lv_tick_timer;
    esp_timer_create(&timer_args, &lv_tick_timer);
    esp_timer_start_periodic(lv_tick_timer, LV_TICK_PERIOD_MS * 1000UL);

    // Draw buffer
    lv_disp_draw_buf_init(&draw_buf, draw_buf_arr, NULL, DRAW_BUF_SIZE);

    // Display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = 320;
    disp_drv.ver_res  = 240;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Input device driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
}

// ─── Public ─────────────────────────────────────────────────────────────────

void initDisplay()
{
    // I2C for touch controller (FT5x06 on pins 15/16)
    Wire.begin(15, 16);
    delay(50);

    gfx.init();
    gfx.initDMA();
    gfx.startWrite();
    gfx.fillScreen(TFT_BLACK);

    initLVGL();

    // Backlight on GPIO 38
    pinMode(38, OUTPUT);
    digitalWrite(38, HIGH);

    gfx.fillScreen(TFT_BLACK);
}
