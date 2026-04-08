// Project  : HelpDesk
// File     : buzzer.h
// Purpose  : Passive-buzzer tone playback (non-blocking, multi-tone)
// Depends  : Arduino ESP32 LEDC (GPIO 8)

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* ── Available tones ─────────────────────────────────────── */
typedef enum {
    BUZZ_TONE_CLICK    = 0,   /* Short high-freq click feedback   */
    BUZZ_TONE_NAVIGATE = 1,   /* Two-note rising swipe            */
    BUZZ_TONE_SUCCESS  = 2,   /* Two-note ascending ding          */
    BUZZ_TONE_ERROR    = 3,   /* Two-note descending buzz         */
    BUZZ_TONE_COUNT    = 4
} buzz_tone_t;

/* ── API ─────────────────────────────────────────────────── */
void         buzzerInit(void);
void         buzzerPlay(buzz_tone_t tone);
void         buzzerLoop(void);              /* call every loop() iteration */

void         buzzerSetMuted(bool muted);
bool         buzzerIsMuted(void);

void         buzzerSetClickTone(buzz_tone_t tone);
buzz_tone_t  buzzerGetClickTone(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
