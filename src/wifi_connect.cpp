// Project  : HelpDesk
// File     : wifi_connect.cpp
// Purpose  : WiFi connection -- implementation
// Depends  : wifi_connect.h, wifi_status.h, settings.h, Arduino.h

#include "wifi_connect.h"
#include "wifi_status.h"
#include "settings.h"
#include <Arduino.h>

// --- Compile-time fallback credentials ---------------------------------------
// Used only when no WiFi credentials have been saved via the companion app.
// Override via platformio.ini build_flags:
//   -DHELPDESK_DEFAULT_WIFI_SSID='"My Network"'
//   -DHELPDESK_DEFAULT_WIFI_PASSWORD='"secret"'
#ifndef HELPDESK_DEFAULT_WIFI_SSID
  #define HELPDESK_DEFAULT_WIFI_SSID     ""
  #define HELPDESK_DEFAULT_WIFI_PASSWORD ""
#endif

// Legacy extern symbols kept for any code that references wifi_ssid directly.
const char * wifi_ssid     = HELPDESK_DEFAULT_WIFI_SSID;
const char * wifi_password = HELPDESK_DEFAULT_WIFI_PASSWORD;

// --- Private state -----------------------------------------------------------
static bool s_was_connected   = false;
static bool s_just_connected  = false;  /* true for exactly one wifiHandleConnection tick */

// --- Implementation ----------------------------------------------------------

/* Non-blocking: fires off WiFi.begin() and returns immediately.
   The UI loads before the connection completes.                               */
void connectToWiFi()
{
    /* Prefer credentials saved via the companion app; fall back to compile-time defaults. */
    const char * ssid = settingsGetWifiSSID();
    const char * pass = settingsGetWifiPassword();
    if (!ssid || ssid[0] == '\0') { ssid = HELPDESK_DEFAULT_WIFI_SSID; }
    if (!pass || pass[0] == '\0') { pass = HELPDESK_DEFAULT_WIFI_PASSWORD; }

    if (!ssid || ssid[0] == '\0') {
        Serial.println("[WiFi] No SSID configured — skipping. Set credentials via companion app.");
        return;
    }

    Serial.print("[WiFi] Starting background connection to: ");
    Serial.println(ssid);
    WiFi.begin(ssid, pass);
}

// --- Status API (extern "C" so these are callable from C files) --------------

extern "C" bool wifiIsConnected(void)
{
    return WiFi.status() == WL_CONNECTED;
}

extern "C" bool wifiJustConnected(void)
{
    bool val = s_just_connected;
    s_just_connected = false;   /* consume the one-shot flag */
    return val;
}

extern "C" void wifiHandleConnection(unsigned long now)
{
    static unsigned long s_last_check = 0;
    if (now - s_last_check < 1000UL) return;
    s_last_check = now;

    bool connected = (WiFi.status() == WL_CONNECTED);

    if (connected && !s_was_connected) {
        Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
        s_was_connected  = true;
        s_just_connected = true;
    } else if (!connected && s_was_connected) {
        Serial.println("[WiFi] Disconnected.");
        s_was_connected  = false;
        s_just_connected = false;
    }
}
