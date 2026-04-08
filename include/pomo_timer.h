// Project  : HelpDesk
// File     : pomo_timer.h
// Purpose  : Pomodoro timer — public interface (C-compatible)
// Depends  : buzzer.h, ui_Screen3.h

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// ─── Configuration ────────────────────────────────────────────────────────────
#ifndef POMO_WORK_SECS
  #define POMO_WORK_SECS  (25 * 60)  /* 25-minute work interval  */
#endif
#ifndef POMO_BREAK_SECS
  #define POMO_BREAK_SECS  (5 * 60)  /* 5-minute short break     */
#endif

// ─── API ─────────────────────────────────────────────────────────────────────
// Call once in setup().
void initPomoTimer(void);

// Call every loop() iteration; advances the countdown.
void handlePomoTimer(unsigned long now_ms);

// Button callbacks — call from LVGL event handlers
void pomoStart(void);
void pomoStop(void);
void pomoReset(void);
void pomoSetWorkMode(void);
void pomoSetBreakMode(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
