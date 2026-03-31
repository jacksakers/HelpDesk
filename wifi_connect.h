// Project  : HelpDesk
// File     : wifi_connect.h
// Purpose  : WiFi connection — public interface
// Depends  : WiFi.h

#pragma once
#include <WiFi.h>

// SSID and password are defined in wifi_connect.cpp.
// Update them there before flashing.
extern const char * wifi_ssid;
extern const char * wifi_password;

// Attempt to connect; blocks until connected or times out.
// Prints status to Serial.
void connectToWiFi();
