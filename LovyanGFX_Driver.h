// Project  : HelpDesk
// File     : LovyanGFX_Driver.h
// Purpose  : LovyanGFX hardware configuration for the Elecrow 3.5" ESP32 HMI display
// Depends  : LovyanGFX library

// ─── Pin map (Elecrow 3.5" ESP32-S3 HMI, ST7789 + FT5x06 touch) ────────────
//   SPI SCLK  : 42      Backlight : 38
//   SPI MOSI  : 39      I2C SDA   : 15  (touch)
//   SPI CS    : 40      I2C SCL   : 16  (touch)
//   SPI D/C   : 41      Touch INT : 47
//   Touch I2C address : 0x38

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <driver/i2c.h>

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7789  _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Touch_FT5x06  _touch_instance;

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

        // ── Display panel ────────────────────────────────────────────────
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = 40;
            cfg.pin_rst          = -1;
            cfg.pin_busy         = -1;
            cfg.memory_width     = 240;
            cfg.memory_height    = 320;
            cfg.panel_width      = 240;
            cfg.panel_height     = 320;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 3;   // landscape, USB-right orientation
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = false;
            cfg.invert           = true;
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = true;
            _panel_instance.config(cfg);
        }

        // ── Touch (FT5x06 via I2C) ───────────────────────────────────────
        {
            auto cfg = _touch_instance.config();
            cfg.x_min          = 0;
            cfg.x_max          = 239;
            cfg.y_min          = 0;
            cfg.y_max          = 319;
            cfg.pin_int        = 47;
            cfg.bus_shared     = false;
            cfg.offset_rotation = 0;
            cfg.i2c_port       = 0;
            cfg.i2c_addr       = 0x38;
            cfg.pin_sda        = 15;
            cfg.pin_scl        = 16;
            cfg.freq           = 400000;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};
