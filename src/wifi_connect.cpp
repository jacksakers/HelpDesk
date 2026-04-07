// Project  : HelpDesk
// File     : wifi_connect.cpp
// Purpose  : WiFi connection — implementation
// Depends  : wifi_connect.h, Arduino.h

#include "wifi_connect.h"
#include <Arduino.h>

// ─── Credentials — update these before flashing ─────────────────────────────
const char * wifi_ssid     = "YOUR_WIFI_SSID";
const char * wifi_password = "YOUR_WIFI_PASSWORD";

// ─── Timeout ────────────────────────────────────────────────────────────────
#define WIFI_CONNECT_TIMEOUT_MS 15000UL

// ─── Implementation ──────────────────────────────────────────────────────────

void connectToWiFi()
{
    Serial.print("[WiFi] Connecting to: ");
    Serial.println(wifi_ssid);

    WiFi.begin(wifi_ssid, wifi_password);

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
