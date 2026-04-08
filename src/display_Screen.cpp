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
// Raw uint16_t buffer for RGB565.  Even though ILI9488 is 18-bit on SPI,
// LovyanGFX converts RGB565→RGB666 automatically during pushImage when the
// source type is lgfx::rgb565_t.
#define DRAW_BUF_PX   (480 * 320 / 10)           // 15360 pixels
static uint16_t      draw_buf_arr[DRAW_BUF_PX];  // 30720 bytes
static lv_display_t * disp = nullptr;

// ─── LVGL tick source ────────────────────────────────────────────────────────
static uint32_t my_tick(void)
{
    return millis();
}

// ─── Debug: I2C bus scan ─────────────────────────────────────────────────────
static void i2c_scan()
{
    Serial.println("[I2C] Scanning bus 0 (SDA=15  SCL=16)...");
    Wire.begin(15, 16);
    int found = 0;
    for(uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if(Wire.endTransmission() == 0) {
            Serial.printf("[I2C]   0x%02X  ← device found\n", addr);
            found++;
        }
    }
    Serial.printf("[I2C] Scan done — %d device(s) found\n", found);
    Wire.end();   // release so LovyanGFX can manage I2C for touch
}

// ─── LVGL callbacks (v9 signatures) ──────────────────────────────────────────

static uint32_t flush_count = 0;

static void my_disp_flush(lv_display_t * d, const lv_area_t * area, uint8_t * px_map)
{
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    // pushImage (synchronous).  LovyanGFX auto-converts RGB565→RGB666 (18-bit)
    // because Panel_ILI9488 defaults to 18-bit SPI color depth.
    // We do NOT call setColorDepth(16) — ILI9488 over SPI often ignores COLMOD 0x55.
    gfx.pushImage(area->x1, area->y1, w, h, (lgfx::rgb565_t *)px_map);
    lv_display_flush_ready(d);

    if(++flush_count <= 10) {
        // Dump first few flushes with pixel sample for diagnosis
        uint16_t px0 = ((uint16_t *)px_map)[0];
        uint16_t px1 = ((uint16_t *)px_map)[1];
        Serial.printf("[flush #%lu] x=%d y=%d %lux%lu  px[0]=0x%04X px[1]=0x%04X\n",
                      flush_count, area->x1, area->y1, w, h, px0, px1);
    }
}

static void my_touchpad_read(lv_indev_t * indev, lv_indev_data_t * data)
{
    (void)indev;
    if(gfx.getTouch(&data->point.x, &data->point.y)) {
        data->state = LV_INDEV_STATE_PRESSED;
        // Log first few touch events
        static int touch_log_count = 0;
        if(touch_log_count < 20) {
            Serial.printf("[Touch] x=%d  y=%d\n", data->point.x, data->point.y);
            touch_log_count++;
        }
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
    disp = lv_display_create(480, 320);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf_arr, NULL,
                           sizeof(draw_buf_arr),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    Serial.printf("[LVGL] draw buf: %u px, %u bytes  (%u bytes/px)\n",
                  (unsigned)DRAW_BUF_PX, (unsigned)sizeof(draw_buf_arr),
                  (unsigned)(sizeof(draw_buf_arr) / DRAW_BUF_PX));
    Serial.printf("[LVGL] lv_color_t size: %u bytes  (should be irrelevant for RGB565 render)\n",
                  (unsigned)sizeof(lv_color_t));
    Serial.println("[LVGL] display driver registered (480x320, RGB565, partial render)");

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
    Serial.printf("[Display] Free heap: %u  Free PSRAM: %u\n",
                  ESP.getFreeHeap(), ESP.getFreePsram());

    // ── I2C scan: discover which address the GT911 is on ──
    i2c_scan();

    // LovyanGFX handles I2C for the touch controller internally
    // (pins configured in LovyanGFX_Driver.h). Wire.end() was called
    // in i2c_scan() so there's no bus conflict.

    gfx.init();
    Serial.println("[Display] gfx.init() done");

    // !! DO NOT call gfx.setColorDepth(16) !!
    // ILI9488 over SPI defaults to 18-bit (3 bytes/pixel).  Many ILI9488
    // panels silently IGNORE the COLMOD 0x55 command and stay in 18-bit mode.
    // If we tell LovyanGFX the panel is 16-bit, pushImage sends 2 bytes/pixel
    // into a panel expecting 3 bytes → garbled stripes after the first pixel row.
    // By leaving the default (18-bit), LovyanGFX auto-converts every RGB565 pixel
    // to RGB666 inside pushImage.  Slightly slower but correct.
    Serial.println("[Display] Panel colour depth left at default (18-bit / RGB666 SPI)");

    gfx.startWrite();
    gfx.fillScreen(TFT_BLACK);

    // ── Visual hardware test: red/green/blue bars ──────────────
    // If you see these, the SPI + display panel work correctly.
    gfx.fillRect(0,   0, 480, 107, TFT_RED);
    gfx.fillRect(0, 107, 480, 107, TFT_GREEN);
    gfx.fillRect(0, 214, 480, 106, TFT_BLUE);
    Serial.println("[Display] RGB test bars drawn — you should see red/green/blue");
    delay(1500);  // pause so you can see the test pattern

    gfx.fillScreen(TFT_BLACK);
    Serial.println("[Display] fillScreen(BLACK) done");

    initLVGL();

    // Backlight on GPIO 38
    pinMode(38, OUTPUT);
    digitalWrite(38, HIGH);
    Serial.println("[Display] Backlight ON (GPIO 38)");

    gfx.fillScreen(TFT_BLACK);
    Serial.printf("[Display] initDisplay complete.  Free heap: %u\n",
                  ESP.getFreeHeap());
}
