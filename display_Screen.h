// Project  : HelpDesk
// File     : display_Screen.h
// Purpose  : Display and LVGL initialisation — public interface
// Depends  : LovyanGFX_Driver.h, lvgl.h

#pragma once

// Initialise the display, DMA, touch input, and LVGL.
// Must be called before ui_init() in setup().
void initDisplay();
