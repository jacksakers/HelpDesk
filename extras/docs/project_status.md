# HelpDesk — Project Status & Context

Last updated: 2026-04-07

---

## Hardware

| Component         | Detail                                         |
|-------------------|-------------------------------------------------|
| Board             | Elecrow CrowPanel **Advance** 3.5" HMI (DIS01835A) |
| SoC               | ESP32-S3-WROOM-1-N16R8                         |
| Flash             | 16 MB (DIO mode — not QIO)                     |
| PSRAM             | 8 MB (OPI)                                      |
| Display IC        | ILI9488 (SPI, 480×320, IPS)                    |
| Touch IC          | GT911 (I2C, capacitive)                         |
| Speaker           | I2S (DOUT=12, BCLK=13, LRCLK=11)              |
| Buzzer            | GPIO 8                                          |
| Backlight         | GPIO 38 (active HIGH)                           |
| SD Card           | SPI (MOSI=6, MISO=4, CLK=5, CS=7)             |

---

## Software Stack

| Layer               | Library / Version                              |
|---------------------|-------------------------------------------------|
| Framework           | Arduino (via PlatformIO)                        |
| Platform            | espressif32                                     |
| Board               | esp32-s3-devkitc-1                              |
| Graphics driver     | LovyanGFX ^1.2.0                               |
| UI framework        | LVGL ^9.2.0 (v9.2.x API)                       |
| UI designer         | SquareLine Studio 1.5.1 (code-generated screens)|
| Build system        | PlatformIO                                      |

---

## Build Configuration (platformio.ini)

```ini
board_build.arduino.memory_type = dio_opi
board_build.flash_mode = dio
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv
build_flags = -DBOARD_HAS_PSRAM -DLV_CONF_INCLUDE_SIMPLE -I include
```

---

## Project Structure

```
HelpDesk/
├── platformio.ini
├── include/
│   ├── LovyanGFX_Driver.h      # Panel + touch pin config
│   ├── lv_conf.h                # LVGL 9.2 configuration
│   ├── display_Screen.h         # initDisplay() prototype
│   ├── wifi_connect.h           # connectToWiFi() prototype
│   ├── ui.h                     # LVGL UI lifecycle (ui_init / ui_destroy)
│   ├── ui_helpers.h             # SquareLine Studio helpers
│   ├── ui_events.h              # UI event handler prototypes
│   └── ui_Screen1..8.h          # Per-screen header files
├── src/
│   ├── main.cpp                 # setup() + loop()
│   ├── display_Screen.cpp       # LovyanGFX + LVGL init + flush
│   ├── wifi_connect.cpp         # WiFi connection
│   ├── ui.c                     # ui_init() / ui_destroy()
│   ├── ui_helpers.c             # SquareLine helpers impl
│   ├── ui_comp_hook.c           # Component hooks
│   └── ui_Screen1..8.c          # Per-screen widget trees
└── extras/
    ├── docs/                    # Documentation (this folder)
    ├── examples/
    │   ├── Desktop_Assistant_35/ # Working example for 3.5" (reference)
    │   └── squareline-example/  # SquareLine export example
    ├── HMI3-5/                  # Elecrow factory demo (different screen!)
    └── UI/                      # SquareLine UI assets
```

---

## Screens (SquareLine Studio)

| Screen | Name     | Purpose                    | Status       |
|--------|----------|----------------------------|--------------|
| 1      | Launcher | Home/app drawer            | Implemented  |
| 2      | unknown  | TBD                        | Placeholder  |
| 3      | PomoFocus| Pomodoro timer             | Placeholder  |
| 4      | unknown  | TBD                        | Placeholder  |
| 5      | unknown  | TBD                        | Placeholder  |
| 6      | unknown  | TBD                        | Placeholder  |
| 7      | unknown  | TBD                        | Placeholder  |
| 8      | unknown  | TBD                        | Placeholder  |

---

## Feature Modules (Planned)

| Module      | Init function      | Loop function           | Status      |
|-------------|--------------------|-------------------------|-------------|
| Display     | initDisplay()      | (in flush callback)     | ✓ Working   |
| Touch Input | (in LovyanGFX)     | (in touch_read cb)      | ✓ Working   |
| WiFi        | connectToWiFi()    | —                       | ✓ Working   |
| NTP Time    | initNTP()          | handleTimeUpdate()      | Not wired   |
| Weather     | getWeatherData()   | handleWeatherUpdate()   | Not wired   |
| Music       | audioInit()        | audioLoop()             | Not wired   |
| Alarm       | initAlarmClock()   | handleAlarmClock()      | Not wired   |
| Timer       | initTimer()        | handleTimer()           | Not wired   |
| PC Monitor  | pcMonitorInit()    | handlePcMonitor()       | Not started |
| PomoFocus   | initPomoTimer()    | handlePomoTimer()       | Not started |

---

## Recent Fixes (2026-04-07)

### ✓ RESOLVED: Garbled LVGL Display

**Issue:** LVGL UI showed colorful streaks/stripes. Direct LovyanGFX drawing worked.  
**Cause:** ILI9488 over SPI defaults to 18-bit color (3 bytes/pixel). Calling
`setColorDepth(16)` confused the pipeline — panel stayed at 18-bit but flush
sent 2 bytes/pixel, causing byte-alignment drift.  
**Fix:** Removed `setColorDepth(16)`. Leave panel at 18-bit default. LovyanGFX
`pushImage` with `rgb565_t*` auto-converts 16→18 bit correctly.

### ✓ RESOLVED: Touch Coordinates Misaligned

**Issue:** Touch detected but coordinates wrong. Upper-left touch triggered center buttons.  
**Cause:** Touch x_max/y_max were set to display dimensions (480×320 landscape),
but GT911 sensor hardware is physically portrait (320×480). LovyanGFX transforms
coordinates from sensor space to display space using `offset_rotation`.  
**Fix:** Set touch x_max=319, y_max=479 (portrait, matches sensor hardware),
then adjusted `offset_rotation = 2` to align with panel rotation 1.

---

## Current Issues

None. Next step: wire up feature modules (NTP, weather, music, etc.).

---

## Previous Issues (Resolved — see troubleshooting_log.md)

### 1. Black Screen

Panel driver was wrong (ST7789), flash mode was wrong (QIO).  
Fixed by using Panel_ILI9488 and DIO flash mode.

### 2. Boot Loop / Crash  

Flash mode QIO caused ROM boot errors. Fixed by switching to DIO.

### 3. Garbled LVGL Display

ILI9488 stayed at 18-bit SPI despite setColorDepth(16) call.  
Fixed by removing setColorDepth and letting LovyanGFX auto-convert.

### 4. Touch Coordinates Misaligned

Touch x/y were set to display orientation (landscape) instead of sensor hardware (portrait).  
Fixed by setting x_max=319, y_max=479, offset_rotation=2.

---

## Reference Examples

### Desktop_Assistant_35 (Primary Reference)

Located at `extras/examples/Desktop_Assistant_35/`. This is the closest
working example for the 3.5" CrowPanel Advance. Key settings:

- Panel: `Panel_ILI9488`, SPI @ 40 MHz
- Touch: GT911 @ I2C addr 0x14, INT=47, RST=48
- LVGL: v9.2.2, LV_MEM_SIZE=64KB
- Uses `pushImageDMA` (but our code uses synchronous `pushImage` for
  single-buffer safety)
- Uses TAMC_GT911 library separately from LovyanGFX touch (redundant)

### HMI3-5 (Different Board — NOT a Reference)

Located at `extras/HMI3-5/`. This is the factory demo for the
**non-Advance** 3.5" board which has a 320×240 display (not 480×320).
Do NOT use its configuration.

---

## Key Lessons Learned

1. **ILI9488 SPI is 18-bit** — don't force 16-bit with setColorDepth(16)
2. **LVGL 9 lv_color_t is 3 bytes** — use `uint16_t[]` for draw buffers
3. **ESP32-S3 needs DIO flash mode** — QIO causes boot loops
4. **GT911 has two addresses** (0x14 and 0x5D) — depends on INT pin state at reset
5. **No Wire.begin() before gfx.init()** — LovyanGFX manages I2C internally
6. **pushImage, not pushImageDMA** with single buffer — DMA returns early
7. **lv_display_set_color_format(RGB565)** must be called explicitly in LVGL 9
