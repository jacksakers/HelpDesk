// Project  : HelpDesk
// File     : weather.cpp
// Purpose  : OpenWeatherMap weather fetch -- updates DeskDash weather labels
// Depends  : weather.h, ui_Screen2.h, settings.h, ArduinoJson, HTTPClient, WiFi

#include "weather.h"
#include "settings.h"
#include "ui.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <lvgl.h>

static unsigned long s_last_weather_ms = 0;
#define WEATHER_INTERVAL_MS 600000UL   // 10 minutes

/* Last successfully fetched data — applied when Screen2 opens */
static char s_last_temp[16] = "";
static char s_last_cond[32] = "";

/* Percent-encode a string for use in a URL query parameter.
 * Letters, digits, '-', '_', '.', '~' are passed through unchanged;
 * everything else (including spaces) is encoded as %XX. */
static void url_encode(const char * src, char * dst, size_t dst_size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t di = 0;
    for (size_t si = 0; src[si] != '\0' && di + 4 < dst_size; si++) {
        unsigned char c = (unsigned char)src[si];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            dst[di++] = (char)c;
        } else {
            dst[di++] = '%';
            dst[di++] = hex[c >> 4];
            dst[di++] = hex[c & 0xF];
        }
    }
    dst[di] = '\0';
}

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
    char city_enc[128];
    url_encode(city, city_enc, sizeof(city_enc));
    snprintf(url, sizeof(url),
             "http://api.openweathermap.org/data/2.5/weather"
             "?q=%s&units=%s&appid=%s",
             city_enc, units, key);
    Serial.printf("[Weather] Fetching: %s\n", url);

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

    /* Cache so weatherApplyToScreen() can re-apply when Screen2 opens */
    strncpy(s_last_temp, temp_str, sizeof(s_last_temp) - 1);
    s_last_temp[sizeof(s_last_temp) - 1] = '\0';
    strncpy(s_last_cond, cond,     sizeof(s_last_cond) - 1);
    s_last_cond[sizeof(s_last_cond) - 1] = '\0';

    weatherApplyToScreen();
}

// --- Apply cached data -------------------------------------------------------
void weatherApplyToScreen(void)
{
    if (ui_WeatherLabel     && s_last_temp[0])
        lv_label_set_text(ui_WeatherLabel,     s_last_temp);
    if (ui_WeatherCondLabel && s_last_cond[0])
        lv_label_set_text(ui_WeatherCondLabel, s_last_cond);
}

// --- Loop handler ------------------------------------------------------------
void handleWeatherUpdate(unsigned long now_ms)
{
    if (now_ms - s_last_weather_ms < WEATHER_INTERVAL_MS &&
        s_last_weather_ms != 0) return;

    s_last_weather_ms = now_ms;
    getWeatherData();
}
