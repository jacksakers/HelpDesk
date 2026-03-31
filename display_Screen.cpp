// Project  : HelpDesk
// File     : display_Screen.cpp
// Purpose  : Initialise LovyanGFX driver, DMA, touch, and LVGL (LVGL v8.3.x API)
// Depends  : LovyanGFX_Driver.h, lvgl.h

#include <lvgl.h>
#include "display_Screen.h"
#include "LovyanGFX_Driver.h"
#include <Arduino.h>
#include <Wire.h>

// ─── Hardware instance ──────────────────────────────────────────────────────
static LGFX gfx;

// ─── LVGL draw buffer (v8 uses lv_disp_draw_buf_t) ──────────────────────────
// Allocated in PSRAM via ps_malloc. A static SRAM array fails with DMA on
// the ESP32-S3 because the DMA engine requires PSRAM-accessible memory.
#define DRAW_BUF_SIZE (320 * 240 / 10)
static lv_color_t *       draw_buf_arr = nullptr;
static lv_disp_draw_buf_t draw_buf;

// ─── LVGL callbacks (v8 signatures) ─────────────────────────────────────────
// No manual tick source needed: the Arduino LVGL 8.3 package enables
// LV_TICK_CUSTOM in lv_conf.h so LVGL reads millis() automatically.

static void my_disp_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_p)
{
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    gfx.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t *)color_p);
    /* Wait for DMA to finish before releasing the buffer back to LVGL.
       Without this, LVGL overwrites the single draw buffer while DMA is
       still reading it, producing a corrupted (black) screen. */
    gfx.waitDMA();
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

    // Allocate draw buffer in PSRAM
    draw_buf_arr = (lv_color_t *)ps_malloc(DRAW_BUF_SIZE * sizeof(lv_color_t));
    if(!draw_buf_arr) {
        Serial.println("[LVGL] ERROR: ps_malloc failed — check PSRAM is enabled in Tools menu");
        return;
    }

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
