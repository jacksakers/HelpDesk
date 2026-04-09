// Project  : HelpDesk
// File     : handshake.cpp
// Purpose  : Serial handshake — broadcasts device info and accepts timesync/settings from companion
// Depends  : handshake.h, sd_card.h, settings.h, WiFi.h, <SD.h>, ArduinoJson

#include "handshake.h"
#include "sd_card.h"
#include "settings.h"
#include <Arduino.h>
#include <WiFi.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <sys/time.h>

// ── Constants ─────────────────────────────────────────────────────────────────
#define HANDSHAKE_STATUS_INTERVAL_MS  10000UL   // Lightweight status every 10 s (heartbeat)
#define FW_VERSION                    "1.0"

// ── Private state ─────────────────────────────────────────────────────────────
static unsigned long s_last_status_ms = 0;
static char          s_screen_name[25] = "launcher";

// Serial RX line buffer — accumulates bytes until \n, then parsed as JSON
static char s_rx_buf[384];
static int  s_rx_pos = 0;

// ── Serial RX helpers ─────────────────────────────────────────────────────────

static void parse_rx_line(const char * line)
{
    JsonDocument doc;
    if (deserializeJson(doc, line) != DeserializationError::Ok) return;

    const char * evt = doc["event"];
    if (!evt) return;

    if (strcmp(evt, "timesync") == 0) {
        long ts = doc["ts"] | 0L;
        if (ts > 0) {
            struct timeval tv = { .tv_sec = (time_t)ts, .tv_usec = 0 };
            settimeofday(&tv, nullptr);
            Serial.printf("[Serial] Timesync applied: %ld\n", ts);
        }
        return;
    }

    if (strcmp(evt, "settings") == 0) {
        if (doc["wifi_ssid"].is<const char*>())     settingsSetWifiSSID(doc["wifi_ssid"]);
        if (doc["wifi_password"].is<const char*>()) settingsSetWifiPassword(doc["wifi_password"]);
        if (doc["owm_api_key"].is<const char*>())   settingsSetOwmKey(doc["owm_api_key"]);
        if (doc["zip_code"].is<const char*>())      settingsSetOwmCity(doc["zip_code"]);
        if (doc["units"].is<const char*>())         settingsSetOwmUnits(doc["units"]);
        settingsSave();
        Serial.println("[Serial] Settings updated and saved to SD.");
        return;
    }
}

static void handle_serial_rx(void)
{
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (s_rx_pos > 0) {
                s_rx_buf[s_rx_pos] = '\0';
                parse_rx_line(s_rx_buf);
                s_rx_pos = 0;
            }
        } else if (s_rx_pos < (int)sizeof(s_rx_buf) - 1) {
            s_rx_buf[s_rx_pos++] = c;
        } else {
            // Buffer overflow — discard and start fresh
            s_rx_pos = 0;
        }
    }
}

// ── Private helpers ───────────────────────────────────────────────────────────

static void send_hello(void)
{
    /* Guard: WiFi.localIP() returns "0.0.0.0" when offline — that is fine.
       The companion app filters it out and does not auto-save the placeholder. */
    bool sd_ok = sdCardMounted();

    const char * ssid = settingsGetWifiSSID();
    if (!ssid) ssid = "";

    char buf[224];
    snprintf(buf, sizeof(buf),
        "{\"event\":\"hello\","
        "\"fw\":\"%s\","
        "\"ip\":\"%s\","
        "\"ssid\":\"%s\","
        "\"sd_ok\":%s,"
        "\"sd_total_mb\":%d,"
        "\"sd_used_mb\":%d}\n",
        FW_VERSION,
        WiFi.localIP().toString().c_str(),
        ssid,
        sd_ok ? "true" : "false",
        sd_ok ? (int)(SD.totalBytes() / (1024ULL * 1024ULL)) : 0,
        sd_ok ? (int)(SD.usedBytes()  / (1024ULL * 1024ULL)) : 0);

    Serial.print(buf);
    Serial.flush();
}

static void send_status(void)
{
    bool sd_ok = sdCardMounted();

    char buf[96];
    snprintf(buf, sizeof(buf),
        "{\"event\":\"status\","
        "\"screen\":\"%s\","
        "\"sd_used_mb\":%d}\n",
        s_screen_name,
        sd_ok ? (int)(SD.usedBytes() / (1024ULL * 1024ULL)) : 0);

    Serial.print(buf);
    Serial.flush();
}

// ── Public API ────────────────────────────────────────────────────────────────

void handshakeInit(void)
{
    s_last_status_ms = 0;
    send_hello();
}

void handleHandshake(unsigned long now_ms)
{
    // Always drain the serial RX buffer regardless of status interval
    handle_serial_rx();

    if (now_ms - s_last_status_ms < HANDSHAKE_STATUS_INTERVAL_MS) return;
    s_last_status_ms = now_ms;
    send_status();
}

void handshakeSendHello(void)
{
    send_hello();
}

void handshakeSetScreen(const char * name)
{
    strncpy(s_screen_name, name, sizeof(s_screen_name) - 1);
    s_screen_name[sizeof(s_screen_name) - 1] = '\0';
}
