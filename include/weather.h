// Project  : HelpDesk
// File     : weather.h
// Purpose  : OpenWeatherMap weather fetch — public interface
// Depends  : wifi_connect.h, ArduinoJson, HTTPClient

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// ─── Configuration ────────────────────────────────────────────────────────────
// Set your OpenWeatherMap API key and city.
// Sign up free at https://openweathermap.org/api
//
// HELPDESK_OWM_KEY  — your API key string
// HELPDESK_OWM_CITY — city name (e.g. "New York") or city ID (numeric string)
//                     City IDs: https://bulk.openweathermap.org/sample/
//
// Override in platformio.ini build_flags, e.g.:
//   -DHELPDESK_OWM_KEY=\"abc123\"
//   -DHELPDESK_OWM_CITY=\"Boston\"
#ifndef HELPDESK_OWM_KEY
  #define HELPDESK_OWM_KEY  "YOUR_API_KEY_HERE"
#endif
#ifndef HELPDESK_OWM_CITY
  #define HELPDESK_OWM_CITY "New York"
#endif

// ─── API ─────────────────────────────────────────────────────────────────────
// Fetch once immediately then call handleWeatherUpdate() in loop().
void getWeatherData(void);

// Call every loop() iteration. Re-fetches every 10 minutes.
// Only acts when Screen2 widgets are live.
void handleWeatherUpdate(unsigned long now_ms);

#ifdef __cplusplus
} /* extern "C" */
#endif
