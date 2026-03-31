// Project  : HelpDesk
// File     : display_Screen.cpp
// Purpose  : Initialise LovyanGFX driver, DMA, touch, and LVGL (LVGL v9 API)
// Depends  : LovyanGFX_Driver.h, lvgl.h

#include <lvgl.h>
#include "display_Screen.h"
#include "LovyanGFX_Driver.h"
#include <Arduino.h>
#include <Wire.h>

// ─── Hardware instance ──────────────────────────────────────────────────────
static LGFX gfx;

// ─── LVGL draw buffer ────────────────────────────────────────────────────────
// Allocated in PSRAM via ps_malloc. A static SRAM array fails with DMA on
// the ESP32-S3 because the DMA engine requires PSRAM-accessible memory.
#define DRAW_BUF_SIZE (320 * 240 / 10)
static lv_color_t *   draw_buf_arr = nullptr;
static lv_display_t * disp         = nullptr;

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
    Serial.printf("[LVGL flush] x1=%d y1=%d w=%lu h=%lu\n", area->x1, area->y1, w, h);
    gfx.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t *)px_map);
    /* Wait for DMA to finish before releasing the buffer back to LVGL.
       Without this, LVGL overwrites the single draw buffer while DMA is
       still reading it, producing a corrupted (black) screen. */
    gfx.waitDMA();
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

    // Allocate draw buffer in PSRAM
    draw_buf_arr = (lv_color_t *)ps_malloc(DRAW_BUF_SIZE * sizeof(lv_color_t));
    if(!draw_buf_arr) {
        Serial.println("[LVGL] ERROR: ps_malloc failed — check PSRAM is enabled in Tools menu");
        return;
    }
    Serial.printf("[LVGL] draw buffer allocated at %p (%u bytes)\n",
                  draw_buf_arr, (unsigned)(DRAW_BUF_SIZE * sizeof(lv_color_t)));

    // Create and configure the display
    disp = lv_display_create(320, 240);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf_arr, NULL,
                           DRAW_BUF_SIZE * sizeof(lv_color_t),
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
