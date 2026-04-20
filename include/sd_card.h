// Project  : HelpDesk
// File     : sd_card.h
// Purpose  : SD card initialisation — public interface
// Depends  : <SPI.h>, <SD.h>

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Initialise the dedicated SPI bus (SPI3/HSPI) and mount the SD card.
// Must be called in setup() before any SD reads/writes (settings, zen_frame).
void sdCardInit(void);

// Returns true if the SD card was successfully mounted.
bool sdCardMounted(void);

// Returns a pointer to the HSPI bus used by the SD card.
// DeskChat may share this bus (same SCK/MISO/MOSI, different CS).
// Only valid after sdCardInit(); returns nullptr before that.
struct SPIClass * sdCardGetSPI(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
