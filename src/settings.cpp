// Project  : HelpDesk
// File     : settings.cpp
// Purpose  : Persistent key-value settings stored on SD card
// Depends  : settings.h, sd_card.h, buzzer.h, <SD.h>

#include "settings.h"
#include "sd_card.h"
#include "buzzer.h"
#include <Arduino.h>
#include <SD.h>

#define SETTINGS_FILE  "/settings/helpdesk.txt"
#define MAX_LINE_LEN   80

/* ── In-memory mirror of persisted values ────────────────────────────────── */
static bool        s_sound_muted = false;
static buzz_tone_t s_click_tone  = BUZZ_TONE_CLICK;
static char s_wifi_ssid[64]     = "";
static char s_wifi_password[64] = "";
static char s_owm_key[64]       = "";
static char s_owm_city[64]      = "";
static char s_owm_units[12]     = "imperial";
static char s_companion_ip[40]  = "";
/* ── Parse one "key=value" line and store into the mirror ───────────────── */
static void apply_kv(const char * key, const char * val)
{
    if (strcmp(key, "sound_muted") == 0) {
        s_sound_muted = (atoi(val) != 0);
    } else if (strcmp(key, "click_tone") == 0) {
        int t = atoi(val);
        if (t >= 0 && t < (int)BUZZ_TONE_COUNT) {
            s_click_tone = (buzz_tone_t)t;
        }
    } else if (strcmp(key, "wifi_ssid") == 0) {
        strncpy(s_wifi_ssid, val, sizeof(s_wifi_ssid) - 1);
        s_wifi_ssid[sizeof(s_wifi_ssid) - 1] = '\0';
    } else if (strcmp(key, "wifi_password") == 0) {
        strncpy(s_wifi_password, val, sizeof(s_wifi_password) - 1);
        s_wifi_password[sizeof(s_wifi_password) - 1] = '\0';
    } else if (strcmp(key, "owm_key") == 0) {
        strncpy(s_owm_key, val, sizeof(s_owm_key) - 1);
        s_owm_key[sizeof(s_owm_key) - 1] = '\0';
    } else if (strcmp(key, "owm_city") == 0) {
        strncpy(s_owm_city, val, sizeof(s_owm_city) - 1);
        s_owm_city[sizeof(s_owm_city) - 1] = '\0';
    } else if (strcmp(key, "owm_units") == 0) {
        strncpy(s_owm_units, val, sizeof(s_owm_units) - 1);
        s_owm_units[sizeof(s_owm_units) - 1] = '\0';
    } else if (strcmp(key, "companion_ip") == 0) {
        strncpy(s_companion_ip, val, sizeof(s_companion_ip) - 1);
        s_companion_ip[sizeof(s_companion_ip) - 1] = '\0';
    }
    /* Unknown keys are silently ignored for forwards compatibility */
}

/* ── Load file into in-memory mirror ────────────────────────────────────── */
static void load_from_sd(void)
{
    if (!sdCardMounted()) return;

    File f = SD.open(SETTINGS_FILE, FILE_READ);
    if (!f) {
        Serial.println("[Settings] No settings file — using defaults.");
        return;
    }

    char line[MAX_LINE_LEN];
    while (f.available()) {
        int n = f.readBytesUntil('\n', line, (int)sizeof(line) - 1);
        line[n] = '\0';

        /* Strip trailing CR (\r\n line endings) */
        if (n > 0 && line[n - 1] == '\r') { line[n - 1] = '\0'; n--; }

        /* Skip blank lines and comment lines */
        if (n == 0 || line[0] == '#') continue;

        char * eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';               /* Split key / value in place */
        apply_kv(line, eq + 1);
    }
    f.close();
    Serial.println("[Settings] Loaded from SD.");
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void settingsInit(void)
{
    /* Set defaults first, then override from file */
    s_sound_muted = false;
    s_click_tone  = BUZZ_TONE_CLICK;
    s_wifi_ssid[0]     = '\0';
    s_wifi_password[0] = '\0';
    s_owm_key[0]       = '\0';
    s_owm_city[0]      = '\0';
    strncpy(s_owm_units, "imperial", sizeof(s_owm_units) - 1);
    s_companion_ip[0]  = '\0';
    load_from_sd();

    /* Push loaded values into their respective subsystems */
    buzzerSetMuted(s_sound_muted);
    buzzerSetClickTone(s_click_tone);

    Serial.printf("[Settings] Applied: sound_muted=%d  click_tone=%d  wifi=%s  city=%s  units=%s\n",
                  (int)s_sound_muted, (int)s_click_tone,
                  s_wifi_ssid[0] ? s_wifi_ssid : "(none)",
                  s_owm_city[0]  ? s_owm_city  : "(none)",
                  s_owm_units);
}

void settingsSave(void)
{
    if (!sdCardMounted()) {
        Serial.println("[Settings] SD not mounted — save skipped.");
        return;
    }

    /* Always read live state from subsystems so the file stays in sync */
    s_sound_muted = buzzerIsMuted();
    s_click_tone  = buzzerGetClickTone();

    if (SD.exists(SETTINGS_FILE)) SD.remove(SETTINGS_FILE);

    File f = SD.open(SETTINGS_FILE, FILE_WRITE);
    if (!f) {
        Serial.println("[Settings] ERROR: Could not open settings file for write.");
        return;
    }

    f.printf("# HelpDesk settings — edited by device\n");
    f.printf("sound_muted=%d\n",   (int)s_sound_muted);
    f.printf("click_tone=%d\n",    (int)s_click_tone);
    f.printf("wifi_ssid=%s\n",     s_wifi_ssid);
    f.printf("wifi_password=%s\n", s_wifi_password);
    f.printf("owm_key=%s\n",       s_owm_key);
    f.printf("owm_city=%s\n",      s_owm_city);
    f.printf("owm_units=%s\n",     s_owm_units);
    f.printf("companion_ip=%s\n",  s_companion_ip);
    f.close();

    Serial.printf("[Settings] Saved: sound_muted=%d  click_tone=%d\n",
                  (int)s_sound_muted, (int)s_click_tone);
}

bool settingsGetSoundMuted(void)        { return s_sound_muted; }
void settingsSetSoundMuted(bool muted)  { s_sound_muted = muted; buzzerSetMuted(muted); }

buzz_tone_t settingsGetClickTone(void)          { return s_click_tone; }
void        settingsSetClickTone(buzz_tone_t t) { s_click_tone = t; buzzerSetClickTone(t); }

const char * settingsGetWifiSSID(void)                  { return s_wifi_ssid; }
void         settingsSetWifiSSID(const char * v)        { strncpy(s_wifi_ssid, v, sizeof(s_wifi_ssid) - 1); s_wifi_ssid[sizeof(s_wifi_ssid)-1] = '\0'; }

const char * settingsGetWifiPassword(void)              { return s_wifi_password; }
void         settingsSetWifiPassword(const char * v)    { strncpy(s_wifi_password, v, sizeof(s_wifi_password) - 1); s_wifi_password[sizeof(s_wifi_password)-1] = '\0'; }

const char * settingsGetOwmKey(void)                    { return s_owm_key; }
void         settingsSetOwmKey(const char * v)          { strncpy(s_owm_key, v, sizeof(s_owm_key) - 1); s_owm_key[sizeof(s_owm_key)-1] = '\0'; }

const char * settingsGetOwmCity(void)                   { return s_owm_city; }
void         settingsSetOwmCity(const char * v)         { strncpy(s_owm_city, v, sizeof(s_owm_city) - 1); s_owm_city[sizeof(s_owm_city)-1] = '\0'; }

const char * settingsGetOwmUnits(void)                  { return s_owm_units; }
void         settingsSetOwmUnits(const char * v)        { strncpy(s_owm_units, v, sizeof(s_owm_units) - 1); s_owm_units[sizeof(s_owm_units)-1] = '\0'; }

const char * settingsGetCompanionIP(void)               { return s_companion_ip; }
void         settingsSetCompanionIP(const char * v)     { strncpy(s_companion_ip, v, sizeof(s_companion_ip) - 1); s_companion_ip[sizeof(s_companion_ip)-1] = '\0'; }
