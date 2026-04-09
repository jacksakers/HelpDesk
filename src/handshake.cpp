// Project  : HelpDesk
// File     : handshake.cpp
// Purpose  : Serial handshake — broadcasts device info and accepts timesync/settings from companion
// Depends  : handshake.h, sd_card.h, settings.h, WiFi.h, <SD.h>

#include "handshake.h"
#include "sd_card.h"
#include "settings.h"
#include <Arduino.h>
#include <WiFi.h>
#include <SD.h>

// ── Constants ─────────────────────────────────────────────────────────────────
#define HANDSHAKE_STATUS_INTERVAL_MS  10000UL   // Lightweight status every 10 s (heartbeat)
#define FW_VERSION                    "1.0"

// ── Private state ─────────────────────────────────────────────────────────────
static unsigned long s_last_status_ms = 0;
static char          s_screen_name[25] = "launcher";

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
    /* Notify the companion immediately — don't wait for the next heartbeat. */
    send_status();
}
