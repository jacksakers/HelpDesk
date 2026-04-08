// Project  : HelpDesk
// File     : weather.cpp
// Purpose  : OpenWeatherMap weather fetch — updates DeskDash weather labels
// Depends  : weather.h, ui_Screen2.h, ArduinoJson, HTTPClient, WiFi

#include "weather.h"
#include "ui_Screen2.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static unsigned long s_last_weather_ms = 0;
#define WEATHER_INTERVAL_MS 600000UL   // 10 minutes

// ─── Internal fetch ───────────────────────────────────────────────────────────
void getWeatherData(void)
{
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Weather] WiFi not connected, skipping fetch.");
        return;
    }

    // Build URL — use q= (city name) for easy config; swap to id= for city ID
    char url[256];
    snprintf(url, sizeof(url),
             "http://api.openweathermap.org/data/2.5/weather"
             "?q=%s&units=imperial&appid=%s",
             HELPDESK_OWM_CITY, HELPDESK_OWM_KEY);

    HTTPClient http;
    http.begin(url);
    int code = http.GET();

    if (code != HTTP_CODE_OK) {
        Serial.printf("[Weather] HTTP error %d\n", code);
        // Update label to show the error if Screen2 is active
        if (ui_WeatherCondLabel) lv_label_set_text(ui_WeatherCondLabel, "Fetch error");
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();

    // Parse JSON
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[Weather] JSON parse error: %s\n", err.c_str());
        return;
    }

    float   temp_f    = doc["main"]["temp"].as<float>();
    float   temp_c    = (temp_f - 32.0f) * 5.0f / 9.0f;
    int     humidity  = doc["main"]["humidity"].as<int>();
    const char * cond = doc["weather"][0]["main"] | "Unknown";
    const char * city = doc["name"] | HELPDESK_OWM_CITY;

    char temp_str[12];
    snprintf(temp_str, sizeof(temp_str), "%.1f°F / %.0f°C", temp_f, temp_c);

    Serial.printf("[Weather] %s: %s, %s, hum=%d%%\n", city, cond, temp_str, humidity);

    // Update Screen2 labels only if they are live
    if (ui_WeatherLabel)     lv_label_set_text(ui_WeatherLabel,     temp_str);
    if (ui_WeatherCondLabel) lv_label_set_text(ui_WeatherCondLabel, cond);
}

// ─── Loop handler ─────────────────────────────────────────────────────────────
void handleWeatherUpdate(unsigned long now_ms)
{
    // On first call (s_last_weather_ms == 0) fetch immediately.
    // Afterwards respect the 10-minute interval.
    if (now_ms - s_last_weather_ms < WEATHER_INTERVAL_MS &&
        s_last_weather_ms != 0) return;

    s_last_weather_ms = now_ms;
    getWeatherData();
}
