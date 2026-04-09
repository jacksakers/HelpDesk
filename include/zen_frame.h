// Project  : HelpDesk
// File     : zen_frame.h
// Purpose  : ZenFrame digital photo frame — public interface
// Depends  : sd_card.h, ui_Screen6.h (LVGL 9.x)

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Scan /images on the SD card, build the playlist, and show the first image.
// Call after sdCardInit() and ui_init() in setup().
void zenFrameInit(void);

// Force-advance to the next image in the playlist immediately.
// Called by the ZenFrame screen tap-to-skip handler.
void zenFrameNext(void);

// Re-display the current image on ui_ZenImage.
// Call from ui_Screen6_screen_init() after the image widget is created.
void zenFrameRefresh(void);

// Called every loop(). Automatically advances to the next image when the
// cycle interval elapses.  No-op if fewer than 2 images are found.
void handleZenFrame(unsigned long now_ms);

// Rescans /images on the SD card and refreshes the current display.
// Call after a new image is uploaded via WiFi so the playlist updates
// without requiring a reboot.
void zenFrameRescan(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
