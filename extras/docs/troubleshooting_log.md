# HelpDesk — Troubleshooting Log

Running list of display/touch problems, their diagnosis, and resolution.

---

## Issue #1: Black Screen on First Boot (RESOLVED)

**Date:** Early development  
**Symptom:** Screen stays black after flash.  
**Cause:** Used `Panel_ST7789` — wrong panel IC. ST7789 maxes out at 240×320.  
**Fix:** Switched to `Panel_ILI9488`.

---

## Issue #2: Boot Loop / Crash (RESOLVED)

**Date:** Early development  
**Symptom:** ESP32-S3 boot loops, ROM prints "mode:DIO" errors.  
**Cause:** `board_build.flash_mode = qio` in platformio.ini.  
**Fix:** Changed to `dio`. ESP32-S3 ROM boots in DIO.

---

## Issue #3: Garbled LVGL, Correct Direct Drawing (IN PROGRESS — Fix Applied)

**Date:** 2026-04-07  
**Symptom:** `fillRect` test bars show correct R/G/B. LVGL UI shows garbled
coloured streaks/stripes.  
**Diagnosis:**  
1. ILI9488 over SPI defaults to 18-bit colour (3 bytes/pixel)
2. `gfx.setColorDepth(16)` sends COLMOD 0x55 to panel
3. Many ILI9488 panels silently ignore COLMOD and stay 18-bit
4. LovyanGFX `pushImage` sends 2 bytes/pixel (thinking panel is 16-bit)
5. Panel reads 3 bytes for each pixel → byte drift → garbled

**Fix:** Removed `gfx.setColorDepth(16)`. Leave Panel_ILI9488 at 18-bit
default. `pushImage` with `rgb565_t*` auto-converts 16→18 bit.  
Also lowered SPI from 80 MHz to 40 MHz (matching working example).

**Verification steps:**
1. Flash updated firmware
2. Check serial output for `[Display]`, `[LVGL]`, `[flush]` messages
3. RGB test bars should show correctly (as before)
4. LVGL UI (launcher screen) should now render correctly
5. If still garbled, check flush pixel dumps: `px[0]` and `px[1]` should
   be valid RGB565 values (not 0x0000 for a coloured widget)

---

## Issue #4: Touch Coordinates Misaligned (RESOLVED)

**Date:** 2026-04-07  
**Symptom:** Touch input detected but coordinates wrong. Touching upper-left quadrant
activates buttons in the middle of the screen. Touch only works in specific areas.  

**Root Cause:** Touch coordinate ranges were set in landscape orientation (x=0-479, y=0-319)
but GT911 hardware sensor is physically mounted in **portrait** orientation. The x_max and
y_max values must match the sensor's physical layout (319×479), NOT the display orientation.
The `offset_rotation` parameter then transforms coordinates to match the display.

**Diagnosis:**
1. Current config had x_max=479, y_max=319 (landscape)
2. Example uses x_max=319, y_max=479 (portrait) with panel rotation 3
3. Our panel uses rotation 1 (180° different from example)
4. Touch offset_rotation was 0, needs to be adjusted for panel rotation delta

**Fix:**  
1. Swapped touch coordinates: `x_max = 319`, `y_max = 479` (portrait, matches hardware)
2. Set `offset_rotation = 2` (accounts for 180° difference: panel rot 1 vs example's rot 3)

**Key Lesson:** GT911 touch sensor coordinates are **hardware-based** (portrait),
not display-based. Always set x_max/y_max to match the sensor's physical orientation,
then use offset_rotation to align with the display's rotation setting.

**Verification:** Touch all areas of screen (corners, edges, center). Coordinates
should match visual position. Check serial output: `[Touch] x=... y=...`

---

## Issue #5: Touch Coordinates Fully Inverted (RESOLVED)

**Date:** 2026-04-07 (evening)  
**Symptom:** Touch detection works but coordinates are inverted on both axes:
- Press lower-right corner → triggers upper-left (back button)
- Press upper-right corner → triggers bottom-left (Zen frame)

**Root Cause:** Touch `offset_rotation = 2` was incorrect. The 180° offset assumption
(panel rot 1 vs example's rot 3) over-rotated the coordinates. Panel rotation 1
(landscape 90° CW) with GT911 portrait sensor needs `offset_rotation = 0`.

**Fix:** Changed touch `offset_rotation` from 2 to 0 in `LovyanGFX_Driver.h`.

**Key Lesson:** Touch offset_rotation mapping is not always a simple arithmetic
difference from reference code. Test with actual touches and adjust empirically.
For panel rotation 1 (landscape, USB-right), touch rotation 0 is correct.

**Verification:** Touch each corner and center. Visual press should match UI response:
- Upper-left → back button
- Upper-right → should NOT trigger Zen frame (lower-left)
- Lower-right → should NOT trigger back button

---

## Diagnostic Serial Output to Watch For

After flashing, open serial monitor at 115200 baud. Key lines:

```
[I2C]   0x14  ← device found          # Confirms GT911 address
[Display] gfx.init() done
[Display] Panel colour depth left at default (18-bit / RGB666 SPI)
[Display] RGB test bars drawn
[LVGL] lv_init() done
[LVGL] draw buf: 15360 px, 30720 bytes  (2 bytes/px)
[LVGL] lv_color_t size: 3 bytes        # Normal for LVGL 9
[flush #1] x=0 y=0 480x32  px[0]=0x... # First LVGL flush
[Touch] x=240  y=160                   # First touch event
[loop] calls=...  heap=...             # Every 5 seconds
```

---

## Things to Check if Still Broken

### Display Still Garbled
- [ ] Is `setColorDepth(16)` truly removed? Grep for it.
- [ ] Try `cfg.freq_write = 20000000` (20 MHz) — maybe 40 MHz is still too fast.
- [ ] Try `cfg.offset_rotation = 3` instead of 1 — the example uses 3.
- [ ] Add `gfx.initDMA()` after `gfx.init()`.
- [ ] Check if LVGL is rendering to the buffer at all: px[0] should not always be 0x0000.
- [ ] If px values look correct but display is garbled, the issue is in the SPI transfer
      path (LovyanGFX bug or timing issue).
- [ ] Try double-buffering: add a second `uint16_t draw_buf_arr2[DRAW_BUF_PX]` and pass
      both to `lv_display_set_buffers`.

### Touch Not Working
- [ ] Check I2C scan output — is any device found?
- [ ] If 0x5D appears instead of 0x14, change `cfg.i2c_addr` back to 0x5D.
- [ ] If no device found, the GT911 may need a reset. Add `cfg.pin_rst = 48` to
      the touch config (the example has RST on GPIO 48, but our display_notes.md
      doesn't list it — the Advance board may not have it connected).
- [ ] Try adding `Wire.begin(15, 16)` before `gfx.init()` (the example does this).

### Performance Issues
- [ ] If UI is too slow at 40 MHz / 18-bit, try raising SPI to 80 MHz first.
- [ ] If still slow, enable DMA with double-buffering and `pushImageDMA`.
- [ ] Reduce draw buffer to `480 * 320 / 20` (fewer pixels per flush).
- [ ] Check heap in serial output — if < 30KB, reduce LVGL widgets or LV_MEM_SIZE.
