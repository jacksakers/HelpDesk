// Project  : HelpDesk
// File     : pc_monitor.h
// Purpose  : PC metrics receiver — parses companion-app serial JSON, updates Screen7
// Depends  : ui_Screen7.h

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Call once in setup() after Serial.begin().
void initPcMonitor(void);

// Call every loop() iteration.  Non-blocking — drains the Serial RX buffer,
// parses any complete JSON lines, and updates the Screen7 LVGL widgets.
void handlePcMonitor(unsigned long now_ms);

#ifdef __cplusplus
} /* extern "C" */
#endif
