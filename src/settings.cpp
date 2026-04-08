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
    load_from_sd();

    /* Push loaded values into their respective subsystems */
    buzzerSetMuted(s_sound_muted);
    buzzerSetClickTone(s_click_tone);

    Serial.printf("[Settings] Applied: sound_muted=%d  click_tone=%d\n",
                  (int)s_sound_muted, (int)s_click_tone);
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
    f.printf("sound_muted=%d\n", (int)s_sound_muted);
    f.printf("click_tone=%d\n",  (int)s_click_tone);
    f.close();

    Serial.printf("[Settings] Saved: sound_muted=%d  click_tone=%d\n",
                  (int)s_sound_muted, (int)s_click_tone);
}

bool settingsGetSoundMuted(void)        { return s_sound_muted; }
void settingsSetSoundMuted(bool muted)  { s_sound_muted = muted; buzzerSetMuted(muted); }

buzz_tone_t settingsGetClickTone(void)          { return s_click_tone; }
void        settingsSetClickTone(buzz_tone_t t) { s_click_tone = t; buzzerSetClickTone(t); }
