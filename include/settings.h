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

// ── WiFi accessors ────────────────────────────────────────────────────────────
// Returns a pointer to the in-memory string; valid until the next settingsSave.
const char * settingsGetWifiSSID(void);
void         settingsSetWifiSSID(const char * ssid);

const char * settingsGetWifiPassword(void);
void         settingsSetWifiPassword(const char * password);

// ── OpenWeatherMap accessors ──────────────────────────────────────────────────
const char * settingsGetOwmKey(void);
void         settingsSetOwmKey(const char * key);

const char * settingsGetOwmCity(void);    /* City name or zip code */
void         settingsSetOwmCity(const char * city);

const char * settingsGetOwmUnits(void);   /* "metric" or "imperial" */
void         settingsSetOwmUnits(const char * units);

// ── Companion-app connection ───────────────────────────────────────────────────
// IP address of the PC running the companion app (used for voice transcription).
const char * settingsGetCompanionIP(void);
void         settingsSetCompanionIP(const char * ip);

// ── DeskChat ──────────────────────────────────────────────────────────────────
// Display name shown in LoRa group chat.  Empty string = "Anon".
const char * settingsGetChatUsername(void);
void         settingsSetChatUsername(const char * name);

#ifdef __cplusplus
} /* extern "C" */
#endif
