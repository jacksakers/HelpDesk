// Project  : HelpDesk
// File     : wifi_connect.cpp
// Purpose  : WiFi connection -- implementation
// Depends  : wifi_connect.h, settings.h, Arduino.h

#include "wifi_connect.h"
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

// --- Timeout -----------------------------------------------------------------
#define WIFI_CONNECT_TIMEOUT_MS 15000UL

// --- Implementation ----------------------------------------------------------

void connectToWiFi()
{
    /* Prefer credentials saved via the companion app; fall back to compile-time defaults. */
    const char * ssid = settingsGetWifiSSID();
    const char * pass = settingsGetWifiPassword();
    if (!ssid || ssid[0] == '\0') { ssid = HELPDESK_DEFAULT_WIFI_SSID; }
    if (!pass || pass[0] == '\0') { pass = HELPDESK_DEFAULT_WIFI_PASSWORD; }

    if (!ssid || ssid[0] == '\0') {
        Serial.println("[WiFi] No SSID configured. Set credentials via the companion app.");
        return;
    }

    Serial.print("[WiFi] Connecting to: ");
    Serial.println(ssid);

    WiFi.begin(ssid, pass);

    unsigned long start = millis();
    while(WiFi.status() != WL_CONNECTED) {
        if(millis() - start >= WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println("\n[WiFi] Connection timed out. Continuing offline.");
            return;
        }
        delay(500);
        Serial.print(".");
    }

    Serial.print("\n[WiFi] Connected. IP: ");
    Serial.println(WiFi.localIP());
}
