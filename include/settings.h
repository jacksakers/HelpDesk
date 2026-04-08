// Project  : HelpDesk
// File     : settings.h
// Purpose  : Persistent settings — load/save key-value pairs to SD card
// Depends  : sd_card.h, buzzer.h

#pragma once

#include "buzzer.h"

#ifdef __cplusplus
extern "C" {
#endif

// Load settings from SD card and apply to subsystems (buzzer, etc.).
// Defaults are used if the file is missing or a key is absent.
// Call after sdCardInit() and buzzerInit() in setup().
void settingsInit(void);

// Flush current subsystem state to the settings file on SD.
// Call after any user-visible setting changes (e.g. from Screen9 events).
void settingsSave(void);

// ── Buzzer / sound accessors ─────────────────────────────────────────────────
bool        settingsGetSoundMuted(void);
void        settingsSetSoundMuted(bool muted);

buzz_tone_t settingsGetClickTone(void);
void        settingsSetClickTone(buzz_tone_t tone);

#ifdef __cplusplus
} /* extern "C" */
#endif
