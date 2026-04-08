// Project  : HelpDesk
// File     : weather.cpp
// Purpose  : OpenWeatherMap weather fetch -- updates DeskDash weather labels
// Depends  : weather.h, ui_Screen2.h, settings.h, ArduinoJson, HTTPClient, WiFi

#include "weather.h"
#include "settings.h"
#include "ui_Screen2.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static unsigned long s_last_weather_ms = 0;
#define WEATHER_INTERVAL_MS 600000UL   // 10 minutes

// --- Internal fetch ----------------------------------------------------------
void getWeatherData(void)
{
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Weather] WiFi not connected, skipping fetch.");
        return;
    }

    /* Runtime settings take priority; compile-time defines are the fallback. */
    const char * key   = settingsGetOwmKey();
    const char * city  = settingsGetOwmCity();
    const char * units = settingsGetOwmUnits();
    if (!key   || key[0]   == '\0') key   = HELPDESK_OWM_KEY;
    if (!city  || city[0]  == '\0') city  = HELPDESK_OWM_CITY;
    if (!units || units[0] == '\0') units = "imperial";

    if (strcmp(key, "YOUR_API_KEY_HERE") == 0) {
        Serial.println("[Weather] API key not set. Configure via companion app.");
        if (ui_WeatherCondLabel) lv_label_set_text(ui_WeatherCondLabel, "No API key");
        return;
    }

    char url[256];
    snprintf(url, sizeof(url),
             "http://api.openweathermap.org/data/2.5/weather"
             "?q=%s&units=%s&appid=%s",
             city, units, key);

    HTTPClient http;
    http.begin(url);
    int code = http.GET();

    if (code != HTTP_CODE_OK) {
        Serial.printf("[Weather] HTTP error %d\n", code);
        if (ui_WeatherCondLabel) lv_label_set_text(ui_WeatherCondLabel, "Fetch error");
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[Weather] JSON parse error: %s\n", err.c_str());
        return;
    }

    float       temp  = doc["main"]["temp"].as<float>();
    int         hum   = doc["main"]["humidity"].as<int>();
    const char* cond  = doc["weather"][0]["main"] | "Unknown";
    const char* city_ = doc["name"] | city;

    /* Format temperature in whichever units the API returned. */
    const char * unit_sym = (strcmp(units, "metric") == 0) ? "\xC2\xB0C" : "\xC2\xB0F";
    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%.0f%s", temp, unit_sym);

    Serial.printf("[Weather] %s: %s, %s, hum=%d%%\n", city_, cond, temp_str, hum);

    if (ui_WeatherLabel)     lv_label_set_text(ui_WeatherLabel,     temp_str);
    if (ui_WeatherCondLabel) lv_label_set_text(ui_WeatherCondLabel, cond);
}

// --- Loop handler ------------------------------------------------------------
void handleWeatherUpdate(unsigned long now_ms)
{
    if (now_ms - s_last_weather_ms < WEATHER_INTERVAL_MS &&
        s_last_weather_ms != 0) return;

    s_last_weather_ms = now_ms;
    getWeatherData();
}
