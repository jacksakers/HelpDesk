# HelpDesk — Key Lessons Learned

Critical insights from bringing up the Elecrow CrowPanel Advance 3.5" ESP32-S3 HMI display.

---

## 1. ILI9488 Color Depth: Let It Be 18-bit

**The Problem:** LVGL UI appeared as garbled colorful streaks, but direct LovyanGFX
drawing (test bars) worked perfectly.

**Root Cause:** ILI9488 over SPI defaults to **18-bit color** (3 bytes per pixel on
the wire). Many ILI9488 panels **ignore** the COLMOD 0x55 command that requests
16-bit mode. When you call `gfx.setColorDepth(16)`, LovyanGFX thinks the panel
accepted 16-bit mode and sends 2 bytes/pixel. The panel reads 3 bytes/pixel →
every pixel after the first is misaligned by one byte → garbled display.

**The Fix:** **Never call** `gfx.setColorDepth(16)` on ILI9488 panels.  
Leave `Panel_ILI9488` at its 18-bit default. When you call:

```cpp
gfx.pushImage(x, y, w, h, (lgfx::rgb565_t *)px_map);
```

LovyanGFX automatically converts RGB565 → RGB666 (18-bit) during the SPI transfer
because it knows the panel is in 18-bit mode. This is transparent and works correctly.

**Key Takeaway:** Panel color depth != buffer color depth. LVGL can render into
`uint16_t` RGB565 buffers, and LovyanGFX handles the conversion to match the panel's
native depth.

---

## 2. Touch Coordinates: Hardware Orientation, Not Display Orientation

**The Problem:** Touch was detected but coordinates were completely wrong. Touching
the upper-left quadrant activated buttons in the center of the screen. Only certain
areas responded to touch.

**Root Cause:** The GT911 capacitive touch sensor is **physically mounted in portrait
orientation** on the PCB (320×480), regardless of how the display is rotated. Touch
coordinates were configured for landscape (x_max=479, y_max=319), mismatching the
sensor hardware.

**The Fix:** Always set touch `x_max` and `y_max` to match the **sensor's physical
orientation**, not the display's logical orientation:

```cpp
// ✓ CORRECT — GT911 sensor is portrait (320×480)
cfg.x_min = 0;
cfg.x_max = 319;    // Sensor physical width
cfg.y_min = 0;
cfg.y_max = 479;    // Sensor physical height
cfg.offset_rotation = 2;  // Transform to align with display rotation
```

**How It Works:** LovyanGFX reads raw coordinates from the sensor (0–319, 0–479 for
portrait), then applies a rotation transform using `offset_rotation` to align with
the display's `offset_rotation`. For our HelpDesk config:
- Panel `offset_rotation = 1` (landscape, USB connector on right)
- Touch `offset_rotation = 0` (no rotation offset — empirically determined)

**Key Takeaway:** Touch sensor coordinates are **hardware-based**, not display-based.
Configure x/y to match the physical sensor, then use `offset_rotation` to align with
how the display is rotated. **Do not assume** offset_rotation is an arithmetic function
of panel rotation — test with actual touches and adjust until corners map correctly.

---

## 3. ESP32-S3 Flash Mode: DIO, Not QIO

**The Problem:** ESP32-S3 boot looped with ROM error messages like "invalid header"
and "mode:DIO" errors.

**Root Cause:** PlatformIO's default `board_build.flash_mode = qio` is wrong for
ESP32-S3. The S3's ROM bootloader always boots in **DIO mode** by default, and the
Elecrow board's flash chip is wired for DIO.

**The Fix:**
```ini
[env:esp32s3]
board_build.flash_mode = dio
board_build.arduino.memory_type = dio_opi
```

**Key Takeaway:** ESP32-S3 uses DIO flash mode by default. Check the board schematic
or ROM boot messages to confirm, and set `flash_mode` explicitly in platformio.ini.

---

## 4. GT911 Touch Has Two I2C Addresses

**The Problem:** Touch initialization was slow or failed sporadically.

**Root Cause:** GT911 can operate at two I2C addresses:
- **0x5D** (primary, INT pin pulled high or floating during reset)
- **0x14** (alternate, INT pin pulled low during reset)

The address is latched during the controller's power-on reset, depending on the
INT pin state. The Elecrow board's hardware design results in address **0x14**.

**The Fix:**
1. Add an I2C bus scan at boot to discover the actual device address:
   ```cpp
   Wire.begin(15, 16);
   for(uint8_t addr = 1; addr < 127; addr++) {
       Wire.beginTransmission(addr);
       if(Wire.endTransmission() == 0) {
           Serial.printf("[I2C]   0x%02X  ← device found\n", addr);
       }
   }
   ```
2. Set `cfg.i2c_addr = 0x14` to match the board's hardware.
3. Enable `cfg.pin_int = 47` for interrupt-driven touch reads (faster, more reliable).

**Key Takeaway:** Always scan the I2C bus at boot to verify device addresses, especially
for touch controllers with multiple possible addresses.

---

## 5. LVGL 9: Use Explicit Color Format

**The Problem:** LVGL 9's `lv_color_t` is **not** guaranteed to be 2 bytes even when
`LV_COLOR_DEPTH == 16`. Internally it may be 3 or 4 bytes (lv_color32_t).

**The Fix:** Declare the draw buffer as `uint16_t[]` and set the color format explicitly:

```cpp
#define DRAW_BUF_PX  (480 * 320 / 10)
static uint16_t draw_buf_arr[DRAW_BUF_PX];  // Not lv_color_t[]!

lv_display_t * disp = lv_display_create(480, 320);
lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
lv_display_set_buffers(disp, draw_buf_arr, NULL, sizeof(draw_buf_arr),
                       LV_DISPLAY_RENDER_MODE_PARTIAL);
```

**Key Takeaway:** In LVGL 9, always declare draw buffers as `uint16_t[]` for RGB565
and set the color format explicitly. Don't assume `lv_color_t` matches your color depth.

---

## 6. pushImage (Synchronous), Not pushImageDMA

**The Problem:** Using `pushImageDMA` with a single buffer caused black screens or
flickering.

**Root Cause:** `pushImageDMA` returns immediately before the DMA transfer completes.
LVGL overwrites the buffer while DMA is still reading it → corruption.

**The Fix:** Use synchronous `pushImage` in the flush callback:

```cpp
void my_disp_flush(lv_display_t * d, const lv_area_t * area, uint8_t * px_map) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    gfx.pushImage(area->x1, area->y1, w, h, (lgfx::rgb565_t *)px_map);
    lv_display_flush_ready(d);  // Called only after transfer completes
}
```

**Key Takeaway:** With single-buffer rendering, use synchronous `pushImage`. Only use
`pushImageDMA` with double-buffering where LVGL has a second buffer to render into
while DMA reads the first.

---

## 7. Backlight Blink: The "Code is Running" Test

**Best Practice:** At the very start of `setup()`, before Serial, before anything else,
blink the backlight (GPIO 38) three times:

```cpp
pinMode(38, OUTPUT);
for(int i = 0; i < 6; i++) {
    digitalWrite(38, i & 1 ? LOW : HIGH);
    delay(150);
}
digitalWrite(38, HIGH);  // Leave on
```

**Why:** If the firmware is broken (crash, wrong partition, infinite loop), you'll
still see the backlight blink. This proves:
- The flash succeeded
- The code is executing
- The problem is later in init, not in basic execution

**Key Takeaway:** Always add a visual "heartbeat" indicator at the start of `setup()`
for embedded devices with displays. It's the fastest way to distinguish "firmware
didn't load" from "firmware loaded but init failed."

---

## 8. IPS Panels Need Color Inversion

**The Problem:** Colors looked washed out or inverted (whites appeared dark, blacks bright).

**Root Cause:** Many IPS panels require the display inversion bit to be set for correct
color rendering. This is hardware-dependent, not standard across all ILI9488 panels.

**The Fix:**
```cpp
cfg.invert = true;  // IPS panel needs color inversion
```

**Key Takeaway:** If an IPS display shows inverted or weird colors, try toggling
`cfg.invert`. Check the working example for your specific hardware.

---

## 9. SPI Speed: Start Conservative, Then Optimize

**Best Practice:** Start with a known-good SPI frequency from a working example, even
if it's slower than the theoretical maximum. For the Elecrow board:

```cpp
cfg.freq_write = 40000000;  // 40 MHz — known working
```

We initially tried 80 MHz (ILI9488 datasheet max) but saw garbled output. After
fixing the color-depth issue, 40 MHz worked reliably. You can experiment with higher
speeds once the display is stable, but start conservative.

**Key Takeaway:** Overclocking SPI can cause subtle corruption that's hard to diagnose.
Match the working example's speed first, verify everything works, *then* optimize.

---

## 10. Diagnostic Serial Output is Essential

**Best Practice:** Add detailed serial logging at each init stage:

```cpp
Serial.println("[Display] Starting initDisplay...");
Serial.printf("[Display] Free heap: %u  Free PSRAM: %u\n",
              ESP.getFreeHeap(), ESP.getFreePsram());
// ... I2C scan results ...
Serial.println("[Display] gfx.init() done");
Serial.println("[LVGL] lv_init() done");
Serial.printf("[flush #%lu] x=%d y=%d %dx%d  px[0]=0x%04X\n", ...);
Serial.printf("[Touch] x=%ld  y=%ld\n", ...);
```

**Why:** When debugging embedded displays, you can't use a debugger or printf to
"see inside" the hardware. Serial logging is your only window into what's happening.
It helps distinguish:
- Code not running (no serial output)
- Init crashing (serial stops mid-sequence)
- Logic bugs (serial shows wrong values)
- Hardware issues (serial shows correct values but screen is wrong)

**Key Takeaway:** Always log major init milestones, hardware interaction results
(I2C scans, flash results), and event data (touch coords, flush regions). The few
minutes spent adding logs will save hours of blind debugging.

---

## Summary: The Debugging Sequence

When bringing up a new display board:

1. **Blink the backlight** first — proves firmware loads  
2. **Check serial output** — proves code is running and reaching init  
3. **Draw test patterns** (direct LovyanGFX) — proves SPI and panel config  
4. **Check LVGL rendering** — proves buffer config and flush callback  
5. **Test touch** — proves I2C, touch config, and coordinate mapping  
6. **Add diagnostics** — log everything: I2C scans, flush regions, touch events  

Fix issues in order: flash/boot → SPI/panel → LVGL/buffer → touch/I2C.
Don't skip steps — each layer builds on the previous.

---

## References

- [LovyanGFX GitHub](https://github.com/lovyan03/LovyanGFX)  
- [LVGL v9 Docs](https://docs.lvgl.io/9.2/)  
- [GT911 Datasheet](https://github.com/goodix/gt9xx_driver_generic)  
- [ILI9488 Datasheet](https://www.displayfuture.com/Display/datasheet/controller/ILI9488.pdf)  
- Elecrow working example: `extras/examples/Desktop_Assistant_35/`  

---

**Last Updated:** April 7, 2026  
**Project:** HelpDesk (Elecrow CrowPanel Advance 3.5" ESP32-S3)
