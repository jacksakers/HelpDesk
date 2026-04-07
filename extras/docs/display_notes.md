# HelpDesk — Display Hardware & Driver Notes

Everything learned while bringing up the Elecrow CrowPanel Advance 3.5" display.
Keep this file updated as new quirks are discovered.

---

## 1. Hardware Summary

| Property          | Value                                        |
|-------------------|----------------------------------------------|
| Product           | Elecrow CrowPanel **Advance** 3.5" HMI       |
| Model             | DIS01835A                                     |
| SoC               | ESP32-S3-WROOM-1-N16R8                        |
| Flash             | 16 MB (DIO mode — **not** QIO)               |
| PSRAM             | 8 MB (OPI)                                    |
| Display IC        | ILI9488                                       |
| Display type      | IPS                                           |
| Resolution        | 480 × 320                                     |
| Color depth       | 16-bit RGB565 (must be forced — see §4)       |
| Interface         | SPI (single-direction, no MISO)               |
| Touch IC          | GT911 (capacitive, single-touch)              |
| Touch interface   | I2C (addr 0x5D)                               |
| Backlight         | GPIO 38 (active HIGH)                         |

---

## 2. Pin Map

### SPI Display

| Signal | GPIO |
|--------|------|
| SCLK   | 42   |
| MOSI   | 39   |
| MISO   | -1 (not connected) |
| CS     | 40   |
| D/C    | 41   |
| RST    | -1 (not connected — no hardware reset pin) |

### Touch (I2C)

| Signal | GPIO |
|--------|------|
| SDA    | 15   |
| SCL    | 16   |
| INT    | 47 (optional — works without it) |

### Other

| Function           | GPIO |
|--------------------|------|
| Backlight          | 38   |
| SD Card MOSI       | 6    |
| SD Card MISO       | 4    |
| SD Card CLK        | 5    |
| SD Card CS         | 7    |
| I2S DOUT (speaker) | 12   |
| I2S BCLK           | 13   |
| I2S LRCLK          | 11   |
| I2S MIC SD         | 10   |
| I2S MIC WS         | 3    |
| I2S MIC CLK        | 9    |
| Buzzer             | 8    |
| RTC SDA            | 15 (shared with touch) |
| RTC SCL            | 16 (shared with touch) |
| Mute control (V1.2)| 21   |

---

## 3. LovyanGFX Driver Configuration

Library: `lovyan03/LovyanGFX @ ^1.2.0`

### Panel class: `Panel_ILI9488`

Despite the wiki calling this an ILI9488, the Elecrow factory code for the
**non-Advance** 3.5" uses `Panel_ST7789` with `memory_width=240`. That config
does **not** work on the Advance model — ST7789 maxes out at 240×320 and
produces a black screen on our 480×320 panel. `Panel_ILI9488` is correct.

### Critical settings

```
cfg.invert      = true     // IPS panel requires color inversion
cfg.rgb_order   = false    // No red/blue swap needed
cfg.bus_shared  = true     // SPI bus shared with SD card
cfg.readable    = false    // No MISO line
cfg.dlen_16bit  = false
```

### Rotation

```
cfg.memory_width     = 320
cfg.memory_height    = 480
cfg.panel_width      = 320
cfg.panel_height     = 480
cfg.offset_rotation  = 1    // landscape, USB connector on the right
```

### SPI bus

```
cfg.spi_host    = SPI2_HOST
cfg.freq_write  = 80000000  // 80 MHz write clock
cfg.freq_read   = 16000000
cfg.dma_channel = SPI_DMA_CH_AUTO
```

### Touch: `Touch_GT911`

```
cfg.i2c_addr        = 0x5D
cfg.i2c_port        = 0
cfg.pin_sda         = 15
cfg.pin_scl         = 16
cfg.freq            = 400000
cfg.offset_rotation = 0    // may need tuning (0–7)
```

The factory code for the non-Advance model uses `Touch_FT5x06` at
address 0x38. That is wrong for the Advance board — the Advance uses a GT911.

---

## 4. The 18-bit vs 16-bit Color Gotcha (KEY LESSON)

**Problem:** `Panel_ILI9488` in LovyanGFX defaults to **18-bit color**
(3 bytes per pixel on SPI). LVGL renders into an RGB565 buffer
(2 bytes per pixel). When the flush callback sends this 2-byte data to a
panel expecting 3 bytes, every pixel after the first is misaligned by
one byte, producing garbled streaks across the screen.

**Symptom:** LovyanGFX direct drawing (`fillRect`, `fillScreen`) looks
correct because LovyanGFX handles the conversion internally. But LVGL
content is garbled/streaked because the flush callback pushes raw
RGB565 bytes.

**Fix:** Call `gfx.setColorDepth(16)` immediately after `gfx.init()`.
This sends the COLMOD command (0x3A with parameter 0x55) to the panel,
switching it to 16-bit pixel format so that the SPI transfer matches
LVGL's RGB565 buffer.

```cpp
gfx.init();
gfx.setColorDepth(16);  // MUST be called before any LVGL rendering
```

**Status:** Test bars (drawn by LovyanGFX) now show correct R/G/B.
LVGL content is still garbled — the flush path likely still has a
buffer type or cast mismatch. See §5.

---

## 5. LVGL Integration (v9.2.x)

### Draw buffer

```cpp
#define DRAW_BUF_PX  (480 * 320 / 10)          // 15360 pixels
static uint16_t     draw_buf_arr[DRAW_BUF_PX]; // 30720 bytes
```

In LVGL 9, `lv_color_t` is **not** guaranteed to be 2 bytes even when
`LV_COLOR_DEPTH == 16` — it can be 3 or 4 bytes (lv_color32_t internally).
The draw buffer must be declared as `uint16_t[]` (not `lv_color_t[]`) and
the display color format must be set explicitly:

```cpp
lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
```

### Flush callback

```cpp
gfx.pushImage(x1, y1, w, h, (lgfx::rgb565_t *)px_map);
```

- Uses synchronous `pushImage`, **not** `pushImageDMA`.
  With a single buffer, DMA returns before transfer completes and LVGL
  overwrites the buffer → corruption or black screen.

### Tick source

```cpp
lv_tick_set_cb(my_tick);  // my_tick returns millis()
```

### lv_conf.h key settings

```
LV_COLOR_DEPTH          16
LV_USE_STDLIB_MALLOC     0  (built-in allocator)
LV_MEM_SIZE             48KB
LV_USE_OS                0
LV_DEF_REFR_PERIOD      33  (~30 fps)
```

---

## 6. Boot Sequence

1. `Serial.begin(115200)` + 500 ms delay for monitor to attach
2. Blink backlight (GPIO 38) 3× as a visual "firmware is running" test
3. `gfx.init()` → `gfx.setColorDepth(16)`
4. Draw R/G/B test bars (1.5 s hold) — visual SPI sanity check
5. `lv_init()` → create display + input drivers
6. Turn backlight on
7. `ui_init()` → builds LVGL screens
8. `connectToWiFi()`
9. Enter `loop()` — calls `lv_timer_handler()` every 5 ms

---

## 7. PlatformIO Build Notes

```ini
board              = esp32-s3-devkitc-1
board_build.flash_mode = dio          # MUST be DIO, not QIO — QIO causes boot loop
board_build.arduino.memory_type = dio_opi
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv
```

**No USB-CDC flags.** `Serial` goes to UART0 which appears as COM3 via
the onboard USB-to-UART chip. Adding `ARDUINO_USB_CDC_ON_BOOT` routes
Serial to the native USB port instead, which won't show on COM3.

---

## 8. Dead Ends & Things That Don't Work

| Attempt | Result | Why |
|---------|--------|-----|
| `Panel_ST7789` with 480×320 | Black screen | ST7789 memory maxes at 240×320 |
| `Panel_ILI9488` without `setColorDepth(16)` | Garbled LVGL | 18-bit SPI vs 16-bit LVGL buffer mismatch |
| `pushImageDMA` with single buffer | Black/corrupt | DMA returns before transfer; LVGL overwrites buffer |
| `ps_malloc` for draw buffer | NULL / crash | PSRAM must be enabled in build flags |
| `lv_color_t[]` draw buffer (LVGL 9) | Garbled | `lv_color_t` is 3–4 bytes in LVGL 9, not 2 |
| `QIO` flash mode | Boot loop crash | ESP32-S3 ROM boots in DIO; flash doesn't support QIO |
| `ARDUINO_USB_CDC_ON_BOOT` flag | No serial on COM3 | Redirects Serial to native USB, not UART0 |
| `Wire.begin()` before `gfx.init()` | I2C conflict | LovyanGFX manages touch I2C internally |
| `cfg.invert = false` | Wrong colors (B/M/Y) | IPS panel needs inversion enabled |
| `cfg.rgb_order = true` | Wrong colors (Y/M/C) | This swaps R↔B; not needed on this panel |

---

## 9. Open Issues

- **LVGL content is garbled** even after `setColorDepth(16)`. LovyanGFX
  direct drawing shows correct colors/pixels. Suspect the flush callback
  still has a buffer format mismatch between what LVGL renders and what
  `pushImage` receives. Need to verify the byte layout LVGL actually
  writes versus what `rgb565_t*` expects.

- **Touch offset_rotation** may need tuning. Touch registers taps but
  axis alignment with the display hasn't been fully verified (values 0–7).

---

## 10. Reference Links

- [Elecrow Wiki — CrowPanel Advance 3.5"](https://www.elecrow.com/wiki/CrowPanel_Advance_3.5-HMI_ESP32_AI_Display.html)
- [Elecrow GitHub repo](https://github.com/Elecrow-RD/CrowPanel-Advance-3.5-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-480x320)
- Factory code uses LVGL 8.3.3 + LovyanGFX 1.1.16 (we use LVGL 9.2 + LovyanGFX ^1.2.0)
