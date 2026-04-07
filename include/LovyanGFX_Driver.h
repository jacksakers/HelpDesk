// Project  : HelpDesk
// File     : LovyanGFX_Driver.h
// Purpose  : LovyanGFX hardware configuration for the Elecrow 3.5" ESP32-S3 HMI display
// Depends  : LovyanGFX library

// ─── Pin map (Elecrow CrowPanel Advance 3.5" ESP32-S3 HMI, 480x320) ────────
//   SPI SCLK  : 42      Backlight : 38
//   SPI MOSI  : 39      I2C SDA   : 15  (touch)
//   SPI CS    : 40      I2C SCL   : 16  (touch)
//   SPI D/C   : 41      Touch INT : 47
//   Touch I2C address : 0x5D (GT911)
//
//   NOTE: The ILI9488 chip on this board accepts ST7789-compatible 16-bit SPI.
//   Using Panel_ILI9488 sends 18-bit (3 bytes/pixel) which garbles the output.
//   Elecrow's official factory code uses Panel_ST7789 for this reason.

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <driver/i2c.h>

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7789  _panel_instance;   // 16-bit SPI (matches what the ILI9488 expects on this board)
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Touch_GT911   _touch_instance;

public:
    LGFX(void)
    {
        // ── SPI bus ──────────────────────────────────────────────────────
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host    = SPI2_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = 80000000;
            cfg.freq_read   = 16000000;
            cfg.spi_3wire   = false;
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = 42;
            cfg.pin_mosi    = 39;
            cfg.pin_miso    = -1;
            cfg.pin_dc      = 41;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        // ── Display panel (480x320, landscape) ──────────────────────────
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = 40;
            cfg.pin_rst          = -1;
            cfg.pin_busy         = -1;
            cfg.memory_width     = 320;
            cfg.memory_height    = 480;
            cfg.panel_width      = 320;
            cfg.panel_height     = 480;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 1;   // landscape, USB-right orientation
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = false;
            cfg.invert           = true;    // required per Elecrow factory code
            cfg.rgb_order        = false;   // no R/B swap needed
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = true;
            _panel_instance.config(cfg);
        }

        // ── Touch (GT911 via I2C) ────────────────────────────────────────
        {
            auto cfg = _touch_instance.config();
            cfg.x_min          = 0;
            cfg.x_max          = 479;
            cfg.y_min          = 0;
            cfg.y_max          = 319;
            cfg.pin_int        = -1;
            cfg.bus_shared     = false;
            cfg.offset_rotation = 0;
            cfg.i2c_port       = 0;
            cfg.i2c_addr       = 0x5D;    // GT911 default address
            cfg.pin_sda        = 15;
            cfg.pin_scl        = 16;
            cfg.freq           = 400000;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};
