// Project  : HelpDesk
// File     : buzzer.cpp
// Purpose  : Passive-buzzer tone playback via ESP32 LEDC (non-blocking)
// Depends  : buzzer.h, Arduino ESP32

#include "buzzer.h"
#include <Arduino.h>

/* ── Hardware ────────────────────────────────────────────── */
#define BUZZER_PIN    8
#define LEDC_RES      8          /* 8-bit resolution (duty 0–255)   */
#define LEDC_DUTY_50  128        /* 50% duty = loudest passive buzz */

/* ── Tone sequence table ─────────────────────────────────── */
/*
 * Each tone is a sequence of {freq_hz, duration_ms} steps.
 * Terminate a sequence with {0, 0}. MAX_STEPS includes the sentinel.
 *
 * CLICK    — short sharp high-pitched tick
 * NAVIGATE — quick two-note rising chirp (screen swipe)
 * SUCCESS  — ascending two-note ding (positive feedback)
 * ERROR    — descending two-note buzz (negative feedback)
 */
typedef struct { uint16_t freq_hz; uint16_t ms; } tone_step_t;

#define MAX_STEPS  4
#define END_STEP   { 0, 0 }

static const tone_step_t k_seqs[BUZZ_TONE_COUNT][MAX_STEPS] = {
    /* CLICK    */ {{ 2400,  40 }, END_STEP},
    /* NAVIGATE */ {{ 1600,  30 }, { 2200,  50 }, END_STEP},
    /* SUCCESS  */ {{ 1200,  80 }, { 2400,  80 }, END_STEP},
    /* ERROR    */ {{ 2400,  70 }, {  500, 110 }, END_STEP},
};

/* ── Playback state ──────────────────────────────────────── */
static bool         s_muted      = false;
static buzz_tone_t  s_click_tone = BUZZ_TONE_CLICK;
static bool         s_playing    = false;
static uint8_t      s_cur_tone   = 0;
static uint8_t      s_cur_step   = 0;
static uint32_t     s_step_start = 0;

/* ── Internal ────────────────────────────────────────────── */
static void start_step(uint8_t tone_idx, uint8_t step_idx)
{
    const tone_step_t * step = &k_seqs[tone_idx][step_idx];
    if(step->freq_hz == 0 && step->ms == 0) {
        /* Sequence complete — silence */
        ledcWrite(BUZZER_PIN, 0);
        s_playing = false;
        return;
    }
    ledcWriteTone(BUZZER_PIN, step->freq_hz);
    ledcWrite(BUZZER_PIN, LEDC_DUTY_50);
    s_cur_tone   = tone_idx;
    s_cur_step   = step_idx;
    s_step_start = (uint32_t)millis();
    s_playing    = true;
}

/* ── Public API ──────────────────────────────────────────── */
void buzzerInit(void)
{
    ledcAttach(BUZZER_PIN, 2000, LEDC_RES);
    ledcWrite(BUZZER_PIN, 0);   /* Silent on start */
}

void buzzerPlay(buzz_tone_t tone)
{
    if(s_muted) return;
    if((uint8_t)tone >= BUZZ_TONE_COUNT) return;
    start_step((uint8_t)tone, 0);
}

void buzzerLoop(void)
{
    if(!s_playing) return;
    if((uint32_t)(millis() - s_step_start) >= k_seqs[s_cur_tone][s_cur_step].ms) {
        start_step(s_cur_tone, s_cur_step + 1);
    }
}

void buzzerSetMuted(bool muted)
{
    s_muted = muted;
    if(muted) {
        ledcWrite(BUZZER_PIN, 0);
        s_playing = false;
    }
}

bool buzzerIsMuted(void)
{
    return s_muted;
}

void buzzerSetClickTone(buzz_tone_t tone)
{
    if((uint8_t)tone < BUZZ_TONE_COUNT) s_click_tone = tone;
}

buzz_tone_t buzzerGetClickTone(void)
{
    return s_click_tone;
}
