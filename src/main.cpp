// Project  : HelpDesk
// File     : HelpDesk.ino
// Purpose  : Main Arduino sketch — initialise hardware and run the event loop
// Depends  : display_Screen.h, wifi_connect.h, get_time.h, weather.h, pomo_timer.h, ui.h

#include <Arduino.h>
#include <lvgl.h>
#include "ui.h"

#include "display_Screen.h"
#include "wifi_connect.h"
#include "buzzer.h"
#include "sd_card.h"
#include "settings.h"
#include "zen_frame.h"
#include "get_time.h"
#include "weather.h"
#include "pomo_timer.h"
#include "http_server.h"
#include "pc_monitor.h"
#include "handshake.h"

// ─── Update intervals ──────────────────────────────────────────────────────────
#define NTP_SYNC_INTERVAL_MS     600000UL   // 10 minutes
#define WEATHER_UPDATE_INTERVAL_MS 600000UL // 10 minutes

void setup()
{
    // ── VERY FIRST THING: blink the backlight so we KNOW the code runs ──
    // This proves the firmware flashed correctly even if Serial is broken.
    pinMode(38, OUTPUT);
    for(int i = 0; i < 6; i++) {          // 3 quick blinks
        digitalWrite(38, i & 1 ? LOW : HIGH);
        delay(150);
    }
    digitalWrite(38, HIGH);               // leave backlight ON

    Serial.begin(115200);
    delay(500);   // brief settle time for UART

    Serial.println();
    Serial.println("===================================");
    Serial.println("===== HelpDesk starting =====");
    Serial.println("===================================");
    Serial.printf("Free heap : %u bytes\n", ESP.getFreeHeap());
    Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
    Serial.printf("SDK       : %s\n", ESP.getSdkVersion());
    Serial.flush();

    // 1. Hardware: display + touch + LVGL must come first
    Serial.println("[setup] Calling initDisplay...");
    Serial.flush();
    initDisplay();

    // 1b. Buzzer: init after display so LEDC is ready
    buzzerInit();
    Serial.println("[setup] Buzzer ready (GPIO 8)");
    Serial.println("[setup] initDisplay done");
    Serial.flush();

    // 1c. SD card: mount before settings and zen_frame need it
    sdCardInit();

    // 1d. Settings: load persisted values and apply to subsystems (buzzer etc.)
    settingsInit();

    // 2. UI: build the launcher screen
    ui_init();
    Serial.println("[HelpDesk] Display and UI ready.");

    // 2b. ZenFrame: scan /images playlist (ui_ZenImage may be NULL until Screen6 opens)
    zenFrameInit();

    // 3. Network: connect once; feature modules rely on this
    connectToWiFi();
    Serial.println("[HelpDesk] WiFi connected.");

    // 4. Feature module init
    initNTP();
    initPomoTimer();
    initPcMonitor();

    // 5. HTTP server and handshake -- must start after WiFi
    if (WiFi.status() == WL_CONNECTED) {
        httpServerInit();
    }
    // Handshake always fires (sends hello with 0.0.0.0 if offline; companion ignores it)
    handshakeInit();

    Serial.println("[HelpDesk] Init complete. Entering loop.");
}

void loop()
{
    // Let LVGL process animations, touch events, and redraws
    lv_timer_handler();

    // Advance non-blocking multi-note buzzer sequences
    buzzerLoop();

    // ── Tick watchdog: print every 5 s so we know lv_timer_handler is running
    static unsigned long last_tick_log = 0;
    static uint32_t handler_calls = 0;
    handler_calls++;
    unsigned long now = millis();
    if(now - last_tick_log >= 5000UL) {
        Serial.printf("[loop] calls=%lu  lv_tick=%lu ms  heap=%u  psram=%u\n",
                      handler_calls, lv_tick_get(),
                      ESP.getFreeHeap(), ESP.getFreePsram());
        last_tick_log = now;
    }

    // Periodic feature updates
    handleTimeUpdate(now);
    handleWeatherUpdate(now);
    handlePomoTimer(now);
    handleZenFrame(now);
    handlePcMonitor(now);
    handleHandshake(now);
    httpServerLoop();

    // 5 ms yield keeps the LVGL timer accurate without blocking touch input
    delay(5);
}
