// Project  : HelpDesk
// File     : pomo_timer.cpp
// Purpose  : Pomodoro timer — state machine, arc + label updates, buzzer chime
// Depends  : pomo_timer.h, ui_Screen3.h, buzzer.h

#include "pomo_timer.h"
#include "ui_Screen3.h"
#include "buzzer.h"
#include <Arduino.h>

// ─── Phase ────────────────────────────────────────────────────────────────────
typedef enum {
    PHASE_WORK  = 0,
    PHASE_BREAK = 1
} pomo_phase_t;

// ─── State ────────────────────────────────────────────────────────────────────
static pomo_phase_t s_phase       = PHASE_WORK;
static bool         s_running     = false;
static uint32_t     s_duration_s  = POMO_WORK_SECS;
static uint32_t     s_remaining_s = POMO_WORK_SECS;
static unsigned long s_tick_ms    = 0;  // millis() of the last counted second

// ─── Helpers ──────────────────────────────────────────────────────────────────

// Refresh time label ("MM:SS") and arc value (0–100) from s_remaining_s
static void update_display(void)
{
    // Widgets are null when Screen3 isn't active
    if (!ui_PomoTimeLabel && !ui_PomoArc) return;

    uint32_t m = s_remaining_s / 60;
    uint32_t s = s_remaining_s % 60;

    if (ui_PomoTimeLabel) {
        char buf[6];
        snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
        lv_label_set_text(ui_PomoTimeLabel, buf);
    }

    if (ui_PomoArc) {
        int32_t pct = (s_duration_s > 0)
                    ? (int32_t)((uint64_t)s_remaining_s * 100 / s_duration_s)
                    : 0;
        lv_arc_set_value(ui_PomoArc, (int32_t)pct);
    }
}

// Set phase label text and colour
static void update_phase_label(void)
{
    if (!ui_PomoPhaseLabel) return;

    if (s_phase == PHASE_WORK) {
        lv_label_set_text(ui_PomoPhaseLabel, "WORK");
        lv_obj_set_style_text_color(ui_PomoPhaseLabel, lv_color_hex(0xF44336), 0);
    } else {
        lv_label_set_text(ui_PomoPhaseLabel, "BREAK");
        lv_obj_set_style_text_color(ui_PomoPhaseLabel, lv_color_hex(0x1565C0), 0);
    }
}

// Transition to the other phase (called when timer hits 0)
static void advance_phase(void)
{
    s_phase = (s_phase == PHASE_WORK) ? PHASE_BREAK : PHASE_WORK;
    s_duration_s  = (s_phase == PHASE_WORK) ? POMO_WORK_SECS : POMO_BREAK_SECS;
    s_remaining_s = s_duration_s;
    s_running     = false;  // pause between phases — user taps Start to begin

    buzzerPlay(BUZZ_TONE_SUCCESS);
    update_phase_label();
    update_display();

    Serial.printf("[Pomo] Phase -> %s (%lu s)\n",
                  s_phase == PHASE_WORK ? "WORK" : "BREAK",
                  (unsigned long)s_duration_s);
}

// ─── Public API ───────────────────────────────────────────────────────────────

extern "C" void initPomoTimer(void)
{
    s_phase       = PHASE_WORK;
    s_running     = false;
    s_duration_s  = POMO_WORK_SECS;
    s_remaining_s = POMO_WORK_SECS;
    s_tick_ms     = 0;
    Serial.println("[Pomo] Timer initialised.");
}

extern "C" void handlePomoTimer(unsigned long now_ms)
{
    if (!s_running) return;

    // Initialise tick reference on first call after Start
    if (s_tick_ms == 0) {
        s_tick_ms = now_ms;
        return;
    }

    unsigned long elapsed = now_ms - s_tick_ms;
    if (elapsed < 1000UL) return;   // less than a second

    // Advance by whole seconds only (avoids drift)
    uint32_t secs = (uint32_t)(elapsed / 1000UL);
    s_tick_ms += secs * 1000UL;

    if (secs >= s_remaining_s) {
        // Timer expired
        s_remaining_s = 0;
        s_running     = false;
        update_display();
        advance_phase();
    } else {
        s_remaining_s -= secs;
        update_display();
    }
}

extern "C" void pomoStart(void)
{
    if (s_running) return;
    if (s_remaining_s == 0) return;  // must reset first
    s_running = true;
    s_tick_ms = 0;  // will be set on next handlePomoTimer() call
    buzzerPlay(BUZZ_TONE_CLICK);
    Serial.printf("[Pomo] Started. %lu s remaining.\n", (unsigned long)s_remaining_s);
}

extern "C" void pomoStop(void)
{
    if (!s_running) return;
    s_running = false;
    buzzerPlay(BUZZ_TONE_CLICK);
    Serial.println("[Pomo] Paused.");
}

extern "C" void pomoReset(void)
{
    s_running     = false;
    s_remaining_s = s_duration_s;
    s_tick_ms     = 0;
    update_display();
    buzzerPlay(BUZZ_TONE_CLICK);
    Serial.printf("[Pomo] Reset to %lu s.\n", (unsigned long)s_duration_s);
}

extern "C" void pomoSetWorkMode(void)
{
    s_phase       = PHASE_WORK;
    s_running     = false;
    s_duration_s  = POMO_WORK_SECS;
    s_remaining_s = POMO_WORK_SECS;
    s_tick_ms     = 0;
    update_phase_label();
    update_display();
    Serial.println("[Pomo] Mode -> WORK");
}

extern "C" void pomoSetBreakMode(void)
{
    s_phase       = PHASE_BREAK;
    s_running     = false;
    s_duration_s  = POMO_BREAK_SECS;
    s_remaining_s = POMO_BREAK_SECS;
    s_tick_ms     = 0;
    update_phase_label();
    update_display();
    Serial.println("[Pomo] Mode -> BREAK");
}
